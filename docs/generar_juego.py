#!/usr/bin/env python3
"""
🐆 Lince — generador del manual del módulo «juego»

Construye docs/juego.html a partir de los .lince de docs/juego/ejemplos/.
Como en el manual del lenguaje, cada ejemplo se EJECUTA al generar la página:
sin pantalla (SDL_VIDEODRIVER=dummy), unos fotogramas, y de su último
fotograma sale la captura que se enseña. Lo que escribe con escribir() sale
como «salida verificada». Así esta página no puede tener código que no
funcione.

Metadatos de cada ejemplo (líneas '# @clave: valor' al principio):
  seccion, titulo     obligatorios
  nota                un párrafo (puede haber varios); `código` y **negrita**
  fotogramas          cuántos ejecuta antes de la captura (30)
  teclas, guion, clic las teclas o clics de prueba (LINCE_JUEGO_PULSAR,
                      LINCE_JUEGO_GUION, LINCE_JUEGO_CLIC)
  escribe             lo que teclea cuando el juego lo pide (LINCE_JUEGO_ESCRIBIR)
  captura: no         sin captura (lo que no dibuja nada: sonido, partidas...)
  ejecutar: no        solo se enseña, sin ejecutarlo (necesita archivos que no hay)
  lenguaje: terminal  son órdenes de la terminal, no Lince (sin resaltar ni ejecutar)

Uso:  python3 docs/generar_juego.py [ruta/al/lince] [--comprobar]
"""
import html
import os
import pathlib
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from generar_manual import resaltar, formato  # mismo resaltado y formato que el manual

RAIZ = pathlib.Path(__file__).resolve().parent.parent
DIR_EJ = RAIZ / "docs" / "juego" / "ejemplos"
DIR_IMG = RAIZ / "docs" / "img" / "juego"
SALIDA = RAIZ / "docs" / "juego.html"
ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
COMPROBAR = "--comprobar" in sys.argv
LINCE = pathlib.Path(ARGS[0]).resolve() if ARGS else RAIZ / "lince"
META = re.compile(r"^#\s*@(\w+):\s*(.*)$")

ORDEN = ["Empezar", "Dibujar", "Texto y letras", "Imágenes y sprites", "Hojas de sprites",
         "Animador", "Entrada", "Móvil", "Sonido", "Movimiento", "Cámara", "Choques",
         "Mapas", "Mapas de Tiled", "Física", "Partículas", "Escenas", "Menús y diálogos",
         "Controles", "Partidas guardadas", "Tareas", "Peleas", "Compartir el juego"]


def leer_ejemplo(ruta):
    meta, notas, codigo = {}, [], []
    cabecera = True
    for linea in ruta.read_text(encoding="utf-8").splitlines():
        m = META.match(linea) if cabecera else None
        if m:
            if m.group(1) == "nota":
                notas.append(m.group(2).strip())
            else:
                meta[m.group(1)] = m.group(2).strip()
        else:
            cabecera = False
            codigo.append(linea)
    for campo in ("seccion", "titulo"):
        if not meta.get(campo):
            sys.exit(f"❌ {ruta.name}: falta '# @{campo}:'")
    meta["notas"] = notas
    meta["codigo"] = "\n".join(codigo).strip("\n")
    meta["archivo"] = ruta.name
    return meta


def hay_sdl():
    if os.name == "nt":
        return True
    r = subprocess.run(["ldconfig", "-p"], capture_output=True, text=True)
    return "libSDL2-2.0.so.0" in r.stdout


def ejecutar(ej, ruta, captura):
    with tempfile.TemporaryDirectory() as partidas:
        entorno = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                       LINCE_JUEGO_FOTOGRAMAS=ej.get("fotogramas", "30"),
                       LINCE_JUEGO_SEMILLA="1", LINCE_JUEGO_GUARDADO=partidas)
        for clave, var in (("teclas", "LINCE_JUEGO_PULSAR"), ("guion", "LINCE_JUEGO_GUION"),
                           ("clic", "LINCE_JUEGO_CLIC"), ("escribe", "LINCE_JUEGO_ESCRIBIR")):
            if ej.get(clave):
                entorno[var] = ej[clave]
        if captura:
            entorno["LINCE_JUEGO_CAPTURA"] = str(captura)
        # se ejecuta desde docs/juego, donde están los recursos de los ejemplos
        r = subprocess.run([str(LINCE), str(ruta)], capture_output=True, text=True,
                           timeout=60, cwd=RAIZ / "docs" / "juego", env=entorno)
    salida = (r.stdout + r.stderr).strip("\n")
    if r.returncode != 0:
        sys.exit(f"❌ {ruta.name} termina con código {r.returncode}. Un ejemplo de la "
                 f"página tiene que funcionar.\n" + "\n".join("   " + l for l in salida.splitlines()[:15]))
    return salida


