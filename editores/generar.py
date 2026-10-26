#!/usr/bin/env python3
"""Genera los archivos de soporte de los editores a partir del propio lenguaje.

Las listas de palabras clave, módulos, funciones integradas y tipos de error
se leen de src/lexer.c, src/modulos.c y src/interprete.c, así que no se pueden
desfasar: si se añade una palabra al lexer, `make editores` la propaga y
`make editores-check` (que corre en la CI) falla mientras no se haya hecho.

Genera:
  vscode/lince-language/lince.tmLanguage.json   (y la copia de Sublime Text)
  vscode/lince-language/snippets.json
  sublime-text/lince.sublime-completions
  notepad++/lince-notepad++.xml                 (sólo las listas de palabras)

Uso:  python3 editores/generar.py            # reescribe
      python3 editores/generar.py --comprobar  # sólo comprueba que están al día
"""
import json, re, sys, pathlib

AQUI = pathlib.Path(__file__).resolve().parent
RAIZ = AQUI.parent

# ─────────────────────────────────────────
# Lectura del lenguaje
# ─────────────────────────────────────────
def leer(ruta):
    return (RAIZ / ruta).read_text(encoding="utf-8")

def palabras_clave():
    t = leer("src/lexer.c")
    bloque = re.search(r"PALABRAS_CLAVE\[\]\s*=\s*\{(.*?)\n\};", t, re.S).group(1)
    return re.findall(r'\{\s*"([a-z_]+)"\s*,', bloque)

def palabras_contextuales():
    t = leer("src/lexer.c")
    bloque = re.search(r"PALABRAS_CONTEXTUALES\[\]\s*=\s*\{(.*?)\};", t, re.S).group(1)
    # 'y' y 'o' también son contextuales, pero se pintan con las lógicas
    return [w for w in re.findall(r'"([a-z_]+)"', bloque) if w not in LOGICAS]

def modulos():
    t = leer("src/modulos.c")
    bloque = re.search(r"MODULOS_ESTANDAR\[\]\s*=\s*\{(.*?)\};", t, re.S).group(1)
    return re.findall(r'"([a-z_]+)"', bloque)

def funciones_integradas():
    t = leer("src/interprete.c")
    nombres = re.findall(r'registrar_fn_nativa\([^,]+,\s*"([a-z_]+)"', t)
    return sorted(set(nombres))

def errores():
    t = leer("src/interprete.c")
    return sorted(set(re.findall(r'"(Error[A-Za-z]*)"', t)) | {"Error"})

# Cada palabra reservada tiene que estar en una categoría: si el lexer gana una
# palabra nueva y nadie la ha clasificado, el generador se queda parado.
CONTROL = ["si", "sino", "para", "cada", "mientras", "hacer", "devolver", "producir",
           "intentar", "capturar", "finalmente", "lanzar", "elegir", "caso", "otro",
           "romper", "continuar"]
DECLARACION = ["sea", "fijo", "funcion", "generador", "clase", "extiende", "implementa",
               "interfaz", "enumeracion", "mi", "padre", "importar", "val", "ref"]
TIPOS = ["numero", "texto", "logico", "lista", "diccionario"]
LOGICAS = ["y", "o", "no"]
LITERALES = ["verdadero", "falso", "nulo"]
OTRAS = ["escribir"]            # van aparte (función integrada)
# 'en' lo usa 'para cada x en lista' y no es palabra reservada del lexer
EXTRA_CONTROL = ["en"]

def clasificar():
    ks = palabras_clave() + LOGICAS
    conocidas = set(CONTROL + DECLARACION + TIPOS + LOGICAS + LITERALES + OTRAS)
    sin = [k for k in ks if k not in conocidas]
    if sin:
        sys.exit(f"editores/generar.py: palabras del lexer sin clasificar: {sin}\n"
                 f"Añádelas a CONTROL / DECLARACION / TIPOS / LOGICAS / LITERALES en el generador.")
    return ks

def alt(lista):
    return "|".join(lista)

