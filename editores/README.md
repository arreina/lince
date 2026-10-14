# 🐆 Lince — Soporte para editores

Resaltado, snippets y, con un editor que hable LSP, errores en vivo y
autocompletado.

## Qué ofrece cada editor

| Editor        | Resaltado | Snippets | Errores en vivo | Autocompletado | Ir a definición |
|---------------|:---------:|:--------:|:---------------:|:--------------:|:---------------:|
| VS Code       | ✅ | ✅ | ✅ (extensión) | ✅ (extensión) | ✅ |
| Sublime Text  | ✅ | ✅ | ✅ (con LSP)   | ✅ (con LSP)   | ✅ (con LSP) |
| Neovim, Helix, Emacs, Zed… | con su gramática | — | ✅ (LSP) | ✅ (LSP) | ✅ (LSP) |
| Notepad++     | ✅ | ❌ | ❌ | ❌ | ❌ |

Todo lo que no es resaltado lo da el **servidor de lenguaje** de Lince:
`lince lsp`. Habla el Language Server Protocol por la entrada y salida
estándar, no depende de nada, y lo entiende cualquier editor con cliente LSP.

## El servidor de lenguaje

```
lince lsp
```

- **Errores en vivo:** el error de sintaxis se subraya mientras se escribe.
- **Autocompletado:** palabras clave, funciones integradas, módulos, lo que
  define el propio archivo; tras `modulo.` salen las funciones de ese módulo
  estándar, tras `mi.` los métodos y campos de la clase, y tras `alias.` lo que
  define la librería importada con `importar "./x" como alias`.
- **Símbolos del documento** e **ir a la definición**.

`lince comprobar archivo.lince` hace la misma comprobación de sintaxis desde la
línea de comandos, sin ejecutar el programa.

### Configurarlo en otros editores

**Neovim** (0.8+):

```lua
vim.filetype.add({ extension = { lince = "lince" } })
vim.api.nvim_create_autocmd("FileType", {
  pattern = "lince",
  callback = function()
    vim.lsp.start({ name = "lince", cmd = { "lince", "lsp" } })
  end,
})
```

**Helix** (`~/.config/helix/languages.toml`):

```toml
[language-server.lince]
command = "lince"
args = ["lsp"]

[[language]]
name = "lince"
scope = "source.lince"
file-types = ["lince"]
comment-token = "#"
language-servers = ["lince"]
```

**Sublime Text** con el paquete *LSP*: en *Preferences → Package Settings → LSP
→ Settings*:

```json
{ "clients": { "lince": {
    "enabled": true,
    "command": ["lince", "lsp"],
    "selector": "source.lince"
} } }
```

**Emacs** (eglot):

```elisp
(add-to-list 'auto-mode-alist '("\\.lince\\'" . prog-mode))
(add-to-list 'eglot-server-programs '(prog-mode . ("lince" "lsp")))
```

## Instalación del resaltado y los snippets

### VS Code

Lo más cómodo es el `.vsix` (trae también el cliente del servidor de lenguaje):

```
make vsix                                        # crea build/lince-language-X.Y.Z.vsix
code --install-extension build/lince-language-*.vsix
```

Sin `.vsix`: copia `vscode/lince-language/` a `~/.vscode/extensions/`.
Si `lince` no está en el `PATH`, indica su ruta en el ajuste `lince.ruta`.

### Sublime Text

Copia `sublime-text/*.tmLanguage.json` y `*.sublime-completions` a una carpeta
`Lince` dentro de Packages y reinicia.

### Notepad++

Importa `notepad++/lince-notepad++.xml` desde **Lenguaje → Definir tu propio
lenguaje → Importar**.

## Para quien toque el lenguaje

La gramática, los snippets y las listas de palabras **no se editan a mano**: se
generan del propio lenguaje con `make editores` (lee `src/lexer.c`,
`src/modulos.c` y `src/interprete.c`). La CI ejecuta `make editores-check`, que
falla si el lenguaje cambió y no se regeneró. Si se añade una palabra reservada
nueva, el generador se detiene hasta que se clasifique en
`editores/generar.py`.

Los tests viven en `tests/lsp/` (servidor), y `tests/editores/` (cada snippet es
Lince válido, y la extensión de VS Code habla bien con el servidor). Todo corre
con `make test-lsp test-editores`.
