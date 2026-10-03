/*
 * 🐆 Lince — compilador.h
 */

#ifndef LINCE_COMPILADOR_H
#define LINCE_COMPILADOR_H

#include "parser.h"

/*
 * Compila el AST a código C y luego a binario nativo.
 * ruta_c   — ruta del archivo .c intermedio generado
 * ruta_bin — ruta del binario de salida
 * Devuelve 0 en éxito, 1 en error.
 */
int compilador_compilar(Nodo *ast, const char *ruta_c, const char *ruta_bin);

/*
 * Expande los 'importar "./x"' de archivos locales: sustituye cada nodo de
 * importación por las sentencias del archivo importado, recursivamente. Hay
 * que llamarlo ANTES de compilar — el resto del compilador no distingue
 * entonces entre lo que venía en el archivo y lo que llegó importado.
 * ruta_fuente — el .lince que se está compilando, para resolver las rutas
 *               relativas junto a él, igual que hace el intérprete.
 * Devuelve 0 en éxito, 1 si algún import no se pudo resolver.
 */
int compilador_expandir_imports(Nodo *ast, const char *ruta_fuente);

#endif /* LINCE_COMPILADOR_H */
