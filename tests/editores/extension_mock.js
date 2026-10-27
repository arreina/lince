// Prueba la extensión de VS Code sin VS Code: se sustituye el módulo 'vscode'
// por uno de mentira y se la hace hablar con el 'lince lsp' de verdad.
//
//   node tests/editores/extension_mock.js ./lince
'use strict';
const Module = require('module');
const path = require('path');

const lince = path.resolve(process.argv[2] || './lince');
const registro = { completado: null, definicion: null, simbolos: null, ayuda: null, formato: null, abrir: null, diag: new Map() };

const vscode = {
    Uri: { parse: (s) => ({ toString: () => s, _s: s }) },
    Range: class { constructor(a, b, c, d) { this.start = { line: a, character: b }; this.end = { line: c, character: d }; } },
    Location: class { constructor(u, r) { this.uri = u; this.range = r; } },
    Diagnostic: class { constructor(r, m, s) { this.range = r; this.message = m; this.severity = s; } },
    DiagnosticSeverity: { Error: 0 },
    CompletionItem: class { constructor(l, k) { this.label = l; this.kind = k; } },
    SymbolInformation: class { constructor(n, k, c, l) { this.name = n; this.kind = k; this.location = l; } },
    Hover: class { constructor(c) { this.contents = c; } },
    MarkdownString: class { constructor(v) { this.value = v; } },
    TextEdit: { replace: (r, t) => ({ range: r, newText: t }) },
    window: { showWarningMessage: (m) => { registro.aviso = m; } },
    workspace: {
        textDocuments: [],
        getConfiguration: () => ({ get: () => lince }),
        onDidOpenTextDocument: (f) => { registro.abrir = f; return {}; }, onDidChangeTextDocument: () => ({}),
        onDidCloseTextDocument: () => ({}), onDidChangeConfiguration: () => ({}),
    },
    languages: {
        createDiagnosticCollection: () => ({
            set: (u, d) => registro.diag.set(u.toString(), d), delete: (u) => registro.diag.delete(u.toString()),
        }),
        registerCompletionItemProvider: (s, p) => { registro.completado = p; return {}; },
        registerDefinitionProvider: (s, p) => { registro.definicion = p; return {}; },
        registerDocumentSymbolProvider: (s, p) => { registro.simbolos = p; return {}; },
        registerHoverProvider: (s, p) => { registro.ayuda = p; return {}; },
        registerDocumentFormattingEditProvider: (s, p) => { registro.formato = p; return {}; },
    },
};

const original = Module._load;
Module._load = function (req, ...r) { return req === 'vscode' ? vscode : original.call(this, req, ...r); };

const ext = require(path.resolve(__dirname, '../../editores/vscode/lince-language/extension.js'));
Module._load = original;

let fallos = 0;
const ok = (c, t) => { console.log((c ? '  ✅ ' : '  ❌ ') + t); if (!c) fallos++; };
const espera = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
    const texto = 'funcion doble(numero n): numero {\n    devolver n * 2\n}\nsea r = doble(2)\nsi r > {\n}\n';
    const doc = {
        languageId: 'lince', version: 1, uri: vscode.Uri.parse('file:///tmp/ext_prueba.lince'),
        getText: () => texto,
    };
    vscode.workspace.textDocuments.push(doc);
    ext.activate({ subscriptions: [] });
    await espera(600);

    // El documento ya estaba abierto: el cliente lo anuncia al arrancar
    await espera(600);
    const d = registro.diag.get('file:///tmp/ext_prueba.lince');
    ok(d && d.length === 1 && d[0].message.length > 0, 'el error de sintaxis llega como diagnóstico');

    const c = await registro.completado.provideCompletionItems(doc, { line: 3, character: 0 });
    ok(c.some((i) => i.label === 'doble') && c.some((i) => i.label === 'privado'),
       'el autocompletado trae lo definido y las palabras clave');

    const s = await registro.simbolos.provideDocumentSymbols(doc);
    ok(s.some((x) => x.name === 'doble'), 'los símbolos del documento incluyen la función');

    const def = await registro.definicion.provideDefinition(doc, { line: 3, character: 10 });
    ok(def && def.range.start.line === 0, 'ir a la definición lleva a la declaración');

    // Otro documento, bien escrito pero sin estilo: ayuda y formatear
    const texto2 = '# Cuánto suma.\nfuncion sumar(numero a,numero b):numero{devolver a+b}\nescribir(sumar(1,2))\n';
    const doc2 = { languageId: 'lince', version: 1, uri: vscode.Uri.parse('file:///tmp/ext_prueba2.lince'), getText: () => texto2 };
    registro.abrir(doc2);
    await espera(600);
    const h = await registro.ayuda.provideHover(doc2, { line: 2, character: 11 });
    ok(h && h.contents.value.includes('Cuánto suma.'), 'al pasar el ratón sale la ayuda de los comentarios');
    const f = await registro.formato.provideDocumentFormattingEdits(doc2);
    ok(f.length === 1 && f[0].newText.includes('funcion sumar(numero a, numero b): numero { devolver a + b }'),
       'formatear deja el documento con el estilo de Lince');

    ext.deactivate();
    await espera(700);
    console.log(fallos ? `❌ ${fallos} comprobación(es) fallaron` : '✅ La extensión de VS Code habla bien con el servidor');
    process.exit(fallos ? 1 : 0);
})();
