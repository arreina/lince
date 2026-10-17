#!/usr/bin/env python3
"""Hace cajas.ttf, una fuente TrueType mínima para las pruebas de juego.fuente():
cada letra es un rectángulo, con medidas fáciles de comprobar. 1000 unidades
por em, ascenso 800 y descenso 200: a 20 píxeles, cada 100 unidades son 2 px.

    A: caja de 100 a 500 (ancho), de 0 a 700 (alto); avanza 600
    B: caja de 100 a 300, de 0 a 700; avanza 400
    a: caja de 100 a 400, de 0 a 400; avanza 500
    ?: caja de 100 a 400, de 0 a 700; avanza 500
    espacio: avanza 300
    kerning: A seguida de B, -100
Uso: python3 hacer_fuente.py salida.ttf
"""
import struct
import sys

GLIFOS = [  # (carácter, avance, caja o None)
    (None, 500, None),                  # .notdef
    (" ", 300, None),
    ("A", 600, (100, 0, 500, 700)),
    ("B", 400, (100, 0, 300, 700)),
    ("a", 500, (100, 0, 400, 400)),
    ("?", 500, (100, 0, 400, 700)),
]
KERNING = [("A", "B", -100)]


def glifo(caja):
    if caja is None:
        return b""
    x0, y0, x1, y1 = caja
    puntos = [(x0, y0), (x0, y1), (x1, y1), (x1, y0)]
    d = struct.pack(">hhhhh", 1, x0, y0, x1, y1)
    d += struct.pack(">HH", 3, 0)           # fin del contorno; sin instrucciones
    d += bytes([1] * 4)                     # cuatro puntos sobre la curva, coordenadas de 16 bits
    px = py = 0
    for x, _ in puntos:
        d += struct.pack(">h", x - px)
        px = x
    for _, y in puntos:
        d += struct.pack(">h", y - py)
        py = y
    if len(d) % 2:
        d += b"\0"
    return d


def main(salida):
    n = len(GLIFOS)
    glyf, loca = b"", []
    for _, _, caja in GLIFOS:
        loca.append(len(glyf) // 2)
        glyf += glifo(caja)
    loca.append(len(glyf) // 2)
    tablas = {}
    tablas[b"head"] = struct.pack(">IIIIHHqqhhhhHHhhh", 0x00010000, 0x00010000, 0, 0x5F0F3CF5, 0, 1000,
                                  0, 0, 0, 0, 600, 700, 0, 8, 2, 0, 0)
    tablas[b"hhea"] = struct.pack(">Ihhh" + "H" + "hhh" + "hh" + "h" + "hhhh" + "h" + "H",
                                  0x00010000, 800, -200, 0, 600, 0, 0, 500, 1, 0, 0, 0, 0, 0, 0, 0, n)
    tablas[b"maxp"] = struct.pack(">IH", 0x00005000, n)
    tablas[b"hmtx"] = b"".join(struct.pack(">Hh", av, caja[0] if caja else 0) for _, av, caja in GLIFOS)
    tablas[b"loca"] = b"".join(struct.pack(">H", x) for x in loca)
    tablas[b"glyf"] = glyf
    grupos = sorted((ord(c), i) for i, (c, _, _) in enumerate(GLIFOS) if c)
    sub = struct.pack(">HHIII", 12, 0, 16 + 12 * len(grupos), 0, len(grupos))
    sub += b"".join(struct.pack(">III", cp, cp, g) for cp, g in grupos)
    tablas[b"cmap"] = struct.pack(">HHHHI", 0, 1, 3, 10, 12) + sub
    indice = {c: i for i, (c, _, _) in enumerate(GLIFOS)}
    pares = sorted((indice[a], indice[b], v) for a, b, v in KERNING)
    kern = struct.pack(">HHHH", len(pares), 0, 0, 0)
    kern += b"".join(struct.pack(">HHh", a, b, v) for a, b, v in pares)
    tablas[b"kern"] = struct.pack(">HH", 0, 1) + struct.pack(">HHH", 0, 6 + len(kern), 1) + kern
    etiquetas = sorted(tablas)
    salida_bytes = struct.pack(">IHHHH", 0x00010000, len(etiquetas), 0, 0, 0)
    pos = 12 + 16 * len(etiquetas)
    cuerpo = b""
    for t in etiquetas:
        datos = tablas[t]
        salida_bytes += struct.pack(">4sIII", t, 0, pos + len(cuerpo), len(datos))
        cuerpo += datos + b"\0" * (-len(datos) % 4)
    with open(salida, "wb") as f:
        f.write(salida_bytes + cuerpo)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "cajas.ttf")
