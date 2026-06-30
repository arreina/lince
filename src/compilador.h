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

#endif /* LINCE_COMPILADOR_H */
