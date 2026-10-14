#!/usr/bin/env python3
"""Cada snippet de los editores tiene que ser Lince válido.

Se sustituyen los marcadores (${1:texto} -> texto, $0 -> nada) y se pasa el
resultado por `lince comprobar`. Así un snippet no puede quedarse con una
sintaxis que el lenguaje ya no acepta.

Uso: python3 tests/editores/comprobar_snippets.py ./lince
"""
import json, pathlib, re, subprocess, sys, tempfile

lince = sys.argv[1] if len(sys.argv) > 1 else "./lince"
raiz = pathlib.Path(__file__).resolve().parents[2]
snips = json.loads((raiz / "editores/vscode/lince-language/snippets.json").read_text(encoding="utf-8"))

# Estos sólo tienen sentido dentro del cuerpo de una clase
EN_CLASE = {"estatico", "privado", "propiedad"}

def expandir(lineas):
    t = "\n".join(lineas)
    for _ in range(3):                      # marcadores anidados
        t = re.sub(r"\$\{\d+:([^${}]*)\}", r"\1", t)
    t = re.sub(r"\$\{\d+\}|\$\d+", "", t)
    return t.replace("\t", "    ")

fallos = 0
for nombre, s in snips.items():
    cuerpo = expandir(s["body"])
    if s["prefix"] in EN_CLASE:
        cuerpo = "clase Prueba {\n" + "\n".join("    " + l for l in cuerpo.split("\n")) + "\n}"
    with tempfile.NamedTemporaryFile("w", suffix=".lince", delete=False, encoding="utf-8") as f:
        f.write(cuerpo + "\n")
    r = subprocess.run([lince, "comprobar", f.name], capture_output=True, text=True)
    if r.returncode != 0:
        fallos += 1
        print(f"  ❌ {nombre} ({s['prefix']}):\n{cuerpo}\n{r.stderr.strip()}\n")
    pathlib.Path(f.name).unlink()

if fallos:
    print(f"❌ {fallos} snippet(s) con sintaxis inválida")
    sys.exit(1)
print(f"✅ {len(snips)} snippets son Lince válido")
