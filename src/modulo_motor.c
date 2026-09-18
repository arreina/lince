/*
 * 🐆 Lince — modulo_motor.c
 *
 * Implementación del módulo nativo `motor`: envuelve la API de
 * lince-motor (include/motor.h, libmotor.a) como funciones Lince.
 *
 * Solo se compila con el build opt-in `make motor` (ver Makefile,
 * LINCE_MOTOR). El `lince` normal no incluye este fichero ni depende de
 * SDL2.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "modulo_motor.h"
#include "motor.h"

#define MOTOR_MAX_TEXTURAS 256

static MotorTextura *_texturas[MOTOR_MAX_TEXTURAS];

/* Entorno del script en el momento de registrar el módulo — necesario
 * para invocar el callback de motor.al_actualizar desde C. Mismo truco
 * que _srv_ent_tmp en el módulo servidor. */
static Entorno *_motor_entorno_tmp;

/* Callback retenido de motor.al_actualizar */
static Valor   *_motor_cb_fn;
static Entorno *_motor_cb_ent;

void modulo_motor_fijar_entorno(Entorno *entorno)
{
    _motor_entorno_tmp = entorno;
}

/* ── Utilidades internas ───────────────────────────────────────────── */

static Valor *_error(const char *msg)
{
    hay_error   = 1;
    valor_error = valor_crear_error("ErrorTipo", msg, 0);
    return valor_nulo();
}

static Valor *_dic2(const char *k1, Valor *v1, const char *k2, Valor *v2)
{
    Valor *d = valor_diccionario_crear();
    d->diccionario.claves[0]  = strdup(k1);
    d->diccionario.valores[0] = v1;
    d->diccionario.claves[1]  = strdup(k2);
    d->diccionario.valores[1] = v2;
    d->diccionario.cantidad   = 2;
    return d;
}

static int _textura_slot_libre(void)
{
    for (int i = 0; i < MOTOR_MAX_TEXTURAS; i++)
        if (!_texturas[i]) return i;
    return -1;
}

/* Valida un handle de textura (numero = indice). NULL si no es valido. */
static MotorTextura *_textura_de_handle(Valor *v)
{
    if (v->tipo != VAL_NUMERO) return NULL;
    int h = (int)v->numero;
    if (h < 0 || h >= MOTOR_MAX_TEXTURAS) return NULL;
    return _texturas[h];
}

/* ── Ciclo de vida y bucle ──────────────────────────────────────────── */

Valor *fn_motor_iniciar(Valor **a, int n)
{
    if (n < 2 || a[0]->tipo != VAL_NUMERO || a[1]->tipo != VAL_NUMERO)
        return _error("motor.iniciar(ancho, alto, [titulo]) espera dos numeros");

    const char *titulo = (n >= 3 && a[2]->tipo == VAL_TEXTO)
        ? a[2]->texto : "Lince Motor";

    return valor_booleano(motor_iniciar((int)a[0]->numero, (int)a[1]->numero, titulo));
}

Valor *fn_motor_terminar(Valor **a, int n)
{
    (void)a; (void)n;

    /* Las texturas vivas, antes de bajar el motor. motor_terminar() se lleva
     * por delante el renderer y con el toda SDL_Texture creada en el, pero no
     * los MotorTextura que las envuelven: se quedarian aqui con un ->sdl
     * colgante. Si el script volviera a llamar a motor.iniciar() y usara un
     * handle viejo, ese puntero muerto acabaria en SDL.
     *
     * Hay que destruirlas con el motor aun iniciado: es la unica ventana en
     * la que motor_textura_destruir libera de verdad la SDL_Texture (ver el
     * comentario de esa funcion en lince-motor/src/motor/textura.c).
     *
     * El callback retenido por al_actualizar se queda: liberarlo aqui seria
     * un use-after-free si el script llama a motor.terminar() desde dentro
     * del propio callback, y es un unico Valor, no algo que crezca. */
    for (int i = 0; i < MOTOR_MAX_TEXTURAS; i++) {
        if (_texturas[i]) {
            motor_textura_destruir(_texturas[i]);
            _texturas[i] = NULL;
        }
    }

    motor_terminar();
    return valor_nulo();
}