# ─────────────────────────────────────────
# Gramática TextMate (VS Code y Sublime Text)
# ─────────────────────────────────────────
def gramatica():
    clasificar()
    ctx = palabras_contextuales()
    mods = modulos()
    ints = [f for f in funciones_integradas()] + ["escribir"]
    ints = sorted(set(ints))
    return {
        "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
        "name": "Lince",
        "scopeName": "source.lince",
        "fileTypes": ["lince"],
        "patterns": [{"include": "#" + n} for n in [
            "comentarios", "cadenas", "numeros", "palabras_clave_control",
            "palabras_clave_declaracion", "palabras_clave_tipos", "palabras_clave_logicas",
            "palabras_clave_errores", "palabras_clave_modulos", "literales", "clases",
            "funciones", "operadores", "identificadores"]],
        "repository": {
            "comentarios": {"name": "comment.line.number-sign.lince", "match": "#.*$"},
            "cadenas": {
                "name": "string.quoted.double.lince", "begin": "\"", "end": "\"",
                "patterns": [
                    {"name": "constant.character.escape.lince",
                     "match": "\\\\(n|t|r|e|\"|\\\\|\\{|\\})"},
                    # "Vidas: {vidas}" (unas llaves vacías, "{}", son texto)
                    {"name": "meta.interpolation.lince",
                     "begin": "\\{(?!\\})", "end": "\\}",
                     "beginCaptures": {"0": {"name": "punctuation.section.interpolation.begin.lince"}},
                     "endCaptures": {"0": {"name": "punctuation.section.interpolation.end.lince"}},
                     "contentName": "source.lince.embedded",
                     "patterns": [{"include": "$self"}]}]},
            "numeros": {"name": "constant.numeric.lince", "match": "\\b(0[xX][0-9a-fA-F_]+|0[bB][01_]+|[0-9]+\\.?[0-9]*)\\b"},
            "palabras_clave_control": {
                "name": "keyword.control.lince",
                "match": "\\b(" + alt(CONTROL + EXTRA_CONTROL) + ")\\b"},
            "palabras_clave_declaracion": {
                "name": "keyword.declaration.lince",
                "match": "\\b(" + alt(DECLARACION + ctx) + ")\\b"},
            "palabras_clave_tipos": {
                "name": "storage.type.lince", "match": "\\b(" + alt(TIPOS) + ")\\b"},
            "palabras_clave_logicas": {
                "name": "keyword.operator.logical.lince", "match": "\\b(" + alt(LOGICAS) + ")\\b"},
            "palabras_clave_errores": {
                "name": "support.class.error.lince", "match": "\\b(" + alt(errores()) + ")\\b"},
            "palabras_clave_modulos": {
                "name": "support.class.module.lince",
                "match": "\\b(" + alt(mods) + ")(?=\\.)"},
            "literales": {"patterns": [
                {"name": "constant.language.boolean.true.lince", "match": "\\b(verdadero)\\b"},
                {"name": "constant.language.boolean.false.lince", "match": "\\b(falso)\\b"},
                {"name": "constant.language.null.lince", "match": "\\b(nulo)\\b"}]},
            "clases": {"name": "entity.name.type.class.lince",
                       "match": "\\b([A-Z][a-zA-Z0-9_]*)\\b"},
            "funciones": {"patterns": [
                {"name": "entity.name.function.lince",
                 "match": "(?<=funcion\\s|generador\\s|tarea\\s)([a-z_][a-zA-Z0-9_]*)"},
                {"name": "support.function.builtin.lince", "match": "\\b(" + alt(ints) + ")\\b"},
                {"name": "entity.name.function.call.lince",
                 "match": "\\b([a-z_][a-zA-Z0-9_]*)(?=\\s*\\()"}]},
            "operadores": {"patterns": [
                {"name": "keyword.operator.assignment.compound.lince", "match": "(\\+=|-=|\\*=|/=|%=)"},
                {"name": "keyword.operator.arithmetic.lince", "match": "(\\+\\+|--|\\+|-|\\*|/|%)"},
                {"name": "keyword.operator.comparison.lince", "match": "(==|!=|<=|>=|<|>)"},
                {"name": "keyword.operator.assignment.lince", "match": "(?<!=)=(?!=)"},
                {"name": "keyword.operator.arrow.lince", "match": "->"},
                {"name": "keyword.operator.null.lince", "match": "(\\?\\?|\\?\\.)"}]},
            "identificadores": {"name": "variable.other.lince",
                                "match": "\\b([a-z_][a-zA-Z0-9_]*)\\b"},
        },
    }

