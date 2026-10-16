#!/usr/bin/env python3
"""
🐆 Lince — generador del tutorial «Tu primer juego»

Construye docs/tutorial.html a partir de los pasos de docs/tutorial/pasos/.
Cada paso es un programa completo que se EJECUTA al generar la página (sin
pantalla, con SDL_VIDEODRIVER=dummy) y del que se saca la captura que se
muestra: así el tutorial no puede tener código que no funcione.

Uso:  python3 docs/generar_tutorial.py [ruta/al/lince]
      python3 docs/generar_tutorial.py [ruta/al/lince] --comprobar

Con --comprobar no escribe nada: ejecuta los pasos (si hay SDL2) y falla si la
página no coincide con la del disco. Las capturas solo se rehacen al generar
(hace falta Pillow).
"""
import difflib
import html
import os
import pathlib
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from generar_manual import resaltar  # mismo resaltado que el manual

RAIZ = pathlib.Path(__file__).resolve().parent.parent
DIR_PASOS = RAIZ / "docs" / "tutorial" / "pasos"
DIR_IMG = RAIZ / "docs" / "img" / "tutorial"
SALIDA = RAIZ / "docs" / "tutorial.html"
ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
COMPROBAR = "--comprobar" in sys.argv
LINCE = pathlib.Path(ARGS[0]).resolve() if ARGS else RAIZ / "lince"
META = re.compile(r"^#\s*@(\w+):\s*(.*)$")


def leer_paso(ruta):
    meta, textos, codigo = {}, [], []
    for linea in ruta.read_text(encoding="utf-8").splitlines():
        m = META.match(linea)
        if not m:
            codigo.append(linea)
        elif m.group(1) == "texto":
            textos.append(m.group(2).strip())
        else:
            meta[m.group(1)] = m.group(2).strip()
    if not meta.get("titulo") or not textos:
        sys.exit(f"❌ {ruta.name}: falta '# @titulo:' o '# @texto:'")
    meta["textos"] = textos
    meta["codigo"] = "\n".join(codigo).strip("\n")
    meta["archivo"] = ruta.name
    return meta


def hay_sdl():
    if os.name == "nt":
        return True
    r = subprocess.run(["ldconfig", "-p"], capture_output=True, text=True)
    return "libSDL2-2.0.so.0" in r.stdout


def ejecutar(paso, ruta, captura=None):
    """Ejecuta el paso sin pantalla; con 'captura', guarda su último fotograma."""
    entorno = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                   LINCE_JUEGO_FOTOGRAMAS=paso.get("fotogramas", "60"),
                   LINCE_JUEGO_PULSAR=paso.get("teclas", ""),
                   LINCE_JUEGO_SEMILLA=paso.get("semilla", "1"))
    if captura:
        entorno["LINCE_JUEGO_CAPTURA"] = str(captura)
    r = subprocess.run([str(LINCE), str(ruta)], capture_output=True, text=True,
                       timeout=60, cwd=RAIZ, env=entorno)
    if r.returncode != 0:
        sys.exit(f"❌ {ruta.name} termina con código {r.returncode}. Un paso del "
                 f"tutorial tiene que funcionar.\n" + (r.stdout + r.stderr)[-1500:])


def formato(texto):
    """**negrita** y `código` sobre el texto ya escapado"""
    t = html.escape(texto)
    t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
    return re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)


def codigo_marcado(actual, anterior):
    """El código resaltado, con las líneas nuevas respecto al paso anterior marcadas"""
    lineas = actual.split("\n")
    nuevas = set()
    if anterior is not None:
        sm = difflib.SequenceMatcher(None, anterior.split("\n"), lineas, autojunk=False)
        for op, _, _, j1, j2 in sm.get_opcodes():
            if op in ("insert", "replace"):
                nuevas.update(range(j1, j2))
    salida = []
    for i, l in enumerate(lineas):
        clase = "linea nueva" if i in nuevas and l.strip() else "linea"
        salida.append(f'<span class="{clase}">{resaltar(l) or " "}</span>')
    return "\n".join(salida), len(nuevas)


def main():
    if not LINCE.exists():
        sys.exit(f"No encuentro el intérprete en {LINCE}. Compila con 'make'.")
    rutas = sorted(DIR_PASOS.glob("*.lince"))
    pasos = [leer_paso(r) for r in rutas]
    sdl = hay_sdl()
    if not sdl:
        print("⚠  Sin SDL2: los pasos no se ejecutan, solo se comprueba la página")

    if not COMPROBAR:
        from PIL import Image
        DIR_IMG.mkdir(parents=True, exist_ok=True)
    nav, cuerpo, anterior = [], [], None
    for n, (ruta, paso) in enumerate(zip(rutas, pasos), 1):
        img = DIR_IMG / f"paso{n}.png"
        if sdl:
            if COMPROBAR:
                ejecutar(paso, ruta)
            else:
                with tempfile.TemporaryDirectory() as tmp:
                    bmp = pathlib.Path(tmp) / "c.bmp"
                    ejecutar(paso, ruta, bmp)
                    Image.open(bmp).convert("RGB").save(img, optimize=True)
        codigo, cuantas = codigo_marcado(paso["codigo"], anterior)
        anterior = paso["codigo"]
        leyenda = ('<p class="leyenda">En <b>amarillo</b>, lo nuevo de este paso.</p>'
                   if n > 1 else "")
        textos = "".join(f"<p>{formato(t)}</p>" for t in paso["textos"])
        titulo = html.escape(paso["titulo"])
        nav.append(f'<a href="#paso{n}" class="sub">{n}. {titulo}</a>')
        cuerpo.append(
            f'<section class="paso" id="paso{n}"><h2>Paso {n} · {titulo}</h2>{textos}'
            f'<figure><img src="img/tutorial/paso{n}.png" alt="Captura del paso {n}: '
            f'{titulo}" loading="lazy" width="640" height="480">'
            f'<figcaption>Captura real del paso {n}, sacada al generar esta página.</figcaption></figure>'
            f'{leyenda}<div class="codigo"><button class="copiar" aria-label="Copiar">copiar</button>'
            f'<pre><code>{codigo}</code></pre></div>'
            f'<span class="fuente">docs/tutorial/pasos/{html.escape(paso["archivo"])}</span></section>')

    plantilla = (RAIZ / "docs" / "tutorial" / "plantilla.html").read_text(encoding="utf-8")
    pagina = (plantilla.replace("<!--NAV-->", "\n".join(nav))
                       .replace("<!--CUERPO-->", "\n".join(cuerpo))
                       .replace("<!--CUENTA-->", str(len(pasos))))
    if COMPROBAR:
        actual = SALIDA.read_text(encoding="utf-8") if SALIDA.exists() else None
        if actual == pagina:
            print(f"✅ El tutorial está al día ({len(pasos)} pasos"
                  f"{', todos ejecutados' if sdl else ''})")
            return
        sys.exit("❌ docs/tutorial.html no está al día. Ejecuta 'make docs' y "
                 "añade el resultado al commit.")
    SALIDA.write_text(pagina, encoding="utf-8")
    print(f"✅ docs/tutorial.html — {len(pasos)} pasos ejecutados, con sus capturas")


if __name__ == "__main__":
    main()
