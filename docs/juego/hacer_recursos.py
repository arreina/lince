#!/usr/bin/env python3
"""
🐆 Lince — los recursos de los ejemplos del módulo juego (docs/juego/recursos)

Dibuja, píxel a píxel, la hoja del lince, el tileset, una hoja desordenada
para hoja() y un nivel de Tiled. Son nuestros (no hay que citar a nadie). La
fuente Press Start 2P es de terceros, con licencia OFL (ver CREDITOS.md).

Uso: python3 docs/juego/hacer_recursos.py   (hace falta Pillow)
"""
import pathlib
import random
from PIL import Image

AQUI = pathlib.Path(__file__).resolve().parent / "recursos"
AQUI.mkdir(exist_ok=True)

COLORES = {"O": (214, 150, 70), "D": (130, 80, 35), "W": (242, 236, 222), "K": (25, 22, 30),
           "P": (232, 140, 150), "G": (90, 70, 60)}

# El lince de lado (16 × 16), mirando a la derecha
CABEZA = [
    "..........K..K..",
    "..........OK.OK.",
    ".........OOOOOO.",
    ".........OKOOKO.",
    ".........OWWWWO.",
    ".........OWPPWO.",
]
CUERPO = [
    "D.OOOOOOOOOWWO..",
    "DOOOOOOOOOOOO...",
    ".OOWWWWWWOOOO...",
    ".OOWWWWWWOOO....",
    "..OOOOOOOOOO....",
]
PATAS = {
    "quieto": ["..OO.OO..OO.OO..", "..OO.OO..OO.OO..", "..DD.DD..DD.DD..", "................"],
    "andar1": [".OO..OO...OO.OO.", "OO...OO...OO..OO", "DD...DD...DD..DD", "................"],
    "andar2": ["..OO.OO..OO.OO..", "..OO.OO..OO.OO..", "..DD.DD..DD.DD..", "................"],
    "andar3": ["...OOOO...OOOO..", "...OO.OO..OO.OO.", "...DD.DD..DD..DD", "................"],
    "saltar": ["..OOO......OOO..", ".OO..........OO.", "DD............DD", "................"],
    "zarpa": ["..OO.OO..OO.OOOO", "..OO.OO..OO...WW", "..DD.DD..DD.....", "................"],
}


def cuadro(patas, cola_alta=False, boca=False):
    filas = [""] + CABEZA + CUERPO + PATAS[patas]
    filas[0] = "................"
    if cola_alta:
        filas[6] = "D" + filas[6][1:]
        filas[7] = "D" + filas[7][1:]
    if boca:
        filas[6] = filas[6][:13] + "K" + filas[6][14:]
    return filas[:16]


def pintar(img, filas, x0, y0):
    for y, fila in enumerate(filas):
        for x, c in enumerate(fila):
            if c in COLORES:
                img.putpixel((x0 + x, y0 + y), COLORES[c] + (255,))


# ── El lince: una fila por animación ──
filas_anim = [
    [cuadro("quieto"), cuadro("quieto", cola_alta=True)],                         # quieto
    [cuadro("andar1"), cuadro("andar2"), cuadro("andar3"), cuadro("andar2")],     # andar
    [cuadro("saltar", cola_alta=True)],                                           # saltar
    [cuadro("quieto"), cuadro("zarpa", boca=True), cuadro("zarpa", boca=True)],   # zarpazo
]
hoja = Image.new("RGBA", (16 * 4, 16 * len(filas_anim)), (0, 0, 0, 0))
for f, cuadros in enumerate(filas_anim):
    for c, filas in enumerate(cuadros):
        pintar(hoja, filas, c * 16, f * 16)
hoja.save(AQUI / "lince.png")

# ── El tileset: 8 columnas de 16 × 16 ──
random.seed(7)
T = 16
NOMBRES = ["hierba", "tierra", "piedra", "agua1", "agua2", "caja", "moneda1", "moneda2",
           "moneda3", "moneda4", "nube", "ladrillo", "plataforma", "cielo", "flor", "pinchos"]
tiles = Image.new("RGBA", (T * 8, T * 2), (0, 0, 0, 0))


def ruido(base, var):
    return tuple(max(0, min(255, b + random.randint(-var, var))) for b in base) + (255,)


