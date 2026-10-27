#!/usr/bin/env python3
"""Prueba de humo del servidor de lenguaje (`lince lsp`).

Habla el protocolo de verdad por stdin/stdout: inicializa, abre un documento
con un error, comprueba el diagnóstico, y pide completado, símbolos y
definición.

Uso: python3 tests/lsp/smoke.py ./lince
"""
import json, os, subprocess, sys, tempfile

lince = sys.argv[1] if len(sys.argv) > 1 else "./lince"
p = subprocess.Popen([lince, "lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

def send(o):
    b = json.dumps(o).encode()
    p.stdin.write(b"Content-Length: %d\r\n\r\n" % len(b) + b)
    p.stdin.flush()

def recv():
    n = 0
    while True:
        l = p.stdout.readline().decode()
        if l == "":
            raise SystemExit("❌ el servidor cerró la salida")
        if l.strip() == "":
            break
        if l.lower().startswith("content-length"):
            n = int(l.split(":")[1])
    return json.loads(p.stdout.read(n))

fallos = []
def comprobar(cond, texto):
    print(("  ✅ " if cond else "  ❌ ") + texto)
    if not cond:
        fallos.append(texto)

def pedir(i, metodo, params):
    send({"jsonrpc": "2.0", "id": i, "method": metodo, "params": params})
    return recv()["result"]

# ── inicialización ──
caps = pedir(1, "initialize", {})["capabilities"]
comprobar(caps["textDocumentSync"] == 1 and "completionProvider" in caps, "initialize anuncia sus capacidades")

# ── diagnóstico de un error de sintaxis ──
uri = "file:///tmp/prueba.lince"
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {
    "uri": uri, "text": "sea x = 1\nsi x > {\n}\n"}}})
d = recv()["params"]["diagnostics"]
comprobar(len(d) == 1 and d[0]["severity"] == 1, "un error de sintaxis da un diagnóstico")
comprobar(d[0]["range"]["start"]["line"] in (1, 2, 3), "el diagnóstico apunta a una línea del documento")
comprobar("Error en línea" not in d[0]["message"] and d[0]["message"] != "", "el mensaje viene limpio")

# ── texto correcto: sin diagnósticos ──
src = ('importar "matematica"\n'
       'funcion doble(numero n): numero {\n    devolver n * 2\n}\n'
       'sea r = matematica.\n'                      # incompleto a propósito
       'clase P {\n  funcion crear(): nulo { mi.a = 1 }\n  funcion f(): nulo { mi. }\n}\n')
send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
    "textDocument": {"uri": uri}, "contentChanges": [{"text": src}]}})
recv()
ok = 'sea a = 1\nescribir(a)\n'
send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
    "textDocument": {"uri": uri}, "contentChanges": [{"text": ok}]}})
comprobar(recv()["params"]["diagnostics"] == [], "código correcto: sin diagnósticos")
# ── aviso: a un 'elegir' le faltan valores de una enumeración ──
falta = ('enumeracion Estado { Quieto Andando Saltando }\nsea e = Estado.Quieto\n'
         'elegir e {\n    caso Estado.Quieto { escribir(1) }\n}\n')
send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
    "textDocument": {"uri": uri}, "contentChanges": [{"text": falta}]}})
d = recv()["params"]["diagnostics"]
comprobar(len(d) == 1 and d[0]["severity"] == 2 and d[0]["range"]["start"]["line"] == 2 and
          "Estado.Andando ni Estado.Saltando" in d[0]["message"], "aviso de un elegir al que le faltan casos")
# ── avisos del revisor: una errata y un tipo que no casa ──
errata = 'sea numero vida = 3\nescribir(vda)\nvida = "mucha"\n'
send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
    "textDocument": {"uri": uri}, "contentChanges": [{"text": errata}]}})
d = recv()["params"]["diagnostics"]
comprobar(len(d) == 2 and all(x["severity"] == 2 for x in d) and
          d[0]["range"]["start"]["line"] == 1 and "¿Querías decir 'vida'?" in d[0]["message"] and
          d[1]["range"]["start"]["line"] == 2 and "es numero" in d[1]["message"],
          "avisos del revisor: un nombre mal escrito y un tipo que no casa")
