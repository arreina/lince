#!/usr/bin/env python3
# 🐆 Lince — src/recursos/hacer_icono.py
# Hace, a partir de lince.png (256×256, sacado de lince.svg con Inkscape):
#  - lince.ico: el icono de lince.exe, con 16, 32, 48 y 256 píxeles, todos sin
#    comprimir (32 bits, BMP). Así cada uno ocupa siempre lo mismo y 'lince
#    repartir' los cambia por el del juego sin tocar nada más del .exe.
#  - ../icono_lince.h: lince.png dentro del programa, para los juegos sin icono.
# Se ejecuta a mano cuando cambia el logo; lo que hace va en el repositorio.
import os, struct
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
TAMAÑOS = [16, 32, 48, 256]

def bmp(img):
    w, h = img.size
    px = img.convert("RGBA").load()
    filas = []
    for y in range(h - 1, -1, -1):                       # de abajo arriba
        filas.append(b"".join(struct.pack("BBBB", *(lambda p: (p[2], p[1], p[0], p[3]))(px[x, y])) for x in range(w)))
    mascara = b"\0" * (((w + 31) // 32) * 4 * h)         # la transparencia va en el alfa
    cab = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, w * h * 4 + len(mascara), 0, 0, 0, 0)
    return cab + b"".join(filas) + mascara

logo = Image.open(os.path.join(AQUI, "lince.png")).convert("RGBA")
imagenes = [bmp(logo.resize((t, t), Image.LANCZOS)) for t in TAMAÑOS]
cab = struct.pack("<HHH", 0, 1, len(imagenes))
pos = 6 + 16 * len(imagenes)
entradas = b""
for t, datos in zip(TAMAÑOS, imagenes):
    entradas += struct.pack("<BBBBHHII", t % 256, t % 256, 0, 0, 1, 32, len(datos), pos)
    pos += len(datos)
with open(os.path.join(AQUI, "lince.ico"), "wb") as f:
    f.write(cab + entradas + b"".join(imagenes))

png = open(os.path.join(AQUI, "lince.png"), "rb").read()
with open(os.path.join(AQUI, "..", "icono_lince.h"), "w") as f:
    f.write("/* 🐆 Lince — el logo (src/recursos/lince.png), para los juegos sin icono propio.\n"
            "   Lo hace src/recursos/hacer_icono.py: no se edita a mano. */\n")
    f.write("static const unsigned char ICONO_LINCE[%d] = {\n" % len(png))
    for i in range(0, len(png), 20):
        f.write("    " + ", ".join(str(b) for b in png[i:i + 20]) + ",\n")
    f.write("};\n")
print("lince.ico y icono_lince.h hechos")
