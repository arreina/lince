# Saca los iconos (RT_ICON, BMP de 32 bits) y el subsistema de un .exe de Windows: para probar.sh
import struct, sys
from PIL import Image
d = open(sys.argv[1], "rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]
nsec, = struct.unpack_from("<H", d, pe + 6); topc, = struct.unpack_from("<H", d, pe + 20)
opc = pe + 24; magia, = struct.unpack_from("<H", d, opc)
sub, = struct.unpack_from("<H", d, opc + 68)
dd = opc + (112 if magia == 0x20b else 96)
secs = opc + topc
def rva(r):
    for s in range(nsec):
        va, tam, raw = struct.unpack_from("<III", d, secs + 40 * s + 12)
        if va <= r < va + tam: return raw + r - va
rsrc = rva(struct.unpack_from("<I", d, dd + 16)[0])
def entradas(o):
    n = sum(struct.unpack_from("<HH", d, o + 12))
    return [struct.unpack_from("<II", d, o + 16 + 8 * i) for i in range(n)]
print("subsistema", sub)
for ident, off in entradas(rsrc):
    if ident != 3: continue
    for _, o2 in entradas(rsrc + (off & 0x7fffffff)):
        for _, o3 in entradas(rsrc + (o2 & 0x7fffffff)):
            r, tam = struct.unpack_from("<II", d, rsrc + o3)
            p = rva(r); w, h2 = struct.unpack_from("<ii", d, p + 4); h = h2 // 2
            img = Image.frombytes("RGBA", (w, h), bytes(d[p + 40:p + 40 + w * h * 4]), "raw", "BGRA", 0, -1)
            img.save(sys.argv[2] + "_%d.png" % w); print("icono", w)