# ─────────────────────────────────────────
# Snippets (una sola fuente para VS Code y Sublime Text)
# Cada cuerpo se comprueba con `lince comprobar` en tests/editores.
# ─────────────────────────────────────────
SNIPPETS = [
    ("Variable", "sea", "Declara una variable", ["sea ${1:nombre} = ${2:valor}"]),
    ("Constante", "fijo", "Declara una constante", ["fijo ${1:NOMBRE} = ${2:valor}"]),
    ("Lista", "lista", "Declara una lista", ["sea lista ${1:nombre} = [${2:1, 2, 3}]"]),
    ("Diccionario", "dic", "Declara un diccionario",
     ["sea diccionario ${1:nombre} = {\"${2:clave}\": ${3:valor}}"]),
    ("Función", "fun", "Define una función",
     ["funcion ${1:nombre}(${2:numero} ${3:param}): ${4:nulo} {", "\t$0", "}"]),
    ("Función sin parámetros", "fun0", "Define una función sin parámetros",
     ["funcion ${1:nombre}(): ${2:nulo} {", "\t$0", "}"]),
    ("Lambda", "lambda", "Función anónima",
     ["sea ${1:f} = funcion(${2:numero} ${3:x}): ${4:numero} {", "\tdevolver ${5:x}", "}"]),
    ("Generador", "generador", "Función generadora",
     ["generador ${1:contar}(numero ${2:n}): numero {", "\tpara (${3:i} = 0; ${3:i} < ${2:n}; ${3:i}++) {",
      "\t\tproducir ${3:i}", "\t}", "}"]),
    ("Tarea", "tarea", "Código que se para a esperar sin parar el juego",
     ["tarea ${1:nombre}() {", "\t$2", "\tesperar(${3:0.5})", "\t$0", "}"]),
    ("Si", "si", "Condicional", ["si ${1:condicion} {", "\t$0", "}"]),
    ("Si / sino", "sino", "Condicional con alternativa",
     ["si ${1:condicion} {", "\t$2", "} sino {", "\t$0", "}"]),
    ("Elegir", "elegir", "Selección múltiple",
     ["elegir ${1:valor} {", "\tcaso ${2:1} {", "\t\t$3", "\t}", "\totro {", "\t\t$0", "\t}", "}"]),
    ("Para clásico", "para", "Bucle con contador",
     ["para (${1:i} = 0; ${1:i} < ${2:10}; ${1:i}++) {", "\t$0", "}"]),
    ("Para cada", "cada", "Recorre una colección",
     ["para cada ${1:numero} ${2:x} en ${3:[1, 2, 3]} {", "\t$0", "}"]),
    ("Mientras", "mientras", "Bucle mientras", ["mientras ${1:condicion} {", "\t$0", "}"]),
    ("Hacer mientras", "hacer", "Bucle que se ejecuta al menos una vez",
     ["hacer {", "\t$0", "} mientras (${1:condicion})"]),
    ("Clase", "clase", "Define una clase",
     ["clase ${1:Nombre} {", "\tfuncion crear(${2:numero} ${3:param}): nulo {",
      "\t\tmi.${3:param} = ${3:param}", "\t}", "\t$0", "}"]),
    ("Clase con herencia", "clasex", "Clase que extiende a otra",
     ["clase ${1:Nombre} extiende ${2:Padre} {", "\tfuncion crear(${3:numero} ${4:param}): nulo {",
      "\t\tpadre(${4:param})", "\t}", "\t$0", "}"]),
    ("Clase abstracta", "abstracta", "Clase que no se instancia",
     ["abstracta clase ${1:Figura} {", "\tabstracto funcion ${2:area}(): ${3:numero}", "\t$0", "}"]),
    ("Método estático", "estatico", "Método de la clase",
     ["estatico funcion ${1:nombre}(): ${2:nulo} {", "\t$0", "}"]),
    ("Campo privado", "privado", "Campo con visibilidad privada",
     ["privado sea ${1:nombre} = ${2:0}"]),
    ("Propiedad", "propiedad", "Pareja obtener / fijar",
     ["obtener ${1:valor}(): ${2:numero} {", "\tdevolver mi._${1:valor}", "}",
      "fijar ${1:valor}(${2:numero} ${3:v}): nulo {", "\tmi._${1:valor} = ${3:v}", "}"]),
    ("Interfaz", "interfaz", "Define una interfaz",
     ["interfaz ${1:Forma} {", "\tfuncion ${2:area}(): ${3:numero}", "}"]),
    ("Enumeración", "enumeracion", "Define una enumeración",
     ["enumeracion ${1:Color} {", "\t${2:Rojo}", "\t${3:Verde}", "}"]),
    ("Intentar", "intentar", "Captura errores",
     ["intentar {", "\t$1", "} capturar (${2:error} ${3:e}) {", "\tescribir(${3:e}.mensaje)", "}"]),
    ("Intentar con finalmente", "finalmente", "Captura errores y siempre limpia",
     ["intentar {", "\t$1", "} capturar (${2:error} ${3:e}) {", "\tescribir(${3:e}.mensaje)",
      "} finalmente {", "\t$0", "}"]),
    ("Lanzar", "lanzar", "Lanza un error", ["lanzar Error(\"${1:mensaje}\")"]),
    ("Importar módulo", "importar", "Importa un módulo estándar", ["importar \"${1:matematica}\""]),
    ("Importar con alias", "importarc", "Importa un archivo con espacio de nombres",
     ["importar \"./${1:libreria}\" como ${2:lib}"]),
    ("Escribir", "esc", "Escribe en pantalla", ["escribir(${1:\"mensaje\"})"]),
]