Valor *fn_motor_error(Valor **a, int n)
{
    (void)a; (void)n;
    return valor_texto(motor_error());
}

Valor *fn_motor_al_actualizar(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_FUNCION)
        return _error("motor.al_actualizar(fn) espera una funcion");

    if (_motor_cb_fn) valor_destruir(_motor_cb_fn);

    a[0]->refs++;
    _motor_cb_fn  = a[0];
    _motor_cb_ent = _motor_entorno_tmp;
    return valor_nulo();
}

/* Puente C -> Lince: lo que le pasamos a motor_al_actualizar. Se llama una
 * vez por frame desde el bucle bloqueante de motor_correr(). */
static void _puente_actualizar(double delta, void *usuario)
{
    (void)usuario;
    if (!_motor_cb_fn) return;

    /* Un error de Lince en el callback deja hay_error a 1 y el interprete se
     * salta el cuerpo en los frames siguientes. Sin esto el bucle seguiria
     * girando para siempre: ventana congelada, ni un mensaje mas, y
     * motor.correr() sin volver nunca — ni siquiera se ejecuta el
     * motor.terminar() de despues. Paramos el bucle y dejamos que el error
     * suba por donde sube cualquier otro. */
    if (hay_error) { motor_parar(); return; }

    /* interprete_llamar_funcion se queda con la propiedad de args[i]: los
     * que adopta el entorno local de la funcion los libera al destruirlo, y
     * los sobrantes los suelta ella misma. No hay que (ni se puede)
     * destruir args[0] aqui: seria un doble free. */
    Valor *args[1] = { valor_numero(delta) };
    Valor *r = interprete_llamar_funcion(_motor_cb_fn, args, 1, _motor_cb_ent);
    if (r) valor_destruir(r);

    if (hay_error) motor_parar();
}

Valor *fn_motor_correr(Valor **a, int n)
{
    (void)a; (void)n;
    motor_al_actualizar(_puente_actualizar, NULL);
    motor_correr();
    return valor_nulo();
}

Valor *fn_motor_parar(Valor **a, int n)
{
    (void)a; (void)n;
    motor_parar();
    return valor_nulo();
}

/* ── Tiempo ─────────────────────────────────────────────────────────── */

Valor *fn_motor_delta(Valor **a, int n)  { (void)a; (void)n; return valor_numero(motor_delta()); }
Valor *fn_motor_tiempo(Valor **a, int n) { (void)a; (void)n; return valor_numero(motor_tiempo()); }
Valor *fn_motor_fps(Valor **a, int n)    { (void)a; (void)n; return valor_numero(motor_fps()); }

Valor *fn_motor_fijar_limite_fps(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_NUMERO)
        return _error("motor.fijar_limite_fps(fps) espera un numero");
    motor_fijar_limite_fps((int)a[0]->numero);
    return valor_nulo();
}

/* ── Entrada ────────────────────────────────────────────────────────── */

Valor *fn_motor_tecla_pulsada(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_TEXTO)
        return _error("motor.tecla_pulsada(nombre) espera un texto");
    return valor_booleano(motor_tecla_pulsada(a[0]->texto));
}

Valor *fn_motor_tecla_recien_pulsada(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_TEXTO)
        return _error("motor.tecla_recien_pulsada(nombre) espera un texto");
    return valor_booleano(motor_tecla_recien_pulsada(a[0]->texto));
}

Valor *fn_motor_raton_posicion(Valor **a, int n)
{
    (void)a; (void)n;
    int x, y;
    motor_raton_posicion(&x, &y);
    return _dic2("x", valor_numero(x), "y", valor_numero(y));
}

Valor *fn_motor_raton_pulsado(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_NUMERO)
        return _error("motor.raton_pulsado(boton) espera un numero");
    return valor_booleano(motor_raton_pulsado((int)a[0]->numero));
}

/* ── Ventana ────────────────────────────────────────────────────────── */

Valor *fn_motor_ventana_tamano(Valor **a, int n)
{
    (void)a; (void)n;
    int ancho, alto;
    motor_ventana_tamano(&ancho, &alto);
    return _dic2("ancho", valor_numero(ancho), "alto", valor_numero(alto));
}