def tile(i, dibujar):
    x0, y0 = (i % 8) * T, (i // 8) * T
    for y in range(T):
        for x in range(T):
            c = dibujar(x, y)
            if c:
                tiles.putpixel((x0 + x, y0 + y), c)


tile(0, lambda x, y: ruido((80, 170, 70), 12) if y < 4 + (x % 3 == 0) else ruido((120, 80, 50), 10))
tile(1, lambda x, y: ruido((120, 80, 50), 12))
tile(2, lambda x, y: (60, 60, 70, 255) if x in (0, 15) or y in (0, 15) else ruido((120, 120, 135), 14))
for k in (3, 4):
    tile(k, lambda x, y, k=k: ruido((60, 120, 210), 6) if (y + (x // 4 + k) % 2 * 2) % 6 else (150, 200, 250, 255))
tile(5, lambda x, y: (90, 55, 25, 255) if x in (0, 15) or y in (0, 15) or x == y or x == 15 - y else (190, 130, 60, 255))
for k, ancho in zip(range(6, 10), (6, 4, 1, 4)):
    tile(k, lambda x, y, a=ancho: (250, 210, 60, 255) if abs(x - 7.5) <= a and abs(y - 7.5) <= 6
         else ((200, 150, 20, 255) if abs(x - 7.5) <= a + 1 and abs(y - 7.5) <= 7 else None))
tile(10, lambda x, y: (250, 250, 255, 230) if (x - 8) ** 2 / 64 + (y - 9) ** 2 / 20 < 1 or (x - 6) ** 2 + (y - 6) ** 2 < 12 else None)
tile(11, lambda x, y: (90, 40, 30, 255) if y % 8 == 0 or (x + (y // 8) * 8) % 16 == 0 else ruido((170, 70, 50), 10))
tile(12, lambda x, y: ruido((150, 110, 70), 10) if y < 6 else None)
tile(13, lambda x, y: (120, 190, 240, 255))
tile(14, lambda x, y: (240, 90, 120, 255) if (x - 8) ** 2 + (y - 7) ** 2 < 6 else ((60, 150, 60, 255) if x == 8 and y > 8 else None))
tile(15, lambda x, y: (200, 200, 210, 255) if y > 6 and abs((x % 8) - 4) < (y - 6) / 2.5 else None)
tiles.save(AQUI / "tiles.png")

# ── Una hoja «desordenada»: figuras de distinto tamaño separadas por huecos ──
fig = Image.new("RGBA", (120, 60), (0, 0, 0, 0))
formas = [(5, 8, 20, 30, (230, 90, 70)), (35, 4, 28, 40, (90, 160, 230)), (75, 12, 14, 22, (120, 200, 90)),
          (100, 6, 14, 34, (240, 200, 60)), (10, 45, 40, 10, (180, 120, 220)), (60, 48, 50, 8, (240, 150, 60))]
for x, y, w, h, c in formas:
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            borde = xx in (x, x + w - 1) or yy in (y, y + h - 1)
            fig.putpixel((xx, yy), (tuple(v // 2 for v in c) if borde else c) + (255,))
fig.save(AQUI / "figuras.png")

# ── Un nivel de Tiled (40 × 15 casillas de 16) con su tileset aparte ──
W, H = 40, 15
cielo = [14] * (W * H)                                         # gid = tile + 1
suelo = [0] * (W * H)
for c in range(W):
    if c not in (14, 15, 26):
        suelo[12 * W + c] = 1
        for f in (13, 14):
            suelo[f * W + c] = 2
for c in range(8, 13):
    suelo[8 * W + c] = 13                                      # plataforma
for c in range(18, 24):
    suelo[6 * W + c] = 12                                      # ladrillos
for c in (30, 31):
    for f in (10, 11):
        suelo[f * W + c] = 6                                   # cajas
for c in (14, 15, 26):
    suelo[14 * W + c] = 4                                      # agua al fondo de los hoyos
for c, f in ((5, 11), (22, 11), (36, 11)):
    suelo[f * W + c] = 15                                      # flores
nubes = [0] * (W * H)
for c, f in ((3, 2), (4, 2), (17, 3), (29, 1), (30, 1), (31, 1)):
    nubes[f * W + c] = 11


def csv(l):
    return ",\n".join(",".join(str(v) for v in l[r * W:(r + 1) * W]) for r in range(H))


objetos = [("inicio", "jugador", 24, 160)] + [("moneda", "moneda", x, y) for x, y in
                                              ((150, 112), (166, 112), (310, 80), (326, 80), (500, 144))]
xml_objetos = "\n".join(
    f'  <object id="{i + 1}" name="{n}" type="{t}" x="{x}" y="{y}" width="16" height="16"/>'
    for i, (n, t, x, y) in enumerate(objetos))
(AQUI / "nivel.tmx").write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.11.0" orientation="orthogonal" renderorder="right-down" width="{W}" height="{H}" tilewidth="16" tileheight="16" infinite="0" nextlayerid="5" nextobjectid="{len(objetos) + 1}">
 <properties>
  <property name="nombre" value="El prado"/>
 </properties>
 <tileset firstgid="1" source="tiles.tsx"/>
 <layer id="1" name="Cielo" width="{W}" height="{H}">
  <data encoding="csv">
{csv(cielo)}
</data>
 </layer>
 <layer id="2" name="Nubes" width="{W}" height="{H}" opacity="0.8" parallaxx="0.5">
  <data encoding="csv">
{csv(nubes)}
</data>
 </layer>
 <layer id="3" name="Suelo" width="{W}" height="{H}">
  <data encoding="csv">
{csv(suelo)}
</data>
 </layer>
 <objectgroup id="4" name="Cosas">
{xml_objetos}
 </objectgroup>
</map>
''', encoding="utf-8")
(AQUI / "tiles.tsx").write_text('''<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.11.0" name="tiles" tilewidth="16" tileheight="16" tilecount="16" columns="8">
 <image source="tiles.png" width="128" height="32"/>
''' + "".join(f''' <tile id="{i}">
  <properties>
   <property name="solido" type="bool" value="true"/>
  </properties>
 </tile>
''' for i in (0, 1, 2, 5, 11)) + ''' <tile id="12">
  <properties>
   <property name="plataforma" type="bool" value="true"/>
  </properties>
 </tile>
 <tile id="3">
  <animation>
   <frame tileid="3" duration="400"/>
   <frame tileid="4" duration="400"/>
  </animation>
 </tile>
</tileset>
''', encoding="utf-8")

# ── Un pueblo isométrico (2.8): rombos de 32 × 16, piezas sueltas y el mapa ──
from PIL import ImageDraw


def csv12(l):
    return ",\n".join(",".join(str(v) for v in l[r * 12:(r + 1) * 12]) for r in range(12))



random.seed(28)
rombos = Image.new("RGBA", (96, 16), (0, 0, 0, 0))
for i, (base, var) in enumerate([((96, 168, 80), 14), ((196, 160, 110), 10), ((70, 130, 200), 12)]):
    for y in range(16):
        ancho = (y + 1) * 2 if y < 8 else (16 - y) * 2     # el rombo, fila a fila
        for x in range(16 - ancho, 16 + ancho):
            if 0 <= x < 32:
                rombos.putpixel((i * 32 + x, y), ruido(base, var))
rombos.save(AQUI / "rombos.png")
arbol = Image.new("RGBA", (32, 48), (0, 0, 0, 0))
d = ImageDraw.Draw(arbol)
d.rectangle([14, 30, 17, 45], fill=(110, 70, 40))
for cx, cy, r, color in [(16, 22, 11, (40, 110, 50)), (12, 16, 8, (55, 135, 60)), (19, 12, 7, (70, 155, 70))]:
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color)
arbol.save(AQUI / "arbol.png")
casa = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
d = ImageDraw.Draw(casa)
d.polygon([(0, 40), (32, 56), (32, 34), (0, 18)], fill=(220, 205, 170))     # la pared de la izquierda
d.polygon([(32, 56), (64, 40), (64, 18), (32, 34)], fill=(190, 170, 135))   # la de la derecha
d.polygon([(0, 18), (32, 2), (64, 18), (32, 34)], fill=(180, 70, 60))      # el tejado
d.rectangle([40, 40, 46, 50], fill=(110, 70, 40))                          # la puerta
casa.save(AQUI / "casa.png")
filas_pueblo = [
    "000000000000", "000000000000", "001111111100", "001000000100", "001000220100", "001000220100",
    "001000000100", "001111111100", "000000000000", "000002200000", "000002200000", "000000000000"]
suelo = [{"0": 1, "1": 2, "2": 3}[ch] for fila in filas_pueblo for ch in fila]
piezas = [0] * 144
piezas[3 * 12 + 4] = 5                                   # la casa (pisa la casilla 4, 3)
solidos = [1 if ch == "2" else 0 for fila in filas_pueblo for ch in fila]
solidos[3 * 12 + 4] = solidos[2 * 12 + 4] = solidos[3 * 12 + 3] = solidos[2 * 12 + 3] = 1
arboles = [(1, 9), (3, 1), (9, 2), (10, 6), (7, 9), (2, 5)]
objetos = "".join(f'  <object id="{i + 2}" type="arbol" gid="4" x="{c * 16 + 8}" y="{f * 16 + 8}" width="32" height="48"/>\n'
                  for i, (c, f) in enumerate(arboles))
(AQUI / "pueblo.tmx").write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.10.2" orientation="isometric" renderorder="right-down" width="12" height="12" tilewidth="32" tileheight="16" infinite="0" nextlayerid="5" nextobjectid="{len(arboles) + 2}">
 <tileset firstgid="1" name="rombos" tilewidth="32" tileheight="16" tilecount="3" columns="3">
  <image source="rombos.png" width="96" height="16"/>
 </tileset>
 <tileset firstgid="4" name="piezas" tilewidth="64" tileheight="64" tilecount="2" columns="0">
  <grid orientation="orthogonal" width="1" height="1"/>
  <tile id="0"><image source="arbol.png" width="32" height="48"/></tile>
  <tile id="1"><image source="casa.png" width="64" height="64"/></tile>
 </tileset>
 <layer id="1" name="suelo" width="12" height="12">
  <data encoding="csv">{csv12(suelo)}</data>
 </layer>
 <layer id="2" name="piezas" width="12" height="12">
  <data encoding="csv">{csv12(piezas)}</data>
 </layer>
 <layer id="3" name="colisiones" width="12" height="12" visible="0">
  <data encoding="csv">{csv12(solidos)}</data>
 </layer>
 <objectgroup id="4" name="cosas">
  <object id="1" type="jugador" x="106" y="134"><point/></object>
{objetos} </objectgroup>
</map>
''', encoding="utf-8")

print("✅ recursos en", AQUI)
