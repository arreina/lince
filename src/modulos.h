/*
 * 🐆 Lince — modulos.h
 * Módulos estándar del lenguaje
 */

#ifndef LINCE_MODULOS_H
#define LINCE_MODULOS_H

#include "interprete.h"

void modulo_cargar(const char *nombre, Entorno *entorno);

/* ─────────────────────────────────────────
   Módulos de anfitrión

   Un programa que empotre el intérprete puede añadir módulos que el lenguaje
   no trae. Es lo que necesita, por ejemplo, un motor de videojuegos para
   exponer su API como 'importar "motor"' sin que el lenguaje tenga que saber
   que ese motor existe.

   El orden es: módulos estándar primero, y si el nombre no es ninguno, el
   gancho de anfitrión antes de dar error.
───────────────────────────────────────── */

/* Función nativa de un módulo: recibe los argumentos y devuelve un Valor.
   Los argumentos son prestados — el intérprete los libera al volver. */
typedef Valor *(*FnNativa)(Valor **args, int num_args);

/* Una entrada de la tabla de funciones de un módulo.
   num_args: cuántos espera, o -1 si es variable. */
typedef struct {
    char     *nombre;
    FnNativa  fn;
    int       num_args;
} FuncionNativa;

/* Registra un módulo en el entorno, accesible como nombre.funcion(). */
void modulo_registrar(Entorno *entorno, const char *nombre,
                      FuncionNativa *fns, int num_fns);

/* Gancho de anfitrión. Devuelve 1 si ha reconocido y registrado el módulo,
   0 si no lo conoce (entonces el intérprete da el error de siempre). */
typedef int (*ModuloExterno)(const char *nombre, Entorno *entorno);
void modulo_fijar_externo(ModuloExterno fn);

#endif /* LINCE_MODULOS_H */
