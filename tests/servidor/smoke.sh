#!/usr/bin/env bash
#
# 🐆 Lince — test de humo del módulo 'servidor'
#
# Levanta tests/servidor/servidor_test.lince, hace peticiones reales y
# comprueba las respuestas. Pensado sobre todo para correr con un binario
# compilado con AddressSanitizer: el servidor hace fork por conexión, así
# que los errores de memoria salen en el proceso hijo y sólo se ven en su
# stderr, no en el código de salida del padre. Por eso al final se rastrea
# el log en busca de avisos del sanitizer.
#
# Uso: tests/servidor/smoke.sh [binario] [puerto]

set -uo pipefail

BIN="${1:-./lince}"
PUERTO="${2:-8123}"
BASE="http://127.0.0.1:${PUERTO}"
LOG="$(mktemp -t lince-servidor-XXXXXX.log)"
FALLOS=0

# Sin detect_leaks: el intérprete no libera el AST ni las tablas de
# módulos al terminar, así que LeakSanitizer daría un ruido enorme que no
# tiene que ver con lo que prueba este script.
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

limpiar() {
    if [ -n "${SRV_PID:-}" ] && kill -0 "$SRV_PID" 2>/dev/null; then
        kill "$SRV_PID" 2>/dev/null
        wait "$SRV_PID" 2>/dev/null
    fi
    rm -f "$LOG"
}
trap limpiar EXIT

comprobar() {  # comprobar <descripción> <esperado> <obtenido>
    if [ "$2" = "$3" ]; then
        printf '  \033[32m✅\033[0m %s\n' "$1"
    else
        printf '  \033[31m❌\033[0m %s\n' "$1"
        printf '       esperaba: %s\n' "$2"
        printf '       obtuvo  : %s\n' "$3"
        FALLOS=$((FALLOS + 1))
    fi
}

contiene_no() {  # contiene_no <descripción> <aguja> <pajar>
    if printf '%s' "$3" | grep -q -- "$2"; then
        printf '  \033[31m❌\033[0m %s (encontrado «%s»)\n' "$1" "$2"
        FALLOS=$((FALLOS + 1))
    else
        printf '  \033[32m✅\033[0m %s\n' "$1"
    fi
}

echo ""
echo "🐆 Test de humo del servidor — $BIN (puerto $PUERTO)"
echo ""

LINCE_PUERTO_TEST="$PUERTO" "$BIN" tests/servidor/servidor_test.lince >"$LOG" 2>&1 &
SRV_PID=$!

# Esperar a que acepte conexiones (hasta ~10s) en vez de dormir a ciegas.
listo=0
for _ in $(seq 1 100); do
    if curl -fsS -o /dev/null "${BASE}/eco/x?q=1" 2>/dev/null; then listo=1; break; fi
    if ! kill -0 "$SRV_PID" 2>/dev/null; then break; fi
    sleep 0.1
done

if [ "$listo" -ne 1 ]; then
    if grep -qE "ERROR: AddressSanitizer|runtime error:" "$LOG"; then
        echo "❌ El sanitizer detectó errores de memoria al atender la"
        echo "   primera petición (el proceso hijo murió). Log:"
    else
        echo "❌ El servidor no llegó a aceptar conexiones. Log:"
    fi
    sed 's/^/    /' "$LOG"
    exit 1
fi

# ── Parámetros de ruta + query + cookies de la petición ────────
# Es el camino que provocaba un doble free en cada petición con ruta.
r=$(curl -fsS "${BASE}/eco/mundo?q=42" -b "sesion=abc")
comprobar "params, consulta y cookies llegan al manejador" \
          "nombre=mundo q=42 galleta=abc" "$r"

# Varias peticiones seguidas: un puntero liberado se nota al reutilizarse.
for i in 1 2 3 4 5; do
    r=$(curl -fsS "${BASE}/eco/n$i?q=$i" -b "sesion=s$i")
    comprobar "petición repetida $i/5" "nombre=n$i q=$i galleta=s$i" "$r"
done

# ── Cookie de respuesta más larga que el buffer de cabeceras ───
# Una cookie que no cabe debe descartarse entera. Si se enviara truncada
# se perdería su \r\n final, las cabeceras no terminarían y el cuerpo se
# leería como una cabecera más: por eso aquí se comprueba el cuerpo.
r=$(curl -fsS "${BASE}/galleta-larga")
comprobar "cookie larga: el cuerpo llega completo" "ok" "$r"

# ── Cookie con CR/LF: no debe poder inyectar cabeceras ─────────
cab=$(curl -fsS -D - -o /dev/null "${BASE}/galleta-crlf")
contiene_no "cookie con CRLF no inyecta una cabecera nueva" "inyectada" "$cab"

# ── POST con cuerpo ────────────────────────────────────────────
r=$(curl -fsS -X POST "${BASE}/enviar" -d "hola")
comprobar "POST recibe el cuerpo" "recibido:hola" "$r"

# ── Ruta inexistente ───────────────────────────────────────────
cod=$(curl -sS -o /dev/null -w '%{http_code}' "${BASE}/no-existe")
comprobar "ruta desconocida devuelve 404" "404" "$cod"

# ── Errores del sanitizer en los procesos hijo ─────────────────
# El padre sale con 0 aunque un hijo aborte, así que hay que mirar el log.
if grep -qE "ERROR: AddressSanitizer|runtime error:" "$LOG"; then
    echo ""
    echo "❌ El sanitizer detectó errores de memoria:"
    sed 's/^/    /' "$LOG"
    FALLOS=$((FALLOS + 1))
else
    printf '  \033[32m✅\033[0m sin errores del sanitizer\n'
fi

echo ""
if [ "$FALLOS" -eq 0 ]; then
    printf '\033[32m✅ Test de humo del servidor: todo correcto\033[0m\n\n'
    exit 0
fi
printf '\033[31m❌ Test de humo del servidor: %d fallos\033[0m\n\n' "$FALLOS"
exit 1
