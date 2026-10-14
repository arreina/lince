#!/usr/bin/env python3
"""Genera src/cripto_embebido.h: el texto de src/cripto.h como cadena C, para
que el compilador lo pegue en los programas que genera. Con --comprobar solo
verifica que el archivo está al día (lo usa la CI)."""
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
fuente = (RAIZ / "src" / "cripto.h").read_text(encoding="utf-8")
destino = RAIZ / "src" / "cripto_embebido.h"

lineas = []
for ln in fuente.split("\n"):
    esc = ln.replace("\\", "\\\\").replace('"', '\\"')
    lineas.append(f'    "{esc}\\n"')
salida = ("/* Generado por tools/embeber_cripto.py desde src/cripto.h — no editar */\n"
          "static const char CRIPTO_C[] =\n" + "\n".join(lineas) + ";\n")

if "--comprobar" in sys.argv:
    if not destino.exists() or destino.read_text(encoding="utf-8") != salida:
        print("❌ src/cripto_embebido.h no coincide con src/cripto.h. Ejecuta 'make embebidos'.")
        sys.exit(1)
    print("✅ El código de cripto embebido en el compilador está al día")
else:
    destino.write_text(salida, encoding="utf-8")
    print("✅ src/cripto_embebido.h")
