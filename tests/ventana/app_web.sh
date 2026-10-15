#!/bin/bash
# Compila la aplicación de ejemplo «Tareas» con --incluir y la prueba empaquetada:
# desde otra carpeta, sin los archivos originales, con peticiones reales.
LINCE="${1:-./lince}"
LINCE="$(cd "$(dirname "$LINCE")" && pwd)/$(basename "$LINCE")"
RAIZ="$(cd "$(dirname "$0")/../.." && pwd)"
T="$(mktemp -d)"
trap 'kill $PID 2>/dev/null; rm -rf "$T"' EXIT
fallos=0
ok()  { echo "  ✅ $1"; }
mal() { echo "  ❌ $1"; fallos=$((fallos+1)); }
echo "🐆 Aplicación web empaquetada"

cp -r "$RAIZ/examples/tareas_web" "$T/fuente"
( cd "$T/fuente" && "$LINCE" compilar app.lince -o "$T/tareas" --incluir plantillas > /dev/null ) || { mal "no compila"; exit 1; }
rm -rf "$T/fuente"                                   # el ejecutable lleva todo dentro
mkdir "$T/ejecucion"
( cd "$T/ejecucion" && exec "$T/tareas" --servidor > "$T/servidor.txt" 2>&1 ) &
PID=$!
for i in $(seq 1 30); do curl -s -o /dev/null localhost:8080/ && break; sleep 0.2; done

pag="$(curl -s localhost:8080/)"
echo "$pag" | grep -q "No hay tareas" && ok "la página se genera desde la plantilla empaquetada" || mal "la página no sale"
curl -s -o /dev/null -w "%{http_code} %{content_type}" localhost:8080/estilo.css | grep -q "200 text/css" && ok "sirve los archivos estáticos empaquetados" || mal "estáticos"
loc="$(curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -d 'texto=Comprar+pan+%26+leche' localhost:8080/anadir)"
[ "$loc" = "302 http://localhost:8080/" ] && ok "añadir redirige a la página" || mal "redirección: $loc"
curl -s -o /dev/null -d 'texto=Probar <b>HTML</b>' localhost:8080/anadir
curl -s -o /dev/null -d 'texto=+++' localhost:8080/anadir
pag="$(curl -s localhost:8080/)"
echo "$pag" | grep -q "2 pendientes de 2" && ok "guarda en SQLite y cuenta (la vacía se rechaza)" || mal "no guardó bien"
echo "$pag" | grep -q "Comprar pan &amp; leche" && ok "escapa el HTML de los datos" || mal "no escapó &"
echo "$pag" | grep -q "&lt;b&gt;HTML&lt;/b&gt;" && ok "no deja pasar etiquetas HTML del usuario" || mal "XSS"
curl -s -o /dev/null -X POST localhost:8080/alternar/1
curl -s localhost:8080/ | grep -q "1 pendientes de 2" && ok "marcar como hecha" || mal "alternar"
curl -s -o /dev/null -X POST localhost:8080/borrar/2
curl -s localhost:8080/ | grep -q "0 pendientes de 1" && ok "borrar" || mal "borrar"
[ "$(curl -s --path-as-is -o /dev/null -w '%{http_code}' 'localhost:8080/../tareas.db')" = "404" ] && ok "no sale de la carpeta de estáticos" || mal "recorrido de rutas"
[ "$fallos" -eq 0 ] && echo "✅ Aplicación web: todo correcto" || { cat "$T/servidor.txt"; exit 1; }
