#!/usr/bin/env python3
"""
🐆 La Piedra del Lince — los mapas de Tiled (mapas/*.tmx)

Hace los tres mapas del juego (el pueblo, el bosque y la fortaleza de Grum)
con los tiles de Kenney (Tiny Town y Tiny Dungeon, CC0). Son mapas de Tiled
normales: se pueden abrir y retocar en Tiled (https://www.mapeditor.org) y el
juego los lee tal cual. Este programa solo sirve para hacerlos la primera vez
(o para rehacerlos desde cero): si los retocas en Tiled, no lo vuelvas a ejecutar.

Cada mapa tiene tres capas de tiles y una de objetos:
  Suelo        la hierba, los caminos, el suelo de piedra
  Decorado     árboles, casas, muros, vallas...
  Colisiones   (oculta) por dónde no se puede pasar
  Cosas        objetos: dónde se entra, las salidas, la gente, los enemigos,
               los cofres, los carteles, los arbustos que se cortan...

Uso: python3 hacer_mapas.py
"""
import pathlib
import random

AQUI = pathlib.Path(__file__).resolve().parent
MAPAS = AQUI / "mapas"
MAPAS.mkdir(exist_ok=True)

PUEBLO = 1          # firstgid de cada tileset: los dos tienen 12 × 11 = 132 tiles
MAZMORRA = 133


def p(i):
    return PUEBLO + i


def m(i):
    return MAZMORRA + i


MURO = m(61)        # el tile con que se pinta la capa Colisiones (no se ve)


