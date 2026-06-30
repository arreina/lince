# 🐆 Lince — Extensión para VSCode

Resaltado de sintaxis, autocompletado y snippets para el lenguaje Lince.

## Instalación manual

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

## Snippets disponibles

| Prefijo    | Descripción                    |
|------------|--------------------------------|
| `fun`      | función con parámetros         |
| `fun0`     | función sin parámetros         |
| `sea`      | variable mutable               |
| `fijo`     | constante                      |
| `lista`    | lista tipada                   |
| `dic`      | diccionario tipado             |
| `si`       | condicional si                 |
| `sino`     | condicional si/sino            |
| `para`     | bucle para clásico             |
| `cada`     | bucle para cada                |
| `mientras` | bucle mientras                 |
| `hacer`    | bucle hacer...mientras         |
| `clase`    | definición de clase            |
| `clasex`   | clase con herencia             |
| `intentar` | bloque intentar/capturar       |
| `importar` | importar módulo estándar       |
| `esc`      | escribir()                     |
