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

/* El handle que ve el script no es el hueco a secas, sino
 * generacion * MOTOR_MAX_TEXTURAS + hueco.
 *
 * Con el índice pelado, destruir una textura y cargar otra devolvía el mismo
 * número: quien siguiera guardando el handle viejo dibujaba la textura nueva
 * sin un solo aviso — el fondo donde iba el jugador. La generación sube cada
 * vez que se libera el hueco, así que un handle caduco deja de cuadrar y se
 * puede decir que no vale en lugar de aliasar en silencio.
 *
 * Los números de Lince son double: con 53 bits de mantisa caben 2^45
 * reutilizaciones de cada hueco antes de perder precisión. */
static MotorTextura *_texturas[MOTOR_MAX_TEXTURAS];
static long long     _generacion[MOTOR_MAX_TEXTURAS];

/* Callback retenido de motor.al_actualizar */
static Valor *_motor_cb_fn;

/* ── Utilidades internas ───────────────────────────────────────────── */

static Valor *_error(const char *tipo, const char *msg)
{
    hay_error   = 1;
    valor_error = valor_error_nuevo(tipo, msg, 0);
    return valor_nulo();
}

/* Un número de Lince como entero, rechazando lo que no quepa en el rango:
 * convertir un double fuera del rango del entero destino es comportamiento
 * indefinido, así que hay que descartarlo ANTES del cast, no después. */
static int _entero(Valor *v, int minimo, int maximo, int *salida)
{
    if (v->tipo != VAL_NUMERO) return 0;
    double d = v->numero;
    if (!(d >= (double)minimo && d <= (double)maximo)) return 0;
    *salida = (int)d;
    return 1;
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

/* Deshace el handle en su hueco comprobando la generación. -1 si no vale:
 * no es un número, no es entero, se sale de rango, la textura ya se destruyó
 * o el hueco se ha reutilizado desde entonces. */
static int _slot_de_handle(Valor *v)
{
    if (v->tipo != VAL_NUMERO) return -1;

    double d = v->numero;
    if (!(d >= 0.0 && d < 9007199254740992.0)) return -1;   /* 2^53 */
    long long h = (long long)d;
    if ((double)h != d) return -1;                          /* no era entero */

    int       hueco = (int)(h % MOTOR_MAX_TEXTURAS);
    long long gen   = h / MOTOR_MAX_TEXTURAS;

    if (_generacion[hueco] != gen) return -1;
    return _texturas[hueco] ? hueco : -1;
}

static MotorTextura *_textura_de_handle(Valor *v)
{
    int hueco = _slot_de_handle(v);
    return hueco < 0 ? NULL : _texturas[hueco];
}

/* Suelta una textura y quema la generación del hueco, para que los handles
 * que la apuntaran dejen de cuadrar. */
static void _soltar_textura(int hueco)
{
    motor_textura_destruir(_texturas[hueco]);
    _texturas[hueco] = NULL;
    _generacion[hueco]++;
}

/* ── Ciclo de vida y bucle ──────────────────────────────────────────── */

Valor *fn_motor_iniciar(Valor **a, int n)
{
    int ancho, alto;
    if (n < 2 || !_entero(a[0], 1, 16384, &ancho) || !_entero(a[1], 1, 16384, &alto))
        return _error("ErrorArgumento",
            "motor.iniciar(ancho, alto, [titulo]) espera dos numeros enteros entre 1 y 16384");

    const char *titulo = (n >= 3 && a[2]->tipo == VAL_TEXTO)
        ? a[2]->texto : "Lince Motor";

    return valor_booleano(motor_iniciar(ancho, alto, titulo));
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
     * comentario de esa funcion en lince-motor/src/motor/textura.c). */
    for (int i = 0; i < MOTOR_MAX_TEXTURAS; i++)
        if (_texturas[i]) _soltar_textura(i);

    /* Y el callback. Si no se suelta aqui, un motor.terminar() seguido de
     * motor.iniciar() y motor.correr() sin volver a registrar nada revive el
     * callback de la sesion anterior, que dibujara con los handles que este
     * terminar acaba de invalidar.
     *
     * Soltarlo es seguro aunque estemos dentro del propio callback:
     * interprete_llamar_funcion se queda su propia referencia mientras dura
     * la llamada, asi que este valor_destruir no puede ser el ultimo. */
    if (_motor_cb_fn) {
        valor_destruir(_motor_cb_fn);
        _motor_cb_fn = NULL;
    }
    motor_al_actualizar(NULL, NULL);

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
        return _error("ErrorTipo", "motor.al_actualizar(fn) espera una funcion");

    if (_motor_cb_fn) valor_destruir(_motor_cb_fn);

    a[0]->refs++;
    _motor_cb_fn = a[0];
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
    Valor *r = interprete_llamar_funcion(_motor_cb_fn, args, 1);
    if (r) valor_destruir(r);

    if (hay_error) motor_parar();
}

