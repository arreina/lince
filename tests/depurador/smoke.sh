#!/bin/bash
# Prueba el depurador de línea de comandos con una sesión guiada por stdin.
LINCE="${1:-./lince}"
DIR="$(cd "$(dirname "$0")" && pwd)"
fallos=0
comprobar() {   # comprobar "descripción" "texto que debe aparecer" "sesión"
    salida="$(printf "$3" | "$LINCE" depurar "$DIR/programa.lince" 2>&1)"
    if echo "$salida" | grep -qF -- "$2"; then echo "  ✅ $1"; else echo "  ❌ $1 (falta: $2)"; echo "$salida" | sed 's/^/      /'; fallos=$((fallos+1)); fi
}
echo "🐆 Depurador"
comprobar "para en la primera línea"            "→ programa.lince:1"       "q\n"
comprobar "paso a paso llega a la línea 6"      "→ programa.lince:6   sea total = 0" "s\ns\nq\n"
comprobar "punto de parada dentro de la función" "Punto de parada en programa.lince:2" "b 2\nc\nq\n"
comprobar "muestra las variables locales"        "a = 0"                    "b 2\nc\nv\nq\n"
comprobar "evalúa expresiones"                   "  30"                     "b 2\nc\np a + b + 29\nq\n"
comprobar "muestra la pila de llamadas"          "#1  (principal)  programa.lince:9" "b 2\nc\nbt\nq\n"
salida="$(printf 'b 9\nc\nn\nq\n' | "$LINCE" depurar "$DIR/programa.lince" 2>&1)"
if [ "$(echo "$salida" | grep -c '→ programa.lince:9')" -eq 2 ] && ! echo "$salida" | grep -q '→ programa.lince:2'; then
    echo "  ✅ 'siguiente' no entra en la función"
else echo "  ❌ 'siguiente' no entra en la función"; echo "$salida" | sed 's/^/      /'; fallos=$((fallos+1)); fi
comprobar "'salir' vuelve a quien llamó"         "→ programa.lince:9"       "b 2\nc\no\nq\n"
comprobar "termina y escribe el resultado"       "6"                        "c\n"
comprobar "sin entrada sigue hasta el final"     "Programa terminado."      ""
[ "$fallos" -eq 0 ] && echo "✅ Depurador: todo correcto" || { echo "❌ $fallos comprobaciones fallaron"; exit 1; }
