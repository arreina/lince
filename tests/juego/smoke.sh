#!/bin/bash
# Prueba el módulo juego sin pantalla (SDL con el controlador «dummy»): dibuja,
# captura la ventana a un BMP y comprueba píxeles concretos. Se salta si no hay SDL2.
LINCE="$(cd "$(dirname "${1:-./lince}")" && pwd)/$(basename "${1:-./lince}")"
DIR="$(cd "$(dirname "$0")" && pwd)"
echo "🐆 Juego"
if ! ldconfig -p 2>/dev/null | grep -q "libSDL2-2.0.so.0" && [ ! -f /mingw64/bin/SDL2.dll ] && [ ! -f /ucrt64/bin/SDL2.dll ]; then
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
# una hoja cuya imagen cambia con el juego en marcha (y un animador que la usa)
(cd rec && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=60 timeout 30 "$LINCE" hoja.lince > ../salida5b.txt 2>&1)
for l in "hoja: ejecución 1, 4 cuadros" "hoja: ejecución 2, 3 cuadros"; do
    grep -qxF "$l" salida5b.txt && ok "recarga de hojas: $l" || mal "falta la línea: $l"
done
python3 -c "
import struct
d = open('rec/hoja.bmp', 'rb').read()
off = struct.unpack_from('<I', d, 10)[0]; w, h = struct.unpack_from('<ii', d, 18)
fila = abs(h) - 1 - 3 if h > 0 else 3
b, g, r, a = d[off + (fila * w + 3) * 4: off + (fila * w + 3) * 4 + 4]
exit(0 if (r, g, b) == (0, 255, 255) else 1)" && ok "recarga de hojas: el animador dibuja la hoja nueva" || mal "el animador no dibuja la hoja nueva"
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
# ── Sexta parte: mapas de casillas (plataformas y vista cenital) y caminos ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=5 timeout 20 "$LINCE" "$DIR/prueba5.lince" > salida8.txt 2>&1
[ $? -eq 0 ] && ok "la sexta prueba termina bien" || mal "la sexta prueba falló"
for l in "tamaño: 8x8 128" "casilla: #.=[]" "solido: verdadero falso verdadero falso" "buscar: 3" "casilla_en: 2 1" \
         "cae: 112 verdadero 0" "pared: 112 1" "atraviesa: verdadero falso" "pisa: 32 verdadero" "cenital: 16 verdadero" \
         "camino: 7 termina en 6" "sin camino: 0" "diagonal: 5"; do
    grep -qxF "$l" salida8.txt && ok "mapa: $l" || mal "falta la línea: $l"
