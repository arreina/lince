#!/bin/bash
# Las herramientas de la 1.4: lince formatear, documentar, probar, perfil y
# los paquetes con versión fija.
LINCE="${1:-./lince}"
LINCE="$(cd "$(dirname "$LINCE")" && pwd)/$(basename "$LINCE")"
DIR="$(cd "$(dirname "$0")" && pwd)"
RAIZ="$(cd "$DIR/../.." && pwd)"
fallos=0
bien() { echo "  ✅ $1"; }
mal()  { echo "  ❌ $1"; fallos=$((fallos+1)); }
sin_r() { tr -d '\r'; }
# igual "qué" "esperado" "obtenido" (en el Windows de la CI no hay herramienta de comparar)
igual() {
    if [ "$2" = "$3" ]; then bien "$1"
    else mal "$1. Esperaba:"; echo "$2" | sed 's/^/      < /'; echo "      pero salió:"; echo "$3" | sed 's/^/      > /'; fi
}
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
echo "🐆 Herramientas"

# ── formatear ──
"$LINCE" formatear --mostrar "$DIR/feo.lince" | sin_r > "$TMP/bonito.lince"
igual "formatear deja feo.lince como bonito.esperado" "$(sin_r < "$DIR/bonito.esperado")" "$(cat "$TMP/bonito.lince")"
igual "formatear otra vez no cambia nada" "$(cat "$TMP/bonito.lince")" "$("$LINCE" formatear --mostrar "$TMP/bonito.lince" | sin_r)"
[ "$("$LINCE" "$DIR/feo.lince" | sin_r)" = "$("$LINCE" "$TMP/bonito.lince" | sin_r)" ] \
    && bien "el programa formateado hace lo mismo" || mal "el programa formateado hace otra cosa"
"$LINCE" formatear --comprobar "$DIR/feo.lince" > /dev/null && mal "--comprobar no ve que feo.lince está sin formatear" \
    || bien "--comprobar sale con 1 si algo está sin formatear"
