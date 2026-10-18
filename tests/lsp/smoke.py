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
       'funcion doble(val numero n): numero {\n    devolver n * 2\n}\n'
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
    open(os.path.join(d_, "lib.lince"), "w").write("funcion suma(val numero a, val numero b): numero {\n    devolver a + b\n}\nsea PI2 = 6\n")
    u2 = "file://" + os.path.join(d_, "main.lince")
    t2 = 'importar "./lib" como mi_lib\nmi_lib.\n'
    send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {"uri": u2, "text": t2}}})
    recv()
    send({"jsonrpc": "2.0", "id": 7, "method": "textDocument/completion",
          "params": {"textDocument": {"uri": u2}, "position": {"line": 1, "character": 7}}})
    r = etiquetas(recv()["result"])
    comprobar("suma" in r and "PI2" in r, "tras 'alias.' salen los nombres de la librería importada")

# ── cierre ordenado ──
send({"jsonrpc": "2.0", "id": 99, "method": "shutdown"})
recv()
send({"jsonrpc": "2.0", "method": "exit"})
comprobar(p.wait(timeout=5) == 0, "shutdown + exit terminan con código 0")

if fallos:
    print(f"❌ {len(fallos)} comprobación(es) fallaron")
    sys.exit(1)
print("✅ Servidor de lenguaje: todo correcto")
