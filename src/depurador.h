/*
 * 🐆 Lince — depurador.h
 * 'lince depurar programa.lince': ejecuta paso a paso, con puntos de parada,
 * inspección de variables y pila de llamadas. Con --ide, para un IDE.
 */

#ifndef LINCE_DEPURADOR_H
#define LINCE_DEPURADOR_H

/* ide: 'lince depurar --ide', con respuestas en JSON para un programa */
int depurador_ejecutar(const char *archivo, int argc, char *argv[], int ide);

#endif /* LINCE_DEPURADOR_H */
