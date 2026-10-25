/*
 * 🐆 Lince — revisor.h
 * Revisa un programa sin ejecutarlo y avisa de lo que casi seguro es un
 * descuido: un nombre que no existe (una errata), una variable que no se usa,
 * código que no se ejecuta nunca, un valor del tipo equivocado, una llamada
 * con los argumentos que no son o un campo que la clase no tiene.
 *
 * Son avisos, no errores: el programa se ejecuta igual. Los da
 * 'lince comprobar' y, con él, el editor (LSP).
 */
#ifndef LINCE_REVISOR_H
#define LINCE_REVISOR_H

#include "parser.h"

/* 'ruta' es el archivo de 'programa' (para seguir sus 'importar "./x"'), o NULL */
void revisar(Nodo *programa, const char *ruta, void (*avisar)(int linea, const char *mensaje));

#endif