Valor *fn_motor_ventana_titulo(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_TEXTO)
        return _error("motor.ventana_titulo(titulo) espera un texto");
    motor_ventana_titulo(a[0]->texto);
    return valor_nulo();
}

Valor *fn_motor_fondo(Valor **a, int n)
{
    if (n < 3 || a[0]->tipo != VAL_NUMERO || a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_NUMERO)
        return _error("motor.fondo(r, g, b) espera tres numeros");
    motor_fondo((uint8_t)a[0]->numero, (uint8_t)a[1]->numero, (uint8_t)a[2]->numero);
    return valor_nulo();
}

/* ── Texturas y sprites ─────────────────────────────────────────────── */

Valor *fn_motor_textura_cargar(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_TEXTO)
        return _error("motor.textura_cargar(ruta) espera un texto");

    int slot = _textura_slot_libre();
    if (slot < 0)
        return _error("motor.textura_cargar: no quedan huecos de textura libres");

    MotorTextura *t = motor_textura_cargar(a[0]->texto);
    if (!t) return valor_nulo();

    _texturas[slot] = t;
    return valor_numero(slot);
}

Valor *fn_motor_textura_destruir(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_NUMERO)
        return _error("motor.textura_destruir(handle) espera un numero");

    int h = (int)a[0]->numero;
    if (h >= 0 && h < MOTOR_MAX_TEXTURAS && _texturas[h]) {
        motor_textura_destruir(_texturas[h]);
        _texturas[h] = NULL;
    }
    return valor_nulo();
}

Valor *fn_motor_textura_tamano(Valor **a, int n)
{
    if (n < 1) return _error("motor.textura_tamano(handle) espera un handle");

    MotorTextura *t = _textura_de_handle(a[0]);
    if (!t) return _error("motor.textura_tamano: handle invalido");

    int ancho, alto;
    motor_textura_tamano(t, &ancho, &alto);
    return _dic2("ancho", valor_numero(ancho), "alto", valor_numero(alto));
}

Valor *fn_motor_dibujar_textura(Valor **a, int n)
{
    if (n < 3) return _error("motor.dibujar_textura(handle, x, y) espera 3 argumentos");

    MotorTextura *t = _textura_de_handle(a[0]);
    if (!t) return _error("motor.dibujar_textura: handle invalido");

    if (a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_NUMERO)
        return _error("motor.dibujar_textura(handle, x, y) espera numeros en x, y");

    motor_dibujar_textura(t, (float)a[1]->numero, (float)a[2]->numero);
    return valor_nulo();
}

Valor *fn_motor_dibujar_sprite(Valor **a, int n)
{
    /* handle, x, y, recorte_x, recorte_y, recorte_ancho, recorte_alto,
       escala, rotacion, voltear_h, voltear_v */
    if (n < 11)
        return _error("motor.dibujar_sprite espera 11 argumentos: handle, x, y, "
                       "recorte_x, recorte_y, recorte_ancho, recorte_alto, escala, "
                       "rotacion, voltear_h, voltear_v");

    MotorTextura *t = _textura_de_handle(a[0]);
    if (!t) return _error("motor.dibujar_sprite: handle invalido");

    for (int i = 1; i <= 8; i++) {
        if (a[i]->tipo != VAL_NUMERO)
            return _error("motor.dibujar_sprite: argumento numerico invalido");
    }

    float  x             = (float)a[1]->numero;
    float  y             = (float)a[2]->numero;
    int    recorte_x     = (int)a[3]->numero;
    int    recorte_y     = (int)a[4]->numero;
    int    recorte_ancho = (int)a[5]->numero;
    int    recorte_alto  = (int)a[6]->numero;
    float  escala        = (float)a[7]->numero;
    double rotacion      = a[8]->numero;
    bool   voltear_h     = a[9]->tipo  == VAL_BOOLEANO && a[9]->booleano;
    bool   voltear_v     = a[10]->tipo == VAL_BOOLEANO && a[10]->booleano;

    motor_dibujar_sprite(t, x, y, recorte_x, recorte_y, recorte_ancho, recorte_alto,
                          escala, rotacion, voltear_h, voltear_v);
    return valor_nulo();
}