Valor *fn_motor_correr(Valor **a, int n)
{
    (void)a; (void)n;

    /* Sin callback, motor_correr() no ejecuta ni una linea de Lince: nadie
     * puede llamar a motor.parar() y el bucle solo termina si el usuario
     * cierra la ventana. Sin pantalla (SDL_VIDEODRIVER=dummy, que es como
     * corre el test de humo) no hay ventana que cerrar y se queda colgado
     * hasta que algo lo mate. Mejor decirlo. */
    if (!_motor_cb_fn)
        return _error("Error",
            "motor.correr() sin callback: llama antes a motor.al_actualizar(fn), "
            "o el bucle no ejecutara nada y no habra forma de pararlo");

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
    int fps;
    if (n < 1 || !_entero(a[0], 0, 1000, &fps))
        return _error("ErrorArgumento",
            "motor.fijar_limite_fps(fps) espera un entero entre 0 y 1000 (0 = sin limite)");
    motor_fijar_limite_fps(fps);
    return valor_nulo();
}

/* ── Entrada ────────────────────────────────────────────────────────── */

Valor *fn_motor_tecla_pulsada(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_TEXTO)
        return _error("ErrorTipo", "motor.tecla_pulsada(nombre) espera un texto");
    return valor_booleano(motor_tecla_pulsada(a[0]->texto));
}

Valor *fn_motor_tecla_recien_pulsada(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_TEXTO)
        return _error("ErrorTipo", "motor.tecla_recien_pulsada(nombre) espera un texto");
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
    int boton;
    if (n < 1 || !_entero(a[0], 1, 32, &boton))
        return _error("ErrorArgumento",
            "motor.raton_pulsado(boton) espera un entero entre 1 y 32 (1 izquierdo, 2 central, 3 derecho)");
    return valor_booleano(motor_raton_pulsado(boton));
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
        return _error("ErrorTipo", "motor.ventana_titulo(titulo) espera un texto");
    motor_ventana_titulo(a[0]->texto);
    return valor_nulo();
}

Valor *fn_motor_fondo(Valor **a, int n)
{
    int r, g, b;
    if (n < 3 || !_entero(a[0], 0, 255, &r) || !_entero(a[1], 0, 255, &g)
              || !_entero(a[2], 0, 255, &b))
        return _error("ErrorArgumento",
            "motor.fondo(r, g, b) espera tres enteros entre 0 y 255");
    motor_fondo((uint8_t)r, (uint8_t)g, (uint8_t)b);
    return valor_nulo();
}

/* ── Texturas y sprites ─────────────────────────────────────────────── */

Valor *fn_motor_textura_cargar(Valor **a, int n)
{
    if (n < 1 || a[0]->tipo != VAL_TEXTO)
        return _error("ErrorTipo", "motor.textura_cargar(ruta) espera un texto");

    int hueco = _textura_slot_libre();
    if (hueco < 0)
        return _error("Error", "motor.textura_cargar: no quedan huecos de textura libres");

    MotorTextura *t = motor_textura_cargar(a[0]->texto);
    if (!t) return valor_nulo();

    _texturas[hueco] = t;
    return valor_numero((double)(_generacion[hueco] * MOTOR_MAX_TEXTURAS + hueco));
}

