#!/bin/bash
# Comprueba lo que el runner de Lince no puede: argumentos, código de salida y
# que el ejecutable no dependa de los archivos originales ni del directorio.
set -e
LINCE="${LINCE_BIN:-./lince}"
LINCE="$(cd "$(dirname "$LINCE")" && pwd)/$(basename "$LINCE")"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/lib"
cat > "$T/lib/saludos.lince" <<'EOF'
funcion saludar(texto n): texto { devolver "hola " + n }
EOF
cat > "$T/prog.lince" <<'EOF'
importar "./lib/saludos"
escribir(saludar("mundo"))
escribir(argumentos())
lanzar Error("fallo a propósito")
EOF
# Con un error y la salida por una tubería, la cabecera sale antes que el error;
# y una letra con tilde no se parte en el aviso
printf 'sea x = 1\nsea y = \xc3\xa9\n' > "$T/malo.lince"
orden="$("$LINCE" compilar "$T/malo.lince" -o "$T/malo" 2>&1 | grep -o "Compilador\|Encontré" | tr '\n' ' ')"
[ "$orden" = "Compilador Encontré " ] || { echo "❌ compilar: la cabecera no sale antes del error ($orden)"; exit 1; }
"$LINCE" compilar "$T/malo.lince" -o "$T/malo" 2>&1 | grep -q "carácter 'é'" || { echo "❌ compilar: la letra con tilde se parte en el aviso"; exit 1; }
"$LINCE" compilar "$T/prog.lince" -o "$T/prog" > /dev/null
mv "$T/lib" "$T/lib_oculta"          # el ejecutable lleva sus archivos dentro
salida="$(cd / && "$T/prog" uno dos 2>&1 || true)"
codigo=0; (cd / && "$T/prog" > /dev/null 2>&1) || codigo=$?
echo "$salida" | grep -q "hola mundo" || { echo "❌ empaquetado: no usa el archivo importado"; exit 1; }
echo "$salida" | grep -q "uno" && echo "$salida" | grep -q "dos" || { echo "❌ empaquetado: no recibe los argumentos"; echo "$salida"; exit 1; }
[ "$codigo" -ne 0 ] || { echo "❌ empaquetado: un error no deja código de salida distinto de 0"; exit 1; }
echo "✅ empaquetado: archivos importados, argumentos y código de salida"

# ── lince repartir: carpeta y .zip con el juego, sus imágenes y sonidos, y SDL2 ──
mkdir "$T/rep"
cd "$T/rep"
python3 - <<'PY'
import struct, zlib
def bloque(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
filas = b"".join(b"\x00" + b"\xff\x00\x00\xff" * 8 for _ in range(6))
open("rojo.png", "wb").write(b"\x89PNG\r\n\x1a\n" + bloque(b"IHDR", struct.pack(">IIBBBBB", 8, 6, 8, 6, 0, 0, 0))
                             + bloque(b"IDAT", zlib.compress(filas)) + bloque(b"IEND", b""))
PY
cat > mini.lince <<'EOF2'
importar "juego"
juego.abrir("Mini", 40, 30)
sea img = juego.imagen("rojo.png")
escribir("imagen " + a_texto(img["ancho"]) + "x" + a_texto(img["alto"]))
juego.salir()
EOF2
"$LINCE" repartir mini.lince > salida.txt 2>&1 || { echo "❌ repartir falló"; cat salida.txt; exit 1; }
zip=mini-linux.zip; exe=mini
[ -f mini-windows.zip ] && zip=mini-windows.zip && exe=mini.exe
python3 - "$zip" "$exe" <<'PY'
import sys, zipfile, os
zip, exe = sys.argv[1], sys.argv[2]
carpeta = zip[:-4]
z = zipfile.ZipFile(zip)
assert z.testzip() is None, "zip dañado"
nombres = z.namelist()
for n in (exe, "LEEME.txt"):
    assert carpeta + "/" + n in nombres, (n, nombres)
if exe == "mini":
    modo = z.getinfo(carpeta + "/mini").external_attr >> 16
    assert modo & 0o111, "el ejecutable pierde el permiso de ejecución"
os.makedirs("fuera", exist_ok=True)
z.extractall("fuera")
if exe == "mini": os.chmod("fuera/" + carpeta + "/mini", 0o755)
print("sdl:", any("SDL2" in n for n in nombres))
PY
rm rojo.png                          # la imagen va dentro del ejecutable
if ldconfig -p 2>/dev/null | grep -q "libSDL2-2.0.so.0" || [ "$exe" = "mini.exe" ]; then
    salida="$(cd fuera/"${zip%.zip}" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=3 ./"$exe" 2>&1)"
    echo "$salida" | grep -qx "imagen 8x6" || { echo "❌ repartir: el juego descomprimido no funciona"; echo "$salida"; exit 1; }
fi
echo "✅ repartir: carpeta y .zip con el juego, la imagen y SDL2"