def ident(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower().replace("ó", "o").replace("é", "e")
                  .replace("í", "i").replace("á", "a").replace("ú", "u").replace("ñ", "n")).strip("-")


def main():
    if not LINCE.exists():
        sys.exit(f"No encuentro el intérprete en {LINCE}. Compila con 'make'.")
    sdl = hay_sdl()
    if not sdl:
        print("⚠  Sin SDL2: los ejemplos no se ejecutan, solo se comprueba la página")
    if not COMPROBAR:
        from PIL import Image
        DIR_IMG.mkdir(parents=True, exist_ok=True)

    ejemplos = []
    for ruta in sorted(DIR_EJ.glob("*.lince")):
        ej = leer_ejemplo(ruta)
        ej["salida"] = ""
        ej["imagen"] = None
        if ej.get("lenguaje") == "terminal":
            ej["ejecutar"] = "no"
        if ej.get("ejecutar") != "no":
            con_captura = ej.get("captura") != "no"
            nombre = ruta.stem + ".png"
            if sdl:
                with tempfile.TemporaryDirectory() as tmp:
                    bmp = pathlib.Path(tmp) / "c.bmp"
                    ej["salida"] = ejecutar(ej, ruta, bmp if con_captura and not COMPROBAR else None)
                    if con_captura and not COMPROBAR:
                        Image.open(bmp).convert("RGB").save(DIR_IMG / nombre, optimize=True)
            if con_captura:
                ej["imagen"] = nombre
        ejemplos.append(ej)
    if not sdl and COMPROBAR:
        # sin SDL2 no se sabe la salida: se toma la de la página que hay
        print("   (sin ejecutar: la salida de los ejemplos no se comprueba)")

    secciones = {}
    for ej in ejemplos:
        secciones.setdefault(ej["seccion"], []).append(ej)
    orden = sorted(secciones, key=lambda s: (ORDEN.index(s) if s in ORDEN else 99, s))
    nav, cuerpo = [], []
    for sec in orden:
        sid = ident(sec)
        enlaces = "".join(f'<a href="#{sid}-{i}" class="sub">{html.escape(ej["titulo"])}</a>'
                          for i, ej in enumerate(secciones[sec]))
        nav.append(f'<div class="grupo"><a href="#{sid}" class="sec">{html.escape(sec)}</a>{enlaces}</div>')
        bloques = []
        for i, ej in enumerate(secciones[sec]):
            notas = "".join(f'<p class="nota">{formato(n)}</p>' for n in ej["notas"])
            es_lince = ej.get("ejecutar") != "no"
            codigo = html.escape(ej["codigo"]) if ej.get("lenguaje") == "terminal" else resaltar(ej["codigo"])
            figura = (f'<figure><img src="img/juego/{ej["imagen"]}" alt="Captura de «{html.escape(ej["titulo"])}»" '
                      f'loading="lazy"><figcaption>Captura real, sacada al generar esta página.</figcaption></figure>'
                      if ej["imagen"] else "")
            salida = (f'<div class="salida"><span class="etiqueta">salida verificada</span>'
                      f'<pre>{html.escape(ej["salida"])}</pre></div>' if ej["salida"] else "")
            fuente = (f'<span class="fuente">docs/juego/ejemplos/{html.escape(ej["archivo"])}</span>'
                      if es_lince else "")
            bloques.append(
                f'<article class="ej" id="{sid}-{i}"><h3>{html.escape(ej["titulo"])}</h3>{notas}'
                f'<div class="codigo"><button class="copiar" aria-label="Copiar">copiar</button>'
                f'<pre><code>{codigo}</code></pre></div>{figura}{salida}{fuente}</article>')
        cuerpo.append(f'<section id="{sid}"><h2>{html.escape(sec)}</h2>' + "".join(bloques) + "</section>")

    plantilla = (RAIZ / "docs" / "juego" / "plantilla.html").read_text(encoding="utf-8")
    pagina = (plantilla.replace("<!--NAV-->", "\n".join(nav))
                       .replace("<!--CUERPO-->", "\n".join(cuerpo))
                       .replace("<!--CUENTA-->", str(sum(1 for e in ejemplos if e.get("ejecutar") != "no"))))
    if COMPROBAR:
        actual = SALIDA.read_text(encoding="utf-8") if SALIDA.exists() else None
        if actual == pagina or (not sdl and actual is not None):
            print(f"✅ La página del módulo juego está al día ({len(ejemplos)} ejemplos"
                  f"{', todos ejecutados' if sdl else ''})")
            return
        sys.exit("❌ docs/juego.html no está al día. Ejecuta 'make docs' y añade el resultado al commit.")
    SALIDA.write_text(pagina, encoding="utf-8")
    print(f"✅ docs/juego.html — {len(ejemplos)} ejemplos, con sus capturas")


if __name__ == "__main__":
    main()