send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
    "textDocument": {"uri": uri}, "contentChanges": [{"text": src}]}})
recv()

pos = lambda l, c: {"textDocument": {"uri": uri}, "position": {"line": l, "character": c}}

# ── completado ──
etiquetas = lambda r: [i["label"] for i in r]
r = etiquetas(pedir(2, "textDocument/completion", pos(4, 19)))       # 'matematica.|'
comprobar("raiz" in r and "potencia" in r, "tras 'matematica.' salen sus funciones")
comprobar("si" not in r, "…y sólo ellas")
r = etiquetas(pedir(3, "textDocument/completion", pos(7, 25)))       # 'mi.|'
comprobar("crear" in r and "a" in r, "tras 'mi.' salen los métodos y campos de la clase")
r = etiquetas(pedir(4, "textDocument/completion", pos(1, 0)))
comprobar(all(w in r for w in ["privado", "interfaz", "doble", "mapear", "matematica", "P"]),
          "en general salen palabras clave, integradas, módulos y lo definido en el archivo")
comprobar(len(r) == len(set(r)), "sin duplicados")

# ── símbolos y definición ──
s = pedir(5, "textDocument/documentSymbol", {"textDocument": {"uri": uri}})
nombres = {x["name"] for x in s}
comprobar({"doble", "P", "crear", "f"} <= nombres, "documentSymbol lista funciones, clases y métodos")
loc = pedir(6, "textDocument/definition", pos(1, 11))                 # sobre 'doble'
comprobar(loc and loc["range"]["start"]["line"] == 1, "ir a la definición lleva a la declaración")

# ── alias de importación: miembros de otra librería ──
with tempfile.TemporaryDirectory() as d_:
    open(os.path.join(d_, "lib.lince"), "w").write("funcion suma(numero a, numero b): numero {\n    devolver a + b\n}\nsea PI2 = 6\n")
    u2 = "file://" + os.path.join(d_, "main.lince")
    t2 = 'importar "./lib" como mi_lib\nmi_lib.\n'
    send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {"uri": u2, "text": t2}}})
    recv()
    send({"jsonrpc": "2.0", "id": 7, "method": "textDocument/completion",
          "params": {"textDocument": {"uri": u2}, "position": {"line": 1, "character": 7}}})
    r = etiquetas(recv()["result"])
    comprobar("suma" in r and "PI2" in r, "tras 'alias.' salen los nombres de la librería importada")

# ── la columna del error, y ir a la definición en archivos importados ──
with tempfile.TemporaryDirectory() as d_:
    os.makedirs(os.path.join(d_, "libs"))
    open(os.path.join(d_, "libs", "util.lince"), "w").write("sea PI2 = 6\nfuncion doble(numero n): numero {\n    devolver n * 2\n}\n")
    open(os.path.join(d_, "libs", "mas.lince"), "w").write('importar "./util"\nfuncion triple(numero n): numero {\n    devolver n * 3\n}\n')
    u3 = "file://" + os.path.join(d_, "main.lince")
    t3 = 'importar "./libs/mas"\nimportar "./libs/util" como u\nsea a = triple(1)\nsea b = doble(2)\nsea c = u.doble(3)\nsea \u00f1 = 1\n'
    send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {"uri": u3, "text": t3}}})
    recv()
    def donde(linea, car):
        return pedir(20 + linea, "textDocument/definition", {"textDocument": {"uri": u3}, "position": {"line": linea, "character": car}})
    r = donde(2, 11)       # triple, de libs/mas
    comprobar(r and r["uri"].endswith("/libs/mas.lince") and r["range"]["start"]["line"] == 1, "ir a la definición salta a un archivo importado")
    r = donde(3, 11)       # doble, de libs/util, que importa libs/mas
    comprobar(r and r["uri"].endswith("/libs/util.lince") and r["range"]["start"]["line"] == 1, "...también si lo importa el importado")
    r = donde(4, 13)       # u.doble
    comprobar(r and r["uri"].endswith("/libs/util.lince") and r["range"]["start"]["line"] == 1, "...y con alias (u.doble)")
    # un error en mitad de la línea subraya solo lo que falla
    send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
        "textDocument": {"uri": u3}, "contentChanges": [{"text": "sea x = 1\nsea n = 5 @\n"}]}})
    d = recv()["params"]["diagnostics"]
    r = d[0]["range"]
    comprobar(d and r["start"]["line"] == 1 and r["start"]["character"] == 10 and r["end"]["character"] == 11,
              "el error de sintaxis dice su columna (%s)" % json.dumps(r))
    send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
        "textDocument": {"uri": u3}, "contentChanges": [{"text": "sea x = 1\nsea y = \u00e9\n"}]}})
    d = recv()["params"]["diagnostics"]
    r = d[0]["range"]
    comprobar(r["start"]["line"] == 1 and r["start"]["character"] == 8 and r["end"]["character"] == 9 and "\u00e9" in d[0]["message"],
              "una letra con tilde no se parte (%s)" % d[0]["message"])