def vscode_snippets():
    return {n: {"prefix": p, "body": b, "description": d} for n, p, d, b in SNIPPETS}

def sublime_completions():
    comps = [{"trigger": f"{p}\t{d}", "contents": "\n".join(b)} for n, p, d, b in SNIPPETS]
    palabras = (palabras_clave() + LOGICAS + palabras_contextuales() + ["en"] + modulos()
                + errores() + funciones_integradas())
    vistas, planas = set(), []
    for w in palabras:
        if w not in vistas:
            vistas.add(w); planas.append(w)
    return {"scope": "source.lince", "completions": comps + planas}

# ─────────────────────────────────────────
# Notepad++: sólo se tocan los bloques de palabras
# ─────────────────────────────────────────
def notepad(texto):
    def poner(t, nombre, palabras):
        return re.sub(r'(<Keywords name="%s">)(.*?)(</Keywords>)' % nombre,
                      lambda m: m.group(1) + "\n        " + " ".join(palabras) + "\n      " + m.group(3),
                      t, flags=re.S)
    clasificar()
    t = texto
    t = poner(t, "Keywords1", CONTROL + EXTRA_CONTROL)
    t = poner(t, "Keywords2", DECLARACION + palabras_contextuales())
    t = poner(t, "Keywords3", TIPOS + ["nulo"])
    t = poner(t, "Keywords4", ["verdadero", "falso"] + LOGICAS)
    t = poner(t, "Keywords5", errores())
    t = poner(t, "Keywords6", sorted(set(funciones_integradas() + ["escribir"])) + modulos())
    return t

# ─────────────────────────────────────────
def volcar(obj):
    return json.dumps(obj, ensure_ascii=False, indent=2) + "\n"

def salidas():
    g = volcar(gramatica())
    npp = notepad((AQUI / "notepad++/lince-notepad++.xml").read_text(encoding="utf-8"))
    return {
        AQUI / "vscode/lince-language/lince.tmLanguage.json": g,
        AQUI / "sublime-text/lince.tmLanguage.json": g,
        AQUI / "vscode/lince-language/snippets.json": volcar(vscode_snippets()),
        AQUI / "sublime-text/lince.sublime-completions": volcar(sublime_completions()),
        AQUI / "notepad++/lince-notepad++.xml": npp,
    }

def main():
    comprobar = "--comprobar" in sys.argv
    mal = []
    for ruta, nuevo in salidas().items():
        actual = ruta.read_text(encoding="utf-8") if ruta.exists() else None
        if actual != nuevo:
            if comprobar:
                mal.append(str(ruta.relative_to(RAIZ)))
            else:
                ruta.write_text(nuevo, encoding="utf-8")
                print("  escrito", ruta.relative_to(RAIZ))
    if comprobar:
        if mal:
            print("❌ Los archivos de editores están desfasados respecto al lenguaje:")
            for m in mal: print("   ", m)
            print("   Ejecuta: make editores")
            sys.exit(1)
        print("✅ El soporte de editores coincide con el lenguaje")

if __name__ == "__main__":
    main()
