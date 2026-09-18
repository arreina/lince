/*
 * 🐆 Lince — modulo_motor.h
 *
 * Binding del módulo nativo `motor` (lince-motor) hacia el intérprete.
 * Solo se compila e incluye cuando el build define LINCE_MOTOR — el
 * `lince` normal no sabe que esto existe ni depende de SDL2.
 *
 * Cada función tiene la firma de una función nativa de Lince:
 * Valor *(Valor **args, int num_args).
 */

#ifndef LINCE_MODULO_MOTOR_H
#define LINCE_MODULO_MOTOR_H

#include "interprete.h"

/* Fija el Entorno en el que se invocará el callback de motor.al_actualizar.
 * Debe llamarse justo antes de registrar el módulo, igual que _srv_ent_tmp
 * para el módulo servidor. */
void modulo_motor_fijar_entorno(Entorno *entorno);

/* Ciclo de vida y bucle */
Valor *fn_motor_iniciar(Valor **a, int n);
Valor *fn_motor_terminar(Valor **a, int n);
Valor *fn_motor_error(Valor **a, int n);
Valor *fn_motor_al_actualizar(Valor **a, int n);
Valor *fn_motor_correr(Valor **a, int n);
Valor *fn_motor_parar(Valor **a, int n);

/* Tiempo */
Valor *fn_motor_delta(Valor **a, int n);
Valor *fn_motor_tiempo(Valor **a, int n);
Valor *fn_motor_fps(Valor **a, int n);
Valor *fn_motor_fijar_limite_fps(Valor **a, int n);

/* Entrada */
Valor *fn_motor_tecla_pulsada(Valor **a, int n);
Valor *fn_motor_tecla_recien_pulsada(Valor **a, int n);
Valor *fn_motor_raton_posicion(Valor **a, int n);
Valor *fn_motor_raton_pulsado(Valor **a, int n);

/* Ventana */
Valor *fn_motor_ventana_tamano(Valor **a, int n);
Valor *fn_motor_ventana_titulo(Valor **a, int n);
Valor *fn_motor_fondo(Valor **a, int n);

/* Texturas y sprites */
Valor *fn_motor_textura_cargar(Valor **a, int n);
Valor *fn_motor_textura_destruir(Valor **a, int n);
Valor *fn_motor_textura_tamano(Valor **a, int n);
Valor *fn_motor_dibujar_textura(Valor **a, int n);
Valor *fn_motor_dibujar_sprite(Valor **a, int n);

#endif /* LINCE_MODULO_MOTOR_H */
