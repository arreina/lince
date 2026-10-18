#!/bin/bash
# Ejecuta cada minijuego de examples/ sin pantalla, con teclas o clics automáticos
# (ver el modo de prueba de src/juego.h) y comprueba que corre sin errores.
# Se salta si no hay SDL2.
LINCE="$(cd "$(dirname "${1:-./lince}")" && pwd)/$(basename "${1:-./lince}")"
FOTOGRAMAS="${2:-1500}"
RAIZ="$(cd "$(dirname "$0")/../.." && pwd)"
echo "🐆 Minijuegos de examples/"
if ! ldconfig -p 2>/dev/null | grep -q "libSDL2-2.0.so.0" && [ ! -f /mingw64/bin/SDL2.dll ] && [ ! -f /ucrt64/bin/SDL2.dll ]; then
    echo "  ⏭  SDL2 no está instalada: se omiten"
    exit 0
fi
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
cd "$T"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
fallos=0
# archivo : variable de prueba que lo maneja
while IFS='|' read -r archivo var valor; do
    salida="$(env LINCE_JUEGO_FOTOGRAMAS="$FOTOGRAMAS" "$var=$valor" timeout 120 "$LINCE" "$RAIZ/examples/$archivo" 2>&1)"
    if [ $? -eq 0 ] && [ -z "$salida" ]; then
        echo "  ✅ $archivo"
    else
        echo "  ❌ $archivo"; echo "$salida" | head -5; fallos=$((fallos+1))
    fi
done <<'LISTA'
01_pong.lince|LINCE_JUEGO_PULSAR|espacio,w
02_serpiente.lince|LINCE_JUEGO_PULSAR|espacio,arriba,izquierda
03_rompeladrillos.lince|LINCE_JUEGO_PULSAR|espacio,derecha
04_pajaro.lince|LINCE_JUEGO_PULSAR|espacio
05_esquiva.lince|LINCE_JUEGO_PULSAR|izquierda
06_memoria.lince|LINCE_JUEGO_CLIC|100,150
07_tres_en_raya.lince|LINCE_JUEGO_CLIC|100,200
08_plataformas.lince|LINCE_JUEGO_PULSAR|derecha,espacio
09_mazmorra.lince|LINCE_JUEGO_PULSAR|derecha,abajo
10_paseo.lince|LINCE_JUEGO_PULSAR|derecha,mayus
11_movil.lince|LINCE_JUEGO_CLIC|430,230
LISTA
[ "$fallos" -eq 0 ] && echo "✅ Minijuegos: todos se ejecutan" || exit 1
