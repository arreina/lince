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
# ── Segunda parte: imágenes, sprites, cámara, sonido y mandos ──
python3 - <<'PY'
import struct, wave
# hoja.png de 32×16: 8 cuadros de 8×8 con colores distintos (PNG sin compresión externa)
import zlib
colores = [(255,0,0),(0,255,0),(0,0,255),(255,255,0),(255,0,255),(0,255,255),(255,255,255),(128,128,128)]
filas = []
for y in range(16):
    fila = b"\x00"
    for x in range(32):
        c = colores[(y // 8) * 4 + x // 8]
        fila += bytes(c) + b"\xff"
    filas.append(fila)
def bloque(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n" + bloque(b"IHDR", struct.pack(">IIBBBBB", 32, 16, 8, 6, 0, 0, 0)) \
    + bloque(b"IDAT", zlib.compress(b"".join(filas))) + bloque(b"IEND", b"")
open("hoja.png", "wb").write(png)
# cuadro.bmp 10×10 azul (24 bits)
w = h = 10; fila = (bytes((255, 0, 0)) * w) + b"\x00\x00"
datos = fila * h
cab = b"BM" + struct.pack("<IHHI", 54 + len(datos), 0, 0, 54) + struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(datos), 2835, 2835, 0, 0)
open("cuadro.bmp", "wb").write(cab + datos)
# tono.wav: 0,5 s, mono, 22050 Hz (se convierte al formato de Lince)
with wave.open("tono.wav", "wb") as f:
    f.setnchannels(1); f.setsampwidth(2); f.setframerate(22050)
    f.writeframes(b"".join(struct.pack("<h", 8000 if (i // 25) % 2 else -8000) for i in range(11025)))
PY
cp "$DIR/tono.ogg" .      # 0,5 s estéreo a 44100 Hz, Ogg Vorbis
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 "$LINCE" "$DIR/prueba2.lince" > salida2.txt 2>&1
[ $? -eq 0 ] && ok "la segunda prueba termina bien" || mal "la segunda prueba falló"
for l in "hoja: 32x16" "misma imagen: verdadero" "bmp: 10x10" "camara: -50" "imagen inexistente: ok" \
         "dibujar sin imagen: ok" "duración: 0.5" "ogg: 0.5" "sonido inválido: ok" "voz: verdadero" "volumen: 0.8" "reproducir sin sonido: ok" \
         "mandos: 0" "sprite: 3x3" "botón a: falso" "eje: 0" "tiempo: verdadero"; do
    grep -qxF "$l" salida2.txt && ok "$l" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("prueba2.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r, a = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 4]
    return (r, g, b)
casos = [((8, 8), (0, 255, 0), "sprite: cuadro 1 de la hoja, escalado"), ((17, 8), (0, 0, 0), "…mide 16 px"),
         ((25, 5), (255, 0, 0), "recorte con ancho/alto"), ((45, 5), (0, 0, 255), "imagen BMP"),
         ((70, 10), (0, 0, 0), "alfa 0 no se ve"), ((55, 45), (255, 255, 0), "la cámara desplaza el dibujo"),
         ((81, 70), (255, 0, 0), "sprite de texto: píxel pintado"), ((81, 61), (0, 0, 0), "sprite de texto: '.' transparente")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ píxel:", nombre)
    else: print("  ❌ píxel:", nombre, "→", px(x, y)); mal += 1
sys.exit(1 if mal else 0)
PY
# ── Tercera parte: texto tecleado, puntero del ratón y acciones ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=10 LINCE_JUEGO_PULSAR=espacio \
    LINCE_JUEGO_ESCRIBIR='Peñaz<ón' timeout 20 "$LINCE" "$DIR/prueba3.lince" > salida3.txt 2>&1
[ $? -eq 0 ] && ok "la tercera prueba termina bien" || mal "la tercera prueba falló"
for l in "nombre: Peñaó" "escrito: Peñazón" "mantenida: 5" "pulsada: 5" "soltada: 4" "direccion: 0" \
         "movimiento: 0" "capturar: verdadero" "redefinida: falso"; do
    grep -qxF "$l" salida3.txt && ok "$l" || mal "falta la línea: $l"
done
for l in "entrada mala: definir_accion(\"mal\"): entrada de mando desconocida" "tecla mala: definir_accion(\"mal\"): tecla desconocida" \
         "sin definir: accion(): la acción 'volar' no está definida"; do
    grep -qF "$l" salida3.txt && ok "${l%%:*}" || mal "falta: $l"
done
# ── Cuarta parte: recarga en caliente ──
# El juego se reescribe a sí mismo: primero con un error de sintaxis (sigue
# el código anterior), luego con una versión nueva (conserva las variables y
# los objetos pasan a la clase nueva) que falla en ejecución; mientras espera
# el arreglo, esta prueba guarda la versión 3.
mkdir rec && cp "$DIR"/recarga/*.lince rec/ && cp rec/v1.lince rec/juego.lince
(cd rec && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=100000 timeout 60 "$LINCE" juego.lince > ../salida4.txt 2> ../errores4.txt; echo "código $?" >> ../salida4.txt) &
for i in $(seq 1 300); do grep -q funcion_que_no_existe errores4.txt 2>/dev/null && break; sleep 0.1; done
sleep 0.5
cp rec/v3.lince rec/juego.lince
wait
for l in "v2 conserva: verdadero hola v2" "v3 tras el error: verdadero 160" "código 0"; do
    grep -qxF "$l" salida4.txt && ok "recarga: $l" || mal "falta la línea: $l"
done
grep -q "capturado" salida4.txt && mal "intentar capturó la recarga" || ok "recarga: intentar no la captura"
grep -qF "Error de sintaxis en juego.lince, línea 5" errores4.txt && ok "recarga: error de sintaxis avisado" || mal "falta el aviso del error de sintaxis"
python3 - <<'PY'
import struct
for n in (10, 20):
    fila = bytes((0, 0, 255)) * n + b"\x00" * ((4 - n * 3 % 4) % 4)
    datos = fila * n
    cab = b"BM" + struct.pack("<IHHI", 54 + len(datos), 0, 0, 54) + struct.pack("<IiiHHIIiiII", 40, n, n, 1, 24, 0, len(datos), 2835, 2835, 0, 0)
    open("rec/a%d.bmp" % n, "wb").write(cab + datos)
PY
cp rec/a10.bmp rec/img.bmp
(cd rec && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=200 timeout 30 "$LINCE" recursos.lince > ../salida5.txt 2>&1)
[ $? -eq 0 ] && ok "recarga de recursos: termina bien" || mal "la recarga de recursos falló"
for l in "util: 1, imagen: 10" "imagen cambiada: 20" "util: 2, imagen: 10"; do
    grep -qxF "$l" salida5.txt && ok "recarga: $l" || mal "falta la línea: $l"
done
(cd rec && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=6 LINCE_JUEGO_PULSAR=f5 timeout 30 "$LINCE" f5.lince > ../salida6.txt 2>&1)
[ "$(grep -cxF "recargado: falso, veces: 0" salida6.txt)" -ge 2 ] && grep -qF "(desde cero)" salida6.txt \
    && ok "F5 empieza desde cero" || mal "F5 no empieza desde cero"
# ── Quinta parte: colisiones, animación, partículas y escenas ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=60 timeout 20 "$LINCE" "$DIR/prueba4.lince" > salida7.txt 2>&1
[ $? -eq 0 ] && ok "la quinta prueba termina bien" || mal "la quinta prueba falló"
for l in "circulos: verdadero falso" "circulo_rect: verdadero falso" "dentro: verdadero falso" "escena: menu verdadero" \
         "cuadro: 0 5 1" "escena nueva tras presentar: falso" "cambio diferido: menu" "fin en el fotograma 20" "vivas: 30, al final: 0"; do
    grep -qxF "$l" salida7.txt && ok "$l" || mal "falta la línea: $l"
done
grep -qF "opción desconocida 'cantida'" salida7.txt && ok "opción de partículas mal escrita" || mal "falta el error de opción desconocida"
grep -q "^escena: partida verdadero" salida7.txt && ok "escena: partida verdadero" || mal "falta la línea: escena: partida"
[ "$fallos" -eq 0 ] && echo "✅ Juego: todo correcto" || { cat salida.txt salida2.txt salida3.txt salida4.txt errores4.txt salida5.txt salida6.txt salida7.txt; exit 1; }
