#!/usr/bin/env python3
"""Mete las bibliotecas escritas en Lince (src/lince/*.lince) en el ejecutable.

Cada src/lince/NOMBRE.lince se convierte en src/biblioteca_NOMBRE.h, con el
código en una cadena de C (BIBLIOTECA_NOMBRE). El .h generado va en el
repositorio para compilar sin Python; 'make bibliotecas-check' (en la CI)
falla si alguien cambia un .lince y no lo regenera.

Uso:  python3 src/lince/incrustar.py              # reescribe los .h
      python3 src/lince/incrustar.py --comprobar  # solo comprueba que están al día
"""
import pathlib, sys

AQUI = pathlib.Path(__file__).resolve().parent
SRC = AQUI.parent


def cadena_c(texto):
    lineas = []
    for linea in texto.splitlines(keepends=True):
        esc = linea.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t")
        # '??' formaría trígrafos en compiladores antiguos
        esc = esc.replace("??", "?\\?")
        lineas.append('    "' + esc + '"')
    return "\n".join(lineas) if lineas else '    ""'


def generar(fuente):
    nombre = fuente.stem
    texto = fuente.read_text(encoding="utf-8")
    simbolo = "BIBLIOTECA_" + nombre.upper()
    return (f"/* Generado por src/lince/incrustar.py a partir de src/lince/{fuente.name}.\n"
            f"   No lo edites: cambia el .lince y ejecuta 'make bibliotecas'. */\n"
            f"#ifndef LINCE_{simbolo}_H\n#define LINCE_{simbolo}_H\n\n"
            f"static const char {simbolo}[] =\n{cadena_c(texto)};\n\n#endif\n")


def main():
    comprobar = "--comprobar" in sys.argv
    mal = []
    for fuente in sorted(AQUI.glob("*.lince")):
        destino = SRC / f"biblioteca_{fuente.stem}.h"
        nuevo = generar(fuente)
        actual = destino.read_text(encoding="utf-8") if destino.exists() else None
        if comprobar:
            if actual != nuevo:
                mal.append(destino.name)
        elif actual != nuevo:
            destino.write_text(nuevo, encoding="utf-8")
            print(f"  escrito src/{destino.name}")
    if mal:
        sys.exit("Las bibliotecas en Lince no están al día: " + ", ".join(mal) +
                 "\nEjecuta 'make bibliotecas'.")
    if comprobar:
        print("✅ Las bibliotecas en Lince están al día")


if __name__ == "__main__":
    main()