cp "$DIR/feo.lince" "$TMP/feo.lince"
"$LINCE" formatear "$TMP/feo.lince" > /dev/null
igual "formatear cambia el archivo" "$(cat "$TMP/bonito.lince")" "$(sin_r < "$TMP/feo.lince")"
printf 'sea x = (1\n' > "$TMP/roto.lince"
if "$LINCE" formatear "$TMP/roto.lince" > /dev/null 2>&1; then mal "formatear tocó un archivo con un error de sintaxis"
else [ "$(cat "$TMP/roto.lince")" = "sea x = (1" ] && bien "con un error de sintaxis no lo toca" || mal "cambió un archivo roto"; fi
# lo que reparte Lince como ejemplo ya tiene el estilo
modelo=()
for f in "$RAIZ"/examples/*.lince "$RAIZ"/examples/*/*.lince "$RAIZ"/juegos/*/*.lince "$RAIZ"/src/lince/*.lince "$RAIZ"/docs/*/ejemplos/*.lince; do
    grep -q "@lenguaje: terminal" "$f" || modelo+=("$f")
done
salida="$("$LINCE" formatear --comprobar "${modelo[@]}" 2>&1)" && bien "los ejemplos, los juegos y el manual tienen el estilo (${#modelo[@]} archivos)" \
    || { mal "hay ejemplos sin formatear (lince formatear …):"; echo "$salida" | sed 's/^/      /'; }

# ── documentar ──
"$LINCE" documentar "$DIR/libreria.lince" | sin_r > "$TMP/ayuda.md"
igual "documentar saca la ayuda de los comentarios" "$(sin_r < "$DIR/documentar.esperado")" "$(cat "$TMP/ayuda.md")"
"$LINCE" documentar "$DIR/libreria.lince" -o "$TMP/a.md" > /dev/null
igual "documentar -o lo escribe en un archivo" "$(cat "$TMP/ayuda.md")" "$(sin_r < "$TMP/a.md")"

# ── probar ──
salida="$(cd "$DIR/proyecto" && "$LINCE" probar pruebas | sin_r)"
codigo=$(cd "$DIR/proyecto" && "$LINCE" probar pruebas > /dev/null 2>&1; echo $?)
igual "probar: las pruebas y su cobertura" "$(sin_r < "$DIR/probar.esperado")" "$salida"
[ "$codigo" = 1 ] && bien "probar sale con 1 si una prueba falla" || mal "probar salió con $codigo y una prueba falla"
(cd "$DIR/proyecto" && "$LINCE" probar pruebas/prueba_cuentas.lince --sin-cobertura > /dev/null) \
    && bien "probar sale con 0 si todas pasan" || mal "probar falla con pruebas que pasan"

# ── perfil ──
salida="$("$LINCE" perfil "$DIR/perfil.lince" 2>&1 | sin_r)"
echo "$salida" | grep -q "^9$" && echo "$salida" | grep -q "Perfil de" && echo "$salida" | grep -q "triple .*3 llamadas" \
    && bien "perfil ejecuta el programa y cuenta sus funciones" || { mal "perfil no dio lo esperado:"; echo "$salida" | sed 's/^/      /'; }

# ── paquetes con versión (con un GitHub de mentira: file://) ──
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) echo "  (paquetes con versión: no se prueban en Windows, sin rutas file:// sencillas)";;
*)
    mkdir -p "$TMP/raw/ana/estrellas/v1.0" "$TMP/raw/ana/estrellas/v2.0" "$TMP/juego" "$TMP/casa"
    echo 'funcion brillo(): numero { devolver 1 }' > "$TMP/raw/ana/estrellas/v1.0/estrellas.lince"
    echo 'funcion brillo(): numero { devolver 2 }' > "$TMP/raw/ana/estrellas/v2.0/estrellas.lince"
    printf 'importar "paquete:estrellas"\nescribir(brillo())\n' > "$TMP/juego/juego.lince"
    export LINCE_PAQUETES_RAW="file://$TMP/raw" HOME="$TMP/casa" USERPROFILE="$TMP/casa"
    cd "$TMP/juego"
    echo s | "$LINCE" instalar https://github.com/ana/estrellas@v1.0 > /dev/null 2>&1
    [ "$("$LINCE" juego.lince)" = 1 ] && grep -q "^estrellas  v1.0  " lince.paquetes \
        && bien "instalar url@versión lo deja en paquetes/ y lo apunta en lince.paquetes" || mal "instalar con versión no funcionó"
    [ "$(cd "$TMP" && "$LINCE" juego/juego.lince)" = 1 ] && bien "paquete: busca junto al programa" || mal "paquete: no lo encuentra desde otra carpeta"
    echo s | "$LINCE" instalar https://github.com/ana/estrellas@v2.0 > /dev/null 2>&1
    [ "$("$LINCE" juego.lince)" = 2 ] && [ "$(grep -c '^estrellas' lince.paquetes)" = 1 ] \
        && bien "otra versión sustituye a la de antes" || mal "cambiar de versión no funcionó"
    rm -rf paquetes
    "$LINCE" instalar > /dev/null 2>&1 && [ "$("$LINCE" juego.lince)" = 2 ] \
        && bien "instalar sin nada baja lo de lince.paquetes" || mal "instalar sin nada no lo bajó"
    echo 'funcion brillo(): numero { devolver 99 }' > "$TMP/raw/ana/estrellas/v2.0/estrellas.lince"
    rm -rf paquetes
    if "$LINCE" instalar > "$TMP/cambio.txt" 2>&1; then mal "aceptó un paquete que ha cambiado"
    else grep -q "ha cambiado" "$TMP/cambio.txt" && [ ! -f paquetes/estrellas.lince ] \
        && bien "si el paquete ha cambiado, no lo instala y lo dice" || mal "no avisó bien del cambio"; fi
    "$LINCE" desinstalar estrellas > /dev/null 2>&1
    grep -q "^estrellas" lince.paquetes && mal "desinstalar no lo quitó de lince.paquetes" || bien "desinstalar lo quita de lince.paquetes"
    cd "$RAIZ";;
esac

[ "$fallos" -eq 0 ] && echo "✅ Herramientas: todo correcto" || { echo "❌ $fallos comprobaciones fallaron"; exit 1; }