done
grep -qF "no es ASCII" salida8.txt && ok "mapa: carácter no ASCII" || mal "falta el error de carácter no ASCII"
grep -qxF "hoja: 2 4" salida8.txt && ok "hoja: 2 columnas, 4 cuadros" || mal "falta la línea: hoja: 2 4"
grep -qF "di qué cuadro" salida8.txt && ok "tiles: hoja sin cuadro" || mal "falta el error de hoja sin cuadro"
grep -qF "no conozco la opción 'separasion'" salida8.txt && ok "hoja: opción mal escrita" || mal "falta el error de opción de hoja"
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("mapa.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r, a = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 4]
    return (r, g, b)
casos = [((5, 5), (255, 0, 0), "pared"), ((60, 40), (0, 0, 255), "plataforma"), ((24, 24), (0, 0, 0), "suelo vacío")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ dibujar_mapa:", nombre)
    else: print("  ❌ dibujar_mapa:", nombre, "→", px(x, y)); mal += 1
d = open("tiles.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
casos = [((8, 8), (255, 0, 0), "tile 0"), ((24, 8), (0, 255, 0), "tile 1"), ((40, 8), (0, 0, 255), "tile 2"),
         ((56, 8), (255, 255, 0), "tile -1 (el último)"), ((72, 8), (0, 255, 0), "tile animado"),
         ((4, 24), (0, 0, 255), "dibujar con cuadro")]
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ tiles:", nombre)
    else: print("  ❌ tiles:", nombre, "→", px(x, y)); mal += 1
sys.exit(1 if mal else 0)
PY
# ── Séptima parte: lince nuevo crea un juego que se puede jugar ──
"$LINCE" nuevo mi_juego > salida9.txt 2>&1 && [ -f mi_juego/mi_juego.lince ] && ok "lince nuevo crea el juego" || mal "lince nuevo falló"
(cd mi_juego && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=2400 LINCE_JUEGO_PULSAR=espacio,derecha,abajo \
    timeout 60 "$LINCE" mi_juego.lince >> ../salida9.txt 2>&1) && ok "el juego nuevo se juega entero (portada, partida y fin)" || mal "el juego nuevo falla"
"$LINCE" nuevo mi_juego >> salida9.txt 2>&1 && mal "lince nuevo pisa una carpeta que existe" || ok "lince nuevo no pisa una carpeta que existe"
# ── Octava parte: efectos de sonido generados ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 30 "$LINCE" "$DIR/prueba6.lince" > salida10.txt 2>&1
[ $? -eq 0 ] && ok "la octava prueba termina bien" || mal "la octava prueba falló"
for l in "mismo sonido: verdadero" "otra semilla: verdadero" "largo: 2" "wav: verdadero" "se vuelve a cargar: verdadero" "voz: verdadero"; do
    grep -qxF "$l" salida10.txt && ok "efecto: $l" || mal "falta la línea: $l"
done
grep -qF "no conozco el efecto 'trompeta'" salida10.txt && ok "efecto: tipo desconocido" || mal "falta el error de efecto desconocido"
grep -qF "opción desconocida 'volumne'" salida10.txt && ok "efecto: opción mal escrita" || mal "falta el error de opción de efecto"
python3 -c "import sys; sys.exit(open('a.wav','rb').read() != open('b.wav','rb').read())" && ok "efecto: la misma semilla da el mismo .wav" || mal "la misma semilla da sonidos distintos"
# Sin pantalla, un error con la ventana abierta termina en vez de esperar el arreglo
printf 'importar "juego"\njuego.abrir("x", 10, 10)\nfalla()\n' > falla.lince
SDL_VIDEODRIVER=dummy timeout 10 "$LINCE" falla.lince > /dev/null 2>&1
[ $? -eq 1 ] && ok "sin pantalla, un error no se queda esperando" || mal "un error sin pantalla se queda esperando"
# ── Novena parte: música en streaming, escuchada con el controlador «disk» de SDL ──
rm -f sdlaudio.raw
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$PWD/sdlaudio.raw" timeout 30 "$LINCE" "$DIR/prueba7.lince" > salida11.txt 2>/dev/null
[ $? -eq 0 ] && grep -qx "fin" salida11.txt && ok "la novena prueba termina bien" || mal "la novena prueba falló"
grep -qF "no se encuentra la música 'no_existe.ogg'" salida11.txt && ok "música: archivo que no existe" || mal "falta el error de música inexistente"
if [ -f sdlaudio.raw ]; then
    python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("sdlaudio.raw", "rb").read()
izq = struct.unpack("<%dh" % (len(d) // 2), d[:len(d) // 2 * 2])[0::2]
sonando = [i for i, x in enumerate(izq) if abs(x) > 50]
if len(izq) < 44100 or not sonando:
    print("  ❌ música: no suena"); sys.exit(1)
racha = peor = 0
for x in izq[sonando[0]:sonando[-1]]:
    racha = racha + 1 if abs(x) <= 50 else 0
    peor = max(peor, racha)
dura = (sonando[-1] - sonando[0]) / 44100
if dura > 1.2 and peor < 441: print("  ✅ música: suena %.1f s seguidos, repitiéndose sin cortes" % dura)
else: print("  ❌ música: %.2f s, hueco de %d muestras" % (dura, peor)); sys.exit(1)
PY
else
    echo "  ⏭  música: este SDL no tiene el controlador «disk»"
fi
# Grabar el sonido a la par que los fotogramas: 90 fotogramas son 1,5 s de .wav
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=90 LINCE_JUEGO_AUDIO=grabado.wav timeout 30 "$LINCE" "$DIR/prueba7.lince" > /dev/null 2>&1
python3 - <<'PY' && ok "el sonido se graba sincronizado con los fotogramas" || mal "el sonido grabado no cuadra"
import wave, struct
w = wave.open("grabado.wav")
d = w.readframes(w.getnframes())
assert w.getnframes() == 90 * 735, w.getnframes()
assert max(abs(x) for x in struct.unpack("<%dh" % (len(d) // 2), d)) > 1000
PY
# ── El animador ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=140 timeout 20 "$LINCE" "$DIR/prueba9.lince" > salida13.txt 2>&1
[ $? -eq 0 ] && ok "la prueba del animador termina bien" || mal "la prueba del animador falló"
for l in "pisadas: 11 31 51" \
         "registro: correr:0:falso correr:1:falso correr:2:falso correr:3:falso correr:0:falso saltar:4:falso saltar:5:falso saltar:6:falso quieto:4:falso una_vez:4:falso una_vez:5:falso una_vez:6:falso una_vez:6:verdadero una_vez:6:verdadero"; do
    grep -qxF "$l" salida13.txt && ok "animador: ${l:0:40}" || mal "falta la línea: $l"
done
for e in "no hay ninguna animación 'volar'" '"fila" va de 0 a 1' "no conozco 'filas'" '"siguiente" es el nombre' "pisada() espera un animador"; do
    grep -qF "$e" salida13.txt && ok "animador: error «$e»" || mal "falta el error: $e"
done
# ── Animaciones generadas a partir de un dibujo (animar) y guardar_imagen ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=2 timeout 20 "$LINCE" "$DIR/prueba8.lince" > salida12.txt 2>&1
[ $? -eq 0 ] && ok "la prueba de animar termina bien" || mal "la prueba de animar falló"
for l in "andar: 8 8x6 relleno 2" "misma hoja: verdadero" "respirar: 4" "girar: 4" "frente: 0" "png: 64x6" "desde archivo: 2" "sola: 5 filas 2 3+2 celda 2x3" "por cuenta: 3 2x2" "con fondo: 2 filas 1 celda 4x6" "reducida: 7" "pisadas: 1 3 de 2" "quitar: 3 pisadas 2 de 1"; do
    grep -qxF "$l" salida12.txt && ok "animar: $l" || mal "falta la línea: $l"
done
for e in "no conozco la opción 'cuadro'" "no conozco la animación 'bailar'" '"vista" es "lado"' '"cuadros" va de 1 a 64' \
         "transparente del todo" "debe acabar en .png" "no hay fila 5" "no encuentro huecos transparentes" '"fondo" es "auto"' '"escala" reduce la imagen' "cada cuadro es un número de 0 a 3" "pisadas() espera una hoja"; do
    grep -qF "$e" salida12.txt && ok "animar: error «$e»" || mal "falta el error: $e"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("andar.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r, a = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 4]
    return (r, g, b)
casos = [((10, 22), (0, 0, 0), "el pie que avanza, levantado"), ((22, 22), (255, 0, 0), "el otro pie, en el suelo"),
         ((10, 14), (255, 0, 0), "la pierna levantada se ve más arriba")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ andar:", nombre)
    else: print("  ❌ andar:", nombre, "→", px(x, y)); mal += 1
d = open("hojas.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
for (x, y), esp, nombre in [((2, 2), (0, 0, 0), "cuadro bajo: arriba vacío"), ((2, 10), (0, 255, 0), "cuadro bajo: apoyado abajo"),
                            ((22, 6), (255, 0, 255), "fila 1, el cuadro da la vuelta"), ((42, 2), (0, 0, 255), "por cuenta: el cuadro 2")]:
    if px(x, y) == esp: print("  ✅ hoja:", nombre)
    else: print("  ❌ hoja:", nombre, "→", px(x, y)); mal += 1
d = open("fondo.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
for (x, y), esp, nombre in [((6, 6), (255, 255, 255), "el blanco de dentro de la figura se queda"),
                            ((2, 18), (0, 0, 0), "el fondo blanco de alrededor se quita"),
                            ((2, 2), (20, 20, 20), "la figura se ve"),
                            ((21, 1), (0, 0, 0), "imagen() con \"fondo\": el borde, transparente")]:
    if px(x, y) == esp: print("  ✅ fondo:", nombre)
    else: print("  ❌ fondo:", nombre, "→", px(x, y)); mal += 1
d = open("andar.png", "rb").read(8)
if d == b"\x89PNG\r\n\x1a\n": print("  ✅ guardar_imagen: es un PNG")
else: print("  ❌ guardar_imagen: no es un PNG"); mal += 1
sys.exit(1 if mal else 0)
PY
[ "$fallos" -eq 0 ] && echo "✅ Juego: todo correcto" || { cat salida.txt salida2.txt salida3.txt salida4.txt errores4.txt salida5.txt salida6.txt salida7.txt salida8.txt salida9.txt salida10.txt salida11.txt salida12.txt salida13.txt salida5b.txt; exit 1; }
