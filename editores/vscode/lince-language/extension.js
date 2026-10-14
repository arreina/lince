// 🐆 Lince — extensión de VS Code
//
// Cliente mínimo del servidor de lenguaje de Lince ('lince lsp'). No usa
// ninguna dependencia: habla el protocolo por la entrada y salida estándar
// del proceso, y con eso da errores de sintaxis mientras se escribe,
// autocompletado, símbolos del documento e "ir a la definición".
//
// El resaltado, los snippets y la configuración del lenguaje no pasan por aquí:
// los declara package.json y funcionan aunque 'lince' no esté instalado.
'use strict';

const vscode = require('vscode');
const cp = require('child_process');

let proceso = null;
let buffer = Buffer.alloc(0);
let siguienteId = 1;
const pendientes = new Map();
let diagnosticos = null;
let avisado = false;
const versiones = new Map();       // uri → temporizador de envío

// ── transporte ──────────────────────────────
function enviar(obj) {
    if (!proceso || !proceso.stdin.writable) return;
    const cuerpo = Buffer.from(JSON.stringify(obj), 'utf8');
    proceso.stdin.write(`Content-Length: ${cuerpo.length}\r\n\r\n`);
    proceso.stdin.write(cuerpo);
}

function pedir(metodo, params) {
    return new Promise((resolve) => {
        if (!proceso) return resolve(null);
        const id = siguienteId++;
        pendientes.set(id, resolve);
        enviar({ jsonrpc: '2.0', id, method: metodo, params });
        setTimeout(() => { if (pendientes.delete(id)) resolve(null); }, 5000);
    });
}

function notificar(metodo, params) {
    enviar({ jsonrpc: '2.0', method: metodo, params });
}

function alRecibir(trozo) {
    buffer = Buffer.concat([buffer, trozo]);
    for (;;) {
        const fin = buffer.indexOf('\r\n\r\n');
        if (fin < 0) return;
        const m = /Content-Length:\s*(\d+)/i.exec(buffer.slice(0, fin).toString('ascii'));
        if (!m) { buffer = buffer.slice(fin + 4); continue; }
        const largo = parseInt(m[1], 10);
        if (buffer.length < fin + 4 + largo) return;
        const msg = JSON.parse(buffer.slice(fin + 4, fin + 4 + largo).toString('utf8'));
        buffer = buffer.slice(fin + 4 + largo);
        atender(msg);
    }
}

function atender(msg) {
    if (msg.id !== undefined && !msg.method) {
        const r = pendientes.get(msg.id);
        if (r) { pendientes.delete(msg.id); r(msg.result === undefined ? null : msg.result); }
    } else if (msg.method === 'textDocument/publishDiagnostics') {
        const uri = vscode.Uri.parse(msg.params.uri);
        diagnosticos.set(uri, msg.params.diagnostics.map((d) => {
            const rango = new vscode.Range(d.range.start.line, d.range.start.character,
                                           d.range.end.line, d.range.end.character);
            const dg = new vscode.Diagnostic(rango, d.message, vscode.DiagnosticSeverity.Error);
            dg.source = d.source || 'lince';
            return dg;
        }));
    }
}

// ── ciclo de vida del servidor ──────────────
function arrancar() {
    const ruta = vscode.workspace.getConfiguration('lince').get('ruta') || 'lince';
    proceso = cp.spawn(ruta, ['lsp'], { stdio: ['pipe', 'pipe', 'ignore'] });
    buffer = Buffer.alloc(0);
    proceso.stdout.on('data', alRecibir);
    proceso.on('error', () => {
        proceso = null;
        if (!avisado) {
            avisado = true;
            vscode.window.showWarningMessage(
                `No se encuentra '${ruta}'. El resaltado funciona, pero sin errores en vivo ni ` +
                `autocompletado. Instala Lince o indica su ruta en el ajuste «lince.ruta».`);
        }
    });
    proceso.on('exit', () => { proceso = null; });
    pedir('initialize', { processId: process.pid, rootUri: null, capabilities: {} })
        .then(() => {
            notificar('initialized', {});
            vscode.workspace.textDocuments.forEach(abrir);
        });
}

