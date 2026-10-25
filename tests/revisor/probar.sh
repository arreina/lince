#!/bin/bash
# Prueba el revisor ('lince comprobar', el mismo que usa el editor): que da los
# avisos que tiene que dar y ninguno de más en programas correctos.
LINCE="${1:-./lince}"
DIR="$(cd "$(dirname "$0")" && pwd)"
RAIZ="$(cd "$DIR/../.." && pwd)"
fallos=0
avisos() { "$LINCE" comprobar "$1" 2>&1 | tr -d '\r' | grep 'Aviso en línea'; }
echo "🐆 Revisor"

salida="$(avisos "$DIR/avisos.lince")"
if [ "$salida" = "$(tr -d '\r' < "$DIR/avisos.esperado")" ]; then
    echo "  ✅ avisa de lo que tiene que avisar ($(echo "$salida" | wc -l) avisos)"
else
    echo "  ❌ los avisos no son los esperados:"
    diff <(tr -d '\r' < "$DIR/avisos.esperado") <(echo "$salida") | sed 's/^/      /'
    fallos=$((fallos+1))
fi

# Programas correctos: ningún aviso (un aviso falso enseña a no hacerles caso)
for f in "$DIR/limpio.lince" "$RAIZ"/examples/*.lince "$RAIZ"/juegos/*/*.lince; do
    [ -f "$f" ] || continue
    salida="$(avisos "$f")"
    if [ -n "$salida" ]; then
        echo "  ❌ avisos falsos en ${f#$RAIZ/}:"; echo "$salida" | sed 's/^/      /'
        fallos=$((fallos+1))
    fi
done
[ "$fallos" -eq 0 ] && echo "  ✅ ningún aviso en limpio.lince, los ejemplos ni los juegos"

[ "$fallos" -eq 0 ] && echo "✅ Revisor: todo correcto" || { echo "❌ $fallos comprobaciones fallaron"; exit 1; }
