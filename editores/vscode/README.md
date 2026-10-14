# 🐆 Lince — Extensión para VSCode

Resaltado de sintaxis, snippets, errores en vivo y autocompletado para el lenguaje Lince.

Los errores en vivo, el autocompletado y «ir a la definición» los da el servidor de
lenguaje de Lince (`lince lsp`): hace falta tener `lince` instalado. El resaltado y los
snippets funcionan sin él.

## Instalación

Desde la raíz del repositorio:

```
make vsix
code --install-extension build/lince-language-*.vsix
```

Si `lince` no está en el `PATH`, indica dónde está en el ajuste **`lince.ruta`**.

## Instalación manual (sin `.vsix`)

1. Copia la carpeta `lince-language` a:
   - **Linux/Mac**: `~/.vscode/extensions/`
   - **Windows**: `%USERPROFILE%\.vscode\extensions\`

2. Reinicia VSCode

3. Abre cualquier archivo `.lince` — el resaltado se activa automáticamente

## Lo que incluye

- ✅ Resaltado de palabras clave en español
- ✅ Resaltado de cadenas, números y comentarios
- ✅ Resaltado de clases, funciones y módulos
- ✅ Cierre automático de `{}`, `[]`, `()`
- ✅ Indentación automática
- ✅ Snippets para constructs comunes
- ✅ Errores de sintaxis subrayados mientras escribes
- ✅ Autocompletado (palabras clave, módulos, lo que defines, `mi.`, `alias.`)
- ✅ Símbolos del documento e ir a la definición

## Snippets

Hay 29 (`fun`, `clase`, `abstracta`, `interfaz`, `enumeracion`, `elegir`, `intentar`,
`importarc`…). Se generan de una sola fuente en `editores/generar.py`, y un test comprueba que
cada uno es Lince válido.
