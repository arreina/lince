/*
 * 🐆 Lince — modulos.h
 * Módulos estándar del lenguaje
 */

#ifndef LINCE_MODULOS_H
#define LINCE_MODULOS_H

#include "interprete.h"

void modulo_cargar(const char *nombre, Entorno *entorno);

/* i-ésimo módulo estándar, o NULL al acabar (lo usan los editores). */
const char *modulo_estandar(int i);

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

/* ── JSON (módulo json y partidas guardadas) ──
   json_a_valor: NULL si el texto no es JSON válido. json_de_valor (malloc):
   con 'sangria' >= 0, en varias líneas. json_lo_que_no_cabe: NULL si el valor
   se puede guardar en JSON, o por qué no. */
Valor      *json_a_valor(const char *s);
char       *json_de_valor(Valor *v, int sangria);
const char *json_lo_que_no_cabe(Valor *v);

/* ── Métodos de un texto que son funciones del módulo texto ──
   t.dividir(",") es texto.dividir(t, ","). Devuelve NULL si 'met' no es uno de
   ellos; si lo es, el resultado (o el error). Los argumentos son prestados. */
Valor *modulo_metodo_texto(const char *met, Valor *texto, Valor **args, int num_args);

/* ── Lo que la 2.0 quitó ──
   Si 'nombre' del módulo 'mod' es algo de la 1.x que la 2.0 quitó (juego.cuerpo,
   texto.dividir...), cómo es ahora: «juego.cuerpo → juego.fisica.cuerpo». Si
   no, NULL. */
const char *modulo_quitado(Valor *mod, const char *nombre);

/* ── Recarga en caliente (módulo juego) ── */
int  juego_ventana_abierta(void);
void juego_preparar_recarga(int conserva);
int  juego_esperar_arreglo(const char *mensaje);

#endif /* LINCE_MODULOS_H */
