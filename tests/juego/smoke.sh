#!/bin/bash
# Prueba el módulo juego sin pantalla (SDL con el controlador «dummy»): dibuja,
# captura la ventana a un BMP y comprueba píxeles concretos. Se salta si no hay SDL2.
LINCE="$(cd "$(dirname "${1:-./lince}")" && pwd)/$(basename "${1:-./lince}")"
DIR="$(cd "$(dirname "$0")" && pwd)"
echo "🐆 Juego"
if ! ldconfig -p 2>/dev/null | grep -q "libSDL2-2.0.so.0" && [ ! -f /mingw64/bin/SDL2.dll ]; then
    echo "  ⏭  SDL2 no está instalada: se omite"
    exit 0
fi
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
cd "$T"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 "$LINCE" "$DIR/prueba.lince" > salida.txt 2>&1
codigo=$?
fallos=0
ok() { echo "  ✅ $1"; }
mal() { echo "  ❌ $1"; fallos=$((fallos+1)); }
[ "$codigo" -eq 0 ] && ok "el programa termina bien" || mal "código de salida $codigo"
for l in "error sin ventana: ok" "abierto: verdadero" "tamaño: 100x80" "ancho texto: 46" "colisión: verdadero falso" \
         "tecla espacio: falso" "tecla desconocida: ok" "color inválido: ok" "abierto tras salir: falso"; do
    grep -qxF "$l" salida.txt && ok "$l" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("captura.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
bpp = struct.unpack_from("<H", d, 28)[0]
assert (w, abs(h), bpp) == (100, 80, 32), (w, h, bpp)
def px(x, y):
    fila = (80 - 1 - y) if h > 0 else y            # los BMP suelen ir de abajo arriba
    b, g, r, a = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 4]
    return (r, g, b)
casos = [((0, 0), (0, 0, 0), "fondo negro"), ((15, 15), (255, 0, 0), "rectángulo rojo"),
         ((50, 20), (0, 255, 0), "rectángulo '#00ff00'"), ((80, 60), (0, 0, 255), "círculo azul"),
         ((50, 79), (255, 255, 255), "línea blanca")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ píxel:", nombre)
    else: print("  ❌ píxel:", nombre, "→", px(x, y)); mal += 1
amar = sum(1 for y in range(36, 52) for x in range(0, 40) if px(x, y) == (255, 255, 0))
if amar > 20: print("  ✅ el texto se dibuja")
else: print("  ❌ el texto no se dibuja"); mal += 1
sys.exit(1 if mal else 0)
PY
[ "$fallos" -eq 0 ] && echo "✅ Juego: todo correcto" || { cat salida.txt; exit 1; }