class Mapa:
    def __init__(self, nombre, ancho, alto, semilla):
        self.nombre, self.w, self.h = nombre, ancho, alto
        self.suelo = [[0] * ancho for _ in range(alto)]
        self.deco = [[0] * ancho for _ in range(alto)]
        self.solido = [[False] * ancho for _ in range(alto)]
        self.objetos = []
        self.propiedades = {}
        self.rnd = random.Random(semilla)

    def dentro(self, c, f):
        return 0 <= c < self.w and 0 <= f < self.h

    def poner(self, c, f, deco=None, suelo=None, solido=None):
        if not self.dentro(c, f):
            return
        if deco is not None:
            self.deco[f][c] = deco
        if suelo is not None:
            self.suelo[f][c] = suelo
        if solido is not None:
            self.solido[f][c] = solido

    def objeto(self, nombre, tipo, c, f, ancho=1, alto=1, **props):
        self.objetos.append((nombre, tipo, c * 16, f * 16, ancho * 16, alto * 16, props))

    def tmx(self):
        for (nombre, tipo, x, y, w, h, pr) in self.objetos:      # nada encima de algo sólido
            if tipo in ("enemigo", "npc", "cofre", "arbusto", "entrada", "jefe", "piedra"):
                for f in range(y // 16, (y + h) // 16):
                    for c in range(x // 16, (x + w) // 16):
                        assert not self.solido[f][c], f"{self.nombre}: '{nombre}' está encima de algo sólido ({c}, {f})"
        def csv(capa):
            return ",\n".join(",".join(str(v) for v in fila) for fila in capa)
        colis = [[MURO if s else 0 for s in fila] for fila in self.solido]
        props = "".join(f'  <property name="{k}" value="{v}"/>\n' for k, v in self.propiedades.items())
        objs = []
        for i, (nombre, tipo, x, y, w, h, pr) in enumerate(self.objetos):
            dentro = ""
            if pr:
                dentro = "\n   <properties>\n" + "".join(
                    f'    <property name="{k}"{tipo_prop(v)} value="{valor_prop(v)}"/>\n' for k, v in pr.items()) + "   </properties>\n  "
            objs.append(f'  <object id="{i + 1}" name="{nombre}" type="{tipo}" x="{x}" y="{y}" width="{w}" height="{h}">{dentro}</object>'
                        if dentro else
                        f'  <object id="{i + 1}" name="{nombre}" type="{tipo}" x="{x}" y="{y}" width="{w}" height="{h}"/>')
        texto = f'''<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.11.0" orientation="orthogonal" renderorder="right-down" width="{self.w}" height="{self.h}" tilewidth="16" tileheight="16" infinite="0" nextlayerid="5" nextobjectid="{len(self.objetos) + 1}">
 <properties>
{props} </properties>
 <tileset firstgid="{PUEBLO}" source="pueblo.tsx"/>
 <tileset firstgid="{MAZMORRA}" source="mazmorra.tsx"/>
 <layer id="1" name="Suelo" width="{self.w}" height="{self.h}">
  <data encoding="csv">
{csv(self.suelo)}
</data>
 </layer>
 <layer id="2" name="Decorado" width="{self.w}" height="{self.h}">
  <data encoding="csv">
{csv(self.deco)}
</data>
 </layer>
 <layer id="3" name="Colisiones" width="{self.w}" height="{self.h}" visible="0">
  <data encoding="csv">
{csv(colis)}
</data>
 </layer>
 <objectgroup id="4" name="Cosas">
{chr(10).join(objs)}
 </objectgroup>
</map>
'''
        (MAPAS / f"{self.nombre}.tmx").write_text(texto, encoding="utf-8")


def tipo_prop(v):
    if isinstance(v, bool):
        return ' type="bool"'
    if isinstance(v, int):
        return ' type="int"'
    return ""


def valor_prop(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v).replace("&", "&amp;").replace('"', "&quot;").replace("<", "&lt;")


def tilesets():
    for nombre, imagen in (("pueblo", "tiles_pueblo.png"), ("mazmorra", "tiles_mazmorra.png")):
        (MAPAS / f"{nombre}.tsx").write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.11.0" name="{nombre}" tilewidth="16" tileheight="16" tilecount="132" columns="12">
 <image source="../recursos/{imagen}" width="192" height="176"/>
</tileset>
''', encoding="utf-8")


# ── Piezas al aire libre ─────────────────────────────────────────────

def hierba(mp):
    for f in range(mp.h):
        for c in range(mp.w):
            r = mp.rnd.random()
            mp.suelo[f][c] = p(1) if r < 0.18 else p(2) if r < 0.22 else p(0)


def arbol(mp, c, f):
    """Un árbol del bosque que tapa el paso (varios tipos, al azar)"""
    mp.poner(c, f, deco=p(mp.rnd.choice([16, 16, 16, 16, 28, 15 if mp.rnd.random() < 0.3 else 16])), solido=True)


def borde_de_bosque(mp, grosor=1, huecos=()):
    for f in range(mp.h):
        for c in range(mp.w):
            if (c < grosor or f < grosor or c >= mp.w - grosor or f >= mp.h - grosor) and (c, f) not in huecos:
                arbol(mp, c, f)


def camino_piedra(mp, c0, f0, c1, f1):
    for f in range(min(f0, f1), max(f0, f1) + 1):
        for c in range(min(c0, c1), max(c0, c1) + 1):
            mp.poner(c, f, suelo=p(43))


def tierra(mp, celdas):
    """Caminos de tierra con su borde de hierba (las 9 piezas de Tiny Town)"""
    celdas = set(celdas)
    for (c, f) in celdas:
        n, s, o, e = (c, f - 1) in celdas, (c, f + 1) in celdas, (c - 1, f) in celdas, (c + 1, f) in celdas
        fila = 0 if not n else (2 if not s else 1)
        col = 0 if not o else (2 if not e else 1)
        if n and s and o and e:
            t = p(mp.rnd.choice([25, 25, 25, 39, 40]))
        else:
            t = p([12, 24, 36][fila] + col)
        mp.poner(c, f, suelo=t)


def casa(mp, c, f, ancho, tejado="rojo", piedra=False, ventana=True):
    """Una casa de 3 de alto: dos filas de tejado y la pared con la puerta en medio"""
    t1 = [52, 53, 54] if tejado == "rojo" else [48, 49, 50]
    t2 = [64, 65, 66] if tejado == "rojo" else [60, 61, 62]
    chimenea = 55 if tejado == "rojo" else 51
    hastial = 67 if tejado == "rojo" else 63
    pared = [76, 77, 79] if piedra else [72, 73, 75]
    puerta = 89 if piedra else 85
    vent = 88 if piedra else 84
    medio = c + ancho // 2
    for i in range(ancho):
        x = c + i
        lado = 0 if i == 0 else (2 if i == ancho - 1 else 1)
        mp.poner(x, f, deco=p(chimenea if i == 1 else t1[lado]), solido=True)
        mp.poner(x, f + 1, deco=p(hastial if x == medio else t2[lado]), solido=True)
        mp.poner(x, f + 2, deco=p(puerta if x == medio else (vent if ventana and i == 1 and x != medio else pared[lado])), solido=True)
    return medio, f + 2       # la puerta


def valla(mp, c0, f0, c1, f1):
    """Una valla recta, horizontal o vertical"""
    if f0 == f1:
        for c in range(c0, c1 + 1):
            mp.poner(c, f0, deco=p(80 if c == c0 else 82 if c == c1 else 81), solido=True)
    else:
        for f in range(f0, f1 + 1):
            mp.poner(c0, f, deco=p(47 if f == f0 else 71 if f == f1 else 59), solido=True)


def adornos(mp, cuantos, prohibido):
    """Flores, brotes y setas por la hierba libre"""
    hecho = 0
    while hecho < cuantos:
        c, f = mp.rnd.randrange(1, mp.w - 1), mp.rnd.randrange(1, mp.h - 1)
        if mp.deco[f][c] or mp.solido[f][c] or (c, f) in prohibido or mp.suelo[f][c] not in (p(0), p(1), p(2)):
            continue
        mp.poner(c, f, deco=p(mp.rnd.choice([17, 17, 17, 29])))
        hecho += 1


# ── El pueblo ────────────────────────────────────────────────────────

def pueblo():
    mp = Mapa("pueblo", 32, 24, 11)
    mp.propiedades = {"nombre": "Pueblo de Valleclaro", "musica": "pueblo"}
    hierba(mp)
    borde_de_bosque(mp, 1, huecos={(15, 0), (16, 0)})
    for c in range(1, 31):                       # una segunda fila de árboles, con claros
        if c not in range(13, 19) and mp.rnd.random() < 0.55:
            arbol(mp, c, 1)
    for f in range(2, 23):
        for c in (1, 30):
            if mp.rnd.random() < 0.6:
                arbol(mp, c, f)
    for c in range(2, 30):
        if mp.rnd.random() < 0.5:
            arbol(mp, c, 22)
    # caminos de piedra: del norte a la plaza, y a cada puerta
    camino_piedra(mp, 15, 0, 16, 12)
    camino_piedra(mp, 6, 7, 26, 7)
    camino_piedra(mp, 12, 10, 19, 13)            # la plaza
    camino_piedra(mp, 6, 13, 26, 13)
    puertas = []
    puertas.append(casa(mp, 4, 3, 4, "rojo"))                   # la casa de la sabia
    puertas.append(casa(mp, 21, 3, 5, "gris", piedra=True))     # la casa del herrero
    puertas.append(casa(mp, 4, 15, 4, "gris"))                  # la tienda
    puertas.append(casa(mp, 22, 15, 4, "rojo", piedra=True))    # la casa de la abuela
    for (c, f) in puertas:
        alto = 7 if f < 10 else 13
        camino_piedra(mp, c, min(f + 1, alto), c, max(f + 1, alto))
    # el pozo (guardar la partida) en medio de la plaza
    mp.poner(15, 11, deco=p(104), solido=True)
    mp.objeto("pozo", "guardar", 15, 11)
    # el huerto de las colmenas
    valla(mp, 10, 16, 15, 16)
    valla(mp, 10, 20, 15, 20)
    valla(mp, 10, 17, 10, 19)
    valla(mp, 15, 17, 15, 18)
    for (c, f) in ((11, 17), (13, 17), (12, 19)):
        mp.poner(c, f, deco=p(94), solido=True)
    for (c, f) in ((26, 18), (27, 18), (8, 9), (9, 9)):
        mp.poner(c, f, deco=p(107 if c % 2 else 130), solido=True)
    mp.poner(18, 17, deco=p(92), solido=True)      # un montón de heno
    mp.poner(19, 17, deco=p(106), solido=True)     # un tronco
    mp.poner(27, 9, deco=m(74), solido=True)       # el yunque del herrero
    mp.poner(20, 10, deco=p(83), solido=True)
    mp.objeto("cartel_plaza", "cartel", 20, 10,
              texto="Valleclaro. Al norte, el Bosque Viejo. Más allá, las ruinas de la fortaleza.")
    mp.poner(28, 20, deco=p(95), solido=True)
    adornos(mp, 12, prohibido=set())
    # flores
    for _ in range(14):
        c, f = mp.rnd.randrange(2, 30), mp.rnd.randrange(2, 22)
        if not mp.deco[f][c] and mp.suelo[f][c] in (p(0), p(1)):
            mp.suelo[f][c] = p(2)
    # la gente
    mp.objeto("inicio", "entrada", 15, 14)
    mp.objeto("norte", "entrada", 15, 2)
    mp.objeto("salida_norte", "salida", 15, 0, 2, 1, destino="bosque", entrada="sur")
    mp.objeto("sabia", "npc", 13, 11, cara=84)
    mp.objeto("herrero", "npc", 25, 8, cara=87)
    mp.objeto("tendera", "npc", 7, 18, cara=99)
    mp.objeto("abuela", "npc", 25, 19, cara=100)
    mp.objeto("nino", "npc", 20, 19, cara=85)
    mp.objeto("guardia", "npc", 15, 2, cara=96)            # cierran el paso hasta que tengas la espada
    mp.objeto("guardia2", "npc", 16, 2, cara=97)
    mp.tmx()


# ── El Bosque Viejo ──────────────────────────────────────────────────

def bosque():
    W, H = 44, 36
    mp = Mapa("bosque", W, H, 23)
    mp.propiedades = {"nombre": "Bosque Viejo", "musica": "bosque"}
    hierba(mp)
    libre = set()

    def abrir(c0, f0, c1, f1):
        for f in range(f0, f1 + 1):
            for c in range(c0, c1 + 1):
                libre.add((c, f))

    def sendero(puntos, ancho=2):
        celdas = set()
        for (a, b) in zip(puntos, puntos[1:]):
            (c0, f0), (c1, f1) = a, b
            for f in range(min(f0, f1), max(f0, f1) + ancho):
                for c in range(min(c0, c1), max(c0, c1) + ancho):
                    celdas.add((c, f))
        libre.update(celdas)
        return celdas

    # los claros
    abrir(18, 26, 25, 34)          # la entrada, desde el pueblo
    abrir(4, 22, 13, 29)           # el claro del oeste (un cofre con monedas)
    abrir(29, 18, 39, 26)          # el claro del este (el cofre de la llave)
    abrir(14, 11, 29, 17)          # el claro de en medio
    abrir(15, 5, 28, 9)            # delante de la fortaleza
    # los senderos de tierra
    t = set()
    t |= sendero([(21, 35), (21, 22), (21, 12)])
    t |= sendero([(21, 24), (8, 24), (8, 27)])
    t |= sendero([(22, 20), (33, 20), (33, 23)])
    t |= sendero([(21, 12), (21, 6)])
    for (c, f) in list(libre):
        if not (0 < c < W - 1 and 0 < f < H):
            libre.discard((c, f))
    for f in range(H):
        for c in range(W):
            if (c, f) not in libre:
                arbol(mp, c, f)
    tierra(mp, [x for x in t if mp.dentro(*x)])
    # la muralla de la fortaleza, arriba, con su puerta en medio
    for c in range(12, 32):
        for f in range(0, 5):
            mp.poner(c, f, deco=0, solido=True)
        mp.poner(c, 0, deco=p(100 if c % 2 else 101))
        for f in range(1, 4):
            mp.poner(c, f, deco=p(126))
        mp.poner(c, 4, deco=p(126))
    for c in (12, 31):                                      # las torres
        mp.poner(c, 0, deco=p(99 if c == 12 else 102))
    mp.poner(21, 3, deco=p(113), solido=True)          # el arco abierto: la reja la pone el juego
    mp.poner(22, 3, deco=p(114), solido=True)
    mp.poner(21, 4, deco=p(123), solido=True)
    mp.poner(22, 4, deco=p(124), solido=True)
    mp.poner(17, 2, deco=p(125))
    mp.poner(26, 2, deco=p(125))
    for c in range(12, 32):                                 # la hierba al pie de la muralla
        mp.poner(c, 5, solido=False)
    mp.objeto("puerta_fortaleza", "puerta", 21, 4, 2, 1, necesita="llave")
    mp.objeto("salida_fortaleza", "salida", 21, 4, 2, 1, destino="fortaleza", entrada="sur")
    mp.objeto("salida_sur", "salida", 21, 35, 2, 1, destino="pueblo", entrada="norte")
    mp.objeto("sur", "entrada", 21, 33)
    mp.objeto("fortaleza", "entrada", 21, 6)
    mp.objeto("cartel_cruce", "cartel", 23, 27,
              texto="Oeste: el claro de las setas. Este: el viejo roble. Norte: la fortaleza de Grum. ¡Cuidado con las babosas!")
    mp.poner(23, 27, deco=p(83), solido=True)
    # los cofres
    mp.objeto("cofre_llave", "cofre", 36, 21, contenido="llave")
    mp.objeto("cofre_oeste", "cofre", 5, 23, contenido="monedas", cantidad=10)
    mp.objeto("cofre_corazon", "cofre", 27, 12, contenido="corazon")
    # arbustos que se cortan con la espada (a veces sueltan algo)
    for (c, f) in ((19, 30), (24, 29), (24, 30), (10, 27), (11, 27), (6, 26), (30, 24), (31, 24),
                   (37, 25), (38, 19), (16, 14), (17, 14), (27, 16), (16, 7), (27, 7), (25, 21),
                   (12, 22), (13, 22)):
        mp.objeto("arbusto", "arbusto", c, f)
    # setas y brotes
    for (c, f) in ((5, 28), (6, 28), (12, 23), (35, 25), (39, 22), (15, 16), (28, 13), (19, 33)):
        mp.poner(c, f, deco=p(29 if (c + f) % 2 else 17))
    # los enemigos
    for (tipo, c, f) in (("babosa", 22, 17), ("babosa", 9, 25), ("babosa", 11, 23), ("babosa", 32, 22),
                         ("babosa", 35, 24), ("babosa", 37, 20), ("murcielago", 20, 14), ("murcielago", 26, 13),
                         ("murcielago", 18, 7), ("babosa", 24, 8)):
        mp.objeto(tipo, "enemigo", c, f)
    mp.tmx()


# ── La fortaleza de Grum (por dentro) ────────────────────────────────

def fortaleza():
    W, H = 24, 46
    mp = Mapa("fortaleza", W, H, 37)
    mp.propiedades = {"nombre": "Fortaleza de Grum", "musica": "mazmorra"}
    for f in range(H):
        for c in range(W):
            mp.poner(c, f, suelo=m(0), solido=True)
    salas = [(3, 1, 20, 12), (2, 17, 21, 28), (4, 33, 19, 43)]     # c0, f0, c1, f1 (con las paredes)
    pasillos = [(11, 12, 17), (11, 28, 33)]                       # columna, fila de la sala de arriba, de la de abajo

    def sala(c0, f0, c1, f1):
        for f in range(f0, f1 + 1):
            for c in range(c0, c1 + 1):
                arriba, abajo, izq, der = f == f0, f == f1, c == c0, c == c1
                if arriba:
                    t = 1 if izq else 3 if der else 2
                elif abajo:
                    t = 25 if izq else 27 if der else 26
                elif izq:
                    t = 13
                elif der:
                    t = 15
                elif f == f0 + 1:
                    t = 40                                  # la pared de ladrillo
                else:
                    t = None
                if t is None:
                    r = mp.rnd.random()
                    mp.poner(c, f, suelo=m(50 if f == f0 + 2 else (42 if r < 0.06 else 49 if r < 0.12 else 48)), solido=False)
                else:
                    mp.poner(c, f, suelo=m(0), deco=m(t), solido=True)

    for s in salas:
        sala(*s)
    for (c, f_arriba, f_abajo) in pasillos:
        # el hueco en la pared de abajo de la sala de arriba
        mp.poner(c - 1, f_arriba, deco=m(5))
        mp.poner(c + 2, f_arriba, deco=m(4))
        for f in range(f_arriba, f_abajo + 1):
            for x in (c, c + 1):
                mp.poner(x, f, deco=0, suelo=m(48), solido=False)
            if f_arriba < f < f_abajo:
                mp.poner(c - 1, f, deco=m(13), solido=True)
                mp.poner(c + 2, f, deco=m(15), solido=True)
        # el hueco en la pared de arriba de la sala de abajo
        mp.poner(c - 1, f_abajo, deco=m(17))
        mp.poner(c + 2, f_abajo, deco=m(16))
        for x in (c, c + 1):
            mp.poner(x, f_abajo + 1, deco=0, suelo=m(48), solido=False)
    # la entrada, abajo
    c = 11
    mp.poner(c - 1, 43, deco=m(5))
    mp.poner(c + 2, 43, deco=m(4))
    for f in (43, 44, 45):
        for x in (c, c + 1):
            mp.poner(x, f, deco=0, suelo=m(48), solido=False)
        if f > 43:
            mp.poner(c - 1, f, deco=m(13), solido=True)
            mp.poner(c + 2, f, deco=m(15), solido=True)
    # adornos: estandartes y columnas en la pared, una fuente en la sala del jefe
    for (x, f) in ((7, 2), (16, 2), (6, 18), (17, 18), (8, 34), (15, 34)):
        mp.poner(x, f, deco=m(29))
    for (x, f) in ((5, 5), (18, 5), (5, 9), (18, 9)):
        mp.poner(x, f, deco=m(6), solido=True)
        mp.poner(x, f + 1, deco=m(18), solido=True)
    mp.poner(11, 2, deco=m(20))
    mp.poner(12, 2, deco=m(19))
    for (x, f) in ((4, 19), (20, 19), (5, 39), (18, 41), (6, 36)):
        mp.poner(x, f, deco=m(82), solido=True)              # barriles
    for (x, f) in ((19, 26), (3, 26)):
        mp.poner(x, f, deco=m(64), solido=True)              # tumbas
    # objetos
    mp.objeto("sur", "entrada", 11, 43)
    mp.objeto("salida_sur", "salida", 11, 45, 2, 1, destino="bosque", entrada="fortaleza")
    mp.objeto("reja_jefe", "reja", 11, 17, 2, 1, sala="sala2")
    mp.objeto("sala2", "sala", 3, 19, 18, 9)
    mp.objeto("cofre_pocion", "cofre", 17, 36, contenido="pocion")
    mp.objeto("cofre_monedas", "cofre", 4, 21, contenido="monedas", cantidad=15)
    for (tipo, x, f) in (("arana", 8, 37), ("arana", 15, 36), ("arana", 12, 39),
                         ("fantasma", 6, 22), ("fantasma", 17, 23), ("arana", 9, 26), ("arana", 15, 21),
                         ("murcielago", 12, 20)):
        mp.objeto(tipo, "enemigo", x, f, sala="sala2" if f < 30 else "")
    mp.objeto("grum", "jefe", 11, 5, 2, 2)
    mp.objeto("piedra", "piedra", 11, 6)
    mp.tmx()


tilesets()
pueblo()
bosque()
fortaleza()
print("✅ mapas en", MAPAS)
