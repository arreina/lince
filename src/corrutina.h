/*
 * 🐆 Lince — corrutina.h
 * Código que se puede parar a mitad y seguir más tarde donde se quedó: lo que
 * hay debajo de las tareas (tarea ... { esperar(1) }).
 *
 * El intérprete recorre el árbol con funciones de C que se llaman unas a
 * otras, así que lo que hay a medio hacer está en la pila de C. Cada
 * corrutina tiene la suya propia, y cambiar de corrutina es cambiar de pila:
 * en Linux con ucontext y en Windows con fibras. Solo corre una cada vez y
 * siempre en el mismo hilo.
 *
 *   corrutina_entrar(c)  sigue c hasta que llame a corrutina_salir(c) o acabe
 *   corrutina_salir(c)   desde dentro de c: vuelve a quien la hizo entrar
 */
#ifndef LINCE_CORRUTINA_H
#define LINCE_CORRUTINA_H

#include <stdlib.h>

typedef struct Corrutina Corrutina;

/* Una corrutina que, la primera vez que se entre en ella, llama a fn(arg).
   NULL si no hay memoria para su pila. */
Corrutina *corrutina_crear(void (*fn)(void *), void *arg);
void       corrutina_entrar(Corrutina *c);
void       corrutina_salir(Corrutina *c);
int        corrutina_terminada(Corrutina *c);
/* Solo una que ha terminado o en la que no se ha entrado nunca */
void       corrutina_destruir(Corrutina *c);

/* Segundos de un reloj que no se atrasa ni salta, y dormir: lo que necesitan
   las tareas fuera de un juego */
double     corrutina_reloj(void);
void       corrutina_dormir(double segundos);

#endif /* LINCE_CORRUTINA_H */