Valor *fn_motor_textura_destruir(Valor **a, int n)
{
    if (n < 1) return _error("ErrorTipo", "motor.textura_destruir(handle) espera un handle");

    /* Pasa por la misma validacion que el resto: con el indice pelado, un
     * handle caduco destruia la textura que hubiera caido en ese hueco. */
    int hueco = _slot_de_handle(a[0]);
    if (hueco >= 0) _soltar_textura(hueco);
    return valor_nulo();
}

Valor *fn_motor_textura_tamano(Valor **a, int n)
{
    if (n < 1) return _error("ErrorTipo", "motor.textura_tamano(handle) espera un handle");

    MotorTextura *t = _textura_de_handle(a[0]);
    if (!t) return _error("ErrorArgumento",
            "motor.textura_tamano: handle invalido (destruido, o de antes de un motor.terminar())");

    int ancho, alto;
    motor_textura_tamano(t, &ancho, &alto);
    return _dic2("ancho", valor_numero(ancho), "alto", valor_numero(alto));
}

Valor *fn_motor_dibujar_textura(Valor **a, int n)
{
    if (n < 3) return _error("ErrorTipo", "motor.dibujar_textura(handle, x, y) espera 3 argumentos");

    MotorTextura *t = _textura_de_handle(a[0]);
    if (!t) return _error("ErrorArgumento",
            "motor.dibujar_textura: handle invalido (destruido, o de antes de un motor.terminar())");

    if (a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_NUMERO)
        return _error("ErrorTipo", "motor.dibujar_textura(handle, x, y) espera numeros en x, y");

    motor_dibujar_textura(t, (float)a[1]->numero, (float)a[2]->numero);
    return valor_nulo();
}

Valor *fn_motor_dibujar_sprite(Valor **a, int n)
{
    /* handle, x, y, recorte_x, recorte_y, recorte_ancho, recorte_alto,
       escala, rotacion, voltear_h, voltear_v */
    if (n < 11)
        return _error("ErrorTipo",
            "motor.dibujar_sprite espera 11 argumentos: handle, x, y, "
            "recorte_x, recorte_y, recorte_ancho, recorte_alto, escala, "
            "rotacion, voltear_h, voltear_v");

    MotorTextura *t = _textura_de_handle(a[0]);
    if (!t) return _error("ErrorArgumento",
            "motor.dibujar_sprite: handle invalido (destruido, o de antes de un motor.terminar())");

    for (int i = 1; i <= 8; i++) {
        if (a[i]->tipo != VAL_NUMERO)
            return _error("ErrorTipo", "motor.dibujar_sprite: argumento numerico invalido");
    }
    /* Los dos volteos tambien se comprueban. Antes cualquier cosa que no
     * fuera booleana se tomaba como 'falso' sin decir nada, asi que escribir
     * 1 y 0 — lo natural cuando los otros ocho argumentos son numeros —
     * dibujaba sin voltear, mientras el mismo despiste en el argumento 8 si
     * daba error. Un solo contrato por firma. */
    for (int i = 9; i <= 10; i++) {
        if (a[i]->tipo != VAL_BOOLEANO)
            return _error("ErrorTipo",
                "motor.dibujar_sprite: voltear_h y voltear_v esperan verdadero o falso");
    }

    float  x             = (float)a[1]->numero;
    float  y             = (float)a[2]->numero;
    int    recorte_x     = (int)a[3]->numero;
    int    recorte_y     = (int)a[4]->numero;
    int    recorte_ancho = (int)a[5]->numero;
    int    recorte_alto  = (int)a[6]->numero;
    float  escala        = (float)a[7]->numero;
    double rotacion      = a[8]->numero;
    bool   voltear_h     = a[9]->booleano;
    bool   voltear_v     = a[10]->booleano;

    motor_dibujar_sprite(t, x, y, recorte_x, recorte_y, recorte_ancho, recorte_alto,
                          escala, rotacion, voltear_h, voltear_v);
    return valor_nulo();
}
