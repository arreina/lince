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
export LINCE_JUEGO_GUARDADO="$T/partidas"   # lo que guarden los ejemplos, aquí y no en tu carpeta
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
12_aventura.lince|LINCE_JUEGO_PULSAR|espacio,derecha
13_fisica.lince|LINCE_JUEGO_PULSAR|derecha,espacio
LISTA
# El juego completo: el principio jugado con teclas (la portada, la sabia, el
# pueblo, guardar en el pozo y entrar en el bosque); tiene que acabar en el bosque.
GUION="170:espacio:1,432:espacio:1,644:espacio:1,846:espacio:1,1018:espacio:1,1220:derecha:67,1287:arriba:50,1357:espacio:1,1509:espacio:1,1571:abajo:110,1701:espacio:1,1873:espacio:1,1915:arriba:97,2012:izquierda:60,2082:espacio:1,2174:espacio:1,2256:espacio:1,2298:derecha:3,2301:arriba:170"
salida="$(cd "$RAIZ/juegos/la_piedra_del_lince" && env LINCE_JUEGO_FOTOGRAMAS=2520 LINCE_JUEGO_SEMILLA=1 LINCE_JUEGO_GUION="$GUION" \
          timeout 120 "$LINCE" la_piedra_del_lince.lince 2>&1)"
if [ $? -eq 0 ] && [ -z "$salida" ] && grep -q '"mapa": *"bosque"' "$T/partidas/partida.json" 2>/dev/null; then
    echo "  ✅ juegos/la_piedra_del_lince (llega al bosque)"
else
    echo "  ❌ juegos/la_piedra_del_lince"; echo "$salida" | head -5; fallos=$((fallos+1))
fi
[ "$fallos" -eq 0 ] && echo "✅ Minijuegos: todos se ejecutan" || exit 1
