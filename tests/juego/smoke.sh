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
         "mandos: ok" "sprite: 3x3" "botón a: falso" "eje: 0" "tiempo: verdadero"; do
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
for l in "v2 conserva: verdadero hola v2" "v2 escena: cueva" "v3 tras el error: verdadero 160" "código 0"; do
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
grep -qF "no conozco la opción 'cantida'" salida7.txt && ok "opción de partículas mal escrita" || mal "falta el error de opción desconocida"
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
grep -qF "no conozco la opción 'volumne'" salida10.txt && ok "efecto: opción mal escrita" || mal "falta el error de opción de efecto"
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
for l in "cuadros_de: 4 3 7" "pisadas de una secuencia: [1, 3] [0] [1]" "trozo: [12, 14, 6, 8]"; do
    grep -qxF "$l" salida13.txt && ok "animador: $l" || mal "falta la línea: $l"
done
for e in "no hay ninguna animación 'volar'" '"fila" va de 0 a 1' "no conozco 'filas'" '"siguiente" es el nombre' "pisada() espera un animador" '"trozo" es [x, y, ancho, alto]'; do
    grep -qF "$e" salida13.txt && ok "animador: error «$e»" || mal "falta el error: $e"
done
# ── caja(): lo que se ve de cada cuadro, y colision/dentro con cajas ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=2 timeout 20 "$LINCE" "$DIR/prueba10.lince" > salida14.txt 2>&1
[ $? -eq 0 ] && ok "la prueba de caja termina bien" || mal "la prueba de caja falló"
for l in "cuadro 0: 11 21 2 3" "cuadro 1: 13 20 3 2" "escala: 16 20 6 4" "espejo: 13 21 2 3" "voltear: 13 22 3 2" \
         "centrado: 8 19 2 3" "margen: 13.5 20.5 2 1" "imagen: 11 20 11 4" "ancho: 2 0 22 4" "sola: celda 3x3, 0 1 3 2 / 0 0 2 3" \
         "animador: 13 20 3 2" "animador otro: 11 21 2 3" "choca: verdadero falso verdadero verdadero" \
         "dentro: verdadero falso" "circulo: verdadero falso"; do
    grep -qxF "$l" salida14.txt && ok "caja: $l" || mal "falta la línea: $l"
done
for e in "colision() espera dos formas" "caja() espera una imagen, una hoja o un animador" "no hay fila 3"; do
    grep -qF "$e" salida14.txt && ok "caja: error «$e»" || mal "falta el error: $e"
done
# ── Fuentes (TrueType y dibujadas), texto con opciones y movimiento suave ──
python3 "$DIR/hacer_fuente.py" cajas.ttf
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=2 timeout 20 "$LINCE" "$DIR/prueba11.lince" > salida15.txt 2>&1
[ $? -eq 0 ] && ok "la prueba de letras termina bien" || mal "la prueba de letras falló"
for l in "alto: 20" "AB: 18 20" "A B: 26 20" "lineas: 24 40" "partido: 30 40" "parecidas: 28" "escala: 36" "misma: verdadero" \
         "lince: 69 21 69" "dibujada: 9 4 espacio 6" "rejilla: 9" "curvas: 0.25 0.125 0.875 0.5 1 1088 0 1" \
         "suavizar: 50 0 10 20" "acercar: 15 9 100 5"; do
    grep -qxF "$l" salida15.txt && ok "letras: $l" || mal "falta la línea: $l"
done
for e in "no se encuentra la fuente 'no_existe.ttf'" "encuentro 3 letras en la imagen y hay 2 caracteres" "no conozco la opción 'colr'" \
         '"alinear" es "izquierda"' "listas de números del mismo largo" 'la curva es "lineal"' "fuente() espera (ruta de la fuente, tamaño"; do
    grep -qF "$e" salida15.txt && ok "letras: error «$e»" || mal "falta el error: $e"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct
