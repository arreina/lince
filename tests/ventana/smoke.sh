#!/bin/bash
# Prueba servidor.ventana() con un «navegador» falso: pide la página, la guarda
# y termina (como al cerrar la ventana). El programa debe acabar solo.
LINCE="${1:-./lince}"
DIR="$(cd "$(dirname "$0")" && pwd)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
cat > "$T/navegador.sh" <<EOF
#!/bin/bash
echo "\$1" > "$T/url.txt"
curl -s "\$1" > "$T/pagina.txt"
EOF
chmod +x "$T/navegador.sh"
LINCE_NAVEGADOR="$T/navegador.sh" timeout 20 "$LINCE" "$DIR/app.lince" > "$T/salida.txt" 2>&1
codigo=$?
fallos=0
ok() { echo "  ✅ $1"; }
mal() { echo "  ❌ $1"; fallos=$((fallos+1)); }
echo "🐆 Ventana"
[ "$codigo" -eq 0 ] && ok "el programa termina solo al cerrarse la ventana" || mal "el programa no terminó bien (código $codigo)"
grep -q "http://127.0.0.1:8766/" "$T/url.txt" 2>/dev/null && ok "el navegador recibe la URL local" || mal "URL incorrecta"
grep -q "hola desde la ventana" "$T/pagina.txt" 2>/dev/null && ok "la página se sirve" || mal "la página no se sirvió"
grep -q "no debería llegar" "$T/salida.txt" && mal "el programa siguió tras la ventana" || ok "nada se ejecuta tras la ventana"
[ "$fallos" -eq 0 ] && echo "✅ Ventana: todo correcto" || { cat "$T/salida.txt"; exit 1; }