function parar() {
    if (!proceso) return;
    try { notificar('shutdown'); notificar('exit'); } catch (e) { /* ya cerrado */ }
    const p = proceso;
    proceso = null;
    setTimeout(() => { try { p.kill(); } catch (e) { /* nada */ } }, 500);
}

// ── sincronización de documentos ────────────
const esLince = (doc) => doc.languageId === 'lince';

function abrir(doc) {
    if (!esLince(doc)) return;
    notificar('textDocument/didOpen', { textDocument: {
        uri: doc.uri.toString(), languageId: 'lince', version: doc.version, text: doc.getText() } });
}

function cambiar(doc) {
    if (!esLince(doc)) return;
    const clave = doc.uri.toString();
    clearTimeout(versiones.get(clave));
    versiones.set(clave, setTimeout(() => {
        notificar('textDocument/didChange', {
            textDocument: { uri: clave, version: doc.version },
            contentChanges: [{ text: doc.getText() }] });
    }, 120));
}

function cerrar(doc) {
    if (!esLince(doc)) return;
    notificar('textDocument/didClose', { textDocument: { uri: doc.uri.toString() } });
    diagnosticos.delete(doc.uri);
}

// ── proveedores ─────────────────────────────
// LSP numera las clases de símbolo y de completado desde 1; VS Code, desde 0.
const aKindCompletado = (k) => (k || 1) - 1;
const aKindSimbolo = (k) => (k || 1) - 1;
const aRango = (r) => new vscode.Range(r.start.line, r.start.character, r.end.line, r.end.character);

// Si había un cambio sin enviar, se envía antes de preguntar
async function alDia(doc) {
    const t = versiones.get(doc.uri.toString());
    if (t) {
        clearTimeout(t);
        versiones.delete(doc.uri.toString());
        notificar('textDocument/didChange', {
            textDocument: { uri: doc.uri.toString(), version: doc.version },
            contentChanges: [{ text: doc.getText() }] });
    }
}

const proveedorCompletado = {
    async provideCompletionItems(doc, pos) {
        await alDia(doc);
        const r = await pedir('textDocument/completion', {
            textDocument: { uri: doc.uri.toString() }, position: { line: pos.line, character: pos.character } });
        return (r || []).map((i) => {
            const it = new vscode.CompletionItem(i.label, aKindCompletado(i.kind));
            return it;
        });
    },
};

const proveedorDefinicion = {
    async provideDefinition(doc, pos) {
        await alDia(doc);
        const r = await pedir('textDocument/definition', {
            textDocument: { uri: doc.uri.toString() }, position: { line: pos.line, character: pos.character } });
        if (!r) return null;
        return new vscode.Location(vscode.Uri.parse(r.uri), aRango(r.range));
    },
};

const proveedorSimbolos = {
    async provideDocumentSymbols(doc) {
        await alDia(doc);
        const r = await pedir('textDocument/documentSymbol', { textDocument: { uri: doc.uri.toString() } });
        return (r || []).map((s) => new vscode.SymbolInformation(
            s.name, aKindSimbolo(s.kind), '',
            new vscode.Location(vscode.Uri.parse(s.location.uri), aRango(s.location.range))));
    },
};

// ── activación ──────────────────────────────
function activate(context) {
    diagnosticos = vscode.languages.createDiagnosticCollection('lince');
    const sel = { language: 'lince' };
    context.subscriptions.push(
        diagnosticos,
        vscode.workspace.onDidOpenTextDocument(abrir),
        vscode.workspace.onDidChangeTextDocument((e) => cambiar(e.document)),
        vscode.workspace.onDidCloseTextDocument(cerrar),
        vscode.workspace.onDidChangeConfiguration((e) => {
            if (e.affectsConfiguration('lince.ruta')) { avisado = false; parar(); arrancar(); }
        }),
        vscode.languages.registerCompletionItemProvider(sel, proveedorCompletado, '.'),
        vscode.languages.registerDefinitionProvider(sel, proveedorDefinicion),
        vscode.languages.registerDocumentSymbolProvider(sel, proveedorSimbolos),
    );
    arrancar();
}

function deactivate() { parar(); }

module.exports = { activate, deactivate };
