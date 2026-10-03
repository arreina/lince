#!/usr/bin/env python3
"""
🐆 Lince — generador del manual

Construye docs/manual.html a partir de los .lince de docs/manual/ejemplos/.
Cada ejemplo se EJECUTA con el intérprete y su salida real se empotra en la
página: así el manual no puede mentir, y si el lenguaje cambia de
comportamiento la regeneración lo detecta (ver 'make docs-check').

Uso:  python3 docs/generar_manual.py [ruta/al/lince]
      python3 docs/generar_manual.py [ruta/al/lince] --comprobar

Con --comprobar no escribe nada: construye la página y falla si no coincide
con la que hay en disco. Así la CI detecta que alguien tocó un ejemplo sin
regenerar, o que el lenguaje cambió de comportamiento.
"""
import html
import pathlib
import re
import subprocess
import sys

RAIZ = pathlib.Path(__file__).resolve().parent.parent
DIR_EJ = RAIZ / "docs" / "manual" / "ejemplos"
SALIDA = RAIZ / "docs" / "manual.html"
# A ruta absoluta: los ejemplos se ejecutan con cwd en la raíz del repo (para
# que las rutas relativas de los 'importar' cuadren), y pathlib se come el
# './' de un './lince', con lo que se buscaría en el PATH.
ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
COMPROBAR = "--comprobar" in sys.argv

LINCE = pathlib.Path(ARGS[0]).resolve() if ARGS else RAIZ / "lince"

# Orden en que aparecen las secciones; lo que no esté aquí va al final.
ORDEN = ["Primeros pasos", "Control de flujo", "Funciones", "Listas",
         "Diccionarios", "Texto", "Clases", "Generadores", "Errores",
         "Módulos y librerías"]

META = re.compile(r"^#\s*@(\w+):\s*(.*)$")


def leer_ejemplo(ruta):
    """Separa metadatos del código. None si es un archivo de apoyo."""
    meta, codigo = {}, []
    for linea in ruta.read_text(encoding="utf-8").splitlines():
        m = META.match(linea)
        if m:
            meta[m.group(1)] = m.group(2).strip()
        else:
            codigo.append(linea)
    if meta.get("oculto") or "seccion" not in meta:
        return None
    meta["codigo"] = "\n".join(codigo).strip("\n")
    meta["archivo"] = ruta.name
    return meta


def ejecutar(ruta):
    """Ejecuta el ejemplo y devuelve su salida tal cual."""
    r = subprocess.run([str(LINCE), str(ruta)], capture_output=True,
                       text=True, timeout=30, cwd=RAIZ)
    return (r.stdout + r.stderr).strip("\n")


# ── Resaltado ────────────────────────────────────────────────────
CLAVES = r"""sea|fijo|funcion|generador|producir|devolver|si|sino|para|cada|
mientras|hacer|elegir|caso|otro|clase|extiende|implementa|interfaz|
enumeracion|mi|padre|verdadero|falso|nulo|escribir|leer|val|ref|intentar|
capturar|finalmente|lanzar|importar|como|en|y|o|no""".replace("\n", "")
TIPOS = "numero|texto|logico|lista|diccionario|error"


def resaltar(codigo):
    """Resaltado por tokens sobre el texto ya escapado, en una sola pasada
    para que nada se resalte dentro de un comentario o de una cadena."""
    patron = re.compile(
        r"(?P<com>#[^\n]*)"
        r"|(?P<cad>&quot;(?:[^&]|&(?!quot;))*&quot;)"
        r"|(?P<num>\b\d+\.?\d*\b)"
        r"|(?P<pal>\b(?:" + CLAVES + r")\b)"
        r"|(?P<tip>\b(?:" + TIPOS + r")\b)")

    def sub(m):
        clase = {"com": "c", "cad": "s", "num": "n", "pal": "k", "tip": "t"}[m.lastgroup]
        return f'<span class="{clase}">{m.group()}</span>'

    return patron.sub(sub, html.escape(codigo))


def main():
    if not LINCE.exists():
        sys.exit(f"No encuentro el intérprete en {LINCE}. Compila con 'make'.")

    ejemplos = []
    for ruta in sorted(DIR_EJ.glob("*.lince")):
        ej = leer_ejemplo(ruta)
        if ej:
            ej["salida"] = ejecutar(ruta)
            ejemplos.append(ej)

    secciones = {}
    for ej in ejemplos:
        secciones.setdefault(ej["seccion"], []).append(ej)
    orden = sorted(secciones, key=lambda s: (ORDEN.index(s) if s in ORDEN else 99, s))

    def ident(s):
        return re.sub(r"[^a-z0-9]+", "-", s.lower().replace("ó", "o")
                      .replace("é", "e").replace("í", "i").replace("á", "a")
                      .replace("ú", "u").replace("ñ", "n")).strip("-")

    nav, cuerpo = [], []
    for sec in orden:
        sid = ident(sec)
        enlaces = "".join(
            f'<a href="#{sid}-{i}" class="sub">{html.escape(ej["titulo"])}</a>'
            for i, ej in enumerate(secciones[sec]))
        nav.append(f'<div class="grupo"><a href="#{sid}" class="sec">'
                   f'{html.escape(sec)}</a>{enlaces}</div>')

        bloques = []
        for i, ej in enumerate(secciones[sec]):
            nota = (f'<p class="nota">{html.escape(ej["nota"])}</p>'
                    if ej.get("nota") else "")
            salida = (f'<div class="salida"><span class="etiqueta">salida '
                      f'verificada</span><pre>{html.escape(ej["salida"])}</pre></div>'
                      if ej["salida"] else "")
            bloques.append(
                f'<article class="ej" id="{sid}-{i}">'
                f'<h3>{html.escape(ej["titulo"])}</h3>{nota}'
                f'<div class="codigo"><button class="copiar" '
                f'aria-label="Copiar">copiar</button>'
                f'<pre><code>{resaltar(ej["codigo"])}</code></pre></div>'
                f'{salida}<span class="fuente">{html.escape(ej["archivo"])}</span>'
                f'</article>')
        cuerpo.append(f'<section id="{sid}"><h2>{html.escape(sec)}</h2>'
                      + "".join(bloques) + "</section>")

    plantilla = (RAIZ / "docs" / "manual" / "plantilla.html").read_text(encoding="utf-8")
    pagina = (plantilla.replace("<!--NAV-->", "\n".join(nav))
                       .replace("<!--CUERPO-->", "\n".join(cuerpo))
                       .replace("<!--CUENTA-->", str(len(ejemplos))))

    if COMPROBAR:
        actual = SALIDA.read_text(encoding="utf-8") if SALIDA.exists() else None
        if actual == pagina:
            print(f"✅ El manual coincide con lo que produce el intérprete "
                  f"({len(ejemplos)} ejemplos)")
            return
        if actual is None:
            sys.exit("❌ Falta docs/manual.html. Ejecuta 'make docs'.")
        sys.exit("❌ docs/manual.html no está al día: algún ejemplo o la salida "
                 "del intérprete han cambiado.\n   Ejecuta 'make docs' y añade "
                 "el resultado al commit.")

    SALIDA.write_text(pagina, encoding="utf-8")
    print(f"✅ docs/manual.html — {len(ejemplos)} ejemplos ejecutados, "
          f"{len(orden)} secciones")


if __name__ == "__main__":
    main()
