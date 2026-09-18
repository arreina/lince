#!/usr/bin/env bash
#
# 🐆 Lince — test de humo del módulo 'motor'
#
# Ejecuta tests/motor/motor_test.lince contra el binario lince-motor y
# comprueba su salida. No hace falta pantalla: se fuerza el driver de vídeo
# 'dummy' de SDL, así que esto corre igual por SSH o en integración continua.
#
# Dos cosas que este script vigila y que no se ven en la salida normal:
#
#   - Que un error de Lince dentro del callback por frame devuelva el
#     control. Cuando no lo hacía, el bucle giraba para siempre: por eso
#     cada ejecución lleva su tiempo límite y un 124 cuenta como fallo.
#
#   - Que lo que se fuga no crezca con los frames. Con un binario de
#     AddressSanitizer se ejecuta el script dos veces con bucles de duración
#     muy distinta: si el número de asignaciones no coincide, algo se está
#     fugando una vez por frame.
#
# Uso: tests/motor/smoke.sh [binario] [dir de lince-motor]

set -uo pipefail

BIN="${1:-./lince-motor}"
MOTOR_DIR="${2:-../lince-motor}"
PNG="${MOTOR_DIR}/assets/lince.png"
LOG="$(mktemp -t lince-motor-XXXXXX.log)"
LOG2="$(mktemp -t lince-motor-XXXXXX.log)"
FALLOS=0

# Sin pantalla y con el render por software: sólo nos interesa que el camino
# de código se recorra entero, no lo que se vea.
export SDL_VIDEODRIVER=dummy
export SDL_RENDER_DRIVER=software
export LINCE_MOTOR_PNG="$PNG"

# Sin detect_leaks en las pasadas normales: LeakSanitizer sale por una vía
# que no vacía stdout, así que el informe se comería la salida del script
# que venimos a comprobar. La comprobación de fugas de más abajo lo vuelve a
# encender, y ahí sólo miramos stderr.
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

limpiar() { rm -f "$LOG" "$LOG2"; }
trap limpiar EXIT

bien()  { printf '  \033[32m✅\033[0m %s\n' "$1"; }
mal()   { printf '  \033[31m❌\033[0m %s\n' "$1"; FALLOS=$((FALLOS + 1)); }

contiene() {  # contiene <descripción> <línea esperada> <fichero>
    if grep -qxF -- "$2" "$3"; then bien "$1"; else
        mal "$1"
        printf '       esperaba la línea: %s\n' "$2"
    fi
}

echo ""
echo "🐆 Test de humo del motor — $BIN"
echo ""

if [ ! -f "$PNG" ]; then
    echo "❌ No encuentro $PNG."
    echo "   Pásale la ruta de lince-motor: tests/motor/smoke.sh $BIN /ruta/a/lince-motor"
    exit 1
fi

# ── Recorrido completo del binding ─────────────────────────────
timeout 60 "$BIN" tests/motor/motor_test.lince >"$LOG" 2>&1
codigo=$?

if [ "$codigo" -eq 124 ]; then
    echo "❌ El script se colgó (60 s sin terminar). Salida hasta ese punto:"
    sed 's/^/    /' "$LOG"
    exit 1
fi

contiene "la ventana se crea con el tamaño pedido" "ventana 320x240"     "$LOG"
contiene "el teclado arranca en reposo"            "entrada en reposo"   "$LOG"
contiene "la textura se carga y mide bien"         "textura 32x32"       "$LOG"
contiene "el bucle llama al callback con delta"    "bucle con parametro ok" "$LOG"
contiene "el delta del frame llega al callback"    "delta llega al callback" "$LOG"
contiene "terminar() invalida los handles viejos"  "handle invalidado tras terminar" "$LOG"
contiene "el bucle acepta un callback sin delta"   "bucle sin parametro ok" "$LOG"
contiene "el script termina limpiamente"           "fin"                 "$LOG"

if grep -q "FALLO" "$LOG"; then
    echo ""
    echo "   El script informó de fallos:"
    grep "FALLO" "$LOG" | sed 's/^/       /'
    FALLOS=$((FALLOS + 1))
fi

# ── Un error en el callback no puede colgar el bucle ───────────
timeout 30 "$BIN" tests/motor/motor_error_test.lince >"$LOG2" 2>&1
codigo=$?

if [ "$codigo" -eq 124 ]; then
    mal "un error en el callback cuelga el bucle (no volvió en 30 s)"
elif grep -q "FALLO" "$LOG2"; then
    mal "un error en el callback no se propagó"
    grep "FALLO" "$LOG2" | sed 's/^/       /'
elif grep -q "ErrorRango" "$LOG2"; then
    bien "un error en el callback para el bucle y se propaga"
else
    mal "un error en el callback no dio el error esperado"
    sed 's/^/       /' "$LOG2"
fi

# ── Errores del sanitizer, si el binario los lleva ─────────────
if grep -qE "ERROR: AddressSanitizer|runtime error:" "$LOG" "$LOG2"; then
    echo ""
    echo "❌ El sanitizer detectó errores de memoria:"
    grep -hE -A20 "ERROR: AddressSanitizer|runtime error:" "$LOG" "$LOG2" | sed 's/^/    /'
    FALLOS=$((FALLOS + 1))
fi

# ── Lo que se fuga no puede crecer con los frames ──────────────
# Sólo tiene sentido con un binario instrumentado; con uno normal no hay
# informe de LeakSanitizer que comparar y se salta.
cuenta_fugas() {  # cuenta_fugas <segundos> <fichero>
    LINCE_MOTOR_SEGUNDOS="$1" ASAN_OPTIONS="detect_leaks=1:abort_on_error=0" \
        timeout 120 "$BIN" tests/motor/motor_test.lince >"$2" 2>&1
    sed -n 's/^SUMMARY: AddressSanitizer: [0-9]* byte(s) leaked in \([0-9]*\) allocation(s)\..*/\1/p' "$2"
}

corto=$(cuenta_fugas 0.2 "$LOG")
largo=$(cuenta_fugas 2.0 "$LOG2")

if [ -z "$corto" ] || [ -z "$largo" ]; then
    printf '  \033[90m•\033[0m sin AddressSanitizer: me salto la comprobación de fugas por frame\n'
elif [ "$corto" = "$largo" ]; then
    bien "lo que se fuga no crece con los frames ($corto asignaciones en ambos)"
else
    mal "algo se fuga por frame: $corto asignaciones con un bucle corto, $largo con uno largo"
fi

echo ""
if [ "$FALLOS" -eq 0 ]; then
    printf '\033[32m✅ Test de humo del motor: todo correcto\033[0m\n\n'
    exit 0
fi
printf '\033[31m❌ Test de humo del motor: %d fallos\033[0m\n\n' "$FALLOS"
exit 1
