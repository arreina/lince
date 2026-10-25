/*
 * 🐆 Lince — formatear.h
 * Deja un programa con el estilo de siempre: sangría de 4 espacios según las
 * llaves, un espacio alrededor de los operadores y tras las comas, sin
 * espacios al final de las líneas ni más de dos líneas en blanco seguidas.
 *
 * No cambia nada más: ni los saltos de línea, ni los textos, ni los
 * comentarios. Antes de dar el resultado lo vuelve a leer y comprueba que el
 * programa es el mismo, token a token.
 */
#ifndef LINCE_FORMATEAR_H
#define LINCE_FORMATEAR_H

/* El código formateado (hay que liberarlo), o NULL si no se pudo hacer sin
   cambiar el programa. 'codigo' tiene que estar bien escrito: con un error de
   sintaxis el lexer termina el proceso, como al ejecutar. */
char *formatear_codigo(const char *codigo);

#endif