d = open("letras.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 3]
    return (r, g, b)
casos = [((15, 20), (255, 0, 0), "la A, en rojo"), ((11, 20), (0, 0, 0), "a la izquierda de la A, nada"),
         ((15, 11), (0, 0, 0), "encima de la A, nada"), ((21, 20), (0, 0, 0), "entre la A y la B (con kerning)"),
         ((23, 20), (255, 0, 0), "la B"), ((100, 50), (255, 255, 255), "centrada: en medio"), ((95, 50), (0, 0, 0), "centrada: fuera"),
         ((158, 10), (255, 255, 0), "dibujada a escala 4: la H"), ((175, 15), (0, 0, 0), "dibujada: el hueco de la O")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ letras:", nombre)
    else: print("  ❌ letras:", nombre, "→", px(x, y)); mal += 1
exit(1 if mal else 0)
PY
# ── Pelear: golpes por cuadro, combos, congelar, velocidad, temblor, blanco y mezcla ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=100 \
    LINCE_JUEGO_GUION=50:abajo,53:derecha,56:z,60:z,70:derecha,72:abajo,74:z \
    timeout 20 "$LINCE" "$DIR/prueba12.lince" > salida16.txt 2>&1
[ $? -eq 0 ] && ok "la prueba de pelear termina bien" || mal "la prueba de pelear falló"
for l in "ventana: 120x60" "celdas: 6" "f0: golpe verdadero libre falso" "f3: golpe verdadero libre falso" "golpe: 105 22 5 6" \
         "espejo: 100 22 5 6" "escala: 100 14 10 12" "libre: falso" "cancelar: verdadero verdadero" "despues: quieto verdadero" \
         "vacias fuera: 0" "congelado: 0 0" "sigue: 10 5" "velocidad: 0.5" "lento: 10" "combo en 56, luego golpe solo: falso" \
         "guardado en 66: verdadero" "guardado en 67: falso" "camara: 10"; do
    grep -qxF "$l" salida16.txt && ok "pelear: $l" || mal "falta la línea: $l"
done
[ "$(grep -c '^combo en' salida16.txt)" -eq 1 ] && ok "pelear: el combo sale una sola vez" || mal "el combo sale más de una vez (o ninguna)"
for e in "golpe() espera un animador" '"golpes" es una lista de [cuadro, x, y, ancho, alto]' '"cancelar" es el cuadro' \
         "la acción 'nada' no está definida" "combo() espera (lista de acciones" "congelar(): los segundos van de 0 a 10" \
         "velocidad(): va de 0" "temblor(): la fuerza" '"blanco" va de 0' '"mezcla" es "normal", "sumar"' 'particulas(): "mezcla"'; do
    grep -qF "$e" salida16.txt && ok "pelear: error «$e»" || mal "falta el error: $e"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct
d = open("golpes.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 3]
    return (r, g, b)
mal = 0
if (w, abs(h)) == (240, 120): print("  ✅ pelear: con escala 2 la ventana mide el doble")
else: print("  ❌ pelear: la ventana mide", w, abs(h)); mal += 1
casos = [((0, 0), (255, 255, 255), "tras camara() no tiembla"), ((10, 10), (100, 100, 100), "y acaba donde debe"),
         ((121, 81), (255, 255, 255), "blanco 1: todo blanco"), ((141, 81), (255, 100, 100), "mezcla sumar"),
         ((161, 81), (255, 127, 127), "blanco 0,5: a medias"), ((181, 81), (255, 0, 0), "sin nada: rojo"),
         ((43, 83), (255, 255, 255), "blanco en una hoja de archivo"), ((55, 83), (100, 100, 100), "su parte transparente sigue transparente"),
         ((73, 83), (255, 255, 255), "blanco en otro cuadro de la misma hoja"),
         ((61, 61), (200, 100, 100), "partículas que suman")]
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ pelear:", nombre)
    else: print("  ❌ pelear:", nombre, "→", px(x, y)); mal += 1
exit(1 if mal else 0)
PY
# ── Lucha: mantenida, pulsaciones, "colores", reproducir con "lado" y "tono", partículas que multiplican ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=80 LINCE_JUEGO_GUION=10:z:30,50:x,52:x,54:x \
    LINCE_JUEGO_AUDIO=lado.wav timeout 20 "$LINCE" "$DIR/prueba13.lince" > salida17.txt 2>&1
[ $? -eq 0 ] && ok "la prueba de lucha termina bien" || mal "la prueba de lucha falló"
for l in "distintas: verdadero" "la misma: verdadero" "voz: verdadero" "cargando 9: 0" "cargando 25: 15" "soltada: 0 con margen: 30" \
         "gastada: verdadero" "pulsaciones: 3 en 0,05 s: 1" "tarde: 0" "tono: verdadero"; do
    grep -qxF "$l" salida17.txt && ok "lucha: $l" || mal "falta la línea: $l"
done
for e in "la ventana no está abierta" "mantenida(): la acción 'nada' no está definida" "mantenida(): el margen va de 0 a 10" \
         "pulsaciones(): los segundos van de 0 a 10" "combo(): los segundos van de 0 a 10" '"colores" es una lista de hasta 16 pares' \
         'imagen(): no conozco la opción '"'"'tinte'"'"' (hay "fondo", "escala" y "colores")' '"escala" y "colores")' \
         '"lado" va de -1' '"tono" va de 0.25 a 4' "no conozco la opción 'eco'" '"multiplicar" (humo que oscurece)'; do
    grep -qF "$e" salida17.txt && ok "lucha: error «$e»" || mal "falta el error: $e"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, array
d = open("lucha.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 3]
    return (r, g, b)
casos = [((0, 0), (40, 200, 40), "colores: el rojo pasa a verde"), ((1, 0), (20, 100, 20), "colores: su sombra, a verde oscuro"),
         ((2, 0), (40, 40, 200), "colores: el azul no se toca"), ((3, 0), (128, 128, 128), "colores: el gris no se toca"),
         ((4, 0), (200, 150, 120), "colores: la piel no se toca"), ((0, 2), (40, 200, 40), "colores: también en una hoja"),
         ((1, 2), (20, 100, 20), "colores: la hoja, la sombra"), ((20, 20), (100, 200, 200), "partículas que multiplican")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ lucha:", nombre)
    else: print("  ❌ lucha:", nombre, "→", px(x, y)); mal += 1
s = array.array("h", open("lado.wav", "rb").read()[44:])
izq, der = s[0::2], s[1::2]
corte = 70 * 735                                            # la segunda voz empieza en el fotograma 70
primera = [i for i in range(corte) if der[i]]
segunda = [i for i in range(corte, len(der)) if der[i]]
if primera and not any(izq[:corte]): print("  ✅ lucha: \"lado\": 1 suena solo a la derecha")
else: print("  ❌ lucha: \"lado\": 1 suena también a la izquierda"); mal += 1
largo1 = primera[-1] - primera[0] if primera else 0
largo2 = segunda[-1] - segunda[0] if segunda else 0
if largo1 and abs(largo2 * 2 - largo1) < 10: print("  ✅ lucha: \"tono\": 2 dura la mitad")
else: print("  ❌ lucha: \"tono\": 2 dura", largo2, "y el normal", largo1); mal += 1
exit(1 if mal else 0)
PY
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
# ── Tareas: avanzan en presentar() y se paran al acabar o al pulsar F5 ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=60 LINCE_JUEGO_CAPTURA=tareas.bmp timeout 20 "$LINCE" "$DIR/prueba14.lince" > salida18.txt 2>&1
[ $? -eq 0 ] && ok "las tareas terminan bien" || mal "la prueba de tareas falló"
for l in "tarea: medio segundo en el fotograma 31" "tarea: x = 30 en el fotograma 34" "fuera de una tarea: verdadero" \
         "fin: 60" "tarea: marca parada" "tiempo real: 1" "cubrir sin color: cubrir() espera un color, por ejemplo [0, 0, 0, 128]"; do
    grep -qxF "$l" salida18.txt && ok "$l" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("tareas.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r, a = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 4]
    return (r, g, b)
if px(5, 5) == (255, 0, 0): print("  ✅ lo que dibuja una tarea se ve")
else: print("  ❌ lo que dibuja una tarea no se ve:", px(5, 5)); sys.exit(1)
if px(2, 25) == (0, 0, 128) and px(38, 2) == (0, 0, 128): print("  ✅ cubrir() tapa toda la ventana")
else: print("  ❌ cubrir():", px(2, 25), px(38, 2)); sys.exit(1)
PY
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=8 LINCE_JUEGO_PULSAR=f5 timeout 20 "$LINCE" "$DIR/prueba14.lince" > salida19.txt 2>&1
[ "$(grep -cxF "tarea: marca parada" salida19.txt)" -ge 2 ] && ok "F5 para las tareas y vuelven a empezar" || mal "F5 no para las tareas"
# ── Pantalla táctil: toques() y botones en la pantalla (el clic de prueba hace de dedo) ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=10 LINCE_JUEGO_CLIC=15,15 timeout 20 "$LINCE" "$DIR/prueba15.lince" > salida20.txt 2>&1
for l in "tactil: falso" "dedo en: 15,15" "pulsada: 5, nuevos: 5"; do
    grep -qxF "$l" salida20.txt && ok "táctil: $l" || mal "falta la línea: $l"
done
grep -qF 'un botón en la pantalla es "pantalla:x,y,ancho,alto"' salida20.txt && ok "táctil: error de un botón mal escrito" || mal "falta el error de pantalla:"
# ── Mando en la pantalla táctil (el clic de prueba encima del botón A) ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=10 LINCE_JUEGO_CLIC=388,214 timeout 20 "$LINCE" "$DIR/prueba16.lince" > salida21.txt 2>&1
for l in 'acciones: {"saltar": ["espacio", "mando:a"]}' "saltos: 5, con boton(): 5" "palanca: 0" "eje limitado: 1"; do
    grep -qxF "$l" salida21.txt && ok "mando táctil: $l" || mal "falta la línea: $l"
done
grep -qF 'no conozco la opción '"'"'boton'"'" salida21.txt && grep -qF '"botones" es una lista' salida21.txt && ok "mando táctil: errores de opciones" || mal "faltan los errores de mando_tactil"
grep -q "no debería" salida21.txt && mal "mando táctil: se pulsó B" || ok "mando táctil: B no se pulsa"
# ── Vibrar e inclinar (en el ordenador: sin vibrador ni acelerómetro) ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=5 timeout 20 "$LINCE" "$DIR/prueba17.lince" > salida22.txt 2>&1
for l in "inclinacion: [0, 0]" "accion: falso" "error: vibrar(): los segundos van de 0 a 10" "error: vibrar(): la fuerza va de 0 a 1"; do
    grep -qxF "$l" salida22.txt && ok "vibrar e inclinar: $l" || mal "falta la línea: $l"
done
grep -qF 'la inclinación es "inclinacion:x-"' salida22.txt && ok "inclinar: error de una entrada mal escrita" || mal "falta el error de inclinacion:"
# ── Partidas guardadas (en una casa de mentira: ~/.local/share/lince/prueba18 o %APPDATA%\Lince\prueba18) ──
mkdir -p casa
CASA="$T/casa"; CASA_W="$CASA"
command -v cygpath > /dev/null && CASA_W="$(cygpath -w "$CASA")"
HOME="$CASA" APPDATA="$CASA_W" XDG_DATA_HOME= timeout 20 "$LINCE" "$DIR/prueba18.lince" > salida23.txt 2>&1
for l in "nada: nulo" "por defecto: 0.8" "nivel: 3, Ñandú, 95.5, verdadero" "opciones: 0.25 es" "otra vez: 4" \
         "olvidar: verdadero falso" "carpeta: verdadero" \
         'error: guardar_partida(): el nombre "../fuera" no vale (sin / \ : * ? " < > |, y que no empiece por punto)' \
         "error: guardar_partida(): no se puede guardar eso; se guardan solo números, textos, lógicos, nulo, listas y diccionarios"; do
    grep -qxF "$l" salida23.txt && ok "partidas: $l" || mal "falta la línea: $l"
done
[ -f casa/.local/share/lince/prueba18/opciones.json ] || [ -f casa/Lince/prueba18/opciones.json ] \
    && ok "partidas: en la carpeta de datos del usuario" || mal "partidas: no está opciones.json en la carpeta de datos"
# ── Menús y diálogos, con teclas y con el ratón ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=100 \
    LINCE_JUEGO_GUION=5:abajo:1,10:entrar:1,20:espacio:1,30:espacio:1,40:abajo:1,45:abajo:1,50:entrar:1,60:escape:1 \
    timeout 30 "$LINCE" "$DIR/prueba19.lince" > salida24.txt 2>&1
for l in "menu: Opciones" "respuesta: Luego" "pausa: Continuar" 'error: menu() espera una lista de textos, como ["Jugar", "Salir"]' \
         'error: menu(): no conozco la opción '"'"'colr'"'"' (hay "x", "y", "escala", "fuente", "color", "color_elegido", "separacion", "atras", "sonido_mover" y "sonido_elegir")' \
         'error: dialogo(): cada página es un texto o un diccionario con "texto"'; do
    grep -qxF "$l" salida24.txt && ok "menús y diálogos: $l" || mal "falta la línea: $l"
done
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=12 LINCE_JUEGO_CLIC=160,150 \
    timeout 30 "$LINCE" "$DIR/prueba19.lince" > salida25.txt 2>&1
grep -qxF "menu: Salir" salida25.txt && ok "menús: se elige con un clic (o el dedo)" || mal "menús: el clic no elige"
# ── Mapas de Tiled (.tmx con tileset .tsx, y .tmj con capa en base64 y zlib) ──
(cd "$DIR" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=3 LINCE_JUEGO_CAPTURA="$T/tiled.bmp" \
    timeout 30 "$LINCE" prueba20.lince) > salida26.txt 2>&1
for ruta in tiled/nivel.tmx tiled/nivel.tmj tiled/infinito.tmx tiled/infinito.tmj; do
    for l in "$ruta: 10x6, casilla 8, capas [\"Fondo\", \"Suelo\"]" "objeto: inicio jugador 8,16 8x8 rectangulo {}"; do
        grep -qxF "$l" salida26.txt && ok "tiled: $l" || mal "falta la línea: $l"
    done
done
for l in "propiedades: tema & fondo 900" 'objeto: moneda moneda 40,16 8x8 tile {"valor": 5}' "casillas: # = . verdadero" \
         "cae a y=34 en_suelo=verdadero" "error: mapa.dibujar_tiled(): el mapa no tiene la capa 'NoExiste' (mira mapa[\"capas\"])" \
         "error: mapa.tiled(): no se encuentra 'tiled/no_esta.tmx'"; do
    [ "$(grep -cxF "$l" salida26.txt)" -ge 1 ] && ok "tiled: $l" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("tiled.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
def px(x, y):
    fila = (abs(h) - 1 - y) if h > 0 else y
    b, g, r, a = d[off + (fila * w + x) * 4: off + (fila * w + x) * 4 + 4]
    return (r, g, b)
casos = [((1, 4), (40, 80, 230), "tile volteado"), ((6, 4), (220, 40, 40), "tile volteado (la otra mitad)"),
         ((20, 20), (250, 250, 250), "fondo"), ((44, 28), (40, 80, 230), "plataforma"),
         ((12, 36), (240, 220, 40), "tile animado"), ((40, 44), (220, 40, 40), "suelo")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ tiled, píxel:", nombre)
    else: print("  ❌ tiled, píxel:", nombre, "→", px(x, y)); mal += 1
sys.exit(1 if mal else 0)
PY
# ── Física sencilla ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=3 timeout 60 "$LINCE" "$DIR/prueba21.lince" > salida27.txt 2>&1
for l in "caja en el suelo: 204 verdadero" "pelota rebota: verdadero y se para: 208 verdadero" "apiladas: 204 184 verdadero" \
         "la moneda no cae y avisa: 180 verdadero" "el pasajero sube con el ascensor: 16 verdadero" "empujar: 200 20" "cuerpos: 8" \
         'error: fisica.cuerpo(): falta "y" (un número; o "radio" en vez de ancho y alto)' "error: fisica.avanzar() espera un mundo de juego.fisica.mundo()"; do
    grep -qxF "$l" salida27.txt && ok "física: $l" || mal "falta la línea: $l"
done
# ── Controles a gusto (en una carpeta de partidas de mentira) ──
LINCE_JUEGO_GUARDADO="$T/controles" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=40 \
    LINCE_JUEGO_GUION=5:entrar:1,10:z:1,20:escape:1 timeout 30 "$LINCE" "$DIR/prueba22.lince" > salida28.txt 2>&1
for l in 'saltar: ["z", "mando:a"]' 'disparar: ["espacio", "mando:x"]' 'guardado: {"disparar": ["espacio", "mando:x"], "saltar": ["z", "mando:a"]}' \
         'al definirla otra vez: ["z", "mando:a"]' 'restablecidos: ["espacio", "mando:a"] ["z", "mando:x"] nulo' "mando: mando:y"; do
    grep -qxF "$l" salida28.txt && ok "controles: $l" || mal "falta la línea: $l"
done
# ── hoja() sin ventana: "origen" de cada figura, quitar, guardar; y dibujarlas al abrirla ──
python3 - <<'PY'
import zlib, struct
W, H = 60, 40
cajas = [(5, 5, 14, 20), (25, 8, 40, 22), (45, 30, 50, 35), (10, 28, 20, 38)]
raw = b""
for y in range(H):
    raw += b"\0"
    for x in range(W):
        raw += bytes([255, 0, 0, 255]) if any(a <= x <= c and b <= y <= d for a, b, c, d in cajas) else bytes([0, 0, 0, 0])
def trozo(t, d): c = struct.pack(">I", len(d)) + t + d; return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
open("figuras.png", "wb").write(b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
                              + trozo(b"IDAT", zlib.compress(raw)) + trozo(b"IEND", b""))
PY
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=3 LINCE_JUEGO_CAPTURA=sinventana.bmp timeout 30 "$LINCE" "$DIR/prueba23.lince" > salida29.txt 2>&1
for l in "cuadros: 4 por_fila: [2, 2]" "origen: [[5, 5, 10, 16], [25, 8, 16, 15], [10, 28, 11, 11], [45, 30, 6, 6]]" \
         "quitando el 1: [[5, 5, 10, 16], [10, 28, 11, 11], [45, 30, 6, 6]] por_fila: [1, 2]" \
         "rejilla: [[0, 0, 30, 20], [30, 0, 30, 20], [0, 20, 30, 20], [30, 20, 30, 20]]" "guardada: verdadero" "pisadas: [1] caja: [3, 0, 10, 16]"; do
    grep -qxF "$l" salida29.txt && ok "hoja sin ventana: $l" || mal "falta la línea: $l"
done
python3 - <<'PY' && ok "hoja sin ventana: al abrir la ventana se dibujan (797 píxeles rojos)" || mal "hoja sin ventana: no se dibuja al abrir la ventana"
import struct
d = open("sinventana.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]; bpp = struct.unpack_from("<H", d, 28)[0] // 8
rojos = sum(1 for i in range(off, len(d) - bpp + 1, bpp) if d[i + 2] > 200 and d[i] < 50)
assert rojos == 797, rojos
PY
# ── LINCE_JUEGO_ESCENA: empezar en otra escena ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=8 timeout 30 "$LINCE" "$DIR/prueba24.lince" > salida30.txt 2>&1
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=8 LINCE_JUEGO_ESCENA=final timeout 30 "$LINCE" "$DIR/prueba24.lince" > salida31.txt 2>&1
{ [ "$(tr -d '\r' < salida30.txt)" = "$(printf 'entra en portada\nentra en partida\nacaba en partida')" ] && ok "escena: sin la variable, el juego va por sus escenas"; } || mal "escena: sin la variable"
{ [ "$(tr -d '\r' < salida31.txt)" = "$(printf 'entra en final\nacaba en final')" ] && ok "escena: LINCE_JUEGO_ESCENA=final empieza en esa escena"; } || mal "escena: LINCE_JUEGO_ESCENA=final"
# ── LINCE_JUEGO_GRABAR_GUION: lo que se graba vuelve a reproducirse igual ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=100 LINCE_JUEGO_GUION="30:derecha:20,50:z:1,10:espacio:5,70:f3:2,80:escape:1" \
    LINCE_JUEGO_GRABAR_GUION=guion1.txt timeout 30 "$LINCE" "$DIR/prueba25.lince" > /dev/null 2>&1
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=100 LINCE_JUEGO_GUION="$(tr -d '\r' < guion1.txt)" \
    LINCE_JUEGO_GRABAR_GUION=guion2.txt timeout 30 "$LINCE" "$DIR/prueba25.lince" > /dev/null 2>&1
[ "$(tr -d '\r' < guion1.txt)" = "10:espacio:5,30:derecha:20,50:z:1,70:f3:2" ] && ok "grabar guion: apunta las teclas (sin Esc), por orden" || mal "grabar guion: $(cat guion1.txt)"
[ "$(tr -d '\r' < guion1.txt)" = "$(tr -d '\r' < guion2.txt)" ] && ok "grabar guion: lo grabado se reproduce igual" || mal "grabar guion: al reproducirlo sale otra cosa: $(cat guion1.txt) / $(cat guion2.txt)"
# ── 1.5: capas, cámara (zoom, giro, zona), a_mundo, texto en una caja e idiomas ──
mkdir -p idiomas
printf '# inglés\nidioma = English\nJugar = Play\nVidas: {} = Lives: {}\n{} de {} = {} of {}\n' > idiomas/en.txt
printf 'idioma = Español\n' > idiomas/es.txt
LANG=en_US.UTF-8 LC_ALL= LANGUAGE= SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=2 LINCE_JUEGO_CAPTURA=capas.bmp \
    timeout 30 "$LINCE" "$DIR/prueba26.lince" > salida32.txt 2>&1
for l in "capa de antes: 0" "a_mundo: [70, 80] a_pantalla: [30, 70]" \
         'camara_actual: {"x": 10, "y": 20, "zoom": 2, "angulo": 90, "zona": [0, 0, 100, 100], "temblor": verdadero}' \
         "sin zoom: [5, 5]" "resto: una vez un lince que vivía en el bosque" "cabe: []" "medir con salto: [22, 32]" \
         'idiomas: {"en": English, "es": Español}' "elegido solo: en" "Play / Lives: 3 / 1 of 5 / Sin traducir" "original: Jugar nulo" \
         "error: idioma(): no está idiomas/fr.txt (cada idioma es un archivo en la carpeta idiomas del juego)" \
         'error: camara(): "zoom" es mayor que 0 (2: todo el doble de grande; 0.5: se ve el doble de mundo)' \
         'error: camara(): "zona" es [x, y, ancho, alto], la parte de la pantalla donde se ve' \
         'error: "capa" es un número: lo de las capas más altas queda encima' \
         'error: texto_caja(): "vertical" es "arriba", "centro" o "abajo"'; do
    grep -qxF "$l" salida32.txt && ok "1.5: $l" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("capas.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]; w, h = struct.unpack_from("<ii", d, 18); B = struct.unpack_from("<H", d, 28)[0] // 8
fila = (w * B + 3) & ~3
def px(x, y):
    f = (abs(h) - 1 - y) if h > 0 else y
    i = off + f * fila + x * B
    return (d[i + 2], d[i + 1], d[i])
casos = [((5, 5), (255, 0, 0), "sin capa, debajo"), ((22, 22), (0, 0, 255), "capa 2 encima de la 0 y la 1"),
         ((45, 45), (0, 255, 0), "capa 0 tras las capas"), ((16, 16), (0, 0, 255), "capa 1 debajo de la 2"),
         ((150, 25), (255, 255, 255), "zoom 2: el centro"), ((141, 16), (255, 255, 255), "zoom 2: mide el doble"),
         ((138, 25), (40, 40, 40), "zoom 2: alrededor"), ((95, 25), (0, 0, 0), "la zona recorta"),
         ((148, 80), (255, 0, 255), "giro 90: vertical"), ((165, 76), (0, 0, 0), "giro 90: ya no es horizontal")]
mal = 0
for (x, y), esp, nombre in casos:
    if px(x, y) == esp: print("  ✅ 1.5, píxel:", nombre)
    else: print("  ❌ 1.5, píxel:", nombre, "→", px(x, y)); mal += 1
sys.exit(1 if mal else 0)
PY
# ── 1.6: física con giros, uniones, polígonos y sonido (grupos, fundidos, con sitio) ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=260 LINCE_JUEGO_AUDIO=mezcla.wav \
    timeout 120 "$LINCE" "$DIR/prueba27.lince" > salida33.txt 2>&1
for l in "pila: 282 quieta: verdadero" "cae de esquina y queda plana: 380 verdadero" "la bola baja rodando: verdadero verdadero" \
         "en_suelo: verdadero verdadero" "la pelota resbala por la esquina: verdadero" \
         "bisagra: no se suelta: verdadero y cae girando: verdadero" "cuerda: no se estira: verdadero" \
         "uniones tras quitar el peso: 0" "mapa: 66 verdadero" "puntos_cuerpo: [[0, 0], [10, 0], [10, 4], [0, 4]]" \
         "sonando: verdadero" "sonando al irse: falso" "grupos: 0 1 1" "cambiar_voz: verdadero falso falso" \
         'error: fisica.cuerpo(): el polígono tiene que ser convexo (sin entrantes); uno con entrantes se hace con varios' \
         'error: fisica.cuerpo(): los puntos del polígono no encierran nada (están en línea)' \
         'error: fisica.cuerda(): ata dos cuerpos distintos (o uno y un punto [x, y])' \
         'error: fisica.bisagra(): ese cuerpo no está en este mundo' \
         'error: volumen(): el grupo es "efectos", "musica" o "voces"' \
         'error: musica(): "fundido" son los segundos del paso de una música a otra (de 0 a 60)' \
         'error: reproducir(): el grupo es "efectos", "musica" o "voces"' \
         'error: reproducir_en(): el lado sale de dónde está (x, y); para ponerlo a mano, reproducir()'; do
    grep -qxF "$l" salida33.txt && ok "1.6: $l" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys, wave
mal = 0
def mira(bien, nombre):
    global mal
    print("  ✅ 1.6:" if bien else "  ❌ 1.6:", nombre)
    mal += not bien
# el sonido, a trozos de fotogramas (735 muestras cada uno)
w = wave.open("mezcla.wav"); n = w.getnframes()
d = struct.unpack("<%dh" % (n * 2), w.readframes(n))
def rms(f0, f1, c):
    s = d[f0 * 1470 + c:f1 * 1470:2]
    return (sum(x * x for x in s) / len(s)) ** 0.5
mira(rms(0, 10, 0) < rms(25, 35, 0) < rms(50, 60, 0), "la música entra subiendo (fundido)")
mira(rms(95, 115, 0) < 50, "volumen(\"musica\", 0) la calla")
mira(rms(140, 148, 0) < rms(122, 128, 0) / 2 and rms(155, 168, 0) < 50, "musica(nulo, fundido) baja y para")
mira(rms(175, 188, 0) > 3 * rms(175, 188, 1), "reproducir_en a la izquierda suena por la izquierda")
mira(rms(195, 208, 1) > 3 * rms(195, 208, 0), "mover_voz a la derecha")
mira(rms(225, 238, 1) < 50, "detener(voz, segundos) acaba callando")
mira(rms(245, 258, 0) < 50, "volumen(\"efectos\", 0) calla los efectos")
# los polígonos
b = open("poligonos.bmp", "rb").read()
off = struct.unpack_from("<I", b, 10)[0]; ancho, alto = struct.unpack_from("<ii", b, 18); B = struct.unpack_from("<H", b, 28)[0] // 8
fila = (ancho * B + 3) & ~3
def px(x, y):
    f = (abs(alto) - 1 - y) if alto > 0 else y
    i = off + f * fila + x * B
    return (b[i + 2], b[i + 1], b[i])
for (x, y), esp, nombre in [((50, 20), (255, 0, 0), "polígono relleno"), ((15, 50), (0, 0, 0), "fuera del polígono"),
                            ((100, 30), (0, 255, 0), "solo el borde"), ((125, 35), (0, 0, 0), "sin relleno dentro"),
                            ((185, 15), (0, 0, 255), "polígono en una capa")]:
    mira(px(x, y) == esp, "píxel: %s %s" % (nombre, "" if px(x, y) == esp else px(x, y)))
sys.exit(1 if mal else 0)
PY
# ── 1.8: varios jugadores (mandos de mentira en el guion) ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=200 \
    LINCE_JUEGO_GUION="5:mando2:a,10:entrar,15:mando1:inicio,20:mando3:a,30:mando2:a,35:espacio,40:mando2:izq_x+:3,55:espacio,62:mando1:b,72:mando3:a,76:mando3:inicio" \
    timeout 60 "$LINCE" "$DIR/prueba28.lince" > salida34.txt 2>&1
for l in "mandos: 3 verdadero falso" "se une el 1 en 5" "se une el 2 en 10" "se une el 3 en 15" \
         'jugadores: [{"numero": 1, "dispositivo": mando, "mando": 2, "conectado": verdadero}, {"numero": 2, "dispositivo": teclado, "mando": nulo, "conectado": verdadero}, {"numero": 3, "dispositivo": mando, "mando": 1, "conectado": verdadero}]' \
         "saltar (de todos) en 30: verdadero" "dirección del 1: 1 del 3: 0" "mantenida del 1: verdadero del 2: 0" \
         "combo del 1: verdadero del 3: falso del 4: falso" \
         'tras quitar al 1: [{"numero": 1, "dispositivo": teclado, "mando": nulo, "conectado": verdadero}, {"numero": 2, "dispositivo": mando, "mando": 1, "conectado": verdadero}]' \
         'menu_jugadores en 76: [{"numero": 1, "dispositivo": mando, "mando": 3, "conectado": verdadero}]' \
         'saltos: ["1@5", "1@30", "2@35", "1@55", "2@62"]' "acciones sin copias: 3" \
         "partida 2: [[0, 0, 100, 100], [100, 0, 100, 100]] 3: 3" "una encima de otra: [0, 50, 200, 50]" \
         "error: accion(): el jugador es un número de 1 a 8" 'error: unirse(): "maximo" va de 1 a 8 jugadores' \
         "error: quitar_jugador(): no hay jugador 7" "error: pantalla_partida(): de 1 a 4 jugadores" \
         "error: mando_conectado(): el mando va de 1 a 4" "error: vibrar(): el mando va de 1 a 4" \
         'error: menu_jugadores(): "minimo" va de 1 a "maximo"'; do
    grep -qxF "$l" salida34.txt && ok "1.8: ${l:0:70}" || mal "falta la línea: $l"
done
# ── 2.0: colision única, juego.fisica.*, juego.mapa.*, partidas, cámara; y lo de la 1.x, un error claro ──
LINCE_JUEGO_GUARDADO="$T/p19" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=3 \
    timeout 60 "$LINCE" "$DIR/prueba29.lince" > salida35.txt 2>&1
for l in "caja-caja: verdadero falso" "punto-caja: verdadero falso falso" "círculo-caja: verdadero falso" \
         "círculo-círculo: verdadero falso" "punto-círculo: verdadero falso" "diccionario: verdadero" \
         'error: colision() espera dos formas: un punto [x, y], una caja [x, y, ancho, alto] (o un diccionario con x, y, ancho y alto) o un círculo {"centro": [x, y], "radio": r}' \
         'mapa: verdadero . [[0, 0]]' "física: verdadero 2" "cámara: 30 falso" 'partida: {"vidas": 3} verdadero' "partículas: 0" \
         "1.x: Esto es de la 1.x, y en la 2.0 cambió: juego.mapa(filas, tamaño) → juego.mapa.crear(filas, tamaño) («lince migrar» lo cambia solo)." \
         "1.x: Esto es de la 1.x, y en la 2.0 cambió: juego.solido → juego.mapa.solido («lince migrar» lo cambia solo)." \
         "1.x: Esto es de la 1.x, y en la 2.0 cambió: juego.cuerpo → juego.fisica.cuerpo («lince migrar» lo cambia solo)." \
         "1.x: Esto es de la 1.x, y en la 2.0 cambió: juego.dentro(px, py, x, y, ancho, alto) → juego.colision([px, py], [x, y, ancho, alto]) («lince migrar» lo cambia solo)." \
         "1.x: Esto es de la 1.x, y en la 2.0 cambió: juego.cargar → juego.cargar_partida («lince migrar» lo cambia solo)." \
         '1.x: camara(): no conozco la opción '"'"'giro'"'"' (hay "x", "y", "zoom", "angulo", "zona" y "temblor")' \
         '1.x: texto(): la escala, el color y la fuente van en las opciones: texto(t, x, y, {"escala": 2, "color": [255, 255, 255]})' \
         '1.x: reproducir(): el volumen y el bucle van en las opciones: reproducir(sonido, {"volumen": 0.5, "bucle": verdadero})'; do
    grep -qxF "$l" salida35.txt && ok "2.0: ${l:0:70}" || mal "falta la línea: $l"
done
# ── 2.1: amortiguar (el filtro de la mezcla) y crear_tono ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=170 LINCE_JUEGO_AUDIO=amortiguar.wav \
    timeout 60 "$LINCE" "$DIR/prueba30.lince" > salida36.txt 2>&1
for l in "tono: verdadero verdadero" "ciclos enteros: verdadero" "amortiguar de salida: 0" \
         "sigue sonando: verdadero" "amortiguar puesto: 1" \
         'error: crear_tono(): "forma" es "seno", "cuadrada", "sierra", "triangulo" o "ruido"' \
         "error: crear_tono(): la frecuencia va de 20 a 20000 Hz y la duración, en segundos, hasta 30" \
         'error: crear_tono(): "volumen" va de 0 a 1' \
         "error: amortiguar() va de 0 (nítido) a 1 (como de otra habitación)" \
         "error: amortiguar(): no conozco la opción 'segundos' (hay \"fundido\")" \
         'error: reproducir(): "amortiguado" va de 0 (nítido) a 1 (como de otra habitación)' \
         'error: reproducir(): "nitido" es verdadero o falso: con verdadero, esta voz no la toca amortiguar()'; do
    grep -qxF "$l" salida36.txt && ok "2.1: ${l:0:70}" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys, wave
mal = 0
def mira(bien, nombre):
    global mal
    print("  ✅ 2.1:" if bien else "  ❌ 2.1:", nombre)
    mal += not bien
w = wave.open("amortiguar.wav"); n = w.getnframes()
d = struct.unpack("<%dh" % (n * 2), w.readframes(n))
def rms(f0, f1, c=0):                      # por fotogramas (735 muestras cada uno)
    s = d[f0 * 1470 + c:f1 * 1470:2]
    return (sum(x * x for x in s) / len(s)) ** 0.5 if s else 0
# un tono de 8 kHz: el filtro corta en 260 Hz, así que tiene que hundirlo
base, voz, general, nitido = rms(10, 30), rms(45, 70), rms(85, 110), rms(125, 150)
mira(base > 2000, "crear_tono suena")
mira(voz < base / 8, "amortiguado=1 en la voz apaga el agudo")
mira(general < base / 8, "amortiguar(1) apaga el conjunto")
mira(nitido > base / 2, "nitido se salta amortiguar(): el detalle sigue nitido")
sys.exit(1 if mal else 0)
PY

# ── 2.2: menu_opciones(), y lo elegido se aplica solo la próxima vez ──
G22="3:izquierda,5:abajo,7:izquierda,9:izquierda,11:abajo,13:abajo,15:entrar,17:abajo,19:entrar,21:escape,23:abajo,25:derecha,27:abajo,29:entrar,31:abajo,33:izquierda,35:escape"
LINCE_JUEGO_GUARDADO="$T/opciones" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=60 LINCE_JUEGO_GUION="$G22" \
    timeout 60 "$LINCE" "$DIR/prueba31.lince" > salida36.txt 2>&1
SEGUNDA=1 LINCE_JUEGO_GUARDADO="$T/opciones" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=10 \
    timeout 60 "$LINCE" "$DIR/prueba31.lince" >> salida36.txt 2>&1
for l in "menu_opciones en 35: volver" "ahora: 0.9 0.8 1" "propias: Difícil verdadero 4" \
         "al abrir: 0.9 0.8 1" "propias: Difícil verdadero 4 7" \
         'error: menu_opciones(): no conozco la opción '"'"'color_raro'"'"' (hay "propias", "mostrar", "titulo", "escala", "fuente", "color" y "color_elegido")' \
         'error: menu_opciones(): la opción '"'"'x'"'"' necesita "valores" (una lista), o un "inicial" lógico o numérico' \
         'error: menu_opciones(): no conozco la opción '"'"'volumen'"'"' (hay "musica", "efectos", "voces", "pantalla_completa", "controles", "idioma" y "accesibilidad")'; do
    grep -qxF "$l" salida36.txt && ok "2.2: ${l:0:70}" || mal "falta la línea: $l"
done
# ── 2.3: accesibilidad (texto, filtro de daltonismo, sin temblor ni destellos, subtítulos) ──
LINCE_JUEGO_GUARDADO="$T/accesible" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=40 \
    LINCE_JUEGO_GUION="3:abajo,5:entrar,8:entrar,10:abajo,12:entrar,14:escape,16:escape" LINCE_JUEGO_CAPTURA=accesible.bmp \
    timeout 60 "$LINCE" "$DIR/prueba32.lince" > salida37.txt 2>&1
for l in 'de serie: {"texto": 1, "daltonismo": no, "temblor": verdadero, "destellos": verdadero, "subtitulos": falso}' \
         "texto: 22 33 33" "volver en 16" "elegido: 1.5 verdadero 1.5" \
         'error: accesibilidad(): "texto" va de 1 (normal) a 4 (cuatro veces más grande)' \
         'error: accesibilidad(): "daltonismo" es "no", "protanopia", "deuteranopia" o "tritanopia"' \
         'error: accesibilidad(): "temblor" es verdadero o falso' \
         'error: reproducir(): "subtitulo" es el texto que sale si el jugador tiene los subtítulos puestos: "Puerta"'; do
    grep -qxF "$l" salida37.txt && ok "2.3: ${l:0:70}" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
d = open("accesible.bmp", "rb").read()
off = struct.unpack_from("<I", d, 10)[0]; w, h = struct.unpack_from("<ii", d, 18); B = struct.unpack_from("<H", d, 28)[0] // 8
fila = (w * B + 3) & ~3
def px(x, y):
    f = (abs(h) - 1 - y) if h > 0 else y
    i = off + f * fila + x * B
    return (d[i + 2], d[i + 1], d[i])
casos = [(px(11, 11) == (0, 0, 0) and px(9, 9) == (255, 255, 255), "sin temblor: el cuadro, justo en su sitio"),
         (px(50, 20)[2] > 40 and px(50, 20)[1] < 30, "filtro de deuteranopía: el rojo tira a magenta %s" % (px(50, 20),)),
         (px(80, 20) == (0, 255, 0), "filtro: el verde se queda"),
         (px(110, 20)[0] > 200 and px(110, 20)[1] < 30, "sin destellos: el sprite sigue rojo"),
         (max(px(100, 80)) < 120, "subtítulo: su caja oscura abajo %s" % (px(100, 80),))]
mal = 0
for bien, nombre in casos:
    print("  ✅ 2.3, píxel:" if bien else "  ❌ 2.3, píxel:", nombre)
    mal += not bien
sys.exit(1 if mal else 0)
PY
# ── 2.4: luces (con la cámara, con zoom, en una capa y en cono) ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=10 timeout 60 "$LINCE" "$DIR/prueba33.lince" > salida38.txt 2>&1
for l in "error: luz(): el radio es mayor que 0" 'error: luz(): "forma" es "circulo" o "cono" (una linterna)' \
         'error: luz(): "intensidad" va de 0 a 1' "a_pantalla: [60, 30]" \
         "error: dibujar_luces() espera el color de lo que no está iluminado, por ejemplo [30, 30, 60]"; do
    grep -qxF "$l" salida38.txt && ok "2.4: $l" || mal "falta la línea: $l"
done
python3 - <<'PY' || fallos=$((fallos+1))
import struct, sys
def lee(n):
    d = open(n, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]; w, h = struct.unpack_from("<ii", d, 18); B = struct.unpack_from("<H", d, 28)[0] // 8
    fila = (w * B + 3) & ~3
    def px(x, y):
        f = (abs(h) - 1 - y) if h > 0 else y
        i = off + f * fila + x * B
        return (d[i + 2], d[i + 1], d[i])
    return px
a, b, c = lee("luces1.bmp"), lee("luces2.bmp"), lee("luces3.bmp")
casos = [(min(a(50, 50)) > 240, "la luz, donde está en el mundo (con la cámara movida)"),
         (a(10, 50) == (0, 0, 0), "lejos de la luz, oscuro"),
         (a(5, 5) == (255, 0, 0), "el marcador, después, sin oscurecer"),
         (min(b(60, 30)) > 240 and b(72, 30) == (0, 0, 0) and 60 < b(66, 30)[0] < 200, "con zoom 2: donde dice a_pantalla y el doble de grande"),
         (c(60, 80)[1] > 30 and c(60, 80)[0] == 0 and c(5, 80) == (0, 0, 0), "un cono verde hacia la derecha"),
         (c(155, 15) == (255, 0, 0) and c(105, 15) == (0, 0, 0), "en la capa 5: lo de la capa 10, encima y sin oscurecer")]
mal = 0
for bien, nombre in casos:
    print("  ✅ 2.4, píxel:" if bien else "  ❌ 2.4, píxel:", nombre)
    mal += not bien
sys.exit(1 if mal else 0)
PY
# ── 2.5: enemigos con cabeza (estados y movimientos) ──
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=10 timeout 60 "$LINCE" "$DIR/prueba34.lince" > salida39.txt 2>&1
[ $? -eq 0 ] && ok "la prueba de enemigos termina bien" || mal "la prueba de enemigos falló"
for l in "estados: entra patrullar / 0: patrullar / 1: patrullar / 2: patrullar / sale patrullar / entra perseguir de patrullar / 3: perseguir / lleva verdadero / 4: perseguir / entra huir / 5: huir" \
         "seguir: 60 0" "seguir centro: 0 50" "frenar: 30 0" "llegado: 0 0" "seguir mapa: 0 -60" "seguir mapa cerca: 60 0" \
         "huir: 0 -40" "huir lejos: 0 0" "rodear: 0 30" "rodear al revés: 0 -30" "rodear lejos: -27 13" \
         "esquivar nada: 0 0" "esquivar: -25 0" "esquivar pared: 0 25" "en_grupo: -10 0" "en_grupo alinear: 10 0" "en_grupo solo: 0 0" \
         "mezclar: 10 20" "mezclar máximo: 6 8" "dirigir: 10 20" "dirigir suave: verdadero" "ve: verdadero falso" \
         "ve delante: verdadero falso" "ve se acuerda: falso verdadero" "ve paredes: falso verdadero" "distancia: 4" \
         "patrullar: 0 0 punto 1" "ida y vuelta: 30 0 punto 0" \
         "error: pensar(): en \"a\", no conozco 'hacr' (hay \"entrar\", \"hacer\" y \"salir\")" \
         'error: pensar(): en "a", "hacer" es una función que recibe la cosa' \
         'error: pensar(): no hay ningún estado "b" (hay "a")' \
         'error: pensar(): los estados se cambian unos a otros al entrar sin parar ("b" y "a")' \
         "error: seguir(): no conozco la opción 'frenr' (hay \"frenar\" y \"mapa\")" \
         'error: rodear(): "sentido" es 1 (como las agujas del reloj) o -1' \
         "error: mezclar(): cada movimiento es [vx, vy] o [[vx, vy], peso]"; do
    grep -qxF "$l" salida39.txt && ok "2.5: $l" || mal "falta la línea: $l"
done
[ "$fallos" -eq 0 ] && echo "✅ Juego: todo correcto" || { cat salida.txt salida2.txt salida3.txt salida4.txt errores4.txt salida5.txt salida6.txt salida7.txt salida8.txt salida9.txt salida10.txt salida11.txt salida12.txt salida13.txt salida14.txt salida15.txt salida16.txt salida17.txt salida18.txt salida19.txt salida20.txt salida21.txt salida22.txt salida23.txt salida24.txt salida25.txt salida26.txt salida27.txt salida28.txt salida29.txt salida30.txt salida31.txt salida32.txt salida33.txt salida34.txt salida35.txt salida36.txt salida37.txt salida38.txt salida39.txt salida5b.txt; exit 1; }