# ── ayuda al pasar el ratón (los comentarios de encima) y formatear ──
with tempfile.TemporaryDirectory() as d_:
    open(os.path.join(d_, "util.lince"), "w").write("# Lo que se tarda en cruzar la pantalla.\nfuncion cruzar(numero v): numero {\n    devolver 320 / v\n}\n")
    u4 = "file://" + os.path.join(d_, "main.lince")
    t4 = '# El doble de un número.\nfuncion doble(numero n): numero { devolver n * 2 }\nimportar "./util"\nescribir(doble(cruzar(2)))\n'
    send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {"uri": u4, "text": t4}}})
    recv()
    caps_h = pedir(30, "textDocument/hover", {"textDocument": {"uri": u4}, "position": {"line": 3, "character": 10}})
    v = caps_h and caps_h["contents"]["value"]
    comprobar(v and "funcion doble(numero n): numero" in v and "El doble de un número." in v,
              "hover: la firma y la ayuda de los comentarios (%r)" % v)
    caps_h = pedir(31, "textDocument/hover", {"textDocument": {"uri": u4}, "position": {"line": 3, "character": 17}})
    v = caps_h and caps_h["contents"]["value"]
    comprobar(v and "cruzar" in v and "cruzar la pantalla" in v, "hover: también de lo que viene de otro archivo")
    comprobar(pedir(32, "textDocument/hover", {"textDocument": {"uri": u4}, "position": {"line": 2, "character": 2}}) is None,
              "hover: nada sobre una palabra sin ayuda")
    feo = "sea x=1\nsi x>0{escribir( x )}\n"
    send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
        "textDocument": {"uri": u4}, "contentChanges": [{"text": feo}]}})
    recv()
    r = pedir(33, "textDocument/formatting", {"textDocument": {"uri": u4}, "options": {"tabSize": 4, "insertSpaces": True}})
    comprobar(len(r) == 1 and r[0]["newText"] == "sea x = 1\nsi x > 0 { escribir(x) }\n" and r[0]["range"]["start"] == {"line": 0, "character": 0},
              "formatting: el documento con el estilo de Lince")
    send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
        "textDocument": {"uri": u4}, "contentChanges": [{"text": "sea x = (1\n"}]}})
    recv()
    comprobar(pedir(34, "textDocument/formatting", {"textDocument": {"uri": u4}}) == [],
              "formatting: con un error de sintaxis no cambia nada")
comprobar(caps.get("hoverProvider") and caps.get("documentFormattingProvider"), "initialize anuncia hover y formatting")

# ── cierre ordenado ──
send({"jsonrpc": "2.0", "id": 99, "method": "shutdown"})
recv()
send({"jsonrpc": "2.0", "method": "exit"})
comprobar(p.wait(timeout=5) == 0, "shutdown + exit terminan con código 0")

if fallos:
    print(f"❌ {len(fallos)} comprobación(es) fallaron")
    sys.exit(1)
print("✅ Servidor de lenguaje: todo correcto")
