/*
 * 🐆 Lince — interprete.c
 * Recorre el AST y ejecuta el programa.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>
#include "interprete.h"
#include "modulos.h"
#include "paquetes.h"
#include "recarga.h"
#include "corrutina.h"

/* ─────────────────────────────────────────
   SEÑAL DE RETORNO
   Usamos una variable global para propagar
   el valor de 'devolver' sin excepciones.
───────────────────────────────────────── */
static int    hay_retorno    = 0;

/* 'romper' y 'continuar': como 'devolver', cortan las sentencias que quedan
   hasta que el bucle que los contiene los recoge. El parser ya comprueba que
   estén dentro de un bucle de la misma función. */
enum { SALTO_NINGUNO, SALTO_ROMPER, SALTO_CONTINUAR };
static int    hay_salto      = SALTO_NINGUNO;

/* Tras el cuerpo de un bucle: 1 si hay que salir ('romper'). 'continuar' no
   hace nada más: el cuerpo ya se ha cortado. */
static inline int fin_de_vuelta(void) {
    int romper = hay_salto == SALTO_ROMPER;
    hay_salto = SALTO_NINGUNO;
    return romper;
}

/* Ruta del fichero Lince que se está ejecutando ahora mismo. La fija main.c
   al arrancar, y NODO_IMPORTAR la cambia y la restaura alrededor de cada
   fichero importado, para que un 'importar "./x"' dentro de una librería se
   resuelva junto a la librería y no junto a quien la usó. */
static char _archivo_actual[1024] = "";

const char *interprete_archivo_actual(void) { return _archivo_actual; }
void interprete_fijar_archivo(const char *ruta) {
    snprintf(_archivo_actual, sizeof(_archivo_actual), "%s", ruta ? ruta : "");
}

/* Directorio de 'archivo' en 'destino'. Cadena vacía si no tiene ninguno. */
static void _directorio_de(const char *archivo, char *destino, size_t max) {
    snprintf(destino, max, "%s", archivo);
    char *barra = strrchr(destino, '/');
#ifdef _WIN32
    char *contra = strrchr(destino, '\\');
    if (!barra || (contra && contra > barra)) barra = contra;
#endif
    if (barra) *barra = '\0';
    else        destino[0] = '\0';
}
static Valor *valor_retorno  = NULL;
static Valor *esto_actual    = NULL;
static ClaseLince *clase_actual = NULL;
#define MAX_PROFUNDIDAD 500
static int    profundidad    = 0;

/* Pila de llamadas para el depurador: nombre y línea de cada llamada activa */
#define PILA_MAX (MAX_PROFUNDIDAD + 8)
static const char *_pila_nombre[PILA_MAX];
static int         _pila_linea[PILA_MAX];
static void (*_hook_sentencia)(Nodo *s, Entorno *e) = NULL;
/* > 0 mientras corre código de una biblioteca estándar escrita en Lince: el
   depurador no se para dentro (sus líneas no son del programa) */
static int _en_biblioteca = 0;

void interprete_fijar_hook(void (*hook)(Nodo *sentencia, Entorno *e)) { _hook_sentencia = hook; }
int  interprete_profundidad(void) { return profundidad; }
const char *interprete_archivo_actual(void);
int  interprete_pila(int nivel, const char **nombre, int *linea) {
    if (nivel < 1 || nivel > profundidad) return 0;
    *nombre = _pila_nombre[nivel]; *linea = _pila_linea[nivel];
    return 1;
}
static Valor *llamar_metodo(Valor *obj, ClaseLince *cls, FuncionLince *f,
                            Valor **args, int nargs, int linea);
static FuncionLince *buscar_metodo(ClaseLince *cls, const char *nombre);
int    hay_error     = 0;
Valor *valor_error   = NULL;

/* ─────────────────────────────────────────
   VALORES — creación
───────────────────────────────────────── */
/* Los valores se crean y destruyen sin parar (cada lectura de una variable
   copia un número). Reutilizar los bloques liberados evita un malloc/free por
   operación. Son bloques sueltos: se pueden mezclar con malloc/free sin riesgo. */
#define MAX_VALORES_LIBRES 4096
static Valor *_val_libres = NULL;
static int    _val_libres_num = 0;

static inline Valor *valor_alloc(void) {
    if (_val_libres) {
        Valor *v = _val_libres;
        _val_libres = *(Valor **)v;
        _val_libres_num--;
        return v;
    }
    return malloc(sizeof(Valor));
}

static inline void valor_soltar(Valor *v) {
    if (_val_libres_num < MAX_VALORES_LIBRES) {
        *(Valor **)v = _val_libres;
        _val_libres = v;
        _val_libres_num++;
    } else {
        free(v);
    }
}

static void entorno_destruir(Entorno *e); /* declaración anticipada */
Valor *valor_numero(double n) {
    Valor *v = valor_alloc();
    v->tipo   = VAL_NUMERO;
    v->refs   = 1; v->es_modulo = 0;
    v->numero = n;
    return v;
}

Valor *valor_texto(const char *s) {
    Valor *v = valor_alloc();
    v->tipo  = VAL_TEXTO;
    v->refs  = 1; v->es_modulo = 0;
    v->ascii = -1; v->nchars = -1;
    v->texto = strdup(s);
    return v;
}

/* Como valor_texto, pero se queda con 's' (reservada con malloc) sin copiarla */
static Valor *valor_texto_propio(char *s) {
    Valor *v = valor_alloc();
    v->tipo  = VAL_TEXTO;
    v->refs  = 1; v->es_modulo = 0;
    v->ascii = -1; v->nchars = -1;
    v->texto = s;
    return v;
}

/* nulo, verdadero y falso son únicos y no se liberan nunca (cuenta de
   referencias inalcanzable): ejecutar() devuelve nulo en cada sentencia y cada
   comparación un lógico, y así no cuestan una reserva. Nadie los modifica:
   los que cambian un valor en su sitio lo hacen sólo con números de refs == 1. */
#define REFS_INMORTAL (1 << 29)
static Valor _val_nulo      = { .tipo = VAL_NULO,     .refs = REFS_INMORTAL };
static Valor _val_verdadero = { .tipo = VAL_BOOLEANO, .refs = REFS_INMORTAL, .booleano = 1 };
static Valor _val_falso     = { .tipo = VAL_BOOLEANO, .refs = REFS_INMORTAL, .booleano = 0 };

Valor *valor_booleano(int b) {
    Valor *v = b ? &_val_verdadero : &_val_falso;
    v->refs++;
    return v;
}

Valor *valor_nulo(void) {
    _val_nulo.refs++;
    return &_val_nulo;
}

static Valor *valor_funcion(FuncionLince *f) {
    Valor *v = valor_alloc();
    v->tipo    = VAL_FUNCION;
    v->refs    = 1; v->es_modulo = 0;
    v->funcion = f;
    return v;
}

static Valor *valor_clase(ClaseLince *c) {
    Valor *v = valor_alloc();
    v->tipo  = VAL_CLASE;
    v->refs  = 1; v->es_modulo = 0;
    v->clase = c;
    return v;
}

static Valor *valor_objeto(ObjetoLince *o) {
    Valor *v = valor_alloc();
    v->tipo   = VAL_OBJETO;
    v->refs   = 1; v->es_modulo = 0;
    v->objeto = o;
    return v;
}

Valor *valor_crear_error(const char *tipo, const char *mensaje, int linea) {
    ErrorLince *e = malloc(sizeof(ErrorLince));
    e->tipo    = strdup(tipo);
    e->mensaje = strdup(mensaje);
    e->linea   = linea;
    Valor *v = valor_alloc();
    v->tipo  = VAL_ERROR;
    v->refs  = 1; v->es_modulo = 0;
    v->error = e;
    return v;
}

/* Pone un error nuevo soltando el que hubiera pendiente. 'valor_error' es una
   global y hasta ahora los ~60 sitios que la escriben la pisaban sin más, con
   lo que el anterior se fugaba entero (el Valor y sus tipo/mensaje duplicados).
   Se nota cuando algo sigue ejecutándose con un error puesto y acaba poniendo
   otro encima. */
Valor *valor_error_nuevo(const char *tipo, const char *mensaje, int linea) {
    /* El primero gana. Con un error ya puesto, lo que venga detrás es
       consecuencia suya y no la causa: al pasarse de MAX_PROFUNDIDAD, el
       'ErrorRecursion' quedaba tapado por el "Operación '+' no soportada" de
       sumar el nulo que devolvió la llamada abortada, y el mensaje útil se
       perdía justo cuando más falta hace. Devolver el pendiente deja intacta
       la forma de los ~60 sitios que escriben valor_error. */
    if (hay_error && valor_error) return valor_error;

    valor_destruir(valor_error);
    valor_error = NULL;
    return valor_crear_error(tipo, mensaje, linea);
}

/* Error de ejecución capturable, con número de línea.
 *
 * Antes, unos cuarenta fallos de uso del lenguaje —un método de lista con
 * mal número de argumentos, un índice fuera de rango, un campo que no existe,
 * llamar a algo que no es una función...— hacían fprintf + exit(1): mataban el
 * proceso, así que un intentar/capturar no podía recuperarse y el REPL se
 * cerraba entero. Y casi ninguno decía la línea. Ahora pasan por aquí.
 *
 * Quien lo llama sigue siendo dueño de lo que tuviera vivo: tiene que soltarlo
 * ANTES de volver, que con exit() no hacía falta y ahora sí. */
#if defined(__GNUC__)
#  define FORMATO_PRINTF(a, b) __attribute__((format(printf, a, b)))
#else
#  define FORMATO_PRINTF(a, b)
#endif

static void poner_error(const char *tipo, int linea, const char *fmt, ...)
    FORMATO_PRINTF(3, 4);
static void poner_error(const char *tipo, int linea, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    valor_error = valor_error_nuevo(tipo, msg, linea);
    hay_error   = 1;
}

static Valor *error_rt(const char *tipo, int linea, const char *fmt, ...)
    FORMATO_PRINTF(3, 4);
static Valor *error_rt(const char *tipo, int linea, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    valor_error = valor_error_nuevo(tipo, msg, linea);
    hay_error   = 1;
    return valor_nulo();
}

/* Incrementa referencias */
static Valor *valor_retener(Valor *v) {
    if (v) v->refs++;
    return v;
}

/* es_modulo de los diccionarios que son módulos (1), interfaces (2) o
   enumeraciones */
#define ES_ENUMERACION 3

typedef struct TareaLince TareaLince;
static void tarea_liberar(TareaLince *t);
static void generador_liberar(TareaLince *g);
static const char *tarea_nombre(TareaLince *t);

/* Decrementa referencias y destruye si llega a 0 */
static void valor_liberar(Valor *v) {
    if (!v) return;
    v->refs--;
    if (v->refs > 0) return;
    if (v == &_val_nulo || v == &_val_verdadero || v == &_val_falso) {   /* no debería pasar */
        v->refs = REFS_INMORTAL;
        return;
    }

    /* Los escalares son los más frecuentes: sin nada que liberar por dentro */
    if (v->tipo == VAL_NUMERO || v->tipo == VAL_BOOLEANO || v->tipo == VAL_NULO) {
        valor_soltar(v);
        return;
    }
    if (v->tipo == VAL_TEXTO) free(v->texto);
    if (v->tipo == VAL_LISTA) {
        for (int i = 0; i < v->lista.cantidad; i++)
            valor_liberar(v->lista.elementos[i]);
        free(v->lista.elementos);
    }
    if (v->tipo == VAL_DICCIONARIO) {
        for (int i = 0; i < v->diccionario.cantidad; i++) {
            free(v->diccionario.claves[i]);
            valor_liberar(v->diccionario.valores[i]);
        }
        free(v->diccionario.claves);
        free(v->diccionario.valores);
        free(v->diccionario.indice);
    }
    if (v->tipo == VAL_OBJETO) {
        for (int i = 0; i < v->objeto->num_campos; i++) {
            free(v->objeto->campos_nombres[i]);
            valor_liberar(v->objeto->campos_valores[i]);
        }
        free(v->objeto);
    }
    if (v->tipo == VAL_FUNCION && v->funcion) {
        /* Liberar la referencia al entorno capturado por el closure */
        if (v->funcion->entorno_closure)
            entorno_destruir(v->funcion->entorno_closure);
        free(v->funcion->nombre);
        free(v->funcion);
    }
    if (v->tipo == VAL_BYTES) free(v->bytes.datos);
    if (v->tipo == VAL_TAREA) tarea_liberar(v->tarea);
    if (v->tipo == VAL_ENUMERADO) {
        free(v->miembro->texto);
        valor_liberar(v->miembro->valor);
        free(v->miembro);
    }
    if (v->tipo == VAL_GENERADOR && v->generador.productor) generador_liberar(v->generador.productor);
    if (v->tipo == VAL_ERROR) {
        free(v->error->tipo);
        free(v->error->mensaje);
        free(v->error);
    }
    valor_soltar(v);
}

/* Alias para claridad — función real para que sea linkeable */
void valor_destruir(Valor *v) { valor_liberar(v); }

Valor *valor_bytes_crear(int n) {
    Valor *v = valor_alloc();
    v->tipo = VAL_BYTES;
    v->refs = 1; v->es_modulo = 0;
    v->bytes.capacidad = n > 16 ? n : 16;
    v->bytes.datos = calloc((size_t)v->bytes.capacidad, 1);
    v->bytes.cantidad = n;
    return v;
}

Valor *valor_lista_crear(void) {
    Valor *v = valor_alloc();
    v->tipo             = VAL_LISTA;
    v->refs             = 1; v->es_modulo = 0;
    v->lista.capacidad  = 8;
    v->lista.cantidad   = 0;
    v->lista.elementos  = malloc(sizeof(Valor*) * 8);
    return v;
}

void lista_agregar(Valor *lista, Valor *elem) {
    if (lista->lista.cantidad >= lista->lista.capacidad) {
        lista->lista.capacidad *= 2;
        lista->lista.elementos = realloc(lista->lista.elementos,
            sizeof(Valor*) * lista->lista.capacidad);
    }
    lista->lista.elementos[lista->lista.cantidad++] = elem;
}

#define DIC_CAP_INICIAL 16

/* ─────────────────────────────────────────
   UTF-8
   El texto de Lince se guarda en UTF-8. Estas ayudas permiten contar y
   recorrer caracteres en vez de bytes, que es lo que espera quien
   escribe en español: "niño" son 4 letras, no 5.
───────────────────────────────────────── */

/* Bytes que ocupa el carácter que empieza en 's'. */
int utf8_tam(const char *s) {
    unsigned char c = (unsigned char)*s;
    if (c == 0)              return 0;
    if (c < 0x80)            return 1;
    if ((c & 0xE0) == 0xC0)  return 2;
    if ((c & 0xF0) == 0xE0)  return 3;
    if ((c & 0xF8) == 0xF0)  return 4;
    return 1;  /* byte suelto no válido: se cuenta como uno */
}

/* Número de caracteres de la cadena. */
int utf8_longitud(const char *s) {
    int n = 0;
    while (*s) { s += utf8_tam(s); n++; }
    return n;
}

/* Desplazamiento en bytes del carácter número 'i', o -1 si no llega. */
int utf8_desplazamiento(const char *s, int i) {
    int pos = 0, n = 0;
    while (s[pos]) {
        if (n == i) return pos;
        pos += utf8_tam(s + pos);
        n++;
    }
    return (n == i) ? pos : -1;
}

/* Cambio de caja para ASCII y para el bloque latino de dos bytes, que
   es donde viven á é í ó ú ü ñ. Ese bloque mantiene el número de bytes
   al cambiar de caja, así que la cadena resultante mide lo mismo.
   Si 'arriba' es 1 pasa a mayúsculas; si es 0, a minúsculas. */
static char *utf8_caja(const char *s, int arriba) {
    size_t n = strlen(s);
    char *r = malloc(n + 1);
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            r[i] = (char)(arriba ? toupper(c) : tolower(c));
            i++;
        } else if (c == 0xC3 && i + 1 < n) {
            unsigned char d = (unsigned char)s[i + 1];
            r[i] = (char)c;
            /* à-þ ↔ À-Þ, separadas por 0x20 igual que en ASCII. Se
               dejan en paz ÷ (0xB7) y × (0x97), que no son letras, y
               ß y ÿ, cuya mayúscula no vive en este bloque. */
            if (arriba && d >= 0xA0 && d <= 0xBE && d != 0xB7)
                r[i + 1] = (char)(d - 0x20);
            else if (!arriba && d >= 0x80 && d <= 0x9E && d != 0x97)
                r[i + 1] = (char)(d + 0x20);
            else
                r[i + 1] = (char)d;
            i += 2;
        } else {
            int k = utf8_tam(s + i);
            for (int j = 0; j < k && i + (size_t)j < n; j++)
                r[i + j] = s[i + j];
            i += (size_t)k;
        }
    }
    r[n] = '\0';
    return r;
}

char *utf8_mayusculas(const char *s) { return utf8_caja(s, 1); }
char *utf8_minusculas(const char *s) { return utf8_caja(s, 0); }

/* ¿Es una letra? Cuenta como tal cualquier carácter de varios bytes,
   que en la práctica es una vocal acentuada o una ñ. */
int utf8_es_letra(const char *s) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) return isalpha(c) ? 1 : 0;
    return 1;
}

Valor *valor_diccionario_crear(void) {
    Valor *v = valor_alloc();
    v->tipo                  = VAL_DICCIONARIO;
    v->refs                  = 1; v->es_modulo = 0;
    v->es_modulo             = 0;
    v->diccionario.cantidad  = 0;
    v->diccionario.indice = NULL; v->diccionario.indice_cap = 0; v->diccionario.indice_n = 0;
    v->diccionario.capacidad = DIC_CAP_INICIAL;
    v->diccionario.claves    = malloc(sizeof(char*)  * DIC_CAP_INICIAL);
    v->diccionario.valores   = malloc(sizeof(Valor*) * DIC_CAP_INICIAL);
    return v;
}

/* Asegura hueco para 'n' entradas, duplicando como hacen las listas.
   Antes el diccionario reservaba 64 huecos fijos y no crecía nunca, así
   que la clave 65 escribía fuera del array y corrompía el montón. */
void valor_diccionario_asegurar(Valor *dic, int n) {
    if (!dic || dic->tipo != VAL_DICCIONARIO) return;
    if (n <= dic->diccionario.capacidad) return;
    int cap = dic->diccionario.capacidad > 0
                ? dic->diccionario.capacidad : DIC_CAP_INICIAL;
    while (cap < n) cap *= 2;
    dic->diccionario.claves  = realloc(dic->diccionario.claves,
                                       sizeof(char*)  * cap);
    dic->diccionario.valores = realloc(dic->diccionario.valores,
                                       sizeof(Valor*) * cap);
    dic->diccionario.capacidad = cap;
}

/* Añade una entrada al final, creciendo si hace falta. No comprueba si
   la clave ya existe: para eso están los sitios que buscan primero. */
void valor_diccionario_agregar(Valor *dic, const char *clave, Valor *valor) {
    if (!dic || dic->tipo != VAL_DICCIONARIO) return;
    valor_diccionario_asegurar(dic, dic->diccionario.cantidad + 1);
    int i = dic->diccionario.cantidad;
    dic->diccionario.claves[i]  = strdup(clave);
    dic->diccionario.valores[i] = valor;
    dic->diccionario.cantidad++;
}

static unsigned hash_cadena(const char *s) {
    unsigned h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

/* Posición de la clave 'k' en el diccionario, o -1. Hasta 8 entradas se busca
   en línea recta; con más, con una tabla hash que se construye al primer uso
   y crece con las entradas nuevas (las entradas solo se añaden al final, salvo
   'eliminar', que invalida la tabla con indice_n = -1). */
int valor_diccionario_buscar(Valor *d, const char *k) {
    int n = d->diccionario.cantidad;
    if (n <= 8) {
        for (int i = 0; i < n; i++)
            if (strcmp(d->diccionario.claves[i], k) == 0) return i;
        return -1;
    }
    if (!d->diccionario.indice || d->diccionario.indice_n < 0 ||
        d->diccionario.indice_n > n || d->diccionario.indice_cap < 2 * n) {
        int cap = 32;
        while (cap < 4 * n) cap *= 2;
        free(d->diccionario.indice);
        d->diccionario.indice = malloc(sizeof(int) * cap);
        memset(d->diccionario.indice, 0xFF, sizeof(int) * cap);
        d->diccionario.indice_cap = cap;
        d->diccionario.indice_n = 0;
    }
    int *tab = d->diccionario.indice, mascara = d->diccionario.indice_cap - 1;
    while (d->diccionario.indice_n < n) {
        int pos = d->diccionario.indice_n++;
        unsigned h = hash_cadena(d->diccionario.claves[pos]) & (unsigned)mascara;
        while (tab[h] != -1) h = (h + 1) & (unsigned)mascara;
        tab[h] = pos;
    }
    unsigned h = hash_cadena(k) & (unsigned)mascara;
    while (tab[h] != -1) {
        if (strcmp(d->diccionario.claves[tab[h]], k) == 0) return tab[h];
        h = (h + 1) & (unsigned)mascara;
    }
    return -1;
}

static Valor *valor_generador_crear(void) {
    Valor *v = valor_alloc();
    v->tipo              = VAL_GENERADOR;
    v->refs              = 1;
    v->es_modulo         = 0;
    v->generador.productor = NULL;
    v->generador.desde = v->generador.hasta = 0;
    v->generador.paso  = 1;
    return v;
}

/* Un texto se comparte entre variables (nunca se modifica), así que la
   longitud y si es ASCII se calculan una vez y se guardan en el propio valor:
   indexar un texto largo en un bucle deja de ser cuadrático. */
static void texto_medir(Valor *v) {
    if (v->nchars >= 0) return;
    int ascii = 1;
    for (const unsigned char *p = (const unsigned char *)v->texto; *p; p++)
        if (*p >= 0x80) { ascii = 0; break; }
    v->ascii  = ascii;
    v->nchars = utf8_longitud(v->texto);
}

static Valor *valor_copiar(Valor *v) {
    if (!v) return valor_nulo();
    switch (v->tipo) {
        /* Números, lógicos, nulos y textos no se modifican nunca: se comparten */
        case VAL_NUMERO:
        case VAL_BOOLEANO:
        case VAL_NULO:
        case VAL_TEXTO:       return valor_retener(v);
        case VAL_FUNCION:
        case VAL_LISTA:
        case VAL_DICCIONARIO:
        case VAL_CLASE:
        case VAL_OBJETO:
        case VAL_BYTES:
        case VAL_TAREA:
        case VAL_ENUMERADO:
            return valor_retener(v);
        default:              return valor_nulo();
    }
}

/* Un valor de enumeración es igual a otro con el mismo texto (aunque sea de
   antes de recargar el programa) y al texto "Estado.Quieto". Devuelve -1 si
   ninguno de los dos es de una enumeración. */
static int iguales_enum(Valor *a, Valor *b) {
    if (a->tipo != VAL_ENUMERADO && b->tipo != VAL_ENUMERADO) return -1;
    const char *ta = a->tipo == VAL_ENUMERADO ? a->miembro->texto : a->tipo == VAL_TEXTO ? a->texto : NULL;
    const char *tb = b->tipo == VAL_ENUMERADO ? b->miembro->texto : b->tipo == VAL_TEXTO ? b->texto : NULL;
    return ta && tb && strcmp(ta, tb) == 0;
}

/* d[Estado.Quieto]: como clave de un diccionario cuenta su texto */
static Valor *clave_de(Valor *idx) {
    if (idx->tipo != VAL_ENUMERADO) return idx;
    Valor *t = valor_texto(idx->miembro->texto);
    valor_destruir(idx);
    return t;
}

/* Convierte un valor a texto para escribir() */
static char *valor_a_texto(Valor *v) {
    char buf[4096];
    if (!v || v->tipo == VAL_NULO) return strdup("nulo");
    switch (v->tipo) {
        case VAL_NUMERO:
            if (v->numero == (long long)v->numero)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->numero);
            else
                snprintf(buf, sizeof(buf), "%g", v->numero);
            return strdup(buf);
        case VAL_TEXTO:
            return strdup(v->texto);
        case VAL_BOOLEANO:
            return strdup(v->booleano ? "verdadero" : "falso");
        case VAL_FUNCION:
            snprintf(buf, sizeof(buf), "<funcion %s>", v->funcion->nombre);
            return strdup(buf);
        case VAL_TAREA:
            snprintf(buf, sizeof(buf), "<tarea %s>", tarea_nombre(v->tarea));
            return strdup(buf);
        case VAL_ENUMERADO:
            return strdup(v->miembro->texto);
        case VAL_CLASE:
            snprintf(buf, sizeof(buf), "<clase %s>", v->clase->nombre);
            return strdup(buf);
        case VAL_OBJETO: {
            /* Un método 'a_texto()' de la clase decide cómo se escribe */
            FuncionLince *ft = buscar_metodo(v->objeto->clase, "a_texto");
            if (ft && !ft->estatico && ft->num_parametros == 0 && !hay_error) {
                Valor *r = llamar_metodo(v, v->objeto->clase, ft, NULL, 0, 0);
                if (r && r->tipo == VAL_TEXTO) {
                    char *t = strdup(r->texto);
                    valor_destruir(r);
                    return t;
                }
                if (r) valor_destruir(r);
            }
            char tmp[4096];
            snprintf(tmp, sizeof(tmp), "%s{", v->objeto->clase->nombre);
            for (int i = 0; i < v->objeto->num_campos; i++) {
                strncat(tmp, v->objeto->campos_nombres[i], sizeof(tmp)-strlen(tmp)-1);
                strncat(tmp, ": ", sizeof(tmp)-strlen(tmp)-1);
                char *s = valor_a_texto(v->objeto->campos_valores[i]);
                strncat(tmp, s, sizeof(tmp)-strlen(tmp)-1);
                free(s);
                if (i < v->objeto->num_campos - 1)
                    strncat(tmp, ", ", sizeof(tmp)-strlen(tmp)-1);
            }
            strncat(tmp, "}", sizeof(tmp)-strlen(tmp)-1);
            return strdup(tmp);
        }
        case VAL_LISTA: {
            char tmp[4096] = "[";
            for (int i = 0; i < v->lista.cantidad; i++) {
                char *s = valor_a_texto(v->lista.elementos[i]);
                if (v->lista.elementos[i]->tipo == VAL_TEXTO) {
                    strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                    strncat(tmp, s, sizeof(tmp) - strlen(tmp) - 1);
                    strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                } else {
                    strncat(tmp, s, sizeof(tmp) - strlen(tmp) - 1);
                }
                free(s);
                if (i < v->lista.cantidad - 1)
                    strncat(tmp, ", ", sizeof(tmp) - strlen(tmp) - 1);
            }
            strncat(tmp, "]", sizeof(tmp) - strlen(tmp) - 1);
            return strdup(tmp);
        }
        case VAL_BYTES: {          /* bytes[48 6f 6c 61], en hexadecimal */
            int n = v->bytes.cantidad, muestra = n > 64 ? 64 : n;
            char *t = malloc((size_t)muestra * 3 + 64);
            strcpy(t, "bytes[");
            size_t p = strlen(t);
            for (int i = 0; i < muestra; i++)
                p += (size_t)sprintf(t + p, i ? " %02x" : "%02x", v->bytes.datos[i]);
            if (n > muestra) p += (size_t)sprintf(t + p, " … %d en total", n);
            strcpy(t + p, "]");
            return t;
        }
        case VAL_DICCIONARIO: {
            char tmp[4096] = "{";
            for (int i = 0; i < v->diccionario.cantidad; i++) {
                strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                strncat(tmp, v->diccionario.claves[i], sizeof(tmp) - strlen(tmp) - 1);
                strncat(tmp, "\": ", sizeof(tmp) - strlen(tmp) - 1);
                char *s = valor_a_texto(v->diccionario.valores[i]);
                strncat(tmp, s, sizeof(tmp) - strlen(tmp) - 1);
                free(s);
                if (i < v->diccionario.cantidad - 1)
                    strncat(tmp, ", ", sizeof(tmp) - strlen(tmp) - 1);
            }
            strncat(tmp, "}", sizeof(tmp) - strlen(tmp) - 1);
            return strdup(tmp);
        }
        default:
            return strdup("nulo");
    }
}

/* Verdad de un valor */
static int es_verdadero(Valor *v) {
    if (!v || v->tipo == VAL_NULO)     return 0;
    if (v->tipo == VAL_BOOLEANO)       return v->booleano;
    if (v->tipo == VAL_NUMERO)         return v->numero != 0;
    if (v->tipo == VAL_TEXTO)          return strlen(v->texto) > 0;
    return 1;
}

/* ─────────────────────────────────────────
   ENTORNO
───────────────────────────────────────── */
/* Lista de entornos reutilizables. Un Entorno ocupa ~6 KB y se crea y
   destruye en cada llamada, cada bloque y cada iteración de bucle, así
   que reciclarlos evita casi todo ese tráfico de malloc/free. Se enlazan
   por el campo 'padre', que está libre mientras el entorno no se usa. */
static Entorno *_ent_libres     = NULL;
static int      _ent_libres_num = 0;
#define MAX_ENT_LIBRES 256   /* tope de retención: ~1,5 MB */

static Entorno *entorno_crear(Entorno *padre) {
    /* malloc, no calloc: 'vars' ocupa casi todo el struct (MAX_VARS
       entradas) y cada entrada se escribe por completo en
       entorno_definir antes de leerse; nadie mira más allá de
       'cantidad'. Poner a cero ~6 KB en cada creación dominaba el
       tiempo de ejecución. */
    Entorno *e;
    if (_ent_libres) {
        e = _ent_libres;
        _ent_libres = e->padre;
        _ent_libres_num--;
    } else {
        e = malloc(sizeof(Entorno));
    }
    e->padre    = padre;
    e->dueno    = padre ? padre->dueno : NULL;
    e->cantidad = 0;
    e->refs     = 1;
    if (padre) padre->refs++;
    return e;
}

/* ¿Hay ya algo con ese nombre en ESTE ámbito?
 *
 * Mira sólo el entorno propio, no los padres: tapar un nombre de fuera dentro
 * de un bloque o una función sigue siendo legal y útil. Lo que no puede pasar
 * es definir dos veces en el mismo sitio, que hasta ahora se aceptaba y se
 * quedaba callado con la PRIMERA definición — ni la última, ni un aviso. */
/* Ficheros ya importados, por ruta canónica.
 *
 * Un 'importar' repetido no hace nada. Hace falta por dos razones: reejecutar
 * el fichero volvería a definir lo suyo y, desde que redefinir es un error,
 * chocaría consigo mismo; y es lo que se espera de un import — que dos
 * librerías puedan depender de una tercera sin que la tercera se cargue dos
 * veces. Se canoniza con realpath para que dos rutas relativas distintas al
 * mismo fichero cuenten como uno. */
#define MAX_IMPORTADOS 256
static char *_importados[MAX_IMPORTADOS];
static int   _num_importados = 0;

/* 1 si ya estaba importado. Si no lo estaba, lo anota y devuelve 0. */
static int importado_ya(const char *ruta) {
    char canon[1024];
#ifndef _WIN32
    char *r = realpath(ruta, NULL);
    if (r) { snprintf(canon, sizeof(canon), "%s", r); free(r); }
    else    { snprintf(canon, sizeof(canon), "%s", ruta); }
#else
    snprintf(canon, sizeof(canon), "%s", ruta);
#endif
    for (int i = 0; i < _num_importados; i++)
        if (strcmp(_importados[i], canon) == 0) return 1;
    if (_num_importados < MAX_IMPORTADOS)
        _importados[_num_importados++] = strdup(canon);
    return 0;
}

/* ── Nombres internados ──
   Cada nombre de variable se guarda una sola vez y para siempre; así los
   ámbitos comparan punteros en vez de cadenas y no copian el nombre al
   declarar. Los nodos guardan en 'nombre_i' el suyo ya internado. */
static const char **_int_tab = NULL;
static size_t       _int_cap = 0, _int_n = 0;

static unsigned hash_nombre(const char *s) {
    unsigned h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

/* Nombre ya internado igual a 's', o NULL si nunca se ha usado */
static const char *intern_buscar(const char *s) {
    if (!_int_n) return NULL;
    size_t m = _int_cap - 1, h = hash_nombre(s) & m;
    while (_int_tab[h]) {
        if (strcmp(_int_tab[h], s) == 0) return _int_tab[h];
        h = (h + 1) & m;
    }
    return NULL;
}

static const char *intern(const char *s) {
    const char *ya = intern_buscar(s);
    if (ya) return ya;
    if ((_int_n + 1) * 2 > _int_cap) {
        size_t cap = _int_cap ? _int_cap * 2 : 1024;
        const char **nueva = calloc(cap, sizeof(char *));
        for (size_t i = 0; i < _int_cap; i++)
            if (_int_tab[i]) {
                size_t h = hash_nombre(_int_tab[i]) & (cap - 1);
                while (nueva[h]) h = (h + 1) & (cap - 1);
                nueva[h] = _int_tab[i];
            }
        free(_int_tab);
        _int_tab = nueva; _int_cap = cap;
    }
    size_t h = hash_nombre(s) & (_int_cap - 1);
    while (_int_tab[h]) h = (h + 1) & (_int_cap - 1);
    _int_tab[h] = strdup(s);
    _int_n++;
    return _int_tab[h];
}

#define NI(n, s)  ((n)->nombre_i ? (n)->nombre_i : ((n)->nombre_i = intern(s)))
#define PI(p)     ((p)->nombre_i ? (p)->nombre_i : ((p)->nombre_i = intern((p)->nombre)))

/* Empaqueta lo definido en 'mod_e' como un diccionario-módulo, igual que los
 * módulos estándar, para exponerlo bajo un alias: 'col.colisionan(a, b)'.
 *
 * No se destruye 'mod_e': las funciones que ha definido lo retienen como su
 * closure, y es lo que les permite verse entre ellas y leer las constantes de
 * la propia librería. Vive lo que viva el programa, como cualquier módulo. */
static Valor *empaquetar_modulo(Entorno *mod_e) {
    Valor *dic = valor_diccionario_crear();
    dic->es_modulo = 1;
    for (int i = 0; i < mod_e->cantidad; i++)
        valor_diccionario_agregar(dic, mod_e->vars[i].nombre,
                                  valor_retener(mod_e->vars[i].valor));
    return dic;
}

static int ya_definido_aqui_i(Entorno *e, const char *id) {
    for (int i = 0; i < e->cantidad; i++)
        if (e->vars[i].nombre == id) return 1;
    return 0;
}
static int ya_definido_aqui(Entorno *e, const char *nombre) {
    const char *id = intern_buscar(nombre);
    return id && ya_definido_aqui_i(e, id);
}

/* Da el error de redefinición si procede. Devuelve 1 si lo ha dado. */
static int redefinido(Entorno *e, const char *nombre, const char *que,
                      int es_variable, int linea) {
    if (!ya_definido_aqui(e, nombre)) return 0;
    char msg[320];
    if (es_variable)
        snprintf(msg, sizeof(msg),
            "Ya hay algo llamado '%s' en este ámbito, así que %s no puede "
            "volver a definirlo. Si querías cambiar su valor, usa "
            "'%s = ...'; si no, elige otro nombre.",
            nombre, que, nombre);
    else
        snprintf(msg, sizeof(msg),
            "Ya hay algo llamado '%s' en este ámbito, así que %s no puede "
            "volver a definirlo. Elige otro nombre.", nombre, que);
    valor_error = valor_error_nuevo("Error", msg, linea);
    hay_error   = 1;
    return 1;
}

void entorno_definir_i(Entorno *e, const char *nombre, Valor *valor, int constante) {
    if (e->cantidad >= MAX_VARS) {
        /* Alcanzable por un programa normal (256 variables en un ámbito), no
           un fallo interno: error capturable y se suelta el valor, que quien
           llama da por entregado. */
        valor_destruir(valor);
        poner_error("Error", 0,
            "Demasiadas variables en el mismo ámbito (el máximo es %d).", MAX_VARS);
        return;
    }
    e->vars[e->cantidad].nombre    = nombre;
    e->vars[e->cantidad].valor     = valor;
    e->vars[e->cantidad].constante = constante;
    e->cantidad++;
}

/* 'nombre' puede ser cualquier cadena: se interna aquí */
void entorno_definir(Entorno *e, const char *nombre, Valor *valor, int constante) {
    entorno_definir_i(e, intern(nombre), valor, constante);
}


static void entorno_destruir(Entorno *e) {
    if (!e) return;
    e->refs--;
    if (e->refs > 0) return;
    for (int i = 0; i < e->cantidad; i++) {
        valor_destruir(e->vars[i].valor);
    }
    Entorno *padre = e->padre;
    if (_ent_libres_num < MAX_ENT_LIBRES) {
        e->padre = _ent_libres;
        _ent_libres = e;
        _ent_libres_num++;
    } else {
        free(e);
    }
    if (padre) entorno_destruir(padre);
}

/* 'id' es el nombre internado (o NULL si ese nombre no existe en ningún sitio);
   'nombre' sólo sirve para el mensaje de error. */
static Valor *entorno_obtener_i(Entorno *e, const char *id, const char *nombre, int linea) {
    for (; e; e = e->padre)
        for (int i = 0; i < e->cantidad; i++)
            if (e->vars[i].nombre == id)
                return valor_copiar(e->vars[i].valor);
    /* Siempre capturable. Antes, sin número de línea (linea == 0 — la
       llamaba el incremento de un 'para') caía a un fprintf y exit(1), y un
       'para (i = 0; i < 3; j++)' con 'j' sin definir mataba el proceso. */
    poner_error("Error", linea, "La variable '%s' no está definida.", nombre);
    return valor_nulo();
}

static void entorno_asignar_i(Entorno *e, const char *id, const char *nombre, Valor *nuevo, int linea);

/* La entrada de 'id' vista desde 'e', o NULL. Igual que buscar ámbito por
   ámbito, pero el nodo recuerda en qué ámbito y posición la encontró: los
   ámbitos de más cerca se siguen mirando enteros (así un nombre que los tape
   gana, como siempre) y en el de la última vez se mira directamente esa
   posición en vez de recorrerlo. Lo que más se ahorra es en las globales, que
   pueden ser cientos. */
static EntradaVar *buscar_var(Nodo *n, Entorno *e, const char *id) {
    int saltos = n->var_saltos, pos = n->var_pos - 1, h = 0;
    Entorno *x = e;
    if (pos >= 0) {
        for (; x && h < saltos; h++, x = x->padre)
            for (int i = 0; i < x->cantidad; i++)
                if (x->vars[i].nombre == id) {
                    n->var_saltos = h; n->var_pos = i + 1;
                    return &x->vars[i];
                }
        if (x && pos < x->cantidad && x->vars[pos].nombre == id) return &x->vars[pos];
        x = e; h = 0;          /* la caché ya no vale (pasa poco): búsqueda completa */
    }
    for (; x; h++, x = x->padre)
        for (int i = 0; i < x->cantidad; i++)
            if (x->vars[i].nombre == id) {
                n->var_saltos = h; n->var_pos = i + 1;
                return &x->vars[i];
            }
    return NULL;
}

/* Como entorno_obtener_i / entorno_asignar_i, con la caché del nodo */
static Valor *obtener_var(Nodo *n, Entorno *e, const char *id, const char *nombre) {
    EntradaVar *v = buscar_var(n, e, id);
    if (v) return valor_copiar(v->valor);
    poner_error("Error", n->linea, "La variable '%s' no está definida.", nombre);
    return valor_nulo();
}

static void asignar_var(Nodo *n, Entorno *e, const char *id, const char *nombre, Valor *nuevo) {
    EntradaVar *v = buscar_var(n, e, id);
    if (!v) { entorno_asignar_i(e, id, nombre, nuevo, n->linea); return; }   /* da el error */
    if (v->constante) { entorno_asignar_i(e, id, nombre, nuevo, n->linea); return; }
    valor_destruir(v->valor);
    v->valor = nuevo;
}

static Valor *entorno_obtener(Entorno *e, const char *nombre, int linea) {
    return entorno_obtener_i(e, intern_buscar(nombre), nombre, linea);
}

static void entorno_asignar_i(Entorno *e, const char *id, const char *nombre, Valor *nuevo, int linea) {
    for (; e; e = e->padre) {
        for (int i = 0; i < e->cantidad; i++) {
            if (e->vars[i].nombre != id) continue;
            if (e->vars[i].constante) {
                /* 'nuevo' se entregó a esta función: si no se asigna, hay que
                   soltarlo o se fuga. */
                valor_destruir(nuevo);
                poner_error("Error", linea,
                    "'%s' es una constante definida con 'fijo' y no puede cambiar. "
                    "Si necesitas que cambie, usa 'sea' en lugar de 'fijo'.", nombre);
                return;
            }
            valor_destruir(e->vars[i].valor);
            e->vars[i].valor = nuevo;
            return;
        }
    }
    valor_destruir(nuevo);
    poner_error("Error", linea,
        "Intentas asignar un valor a '%s', pero nunca la definiste. "
        "¿Querías escribir 'sea %s = ...' primero?", nombre, nombre);
}

static void entorno_asignar(Entorno *e, const char *nombre, Valor *nuevo, int linea) {
    entorno_asignar_i(e, intern_buscar(nombre), nombre, nuevo, linea);
}

/* ─────────────────────────────────────────
   EJECUCIÓN DE NODOS
───────────────────────────────────────── */
static Valor *ejecutar(Nodo *n, Entorno *e);

/* Busca un método en la clase y sus ancestros */
static FuncionLince *buscar_metodo(ClaseLince *cls, const char *nombre) {
    while (cls) {
        for (int i = 0; i < cls->num_metodos; i++) {
            if (strcmp(cls->metodos[i]->nombre, nombre) == 0)
                return cls->metodos[i];
        }
        cls = cls->padre;
    }
    return NULL;
}

/* ─────────────────────────────────────────
   Programación orientada a objetos: ayudas
───────────────────────────────────────── */

/* ¿'c' es 'base' o desciende de ella? */
static int clase_deriva(ClaseLince *c, ClaseLince *base) {
    for (; c; c = c->padre) if (c == base) return 1;
    return 0;
}

/* ¿La clase (o una antecesora) declara 'implementa' esa interfaz? */
static int clase_implementa(ClaseLince *c, InterfazLince *iface) {
    for (; c; c = c->padre)
        for (int i = 0; i < c->num_interfaces; i++)
            if (c->interfaces[i] == iface) return 1;
    return 0;
}

/* Visibilidad: lo privado sólo lo ve el código escrito dentro de la clase que
   lo declara; lo protegido, también el de sus subclases. 'e' es el ámbito desde
   el que se accede: lleva anotada la clase en cuyo cuerpo se escribió el código
   (también las lambdas definidas dentro de un método). */
static int acceso_permitido(int vis, ClaseLince *duena, Entorno *e) {
    if (vis == VIS_PUBLICO) return 1;
    ClaseLince *d = e ? e->dueno : NULL;
    if (!d) return 0;
    if (vis == VIS_PRIVADO) return d == duena;
    return clase_deriva(d, duena);
}

static const char *nombre_vis(int vis) {
    return vis == VIS_PRIVADO ? "privado" : "protegido";
}

/* Campo declarado con 'sea' / 'fijo' en la clase o en una antecesora. */
static CampoClase *buscar_campo(ClaseLince *c, const char *nombre, int estatico) {
    for (; c; c = c->padre)
        for (int i = 0; i < c->num_campos; i++)
            if (c->campos[i].estatico == estatico &&
                strcmp(c->campos[i].nombre, nombre) == 0)
                return &c->campos[i];
    return NULL;
}

/* x es Tipo — clase, interfaz o tipo básico. */
static Valor *exec_es(Nodo *n, Entorno *e) {
    Valor *v = ejecutar(n->binario.izquierda, e);
    const char *nm = n->binario.derecha->identificador;
    int r = 0;

    if      (strcmp(nm, "numero") == 0)      r = v->tipo == VAL_NUMERO;
    else if (strcmp(nm, "texto") == 0)       r = v->tipo == VAL_TEXTO;
    else if (strcmp(nm, "logico") == 0)      r = v->tipo == VAL_BOOLEANO;
    else if (strcmp(nm, "lista") == 0)       r = v->tipo == VAL_LISTA;
    else if (strcmp(nm, "bytes") == 0)       r = v->tipo == VAL_BYTES;
    else if (strcmp(nm, "diccionario") == 0) r = v->tipo == VAL_DICCIONARIO && !v->es_modulo;
    else if (strcmp(nm, "nulo") == 0)        r = v->tipo == VAL_NULO;
    else if (strcmp(nm, "funcion") == 0)     r = v->tipo == VAL_FUNCION;
    else if (strcmp(nm, "tarea") == 0)       r = v->tipo == VAL_TAREA;
    else {
        Valor *t = entorno_obtener(e, nm, n->linea);
        if (hay_error) { valor_destruir(v); valor_destruir(t); return valor_nulo(); }
        if (t->tipo == VAL_CLASE) {
            if (v->tipo == VAL_OBJETO && !v->objeto->clase) {
                /* El error que recibe un 'capturar' es un objeto sin clase,
                   con los campos mensaje / tipo / linea */
                for (int i = 0; i < v->objeto->num_campos; i++)
                    if (strcmp(v->objeto->campos_nombres[i], "tipo") == 0 &&
                        v->objeto->campos_valores[i]->tipo == VAL_TEXTO)
                        r = strcmp(v->objeto->campos_valores[i]->texto, nm) == 0 ||
                            strcmp(nm, "Error") == 0;
            }
            else if (v->tipo == VAL_OBJETO) r = clase_deriva(v->objeto->clase, t->clase);
            else if (v->tipo == VAL_ERROR)
                r = strcmp(v->error->tipo, nm) == 0 || strcmp(nm, "Error") == 0;
        } else if (t->tipo == VAL_DICCIONARIO && t->es_modulo == ES_ENUMERACION) {
            r = v->tipo == VAL_ENUMERADO && v->miembro->largo_enum == (int)strlen(nm) &&
                strncmp(v->miembro->texto, nm, (size_t)v->miembro->largo_enum) == 0;
        } else if (t->tipo == VAL_DICCIONARIO && t->es_modulo == 2) {
            InterfazLince *iface = (InterfazLince*)(uintptr_t)
                (long long)t->diccionario.valores[0]->numero;
            r = v->tipo == VAL_OBJETO && clase_implementa(v->objeto->clase, iface);
        } else {
            valor_destruir(v); valor_destruir(t);
            return error_rt("ErrorTipo", n->linea,
                "'%s' no es una clase, una interfaz ni un tipo; no se puede usar con 'es'.", nm);
        }
        valor_destruir(t);
    }
    valor_destruir(v);
    return valor_booleano(r);
}

/* Llama al método 'nombre' de 'izq' con 'der' de argumento; *ok = 0 si no
   existe (o no es de instancia con un parámetro). Consume una referencia de 'der'
   solo si lo llama; 'izq' siempre queda a cargo de quien llama. */
static Valor *llamar_operador(Valor *izq, const char *nombre, Valor *der, int *ok, int linea) {
    ClaseLince *cls = izq->objeto->clase;
    FuncionLince *f = buscar_metodo(cls, nombre);
    if (!f || f->estatico || f->abstracto || f->num_parametros != 1) { *ok = 0; return NULL; }
    *ok = 1;
    Valor *args[1] = { valor_retener(der) };
    return llamar_metodo(izq, cls, f, args, 1, linea);
}

/* Operadores sobre objetos: se resuelven con métodos de la clase
   (sumar, restar, multiplicar, dividir, modulo, igual, menor, mayor,
   menor_igual, mayor_igual). Devuelve NULL si 'op' no tiene método. Si lo
   resuelve, se hace cargo de 'izq' y 'der'. */
static Valor *operador_objeto(Valor *izq, Valor *der, const char *op, int linea) {
    static const char *const directos[][2] = {
        {"+", "sumar"}, {"-", "restar"}, {"*", "multiplicar"},
        {"/", "dividir"}, {"%", "modulo"},
    };
    int ok = 0;
    Valor *r = NULL;

    for (size_t i = 0; i < sizeof(directos) / sizeof(directos[0]); i++) {
        if (strcmp(op, directos[i][0]) == 0) {
            r = llamar_operador(izq, directos[i][1], der, &ok, linea);
            if (ok) { valor_destruir(izq); valor_destruir(der); return r ? r : valor_nulo(); }
            return NULL;
        }
    }

    int igual = -1, menor = -1;     /* -1 = desconocido */
    #define CONSULTA(var, nombre) do { \
        Valor *q = llamar_operador(izq, nombre, der, &ok, linea); \
        if (ok) { var = q ? es_verdadero(q) : 0; if (q) valor_destruir(q); } } while (0)

    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        CONSULTA(igual, "igual");
        if (igual < 0) {
            /* Sin 'igual': dos referencias al mismo objeto son iguales */
            if (der->tipo != VAL_OBJETO) return NULL;
            igual = izq->objeto == der->objeto;
        }
        int res = op[0] == '=' ? igual : !igual;
        valor_destruir(izq); valor_destruir(der);
        return valor_booleano(res);
    }

    int res;
    if (strcmp(op, "<") == 0) {
        CONSULTA(menor, "menor");
        if (menor < 0) return NULL;
        res = menor;
    } else if (strcmp(op, ">") == 0) {
        int mayor = -1;
        CONSULTA(mayor, "mayor");
        if (mayor >= 0) res = mayor;
        else {
            CONSULTA(menor, "menor"); CONSULTA(igual, "igual");
            if (menor < 0 || igual < 0) return NULL;
            res = !menor && !igual;
        }
    } else if (strcmp(op, "<=") == 0) {
        int me = -1;
        CONSULTA(me, "menor_igual");
        if (me >= 0) res = me;
        else {
            CONSULTA(menor, "menor"); CONSULTA(igual, "igual");
            if (menor < 0 || igual < 0) return NULL;
            res = menor || igual;
        }
    } else if (strcmp(op, ">=") == 0) {
        int ma = -1;
        CONSULTA(ma, "mayor_igual");
        if (ma >= 0) res = ma;
        else {
            CONSULTA(menor, "menor");
            if (menor < 0) return NULL;
            res = !menor;
        }
    } else {
        return NULL;
    }
    #undef CONSULTA
    valor_destruir(izq); valor_destruir(der);
    return valor_booleano(res);
}

static Valor *operar(Valor *izq, Valor *der, const char *op, int linea);

static Valor *exec_binario(Nodo *n, Entorno *e) {
    if (n->binario.operador[0] == 'e' && n->binario.operador[1] == 's' && !n->binario.operador[2])
        return exec_es(n, e);
    Valor *izq = ejecutar(n->binario.izquierda, e);
    const char *op = n->binario.operador;
    /* 'y' / 'o' en cortocircuito: si la izquierda ya decide, la derecha no se
       evalúa (así 'lista.longitud() > 0 y lista[0] == x' no falla con la lista vacía) */
    if ((op[0] == 'y' || op[0] == 'o') && !op[1] && izq->tipo != VAL_OBJETO) {
        int v = es_verdadero(izq);
        valor_destruir(izq);
        if (hay_error) return valor_nulo();
        if (op[0] == 'y' ? !v : v) return valor_booleano(v);
        Valor *der = ejecutar(n->binario.derecha, e);
        int d = es_verdadero(der);
        valor_destruir(der);
        if (hay_error) return valor_nulo();
        return valor_booleano(d);
    }
    Valor *der = ejecutar(n->binario.derecha,   e);
    /* Lo más frecuente, número con número, sin pasar por operar(). El
       resultado reutiliza uno de los dos si nadie más lo tiene. */
    if (izq->tipo == VAL_NUMERO && der->tipo == VAL_NUMERO) {
        double a = izq->numero, b = der->numero, r;
        int logico = -1;
        switch (op[0] | (op[1] << 8)) {
            case '+': r = a + b; break;
            case '-': r = a - b; break;
            case '*': r = a * b; break;
            case '<': logico = a < b;  break;
            case '>': logico = a > b;  break;
            case '<' | ('=' << 8): logico = a <= b; break;
            case '>' | ('=' << 8): logico = a >= b; break;
            case '=' | ('=' << 8): logico = a == b; break;
            case '!' | ('=' << 8): logico = a != b; break;
            default: return operar(izq, der, op, n->linea);   /* '/' y '%' con sus errores */
        }
        if (op[1] && op[2]) return operar(izq, der, op, n->linea);
        if (logico >= 0) {
            valor_destruir(izq); valor_destruir(der);
            return valor_booleano(logico);
        }
        if (izq->refs == 1) { izq->numero = r; valor_destruir(der); return izq; }
        if (der->refs == 1) { der->numero = r; valor_destruir(izq); return der; }
        valor_destruir(izq); valor_destruir(der);
        return valor_numero(r);
    }
    return operar(izq, der, op, n->linea);
}

/* izq op der, quedándose con los dos valores. También lo usan '+=', '-='... */
static Valor *operar(Valor *izq, Valor *der, const char *op, int linea) {
    Valor *resultado = NULL;

    /* Un objeto a la izquierda: los operadores los define su clase */
    if (izq->tipo == VAL_OBJETO && op[0] != 'y' && op[0] != 'o') {
        Valor *r = operador_objeto(izq, der, op, linea);
        if (r) return r;
    }

    /* Concatenación de texto */
    if (op[0] == '+' && !op[1] && (izq->tipo == VAL_TEXTO || der->tipo == VAL_TEXTO)) {
        char *si = izq->tipo == VAL_TEXTO ? izq->texto : valor_a_texto(izq);
        char *sd = der->tipo == VAL_TEXTO ? der->texto : valor_a_texto(der);
        size_t li = strlen(si), ld = strlen(sd);
        char *buf = malloc(li + ld + 1);
        memcpy(buf, si, li); memcpy(buf + li, sd, ld + 1);
        resultado = valor_texto_propio(buf);
        if (izq->tipo != VAL_TEXTO) free(si);
        if (der->tipo != VAL_TEXTO) free(sd);
        valor_destruir(izq); valor_destruir(der);
        return resultado;
    }

    /* Operaciones numéricas */
    if (izq->tipo == VAL_NUMERO && der->tipo == VAL_NUMERO) {
        double a = izq->numero, b = der->numero;
        char o0 = op[0], o1 = op[1];
        if (!o1) {
            switch (o0) {
                case '+': resultado = valor_numero(a + b); break;
                case '-': resultado = valor_numero(a - b); break;
                case '*': resultado = valor_numero(a * b); break;
                case '/':
                    if (b == 0) {
                        valor_error = valor_error_nuevo("ErrorMatematico", "División por cero.", linea);
                        hay_error = 1;
                        valor_destruir(izq);
                        valor_destruir(der);
                        return valor_nulo();
                    }
                    resultado = valor_numero(a / b);
                    break;
                case '%': resultado = valor_numero(fmod(a, b)); break;
                case '<': resultado = valor_booleano(a < b); break;
                case '>': resultado = valor_booleano(a > b); break;
            }
        } else if (o1 == '=' && !op[2]) {
            switch (o0) {
                case '<': resultado = valor_booleano(a <= b); break;
                case '>': resultado = valor_booleano(a >= b); break;
                case '=': resultado = valor_booleano(a == b); break;
                case '!': resultado = valor_booleano(a != b); break;
            }
        }
    }

    /* Comparaciones entre texto */
    if (!resultado && izq->tipo == VAL_TEXTO && der->tipo == VAL_TEXTO) {
        int cmp = strcmp(izq->texto, der->texto);
        if (strcmp(op, "==") == 0) resultado = valor_booleano(cmp == 0);
        else if (strcmp(op, "!=") == 0) resultado = valor_booleano(cmp != 0);
    }

    /* Bytes: iguales si tienen el mismo contenido */
    if (!resultado && izq->tipo == VAL_BYTES && der->tipo == VAL_BYTES &&
        (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0)) {
        int ig = izq->bytes.cantidad == der->bytes.cantidad &&
                 memcmp(izq->bytes.datos, der->bytes.datos, (size_t)izq->bytes.cantidad) == 0;
        resultado = valor_booleano(op[0] == '=' ? ig : !ig);
    }

    /* Enumeraciones */
    if (!resultado && (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0)) {
        int ig = iguales_enum(izq, der);
        if (ig >= 0) resultado = valor_booleano(op[0] == '=' ? ig : !ig);
    }

    /* Tareas: iguales si son la misma */
    if (!resultado && izq->tipo == VAL_TAREA && der->tipo == VAL_TAREA &&
        (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0))
        resultado = valor_booleano((izq->tarea == der->tarea) == (op[0] == '='));

    /* Comparaciones entre lógicos. Sin esto caerían en el caso general
       de más abajo, que sólo sabe comparar nulos, y 'verdadero ==
       verdadero' daba falso. */
    if (!resultado && izq->tipo == VAL_BOOLEANO && der->tipo == VAL_BOOLEANO) {
        int a = izq->booleano ? 1 : 0;
        int b = der->booleano ? 1 : 0;
        if (strcmp(op, "==") == 0) resultado = valor_booleano(a == b);
        else if (strcmp(op, "!=") == 0) resultado = valor_booleano(a != b);
    }

    /* Lógica booleana */
    if (!resultado) {
        if (strcmp(op, "y") == 0)
            resultado = valor_booleano(es_verdadero(izq) && es_verdadero(der));
        else if (strcmp(op, "o") == 0)
            resultado = valor_booleano(es_verdadero(izq) || es_verdadero(der));
        else if (strcmp(op, "==") == 0)
            resultado = valor_booleano(izq->tipo == VAL_NULO && der->tipo == VAL_NULO);
        else if (strcmp(op, "!=") == 0)
            resultado = valor_booleano(!(izq->tipo == VAL_NULO && der->tipo == VAL_NULO));
    }

    valor_destruir(izq);
    valor_destruir(der);

    if (!resultado) {
        /* Con un error ya pendiente, esto es su consecuencia y no la causa: la
           operación ha fallado porque uno de los lados es el nulo que devolvió
           algo que ya falló. Se propaga el de verdad y no se dice nada aquí —
           es lo que tapaba el 'ErrorRecursion' al pasarse de MAX_PROFUNDIDAD.

           Y cuando sí es la causa, error capturable en vez de exit(1): matar
           el proceso impedía que un intentar/capturar se recuperase. */
        if (hay_error) return valor_nulo();

        char msg[192];
        snprintf(msg, sizeof(msg),
            "Operación '%s' no soportada entre esos tipos.", op);
        valor_error = valor_error_nuevo("ErrorTipo", msg, linea);
        hay_error   = 1;
        return valor_nulo();
    }
    return resultado;
}

static Valor *ejecutar(Nodo *n, Entorno *e);

/* Valida que un valor coincide con el tipo declarado */
static void validar_tipo(Valor *v, TipoDato tipo, const char *contexto,
                          const char *nombre, int linea) {
    if (tipo == TIPO_CUALQUIERA || tipo == TIPO_CLASE) return;
    int ok = 1;
    switch (tipo) {
        case TIPO_NUMERO:      ok = (v->tipo == VAL_NUMERO);      break;
        /* un valor de enumeración se escribe y se compara como su texto:
           así siguen valiendo las funciones de cuando lo era */
        case TIPO_TEXTO:       ok = (v->tipo == VAL_TEXTO || v->tipo == VAL_ENUMERADO); break;
        case TIPO_LOGICO:      ok = (v->tipo == VAL_BOOLEANO);    break;
        case TIPO_LISTA:       ok = (v->tipo == VAL_LISTA);       break;
        case TIPO_DICCIONARIO: ok = (v->tipo == VAL_DICCIONARIO); break;
        case TIPO_NULO:        ok = (v->tipo == VAL_NULO);        break;
        case TIPO_TAREA:       ok = (v->tipo == VAL_TAREA);       break;
        default: break;
    }
    if (!ok) {
        const char *esperado[] = {
            "numero", "texto", "logico", "lista", "diccionario", "nulo", "clase", "funcion",
            "cualquiera", "tarea"
        };
        const char *recibido[] = {
            "numero", "texto", "logico", "nulo", "funcion",
            "lista", "diccionario", "clase", "objeto", "error", "generador", "bytes", "tarea",
            "valor de enumeración"
        };
        char msg[256];
        snprintf(msg, sizeof(msg),
            "Error de tipo en %s '%s': se esperaba '%s' pero se recibió '%s'.",
            contexto, nombre,
            tipo <= TIPO_TAREA ? esperado[tipo] : "?",
            v->tipo <= VAL_ENUMERADO ? recibido[v->tipo] : "?");
        valor_error = valor_error_nuevo("ErrorTipo", msg, linea);
        hay_error = 1;
    }
}

/* El valor que devuelve una función: el de su 'devolver' si lo hubo, y nulo
   si no. Hay que mirar hay_retorno ANTES de crear el nulo — creándolo primero
   y pisándolo después se fugaba uno por cada llamada que devolviera algo, que
   en mapear/filtrar/reducir es uno por elemento de la lista. */
static Valor *tomar_retorno(void) {
    if (!hay_retorno) return valor_nulo();
    Valor *r      = valor_retorno;
    valor_retorno = NULL;
    hay_retorno   = 0;
    return r;
}

/* ─────────────────────────────────────────
   TAREAS
   'tarea nombre(...) { ... }' es una función que, al llamarla, empieza a
   correr y devuelve la tarea en cuanto se pone a esperar: esperar() (al
   siguiente fotograma), esperar(segundos) o esperar(otra_tarea). Cada tarea
   tiene su propia pila de C (corrutina.h), así que se puede esperar desde
   cualquier función que llame, y se sigue justo donde se quedó.

   Quien tiene el bucle de fotogramas (juego.presentar) las hace seguir con
   interprete_avanzar_tareas(), una vez por fotograma y con su reloj. Sin él,
   un esperar() fuera de las tareas es quien las hace avanzar. Las que quedan
   al acabar el programa se paran, y se ejecutan sus 'finalmente'.
───────────────────────────────────────── */
#define ERROR_PARADA "TareaParada"   /* deshace la tarea que se para; nadie lo captura */

enum { TAREA_NUEVA, TAREA_CORRIENDO, TAREA_ESPERANDO, TAREA_TERMINADA };
enum { ESPERA_FOTOGRAMA, ESPERA_SEGUNDOS, ESPERA_TAREA };

struct TareaLince {
    Corrutina    *co;
    FuncionLince *f;
    Entorno      *fn_e;         /* sus parámetros */
    Valor        *esto;         /* el objeto, si es un método (lo retiene) */
    int           estado, parar, espera;
    double        hasta;        /* ESPERA_SEGUNDOS: hasta qué hora */
    long          fotograma;    /* _tareas_fotograma cuando se puso a esperar */
    Valor        *otra;         /* ESPERA_TAREA: a cuál */
    Valor        *resultado;    /* lo que devolvió, al acabar */
    int           linea;        /* donde se llamó */
    /* Lo del intérprete que es suyo mientras está parada */
    int           niveles;      /* llamadas abiertas dentro de ella */
    const char  **pila_nombre;
    int          *pila_linea;
    int           pila_cap;
    Valor        *esto_actual;
    ClaseLince   *clase_actual;
    int           en_biblioteca;
    int           es_generador; /* el cuerpo de un 'generador', no una tarea */
    Valor        *producido;    /* lo último que ha producido */
    char          archivo[sizeof _archivo_actual];
};

static TareaLince *_tarea_actual = NULL;     /* la que corre ahora; NULL: el programa */
static Valor     **_tareas       = NULL;     /* las que no han acabado, en orden de inicio */
static int         _ntareas      = 0, _cap_tareas = 0;
static long        _tareas_fotograma = 0;    /* veces que se han avanzado */
static double    (*_reloj_tareas)(void) = NULL;
static int         _avanzando    = 0;

void interprete_reloj_tareas(double (*reloj)(void)) { _reloj_tareas = reloj; }
static double tareas_ahora(void) { return _reloj_tareas ? _reloj_tareas() : corrutina_reloj(); }

/* La línea desde la que se ha llamado a la función nativa que corre ahora */
static int linea_llamada(void) {
    return profundidad > 0 && profundidad < PILA_MAX ? _pila_linea[profundidad] : 0;
}

static void tarea_liberar(TareaLince *t) {
    corrutina_destruir(t->co);
    valor_destruir(t->resultado);
    valor_destruir(t->otra);
    valor_destruir(t->producido);
    free(t->pila_nombre);
    free(t->pila_linea);
    free(t);
}

static const char *tarea_nombre(TareaLince *t) { return t->f->nombre; }

static int es_tarea_parada(void) {
    return hay_error && valor_error && valor_error->tipo == VAL_ERROR &&
           strcmp(valor_error->error->tipo, ERROR_PARADA) == 0;
}

/* Sigue la tarea 't' hasta que espere o acabe. Mientras corre, la pila de
   llamadas, el objeto de 'mi', etc. son los suyos; al volver, los de quien
   la ha hecho seguir. */
static void tarea_entrar(TareaLince *t) {
    TareaLince *antes   = _tarea_actual;
    int         base    = profundidad < 0 ? 0 : profundidad;
    Valor      *esto_g  = esto_actual;
    ClaseLince *clase_g = clase_actual;
    int         bib_g   = _en_biblioteca;
    char        archivo_g[sizeof _archivo_actual];
    memcpy(archivo_g, _archivo_actual, sizeof archivo_g);

    for (int i = 0; i < t->niveles && base + 1 + i < PILA_MAX; i++) {
        _pila_nombre[base + 1 + i] = t->pila_nombre[i];
        _pila_linea[base + 1 + i]  = t->pila_linea[i];
    }
    profundidad      = base + t->niveles;
    esto_actual      = t->esto_actual;
    clase_actual     = t->clase_actual;
    _en_biblioteca   = t->en_biblioteca;
    memcpy(_archivo_actual, t->archivo, sizeof _archivo_actual);
    _tarea_actual    = t;
    t->estado        = TAREA_CORRIENDO;

    corrutina_entrar(t->co);

    _tarea_actual = antes;
    t->niveles = profundidad - base;
    if (t->niveles < 0) t->niveles = 0;
    if (t->niveles > t->pila_cap) {
        t->pila_cap    = t->niveles + 8;
        t->pila_nombre = realloc(t->pila_nombre, sizeof(const char *) * (size_t)t->pila_cap);
        t->pila_linea  = realloc(t->pila_linea, sizeof(int) * (size_t)t->pila_cap);
    }
    for (int i = 0; i < t->niveles && base + 1 + i < PILA_MAX; i++) {
        t->pila_nombre[i] = _pila_nombre[base + 1 + i];
        t->pila_linea[i]  = _pila_linea[base + 1 + i];
    }
    t->esto_actual   = esto_actual;
    t->clase_actual  = clase_actual;
    t->en_biblioteca = _en_biblioteca;
    memcpy(t->archivo, _archivo_actual, sizeof t->archivo);

    profundidad      = base;
    esto_actual      = esto_g;
    clase_actual     = clase_g;
    _en_biblioteca   = bib_g;
    memcpy(_archivo_actual, archivo_g, sizeof archivo_g);
}

/* Lo que corre en la pila de la tarea */
static void tarea_cuerpo(void *arg) {
    TareaLince *t = (TareaLince *)arg;
    valor_destruir(ejecutar(t->f->cuerpo, t->fn_e));   /* ver la nota de NODO_LLAMADA */
    Valor *r = tomar_retorno();
    hay_salto = SALTO_NINGUNO;
    if (es_tarea_parada()) {                /* se ha parado: no es un error */
        valor_destruir(valor_error);
        valor_error = NULL;
        hay_error   = 0;
    } else if (!hay_error && !t->es_generador) {
        validar_tipo(r, t->f->tipo_tarea, "retorno de tarea", t->f->nombre, linea_llamada());
    }
    if (t->es_generador) valor_destruir(r);     /* un 'devolver' en un generador no le llega a nadie */
    else t->resultado = r;
    entorno_destruir(t->fn_e);
    t->fn_e = NULL;
    valor_destruir(t->esto);
    t->esto = NULL;
    t->estado = TAREA_TERMINADA;
    /* Un error se queda puesto: le llega a quien la ha hecho seguir */
}

static Valor *valor_tarea(TareaLince *t) {
    Valor *v = valor_alloc();
    v->tipo = VAL_TAREA;
    v->refs = 1; v->es_modulo = 0;
    v->tarea = t;
    return v;
}

/* Llamar a una tarea: la crea con los parámetros ya puestos en 'fn_e' y la
   sigue hasta que espere por primera vez (o acabe) */
static TareaLince *tarea_nueva(FuncionLince *f, Entorno *fn_e, int linea) {
    TareaLince *t = calloc(1, sizeof(TareaLince));
    if (t) t->co = corrutina_crear(tarea_cuerpo, t);
    if (!t || !t->co) {
        free(t);
        error_rt("Error", linea, "No hay memoria para empezar '%s'.", f->nombre);
        return NULL;
    }
    t->f = f;
    t->fn_e = fn_e;
    fn_e->refs++;
    t->esto          = esto_actual ? valor_retener(esto_actual) : NULL;
    t->esto_actual   = esto_actual;
    t->clase_actual  = clase_actual;
    t->en_biblioteca = _en_biblioteca;
    t->linea         = linea;
    memcpy(t->archivo, _archivo_actual, sizeof t->archivo);
    return t;
}

static Valor *tarea_empezar(FuncionLince *f, Entorno *fn_e) {
    TareaLince *t = tarea_nueva(f, fn_e, linea_llamada());
    if (!t) return valor_nulo();
    Valor *v = valor_tarea(t);
    if (_ntareas == _cap_tareas) {
        _cap_tareas = _cap_tareas ? _cap_tareas * 2 : 16;
        _tareas = realloc(_tareas, sizeof(Valor *) * (size_t)_cap_tareas);
    }
    _tareas[_ntareas++] = valor_retener(v);
    tarea_entrar(t);
    return v;
}

/* Una función declarada con 'tarea': su llamada da la tarea, y el tipo
   declarado es el de lo que devuelve al acabar */
static void marcar_tarea(FuncionLince *f, Nodo *n) {
    if (!n->funcion.es_tarea) return;
    f->es_tarea     = 1;
    f->tipo_tarea   = f->tipo_retorno;
    f->tipo_retorno = TIPO_CUALQUIERA;
}

/* El cuerpo de una función ya llamada, con sus parámetros en 'fn_e'. Si es
   una tarea, la empieza y deja la tarea como lo que devuelve la llamada. */
static Valor *cuerpo_de_tarea(FuncionLince *f, Entorno *fn_e) {
    Valor *t = tarea_empezar(f, fn_e);
    if (hay_error) { valor_destruir(t); return valor_nulo(); }
    valor_retorno = t;
    hay_retorno   = 1;
    return valor_nulo();
}
static inline Valor *ejecutar_cuerpo(FuncionLince *f, Entorno *fn_e) {
    if (__builtin_expect(!f->es_tarea, 1)) return ejecutar(f->cuerpo, fn_e);
    return cuerpo_de_tarea(f, fn_e);
}

/* Suelta las que han acabado */
static void tareas_recoger(void) {
    int j = 0;
    for (int i = 0; i < _ntareas; i++) {
        if (_tareas[i]->tarea->estado == TAREA_TERMINADA) valor_destruir(_tareas[i]);
        else _tareas[j++] = _tareas[i];
    }
    _ntareas = j;
}

static int tarea_lista(TareaLince *t, double ahora) {
    switch (t->espera) {
        case ESPERA_TAREA:    return t->otra->tarea->estado == TAREA_TERMINADA;
        case ESPERA_SEGUNDOS: return t->fotograma < _tareas_fotograma && ahora >= t->hasta;
        default:              return t->fotograma < _tareas_fotograma;
    }
}

void interprete_avanzar_tareas(void) {
    if (_avanzando || hay_error) return;
    _avanzando = 1;
    double ahora = tareas_ahora();
    int n = _ntareas;                 /* las que empiecen ahora ya han corrido */
    for (int i = 0; i < n && !hay_error; i++) {
        TareaLince *t = _tareas[i]->tarea;
        if (t->estado == TAREA_ESPERANDO && tarea_lista(t, ahora)) tarea_entrar(t);
    }
    _tareas_fotograma++;
    tareas_recoger();
    _avanzando = 0;
}

/* Para la tarea: se deshace desde donde esperaba, ejecutando sus
   'finalmente'. Si es la que corre, se deshace en cuanto vuelva esto. */
static void tarea_parar(TareaLince *t) {
    if (t->estado == TAREA_TERMINADA || t->parar) return;
    t->parar = 1;
    if (t == _tarea_actual)
        error_rt(ERROR_PARADA, linea_llamada(), "La tarea '%s' se ha parado.", t->f->nombre);
    else if (t->estado == TAREA_ESPERANDO)
        tarea_entrar(t);
    /* Si no, ha llamado a la que corre ahora: se para en cuanto espere */
}

void interprete_parar_tareas(void) {
    int    error_g = hay_error;
    Valor *valor_g = valor_error;
    hay_error   = 0;
    valor_error = NULL;
    for (int i = 0; i < _ntareas; i++) {        /* las que empiecen al pararse, también */
        tarea_parar(_tareas[i]->tarea);
        if (hay_error) {                         /* un error al pararse ya no va a ningún sitio */
            valor_destruir(valor_error);
            valor_error = NULL;
            hay_error   = 0;
        }
    }
    for (int i = 0; i < _ntareas; i++) valor_destruir(_tareas[i]);
    free(_tareas);
    _tareas  = NULL;
    _ntareas = _cap_tareas = 0;
    hay_error   = error_g;
    valor_error = valor_g;
}

/* esperar() fuera de las tareas: las hace avanzar hasta que se cumpla, a
   60 vueltas por segundo con el reloj real */
static Valor *esperar_fuera(int espera, double segundos, Valor *otra, int linea) {
    if (_reloj_tareas)
        return error_rt("Error", linea,
            "En un juego, esperar() solo puede ir dentro de una tarea: el bucle del juego no "
            "se puede parar. Las tareas avanzan solas en cada juego.presentar().");
    if (_avanzando)
        return error_rt("Error", linea, "esperar() no puede ir aquí: las tareas están avanzando.");
    double meta = corrutina_reloj() + segundos;
    while (1) {
        double vuelta = corrutina_reloj();
        interprete_avanzar_tareas();
        if (hay_error) return valor_nulo();
        if (espera == ESPERA_FOTOGRAMA) break;
        if (espera == ESPERA_TAREA ? otra->tarea->estado == TAREA_TERMINADA
                                   : corrutina_reloj() >= meta) break;
        double dormir = 1.0 / 60 - (corrutina_reloj() - vuelta);
        if (espera == ESPERA_SEGUNDOS && meta - corrutina_reloj() < dormir) dormir = meta - corrutina_reloj();
        corrutina_dormir(dormir);
    }
    return otra ? valor_copiar(otra->tarea->resultado) : valor_nulo();
}

/* esperar() · esperar(segundos) · esperar(tarea) */
static Valor *fn_esperar(Valor **a, int n) {
    int linea = linea_llamada();
    int espera = ESPERA_FOTOGRAMA;
    double segundos = 0;
    Valor *otra = NULL;
    if (n > 1)
        return error_rt("ErrorArgumento", linea,
            "esperar() recibe como mucho una cosa: los segundos, o la tarea a la que esperar.");
    if (n == 1) {
        if (a[0]->tipo == VAL_NUMERO) {
            segundos = a[0]->numero;
            if (!(segundos >= 0) || isinf(segundos))
                return error_rt("Error", linea, "esperar(): los segundos no pueden ser negativos.");
            if (segundos > 0) espera = ESPERA_SEGUNDOS;
        } else if (a[0]->tipo == VAL_TAREA) {
            otra = a[0];
            espera = ESPERA_TAREA;
        } else {
            return error_rt("ErrorTipo", linea,
                "esperar() espera los segundos (un número) o una tarea; sin nada, espera al siguiente fotograma.");
        }
    }
    TareaLince *t = _tarea_actual;
    if (t && t->es_generador)
        return error_rt("Error", linea,
            "esperar() no puede ir dentro de un generador: el generador da sus valores a quien lo "
            "recorre, y es ese quien puede esperar.");
    if (otra && otra->tarea == t)
        return error_rt("Error", linea, "Una tarea no puede esperarse a sí misma.");
    if (!t) return esperar_fuera(espera, segundos, otra, linea);
    if (t->parar)
        return error_rt(ERROR_PARADA, linea, "La tarea '%s' se ha parado.", t->f->nombre);
    if (otra && otra->tarea->estado == TAREA_TERMINADA)
        return valor_copiar(otra->tarea->resultado);

    t->espera    = espera;
    t->fotograma = _tareas_fotograma;
    t->hasta     = tareas_ahora() + segundos;
    t->otra      = otra ? valor_retener(otra) : NULL;
    t->estado    = TAREA_ESPERANDO;
    corrutina_salir(t->co);               /* ... y aquí se sigue más tarde */

    Valor *r = valor_nulo();
    if (t->otra) {
        valor_destruir(r);
        r = valor_copiar(t->otra->tarea->resultado);
        valor_destruir(t->otra);
        t->otra = NULL;
    }
    if (t->parar) {
        valor_destruir(r);
        return error_rt(ERROR_PARADA, linea, "La tarea '%s' se ha parado.", t->f->nombre);
    }
    return r;
}

/* ── Generadores ──
   El cuerpo de un generador corre como una tarea que no está en la lista de
   las que avanzan: 'para cada' lo hace seguir hasta que produce un valor, y
   al salir del bucle lo para (con sus 'finalmente') si no había acabado. */

/* El siguiente valor, o NULL si ha acabado o ha fallado (con hay_error) */
static Valor *generador_siguiente(TareaLince *g) {
    if (g->estado == TAREA_TERMINADA) return NULL;
    tarea_entrar(g);
    if (hay_error || g->estado == TAREA_TERMINADA) return NULL;
    Valor *v = g->producido;
    g->producido = NULL;
    return v;
}

/* Lo deshace si se ha quedado a medias, sin tocar lo que estuviera pendiente
   en quien lo recorría (un error, un 'devolver', un 'romper') */
static void generador_cerrar(TareaLince *g) {
    if (g->estado != TAREA_ESPERANDO) return;
    int    error_g = hay_error,   ret_g = hay_retorno, salto_g = hay_salto;
    Valor *verr_g  = valor_error, *vret_g = valor_retorno;
    hay_error = 0; valor_error = NULL; hay_retorno = 0; valor_retorno = NULL; hay_salto = SALTO_NINGUNO;
    g->parar = 1;
    tarea_entrar(g);
    if (hay_error && (error_g || ret_g)) {       /* ya había algo pendiente: gana eso */
        valor_destruir(valor_error);
        valor_error = NULL;
        hay_error   = 0;
    }
    if (!hay_error) { hay_error = error_g; valor_error = verr_g; }
    else valor_destruir(verr_g);
    hay_retorno = ret_g; valor_retorno = vret_g; hay_salto = salto_g;
}

static void generador_liberar(TareaLince *g) {
    generador_cerrar(g);
    tarea_liberar(g);
}

/* producir valor: lo da a quien recorre el generador y espera a que pida otro */
static Valor *exec_producir(Nodo *n, Entorno *e) {
    TareaLince *g = _tarea_actual;
    if (!g || !g->es_generador)
        return error_rt("Error", n->linea, "'producir' solo puede usarse dentro de un generador.");
    Valor *v = ejecutar(n->producir, e);
    if (hay_error) { valor_destruir(v); return valor_nulo(); }
    if (g->parar) { valor_destruir(v); return error_rt(ERROR_PARADA, n->linea, "El generador se ha parado."); }
    g->producido = v;
    g->estado    = TAREA_ESPERANDO;
    corrutina_salir(g->co);
    if (g->parar) return error_rt(ERROR_PARADA, n->linea, "El generador se ha parado.");
    return valor_nulo();
}

/* tarea.terminada() · tarea.parar() */
static Valor *metodo_tarea(Valor *obj, const char *met, Nodo *n, Entorno *e) {
    (void)e;
    TareaLince *t = obj->tarea;
    Valor *r;
    if (n->metodo.num_argumentos != 0)
        r = error_rt("ErrorArgumento", n->linea, "%s() no recibe nada.", met);
    else if (strcmp(met, "terminada") == 0)
        r = valor_booleano(t->estado == TAREA_TERMINADA);
    else if (strcmp(met, "parar") == 0) {
        tarea_parar(t);
        r = valor_nulo();
    } else
        r = error_rt("Error", n->linea,
            "Una tarea no tiene el método '%s'. Tiene terminada() y parar(); para esperar a que acabe, esperar(tarea).", met);
    valor_destruir(obj);
    return r;
}

/* ─────────────────────────────────────────
   ORDENACIÓN DE LISTAS
───────────────────────────────────────── */

/* Orden natural: números por valor, textos alfabéticamente, lógicos con
   falso antes que verdadero. Entre tipos distintos se ordena por tipo,
   para que el resultado sea predecible en vez de depender del orden de
   entrada. */
static int valor_comparar_natural(Valor *a, Valor *b) {
    if (a->tipo == VAL_NUMERO && b->tipo == VAL_NUMERO)
        return a->numero < b->numero ? -1 : (a->numero > b->numero ? 1 : 0);
    if (a->tipo == VAL_TEXTO && b->tipo == VAL_TEXTO)
        return strcmp(a->texto, b->texto);
    if (a->tipo == VAL_BOOLEANO && b->tipo == VAL_BOOLEANO)
        return (a->booleano ? 1 : 0) - (b->booleano ? 1 : 0);
    return (int)a->tipo - (int)b->tipo;
}

/* Comparador de ordenar(): el natural si no se pasó función, y si se
   pasó, lo que devuelva ella (negativo, cero o positivo). */
typedef struct {
    Valor *fn;   /* función comparadora de Lince, o NULL */
} CtxOrden;

static int orden_comparar(CtxOrden *ctx, Valor *a, Valor *b) {
    if (!ctx->fn) return valor_comparar_natural(a, b);

    FuncionLince *f = ctx->fn->funcion;
    Valor *r = NULL;

    if (f->entorno_closure == NULL) {
        /* Comparador nativo */
        typedef Valor *(*FnNativa)(Valor**, int);
        FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
        Valor *args[2] = { valor_copiar(a), valor_copiar(b) };
        r = fn ? fn(args, 2) : valor_nulo();
        valor_destruir(args[0]);
        valor_destruir(args[1]);
    } else {
        /* El entorno se queda con los argumentos, así que no se liberan
           aquí: los destruye entorno_destruir al salir. */
        Entorno *fn_e = entorno_crear(f->entorno_closure);
        if (f->num_parametros >= 1)
            entorno_definir(fn_e, f->parametros[0].nombre, valor_copiar(a), 0);
        if (f->num_parametros >= 2)
            entorno_definir(fn_e, f->parametros[1].nombre, valor_copiar(b), 0);
        /* ver la nota de NODO_LLAMADA: el valor del cuerpo lo posee quien
           llama. Aquí importa más que en otros sitios, porque esto se llama
           O(n log n) veces por cada ordenar(). */
        valor_destruir(ejecutar_cuerpo(f, fn_e));
        if (hay_retorno) {
            r = valor_retorno; valor_retorno = NULL; hay_retorno = 0;
        }
        entorno_destruir(fn_e);
    }

    int res = 0;
    if (r && r->tipo == VAL_NUMERO)
        res = r->numero < 0 ? -1 : (r->numero > 0 ? 1 : 0);
    if (r) valor_destruir(r);
    return res;
}

/* Mezcla ordenada de [ini,med) con [med,fin) sobre 'src', dejando el
   resultado en 'tmp' y copiándolo de vuelta. */
static void orden_mezclar(Valor **src, Valor **tmp, int ini, int med, int fin,
                          CtxOrden *ctx) {
    int i = ini, j = med, k = ini;
    while (i < med && j < fin) {
        /* <= 0 para que la ordenación sea estable: ante elementos
           equivalentes se conserva el orden original. */
        if (orden_comparar(ctx, src[i], src[j]) <= 0) tmp[k++] = src[i++];
        else                                          tmp[k++] = src[j++];
    }
    while (i < med) tmp[k++] = src[i++];
    while (j < fin) tmp[k++] = src[j++];
    for (int x = ini; x < fin; x++) src[x] = tmp[x];
}

/* Merge sort: estable y O(n log n), a diferencia de la burbuja que hay
   que escribir a mano en los ejemplos. */
static void orden_merge(Valor **src, Valor **tmp, int ini, int fin,
                        CtxOrden *ctx) {
    if (fin - ini <= 1 || hay_error) return;
    int med = ini + (fin - ini) / 2;
    orden_merge(src, tmp, ini, med, ctx);
    orden_merge(src, tmp, med, fin, ctx);
    orden_mezclar(src, tmp, ini, med, fin, ctx);
}

/* ── Fuentes de los 'importar' ──
   Un programa empaquetado (lince compilar) lleva dentro los archivos que
   importa; el anfitrión que lo arranca registra un lector que los sirve por
   su ruta. Si el lector no los tiene, se leen del disco como siempre. */
static const char *(*_lector_embebido)(const char *ruta, size_t *largo) = NULL;

void interprete_fijar_lector(const char *(*lector)(const char *ruta, size_t *largo)) {
    _lector_embebido = lector;
}

/* Contenido del archivo 'ruta' (con malloc, terminado en '\0'; *largo = bytes
   leídos), o NULL si no existe. Sirve igual para texto y para binarios. */
char *interprete_leer_bytes(const char *ruta, size_t *largo) {
    if (_lector_embebido) {
        size_t n = 0;
        const char *emb = _lector_embebido(ruta, &n);
        if (emb) {
            char *c = malloc(n + 1);
            memcpy(c, emb, n); c[n] = '\0';
            if (largo) *largo = n;
            return c;
        }
    }
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    rewind(f);
    if (tam < 0) { fclose(f); return NULL; }
    char *codigo = malloc((size_t)tam + 1);
    size_t leido = fread(codigo, 1, (size_t)tam, f);
    codigo[leido] = '\0';
    fclose(f);
    if (largo) *largo = leido;
    return codigo;
}

/* Contenido del archivo 'ruta' (con malloc), o NULL si no existe */
char *interprete_leer_fuente(const char *ruta) {
    return interprete_leer_bytes(ruta, NULL);
}

/* Ruta de 'importar "./x"' hecho desde 'archivo_actual': primero junto a ese
   fichero y, si ahí no está, en el directorio de trabajo. 1 si existe. */
int interprete_resolver_import(const char *archivo_actual, const char *nombre,
                               char *ruta, size_t max) {
    if (archivo_actual && archivo_actual[0]) {
        char dir[1024];
        _directorio_de(archivo_actual, dir, sizeof(dir));
        if (dir[0]) {
            snprintf(ruta, max, "%s/%s.lince", dir, nombre);
            char *c = interprete_leer_fuente(ruta);
            if (c) { free(c); return 1; }
        }
    }
    snprintf(ruta, max, "%s.lince", nombre);
    char *c = interprete_leer_fuente(ruta);
    if (c) { free(c); return 1; }
    return 0;
}

/* El caso NODO_IMPORTAR, fuera de ejecutar() a propósito.
 *
 * ejecutar() es recursiva y su marco de pila se paga en cada nodo del árbol.
 * Este caso tiene tres buffers de ruta de 1 KB que no se usan en ningún otro
 * sitio, y ahí dentro engordaban el marco de TODAS las llamadas: con la
 * recursión profunda de tests/lince/test_recursion.lince eso se traducía en
 * reventar los 8 MB de pila por defecto. Aquí sólo se pagan al importar. */
static Valor *ejecutar_importar(Nodo *n, Entorno *e) {
        const char *nombre = n->importar.nombre;
        const char *alias  = n->importar.alias;

        /* ¿Es un paquete instalado? paquete:nombre */
        if (strncmp(nombre, "paquete:", 8) == 0) {
            const char *nom_paquete = nombre + 8;
            char ruta[1024];
            if (!paquetes_resolver(nom_paquete, ruta, sizeof(ruta))) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "El paquete '%s' no está instalado.\n"
                    "   Instálalo con: lince instalar <url>",
                    nom_paquete);
                valor_error = valor_error_nuevo("Error", msg, n->linea);
                hay_error = 1;
                return valor_nulo();
            }
            /* Ejecutar el archivo del paquete en el entorno actual */
            char *codigo = interprete_leer_fuente(ruta);
            if (!codigo) {
                valor_error = valor_error_nuevo("Error",
                    "No se pudo leer el paquete.", n->linea);
                hay_error = 1;
                return valor_nulo();
            }
            if (importado_ya(ruta)) { free(codigo); return valor_nulo(); }

            Lexer  *lx  = lexer_crear(codigo);
            int     cnt = 0;
            Token  *tok = lexer_tokenizar(lx, &cnt);
            Parser *pa  = parser_crear(tok, cnt);
            Nodo   *ast = parser_parsear(pa);
            valor_destruir(ejecutar(ast, e));   /* ver la nota de NODO_LLAMADA */
            /* No destruimos el AST — las funciones definidas lo referencian */
            parser_destruir(pa);
            lexer_destruir(lx);
            free(codigo);
            return valor_nulo();
        }

        /* ¿Es un archivo local relativo? ./archivo o ../archivo */
        if (strncmp(nombre, "./", 2) == 0 || strncmp(nombre, "../", 3) == 0) {
            /* Se busca primero junto al fichero que importa, y sólo si ahí
             * no está, en el directorio de trabajo. El orden importa:
             * resolviendo sólo contra el cwd, una librería en Lince se
             * podía usar únicamente desde la carpeta en la que vivía, lo
             * que la deja en «fichero que se incluye» y no en librería. El
             * respaldo contra el cwd se mantiene para no romper nada. */
            char ruta[1024];

            if (!interprete_resolver_import(_archivo_actual, nombre, ruta, sizeof(ruta))) {
                char msg[384];
                snprintf(msg, sizeof(msg),
                    "No se encontró '%s.lince', ni junto a '%s' ni en el "
                    "directorio actual.", nombre,
                    _archivo_actual[0] ? _archivo_actual : "(sin fichero)");
                valor_error = valor_error_nuevo("Error", msg, n->linea);
                hay_error = 1;
                return valor_nulo();
            }
            /* La idempotencia es cosa del importar SIN alias, que vuelca
               nombres en este ámbito y por tanto chocaría consigo mismo.
               Con alias, cada importación construye su propio ámbito y su
               propio diccionario, así que no se salta ni se anota: si no,
               un 'como geo' seguido de un 'como otro' no definiría 'otro',
               y un 'como geo' dejaría muda una importación simple
               posterior. Repetir el mismo alias lo caza el error de
               redefinición de más abajo. */
            if (!alias && importado_ya(ruta)) return valor_nulo();
            char *codigo = interprete_leer_fuente(ruta);
            recarga_vigilar(ruta);

            Lexer  *lx  = lexer_crear(codigo);
            int     cnt = 0;
            Token  *tok = lexer_tokenizar(lx, &cnt);
            Parser *pa  = parser_crear(tok, cnt);
            Nodo   *ast = parser_parsear(pa);

            /* Mientras corre el fichero importado, él es el fichero actual:
               así sus propios 'importar "./x"' se resuelven junto a él. */
            char anterior[1024];
            snprintf(anterior, sizeof(anterior), "%s", _archivo_actual);
            interprete_fijar_archivo(ruta);

            if (alias) {
                /* Con alias, el fichero corre en su propio ámbito y lo que
                   defina se expone bajo ese nombre, sin volcarse aquí. El
                   padre es el ámbito actual para que la librería alcance
                   escribir(), los módulos estándar y demás. */
                Entorno *mod_e = entorno_crear(e);
                valor_destruir(ejecutar(ast, mod_e));
                if (!hay_error) {
                    Valor *dic = empaquetar_modulo(mod_e);
                    if (redefinido(e, alias, "este 'importar ... como'", 0,
                                   n->linea))
                        valor_destruir(dic);
                    else
                        entorno_definir(e, alias, dic, 0);
                }
            } else {
                valor_destruir(ejecutar(ast, e));   /* ver la nota de NODO_LLAMADA */
            }

            interprete_fijar_archivo(anterior);

            /* No destruimos el AST — las funciones definidas lo referencian */
            parser_destruir(pa);
            lexer_destruir(lx);
            free(codigo);
            return valor_nulo();
        }

        /* Módulo estándar */
        modulo_cargar(nombre, e);
        return valor_nulo();
}

/* Ejecuta el método 'f' de 'cls' sobre 'obj' (NULL en uno estático) con los
   argumentos ya evaluados, de los que se hace cargo. */
static Valor *llamar_metodo(Valor *obj, ClaseLince *cls, FuncionLince *f,
                            Valor **args, int nargs, int linea) {
    if (nargs != f->num_parametros) {
        for (int i = 0; i < nargs; i++) valor_destruir(args[i]);
        return error_rt("ErrorArgumento", linea,
            "El método '%s' espera %d argumento(s), recibió %d.",
            f->nombre, f->num_parametros, nargs);
    }
    if (profundidad >= 0 && profundidad + 1 < PILA_MAX) { _pila_nombre[profundidad + 1] = f->nombre; _pila_linea[profundidad + 1] = linea; }
    if (++profundidad > MAX_PROFUNDIDAD) {
        profundidad = 0;
        char msg[128];
        snprintf(msg, sizeof(msg), "Demasiadas llamadas anidadas en método '%s'.", f->nombre);
        valor_error = valor_error_nuevo("ErrorRecursion", msg, linea);
        hay_error   = 1;
        for (int i = 0; i < nargs; i++) valor_destruir(args[i]);
        return valor_nulo();
    }
    Entorno *fn_e = entorno_crear(f->entorno_closure);
    for (int j = 0; j < f->num_parametros; j++) {
        validar_tipo(args[j], f->parametros[j].tipo, "parámetro", f->parametros[j].nombre, linea);
        entorno_definir_i(fn_e, PI(&f->parametros[j]), args[j], 0);
    }
    Valor *esto_anterior = esto_actual;
    ClaseLince *cls_ant  = clase_actual;
    esto_actual  = obj;
    clase_actual = cls;
    valor_destruir(ejecutar_cuerpo(f, fn_e));
    Valor *retorno = tomar_retorno();
    esto_actual  = esto_anterior;
    clase_actual = cls_ant;
    entorno_destruir(fn_e);
    profundidad--;
    return retorno;
}

/* Pone un campo en el objeto (o lo sustituye). Devuelve 0 si no cabe. */
static int objeto_poner_campo(ObjetoLince *o, const char *nombre, Valor *v) {
    for (int i = 0; i < o->num_campos; i++)
        if (strcmp(o->campos_nombres[i], nombre) == 0) {
            valor_destruir(o->campos_valores[i]);
            o->campos_valores[i] = v;
            return 1;
        }
    if (o->num_campos >= MAX_CAMPOS) return 0;
    o->campos_nombres[o->num_campos] = strdup(nombre);
    o->campos_valores[o->num_campos] = v;
    o->num_campos++;
    return 1;
}

/* Campos declarados con 'sea' / 'fijo' en la clase y sus antecesoras (las de
   arriba primero), con su valor inicial. Devuelve 0 si falla. */
static int inicializar_campos(Valor *vobj, ClaseLince *c, int linea) {
    if (!c) return 1;
    if (!inicializar_campos(vobj, c->padre, linea)) return 0;
    for (int i = 0; i < c->num_campos; i++) {
        CampoClase *cd = &c->campos[i];
        if (cd->estatico) continue;
        Valor *v;
        if (cd->inicial) {
            Valor *esto_ant = esto_actual; ClaseLince *cls_ant = clase_actual;
            esto_actual = vobj; clase_actual = c;
            Entorno *ie = entorno_crear(c->entorno);
            v = ejecutar(cd->inicial, ie);
            entorno_destruir(ie);
            esto_actual = esto_ant; clase_actual = cls_ant;
            if (hay_error) { valor_destruir(v); return 0; }
        } else {
            v = valor_nulo();
        }
        if (!objeto_poner_campo(vobj->objeto, cd->nombre, v)) {
            valor_destruir(v);
            valor_destruir(error_rt("Error", linea, "Un objeto no puede tener más de %d campos.", MAX_CAMPOS));
            return 0;
        }
    }
    return 1;
}

/* Instancia la clase 'vcls' (la consume) con los argumentos 'argn', que se
   evalúan en 'e'. Lo comparten 'Clase(...)' y 'modulo.Clase(...)'. */
static Valor *instanciar_clase(Valor *vcls, Nodo **argn, int nargs, Entorno *e, int linea) {
    ClaseLince *cls = vcls->clase;

    /* ¿Es un tipo de error predefinido o sin constructor? */
    int es_error_pred = (cls->num_metodos == 0 &&
        (strncmp(cls->nombre, "Error", 5) == 0));

    if (es_error_pred) {
        /* Error("mensaje") — el primer argumento es el mensaje */
        char *msg = strdup("sin mensaje");
        if (nargs >= 1) {
            Valor *varg = ejecutar(argn[0], e);
            free(msg);
            msg = valor_a_texto(varg);
            valor_destruir(varg);
        }
        Valor *verr = valor_crear_error(cls->nombre, msg, 0);
        free(msg);
        valor_destruir(vcls);
        return verr;
    }

    if (cls->abstracta) {
        Valor *err = error_rt("ErrorTipo", linea,
            "'%s' es una clase abstracta y no se puede instanciar.", cls->nombre);
        valor_destruir(vcls);
        return err;
    }

    FuncionLince *f = buscar_metodo(cls, "crear");
    if (f && f->vis != VIS_PUBLICO && !acceso_permitido(f->vis, f->duena, e)) {
        Valor *err = error_rt("ErrorAcceso", linea,
            "El constructor de '%s' es %s: no se puede instanciar desde aquí.",
            cls->nombre, nombre_vis(f->vis));
        valor_destruir(vcls);
        return err;
    }

    ObjetoLince *obj = calloc(1, sizeof(ObjetoLince));
    obj->clase      = cls;
    obj->num_campos = 0;
    Valor *vobj = valor_objeto(obj);

    if (!inicializar_campos(vobj, cls, linea)) {
        valor_destruir(vobj);
        valor_destruir(vcls);
        return valor_nulo();
    }

    /* Llamar a 'crear' (buscado en la jerarquía) */
    if (f) {
        if (nargs != f->num_parametros) {
            Valor *err = error_rt("ErrorArgumento", linea,
                "'%s' espera %d argumento(s) en 'crear', recibió %d.",
                cls->nombre, f->num_parametros, nargs);
            valor_destruir(vobj);
            valor_destruir(vcls);
            return err;
        }
        Entorno *fn_e = entorno_crear(f->entorno_closure);
        for (int j = 0; j < f->num_parametros; j++) {
            Valor *arg = ejecutar(argn[j], e);
            validar_tipo(arg, f->parametros[j].tipo, "parámetro", f->parametros[j].nombre, linea);
            entorno_definir_i(fn_e, PI(&f->parametros[j]), arg, 0);
        }
        Valor *esto_anterior   = esto_actual;
        ClaseLince *cls_ant    = clase_actual;
        esto_actual  = vobj;
        clase_actual = cls;
        valor_destruir(ejecutar_cuerpo(f, fn_e));   /* ver la nota de NODO_LLAMADA */
        esto_actual  = esto_anterior;
        clase_actual = cls_ant;
        if (hay_retorno) valor_destruir(valor_retorno);
        hay_retorno   = 0;
        valor_retorno = NULL;
        entorno_destruir(fn_e);
    }
    valor_destruir(vcls);
    return vobj;
}

/* ── Recarga en caliente ──
   Mientras corre el código recién recargado, un 'sea' del nivel superior cuyo
   nombre ya existía en el programa anterior se queda con el valor que tenía en
   vez de evaluar su expresión. Las funciones, clases y módulos sí salen del
   código nuevo, y los objetos conservados pasan a usar la clase nueva. */
static Entorno *_conservar_de = NULL;     /* global del programa anterior */
static Entorno *_conservar_en = NULL;     /* global del programa nuevo */

void interprete_conservar(Entorno *anterior, Interprete *nuevo) {
    _conservar_de = anterior;
    _conservar_en = anterior && nuevo ? nuevo->global : NULL;
}

/* Objetos ya visitados al cambiarles la clase (puede haber ciclos) */
static Valor **_vistos = NULL;
static int     _nvistos = 0, _cap_vistos = 0;

static void reclasificar(Valor *v, Entorno *e) {
    if (!v || (v->tipo != VAL_LISTA && v->tipo != VAL_DICCIONARIO && v->tipo != VAL_OBJETO)) return;
    for (int i = 0; i < _nvistos; i++) if (_vistos[i] == v) return;
    if (_nvistos == _cap_vistos) {
        _cap_vistos = _cap_vistos ? _cap_vistos * 2 : 64;
        _vistos = realloc(_vistos, sizeof(Valor *) * _cap_vistos);
    }
    _vistos[_nvistos++] = v;
    if (v->tipo == VAL_LISTA)
        for (int i = 0; i < v->lista.cantidad; i++) reclasificar(v->lista.elementos[i], e);
    else if (v->tipo == VAL_DICCIONARIO)
        for (int i = 0; i < v->diccionario.cantidad; i++) reclasificar(v->diccionario.valores[i], e);
    else {
        ObjetoLince *o = v->objeto;
        if (o->clase) {
            const char *id = intern(o->clase->nombre);
            for (int i = 0; i < e->cantidad; i++)
                if (e->vars[i].nombre == id && e->vars[i].valor->tipo == VAL_CLASE) {
                    o->clase = e->vars[i].valor->clase;
                    break;
                }
        }
        for (int i = 0; i < o->num_campos; i++) reclasificar(o->campos_valores[i], e);
    }
}

/* El valor que el 'sea' de nombre 'id' tenía antes de recargar, o NULL */
static Valor *conservado(const char *id, Entorno *e, int constante, int es_lista, int es_diccionario) {
    if (!_conservar_de || e != _conservar_en || constante) return NULL;
    for (int i = 0; i < _conservar_de->cantidad; i++) {
        if (_conservar_de->vars[i].nombre != id) continue;
        Valor *v = _conservar_de->vars[i].valor;
        if (_conservar_de->vars[i].constante || v->es_modulo ||
            v->tipo == VAL_FUNCION || v->tipo == VAL_CLASE || v->tipo == VAL_ERROR) return NULL;
        if (es_lista && v->tipo != VAL_LISTA) return NULL;
        if (es_diccionario && v->tipo != VAL_DICCIONARIO) return NULL;
        _nvistos = 0;
        reclasificar(v, e);
        return valor_retener(v);
    }
    return NULL;
}

static Valor *exec_declaracion(Nodo *n, Entorno *e) {
    Valor *v = _conservar_de ? conservado(NI(n, n->declaracion.nombre), e, n->declaracion.constante,
                                          n->declaracion.es_lista, n->declaracion.es_diccionario) : NULL;
    if (!v) v = ejecutar(n->declaracion.valor, e);
    /* Comprobar el tipo anunciado en 'sea lista x' / 'sea
       diccionario x'. Se hace aquí y no en el parser porque sólo
       en ejecución se conoce el tipo de verdad: antes se exigía
       un literal, así que 'sea lista l = datos.ordenar()' —o
       cualquier llamada— se rechazaba sin motivo. */
    if (!hay_error && n->declaracion.es_lista && v->tipo != VAL_LISTA) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "Declaraste '%s' como lista pero el valor no es una lista.",
            n->declaracion.nombre);
        valor_error = valor_error_nuevo("ErrorTipo", msg, n->linea);
        hay_error = 1;
        valor_destruir(v);
        return valor_nulo();
    }
    if (!hay_error && n->declaracion.es_diccionario && v->tipo != VAL_DICCIONARIO) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "Declaraste '%s' como diccionario pero el valor no es un diccionario.",
            n->declaracion.nombre);
        valor_error = valor_error_nuevo("ErrorTipo", msg, n->linea);
        hay_error = 1;
        valor_destruir(v);
        return valor_nulo();
    }
    const char *id = NI(n, n->declaracion.nombre);
    if (ya_definido_aqui_i(e, id) &&
        redefinido(e, n->declaracion.nombre,
                   n->declaracion.constante ? "'fijo'" : "'sea'",
                   1, n->linea)) {
        valor_destruir(v);
        return valor_nulo();
    }
    entorno_definir_i(e, id, v, n->declaracion.constante);
    return valor_nulo();
}

/* sea [x, y] = pos  /  sea {x, y} = d  /  [a, b] = [b, a] */
static Valor *exec_desestructurar(Nodo *n, Entorno *e) {
    int num = n->desestructurar.num_nombres, dic = n->desestructurar.de_diccionario;
    if (!n->desestructurar.nombres_i) {
        n->desestructurar.nombres_i = malloc(sizeof(char*) * num);
        for (int i = 0; i < num; i++) n->desestructurar.nombres_i[i] = intern(n->desestructurar.nombres[i]);
    }
    Valor *v = ejecutar(n->desestructurar.valor, e);
    if (hay_error) { valor_destruir(v); return valor_nulo(); }

    Valor *err = NULL;
    if (dic && v->tipo != VAL_DICCIONARIO)
        err = error_rt("ErrorTipo", n->linea, "Con '{...} =' el valor tiene que ser un diccionario.");
    else if (!dic && v->tipo != VAL_LISTA)
        err = error_rt("ErrorTipo", n->linea, "Con '[...] =' el valor tiene que ser una lista.");
    else if (!dic && v->lista.cantidad != num)
        err = error_rt("ErrorRango", n->linea,
            "Hay %d nombre%s entre corchetes y la lista tiene %d elemento%s.",
            num, num == 1 ? "" : "s", v->lista.cantidad, v->lista.cantidad == 1 ? "" : "s");
    else if (dic)
        for (int i = 0; i < num && !err; i++)
            if (valor_diccionario_buscar(v, n->desestructurar.nombres[i]) < 0)
                err = error_rt("ErrorRango", n->linea, "El diccionario no tiene la clave '%s'.",
                               n->desestructurar.nombres[i]);
    if (err) { valor_destruir(v); return err; }

    /* Se toman todos antes de guardar ninguno: así '[a, b] = [b, a]' cambia
       los dos (la lista de la derecha ya está hecha) */
    Valor *vals_pila[8], **vals = num <= 8 ? vals_pila : malloc(sizeof(Valor*) * num);
    for (int i = 0; i < num; i++)
        vals[i] = valor_retener(dic ? v->diccionario.valores[valor_diccionario_buscar(v, n->desestructurar.nombres[i])]
                                    : v->lista.elementos[i]);
    valor_destruir(v);

    for (int i = 0; i < num; i++) {
        const char *id = n->desestructurar.nombres_i[i], *nombre = n->desestructurar.nombres[i];
        if (hay_error) { valor_destruir(vals[i]); continue; }
        if (!n->desestructurar.declarar) {
            entorno_asignar_i(e, id, nombre, vals[i], n->linea);
            continue;
        }
        Valor *c = _conservar_de ? conservado(id, e, n->desestructurar.constante, 0, 0) : NULL;
        if (c) { valor_destruir(vals[i]); vals[i] = c; }
        if (ya_definido_aqui_i(e, id) &&
            redefinido(e, nombre, n->desestructurar.constante ? "'fijo'" : "'sea'", 1, n->linea)) {
            valor_destruir(vals[i]);
            continue;
        }
        entorno_definir_i(e, id, vals[i], n->desestructurar.constante);
    }
    if (vals != vals_pila) free(vals);
    return valor_nulo();
}

/* "Vidas: {vidas}": cada trozo se escribe como lo haría 'escribir' (y un
   objeto, con su método a_texto()). */
static Valor *exec_plantilla(Nodo *n, Entorno *e) {
    size_t largo = 0, cap = 64;
    char  *buf   = malloc(cap);
    buf[0] = '\0';
    for (int i = 0; i < n->lista.cantidad; i++) {
        Nodo *parte = n->lista.elementos[i];
        const char *t;
        char *propio = NULL;
        Valor *v = NULL;
        if (parte->tipo == NODO_TEXTO) t = parte->texto;
        else {
            v = ejecutar(parte, e);
            if (hay_error) { valor_destruir(v); free(buf); return valor_nulo(); }
            t = v->tipo == VAL_TEXTO ? v->texto : (propio = valor_a_texto(v));
        }
        size_t l = strlen(t);
        if (largo + l + 1 > cap) {
            while (largo + l + 1 > cap) cap *= 2;
            buf = realloc(buf, cap);
        }
        memcpy(buf + largo, t, l + 1);
        largo += l;
        free(propio);
        if (v) valor_destruir(v);
    }
    return valor_texto_propio(buf);
}

/* Ejecuta un bloque en un ámbito nuevo, salvo que no defina nada: entonces
   no hace falta (nadie notaría la diferencia) y se ahorra crearlo. */
static Valor *ejecutar_en_ambito(Nodo *b, Entorno *e) {
    if (b && b->tipo == NODO_BLOQUE && !b->bloque.declara) return ejecutar(b, e);
    Entorno *nuevo = entorno_crear(e);
    Valor   *r     = ejecutar(b, nuevo);
    entorno_destruir(nuevo);
    return r;
}

static Valor *exec_si(Nodo *n, Entorno *e) {
    Valor *cond = ejecutar(n->si.condicion, e);
    int    ok   = es_verdadero(cond);
    valor_destruir(cond);

    if (ok) {
        Valor   *r     = ejecutar_en_ambito(n->si.entonces, e);
        return r;
    }
    for (int i = 0; i < n->si.cantidad_sino_si; i++) {
        Valor *c = ejecutar(n->si.condiciones_sino_si[i], e);
        int    b = es_verdadero(c);
        valor_destruir(c);
        if (b) {
            Valor   *r     = ejecutar_en_ambito(n->si.bloques_sino_si[i], e);
            return r;
        }
    }
    if (n->si.sino) {
        Valor   *r     = ejecutar_en_ambito(n->si.sino, e);
        return r;
    }
    return valor_nulo();
}

static Valor *exec_para(Nodo *n, Entorno *e) {
    Entorno *bucle = entorno_crear(e);
    Valor   *vinicio = ejecutar(n->para.val_inicio, bucle);
    entorno_definir(bucle, n->para.var_inicio, vinicio, 0);

    while (!hay_retorno && !hay_error) {
        Valor *cond = ejecutar(n->para.condicion, bucle);
        int    ok   = es_verdadero(cond);
        valor_destruir(cond);
        if (!ok) break;

        valor_destruir(ejecutar_en_ambito(n->para.cuerpo, bucle));
        if (hay_salto && fin_de_vuelta()) break;
        if (hay_retorno || hay_error) break;

        if (n->para.paso) {                       /* i += 2 */
            valor_destruir(ejecutar(n->para.paso, bucle));
            continue;
        }
        EntradaVar *ev = buscar_var(n, bucle, NI(n, n->para.var_incremento));
        if (ev && !ev->constante && ev->valor->tipo == VAL_NUMERO && ev->valor->refs == 1) {
            ev->valor->numero += n->para.op_incremento[0] == '+' ? 1 : -1;
            continue;
        }
        Valor *vactual = obtener_var(n, bucle, NI(n, n->para.var_incremento), n->para.var_incremento);
        if (hay_error) { valor_destruir(vactual); break; }
        Valor *vnuevo;
        if (strcmp(n->para.op_incremento, "++") == 0)
            vnuevo = valor_numero(vactual->numero + 1);
        else
            vnuevo = valor_numero(vactual->numero - 1);
        valor_destruir(vactual);
        asignar_var(n, bucle, NI(n, n->para.var_incremento), n->para.var_incremento, vnuevo);
    }
    entorno_destruir(bucle);
    return valor_nulo();
}

static Valor *exec_para_cada(Nodo *n, Entorno *e) {
    Valor *col = ejecutar(n->para_cada.coleccion, e);

    /* ── Bytes: cada uno es un número de 0 a 255 ── */
    if (col->tipo == VAL_BYTES) {
        for (int i = 0; i < col->bytes.cantidad && !hay_retorno && !hay_error; i++) {
            Valor *elem = valor_numero(col->bytes.datos[i]);
            validar_tipo(elem, n->para_cada.tipo, "variable del bucle", n->para_cada.variable, n->linea);
            Entorno *iter = entorno_crear(e);
            entorno_definir_i(iter, NI(n, n->para_cada.variable), elem, 0);
            valor_destruir(ejecutar(n->para_cada.cuerpo, iter));
            entorno_destruir(iter);
            if (hay_salto && fin_de_vuelta()) break;
        }
        valor_destruir(col);
        return valor_nulo();
    }

    /* ── Generador: sus valores de uno en uno ── */
    if (col->tipo == VAL_GENERADOR) {
        TareaLince *g = col->generador.productor;
        double i = col->generador.desde, hasta = col->generador.hasta, paso = col->generador.paso;
        while (!hay_retorno && !hay_error) {
            Valor *elem;
            if (g) {
                elem = generador_siguiente(g);
                if (!elem) break;
            } else {                                   /* rango() */
                if (paso > 0 ? i >= hasta : i <= hasta) break;
                elem = valor_numero(i);
                i += paso;
            }
            validar_tipo(elem, n->para_cada.tipo,
                         "variable del bucle", n->para_cada.variable, n->linea);
            Entorno *iter = entorno_crear(e);
            entorno_definir_i(iter, NI(n, n->para_cada.variable), elem, 0);
            valor_destruir(ejecutar(n->para_cada.cuerpo, iter));
            entorno_destruir(iter);
            if (hay_salto && fin_de_vuelta()) break;
        }
        if (g) generador_cerrar(g);
        valor_destruir(col);
        return valor_nulo();
    }

    /* ── Lista ── */
    if (col->tipo == VAL_LISTA) {
        int cant = col->lista.cantidad;
        for (int i = 0; i < cant && !hay_retorno && !hay_error; i++) {
            Valor *elem = valor_copiar(col->lista.elementos[i]);
            validar_tipo(elem, n->para_cada.tipo,
                         "variable del bucle", n->para_cada.variable, n->linea);
            Entorno *iter = entorno_crear(e);
            entorno_definir_i(iter, NI(n, n->para_cada.variable), elem, 0);
            Valor *r = ejecutar(n->para_cada.cuerpo, iter);
            valor_destruir(r);
            entorno_destruir(iter);
            if (hay_salto && fin_de_vuelta()) break;
        }
        valor_destruir(col);
        return valor_nulo();
    }

    /* ── Enumeración: sus valores, en orden ── */
    if (col->tipo == VAL_DICCIONARIO && col->es_modulo == ES_ENUMERACION) {
        for (int i = 0; i < col->diccionario.cantidad && !hay_retorno && !hay_error; i++) {
            Valor *elem = valor_retener(col->diccionario.valores[i]);
            validar_tipo(elem, n->para_cada.tipo, "variable del bucle", n->para_cada.variable, n->linea);
            Entorno *iter = entorno_crear(e);
            entorno_definir_i(iter, NI(n, n->para_cada.variable), elem, 0);
            valor_destruir(ejecutar(n->para_cada.cuerpo, iter));
            entorno_destruir(iter);
            if (hay_salto && fin_de_vuelta()) break;
        }
        valor_destruir(col);
        return valor_nulo();
    }

    /* ── Diccionario — itera sobre claves ── */
    if (col->tipo == VAL_DICCIONARIO) {
        for (int i = 0; i < col->diccionario.cantidad && !hay_retorno && !hay_error; i++) {
            Valor *clave = valor_texto(col->diccionario.claves[i]);

            validar_tipo(clave, n->para_cada.tipo, "variable del bucle", n->para_cada.variable, n->linea);

            Entorno *iter = entorno_crear(e);
            entorno_definir(iter, n->para_cada.variable, clave, 0);
            Valor *r = ejecutar(n->para_cada.cuerpo, iter);
            valor_destruir(r);
            entorno_destruir(iter);
            if (hay_salto && fin_de_vuelta()) break;
        }
        valor_destruir(col);
        return valor_nulo();
    }

    /* ── Texto — itera carácter a carácter ── */
    if (col->tipo == VAL_TEXTO) {
        /* Recorre caracteres, no bytes: en "camión" el bucle da
           seis vueltas y una de ellas vale "ó". */
        for (int i = 0; col->texto[i] && !hay_retorno && !hay_error; ) {
            int tam = utf8_tam(col->texto + i);
            char tmp[5] = {0};
            for (int k = 0; k < tam && k < 4; k++) tmp[k] = col->texto[i + k];
            i += tam;
            Valor *c = valor_texto(tmp);

            validar_tipo(c, n->para_cada.tipo, "variable del bucle", n->para_cada.variable, n->linea);

            Entorno *iter = entorno_crear(e);
            entorno_definir(iter, n->para_cada.variable, c, 0);
            Valor *r = ejecutar(n->para_cada.cuerpo, iter);
            valor_destruir(r);
            entorno_destruir(iter);
            if (hay_salto && fin_de_vuelta()) break;
        }
        valor_destruir(col);
        return valor_nulo();
    }

    valor_destruir(col);
    return error_rt("ErrorTipo", n->linea,
        "'para cada' solo funciona con listas, diccionarios o texto.");
}

static Valor *exec_llamada(Nodo *n, Entorno *e) {
    if (profundidad >= 0 && profundidad + 1 < PILA_MAX) { _pila_nombre[profundidad + 1] = n->llamada.nombre; _pila_linea[profundidad + 1] = n->linea; }
    if (++profundidad > MAX_PROFUNDIDAD) {
        profundidad = 0;
        char msg[128];
        snprintf(msg, sizeof(msg),
            "Demasiadas llamadas anidadas — ¿hay una recursión infinita en '%s'?",
            n->llamada.nombre);
        valor_error = valor_error_nuevo("ErrorRecursion", msg, n->linea);
        hay_error   = 1;
        return valor_nulo();
    }
    Valor *vfun = obtener_var(n, e, NI(n, n->llamada.nombre), n->llamada.nombre);

    /* Si el nombre no existe, entorno_obtener ya ha puesto el error:
       se propaga tal cual en vez de añadir un "no es una función"
       encima, que sería la consecuencia y no la causa. */
    if (hay_error) {
        valor_destruir(vfun);
        profundidad--;
        return valor_nulo();
    }
    if (vfun->tipo != VAL_FUNCION) {
        /* Error capturable, no exit(1): matar el proceso por esto
           impedía que un intentar/capturar se recuperase, y el pilar
           del lenguaje es que los errores se cuenten, no que aborten. */
        char msg[256];
        snprintf(msg, sizeof(msg),
            "'%s' no es una función, así que no se puede llamar.",
            n->llamada.nombre);
        valor_error = valor_error_nuevo("ErrorTipo", msg, n->linea);
        hay_error   = 1;
        valor_destruir(vfun);
        profundidad--;
        return valor_nulo();
    }
    FuncionLince *f = vfun->funcion;

    /* ¿Es un generador? */
    if (vfun->es_modulo == 99) {
        /* Ejecutar el cuerpo acumulando valores producidos */
        int args_dados   = n->llamada.num_argumentos;
        int params_total = f->num_parametros;
        int params_req   = 0;
        for (int i = 0; i < params_total; i++)
            if (!f->parametros[i].valor_defecto) params_req++;

        if (args_dados < params_req || args_dados > params_total) {
            Valor *err = error_rt("ErrorArgumento", n->linea,
                "El generador '%s' espera entre %d y %d argumento(s).",
                f->nombre, params_req, params_total);
            valor_destruir(vfun);
            profundidad--;
            return err;
        }

        Entorno *fn_e = entorno_crear(f->entorno_closure);
        for (int i = 0; i < params_total; i++) {
            Valor *arg = (i < args_dados)
                ? ejecutar(n->llamada.argumentos[i], e)
                : ejecutar(f->parametros[i].valor_defecto, e);
            entorno_definir_i(fn_e, PI(&f->parametros[i]), arg, 0);
        }

        /* El cuerpo no empieza hasta que se recorre */
        Valor *gen = valor_nulo();
        if (!hay_error) {
            TareaLince *g = tarea_nueva(f, fn_e, n->linea);
            if (g) {
                g->es_generador = 1;
                valor_destruir(gen);
                gen = valor_generador_crear();
                gen->generador.productor = g;
            }
        }
        entorno_destruir(fn_e);
        valor_destruir(vfun);
        profundidad--;
        return gen;
    }

    /* ¿Es una función nativa? (entorno_closure == NULL) */
    if (f->entorno_closure == NULL) {
        typedef Valor *(*FnNativa)(Valor**, int);
        FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
        int nargs = n->llamada.num_argumentos;
        Valor *args_pila[8];
        Valor **args = nargs < 8 ? args_pila : malloc(sizeof(Valor*) * (nargs + 1));
        for (int i = 0; i < nargs; i++)
            args[i] = ejecutar(n->llamada.argumentos[i], e);
        Valor *resultado = fn(args, nargs);
        for (int i = 0; i < nargs; i++) valor_destruir(args[i]);
        if (args != args_pila) free(args);
        valor_destruir(vfun);
        profundidad--;
        return resultado ? resultado : valor_nulo();
    }

    /* Contar parámetros requeridos (sin valor por defecto) */
    int args_dados   = n->llamada.num_argumentos;
    int params_total = f->num_parametros;
    int params_req   = 0;
    for (int i = 0; i < params_total; i++)
        if (!f->parametros[i].valor_defecto) params_req++;

    if (args_dados < params_req || args_dados > params_total) {
        Valor *err = error_rt("ErrorArgumento", n->linea,
            "La función '%s' espera entre %d y %d argumento(s) pero recibió %d.",
            f->nombre, params_req, params_total, args_dados);
        valor_destruir(vfun);
        profundidad--;
        return err;
    }

    Entorno *fn_e = entorno_crear(f->entorno_closure);
    for (int i = 0; i < params_total; i++) {
        Parametro *param = &f->parametros[i];
        Valor *arg;

        if (i < args_dados) {
            if (param->por_ref) {
                arg = ejecutar(n->llamada.argumentos[i], e);
            } else {
                Valor *tmp = ejecutar(n->llamada.argumentos[i], e);
                if (tmp->tipo == VAL_NUMERO || tmp->tipo == VAL_TEXTO ||
                    tmp->tipo == VAL_BOOLEANO || tmp->tipo == VAL_NULO) {
                    arg = valor_copiar(tmp);
                    valor_destruir(tmp);
                } else {
                    arg = tmp;
                }
            }
        } else {
            /* Usar valor por defecto */
            arg = ejecutar(param->valor_defecto, e);
        }

        validar_tipo(arg, param->tipo, "parámetro", param->nombre, n->linea);
        entorno_definir_i(fn_e, PI(param), arg, 0);
    }

    /* El valor del cuerpo no es el retorno de la función (eso va por
       valor_retorno): es el de su última sentencia, y lo posee quien
       llama — NODO_BLOQUE ya libera así todos los anteriores. Sin
       este destruir se fugaban 40 bytes por llamada, que en un bucle
       de juego son 40 por función y por frame. */
    valor_destruir(ejecutar_cuerpo(f, fn_e));
    entorno_destruir(fn_e);

    Valor *retorno = tomar_retorno();

    if (!hay_error)
        validar_tipo(retorno, f->tipo_retorno, "retorno de función", f->nombre, n->linea);

    valor_destruir(vfun);
    profundidad--;
    return retorno;
}

/* Posición i (negativa: desde el final) dentro de unos bytes, o -1 si se sale */
static int pos_bytes(Valor *b, Valor *idx) {
    if (idx->tipo != VAL_NUMERO) return -1;
    int i = (int)idx->numero;
    if (i < 0) i += b->bytes.cantidad;
    return i >= 0 && i < b->bytes.cantidad ? i : -1;
}

static Valor *exec_indice(Nodo *n, Entorno *e) {
    Valor *obj = ejecutar(n->acceso_indice.objeto, e);
    Valor *idx = clave_de(ejecutar(n->acceso_indice.indice, e));

    if (obj->tipo == VAL_BYTES) {
        int i = pos_bytes(obj, idx), cant = obj->bytes.cantidad;
        double pedido = idx->tipo == VAL_NUMERO ? idx->numero : 0;
        Valor *r = i >= 0 ? valor_numero(obj->bytes.datos[i]) : NULL;
        int no_numero = idx->tipo != VAL_NUMERO;
        valor_destruir(obj); valor_destruir(idx);
        if (r) return r;
        if (no_numero) return error_rt("ErrorTipo", n->linea, "El índice de unos bytes debe ser un número.");
        return error_rt("ErrorRango", n->linea, "Índice %.0f fuera de rango. Hay %d bytes.", pedido, cant);
    }

    if (obj->tipo == VAL_LISTA) {
        if (idx->tipo != VAL_NUMERO) {
            char msg[128];
            snprintf(msg, sizeof(msg), "El índice de una lista debe ser un número.");
            valor_error = valor_error_nuevo("ErrorTipo", msg, n->linea);
            hay_error = 1;
            valor_destruir(obj); valor_destruir(idx);
            return valor_nulo();
        }
        int i = (int)idx->numero;
        if (i < 0) i = obj->lista.cantidad + i;
        if (i < 0 || i >= obj->lista.cantidad) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                "Índice %d fuera de rango. La lista tiene %d elementos.",
                i, obj->lista.cantidad);
            valor_error = valor_error_nuevo("ErrorRango", msg, n->linea);
            hay_error = 1;
            valor_destruir(obj); valor_destruir(idx);
            return valor_nulo();
        }
        Valor *res = valor_copiar(obj->lista.elementos[i]);
        valor_destruir(obj); valor_destruir(idx);
        return res;
    }
    if (obj->tipo == VAL_DICCIONARIO) {
        if (idx->tipo != VAL_TEXTO) {
            valor_error = valor_error_nuevo("ErrorTipo",
                "La clave de un diccionario debe ser texto.", n->linea);
            hay_error = 1;
            valor_destruir(obj); valor_destruir(idx);
            return valor_nulo();
        }
        {
            int i = valor_diccionario_buscar(obj, idx->texto);
            if (i >= 0) {
                Valor *res = valor_copiar(obj->diccionario.valores[i]);
                valor_destruir(obj); valor_destruir(idx);
                return res;
            }
        }
        char msg[256];
        snprintf(msg, sizeof(msg),
            "La clave \"%s\" no existe en el diccionario.", idx->texto);
        valor_error = valor_error_nuevo("ErrorRango", msg, n->linea);
        hay_error = 1;
        valor_destruir(obj); valor_destruir(idx);
        return valor_nulo();
    }
    if (obj->tipo == VAL_TEXTO) {
        if (idx->tipo != VAL_NUMERO) {
            valor_error = valor_error_nuevo("ErrorTipo",
                "El índice de un texto debe ser un número.", n->linea);
            hay_error = 1;
            valor_destruir(obj); valor_destruir(idx);
            return valor_nulo();
        }
        int i = (int)idx->numero;
        texto_medir(obj);
        int len = obj->nchars;
        if (i < 0) i = len + i;
        if (i < 0 || i >= len) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                "Índice %d fuera de rango. El texto tiene %d caracteres.", i, len);
            valor_error = valor_error_nuevo("ErrorRango", msg, n->linea);
            hay_error = 1;
            valor_destruir(obj); valor_destruir(idx);
            return valor_nulo();
        }
        /* El carácter puede ocupar varios bytes, así que se copia
           entero en vez de quedarse con el primero. */
        int off = obj->ascii ? i : utf8_desplazamiento(obj->texto, i);
        int tam = obj->ascii ? 1 : utf8_tam(obj->texto + off);
        char tmp[5] = {0};
        for (int k = 0; k < tam && k < 4; k++) tmp[k] = obj->texto[off + k];
        Valor *res = valor_texto(tmp);
        valor_destruir(obj); valor_destruir(idx);
        return res;
    }
    valor_error = valor_error_nuevo("ErrorTipo",
        "Solo puedes usar [] en listas, diccionarios o texto.", n->linea);
    hay_error = 1;
    valor_destruir(obj); valor_destruir(idx);
    return valor_nulo();
}

/* lista[i] += v: el valor que hay ahora en obj[idx] (retenido), o NULL si
   no lo hay. Lo que falte lo dice luego la asignación, con su mensaje. */
static Valor *valor_en_indice(Valor *obj, Valor *idx) {
    if (obj->tipo == VAL_BYTES) {
        int i = pos_bytes(obj, idx);
        return i >= 0 ? valor_numero(obj->bytes.datos[i]) : NULL;
    }
    if (obj->tipo == VAL_LISTA && idx->tipo == VAL_NUMERO) {
        int i = (int)idx->numero;
        if (i < 0) i += obj->lista.cantidad;
        return i >= 0 && i < obj->lista.cantidad ? valor_retener(obj->lista.elementos[i]) : NULL;
    }
    if (obj->tipo == VAL_DICCIONARIO && idx->tipo == VAL_TEXTO) {
        int i = valor_diccionario_buscar(obj, idx->texto);
        return i >= 0 ? valor_retener(obj->diccionario.valores[i]) : NULL;
    }
    return NULL;
}

static Valor *exec_asignacion_indice(Nodo *n, Entorno *e) {
    Valor *obj = ejecutar(n->asignacion_indice.objeto, e);
    Valor *idx = clave_de(ejecutar(n->asignacion_indice.indice, e));
    Valor *val = ejecutar(n->asignacion_indice.valor,  e);

    if (n->asignacion_indice.op[0] && !hay_error) {          /* lista[i] += v */
        Valor *antes = valor_en_indice(obj, idx);
        if (!antes && obj->tipo == VAL_DICCIONARIO && idx->tipo == VAL_TEXTO) {
            Valor *err = error_rt("ErrorRango", n->linea,
                "El diccionario no tiene la clave '%s': con '%s=' tiene que existir antes.",
                idx->texto, n->asignacion_indice.op);
            valor_destruir(obj); valor_destruir(idx); valor_destruir(val);
            return err;
        }
        if (antes) val = operar(antes, val, n->asignacion_indice.op, n->linea);
    }
    if (hay_error) {
        valor_destruir(obj); valor_destruir(idx); valor_destruir(val);
        return valor_nulo();
    }

    if (obj->tipo == VAL_BYTES) {
        int i = pos_bytes(obj, idx), cant = obj->bytes.cantidad;
        double pedido = idx->tipo == VAL_NUMERO ? idx->numero : 0;
        int bien = val->tipo == VAL_NUMERO && val->numero >= 0 && val->numero <= 255 && val->numero == floor(val->numero);
        if (i >= 0 && bien) obj->bytes.datos[i] = (unsigned char)val->numero;
        valor_destruir(obj); valor_destruir(idx); valor_destruir(val);
        if (i < 0) return error_rt("ErrorRango", n->linea, "Índice %.0f fuera de rango. Hay %d bytes.", pedido, cant);
        if (!bien) return error_rt("ErrorTipo", n->linea, "Un byte es un número entero de 0 a 255.");
        return valor_nulo();
    }

    if (obj->tipo == VAL_LISTA) {
        int i = (int)idx->numero;
        if (i < 0) i = obj->lista.cantidad + i;
        if (i < 0 || i >= obj->lista.cantidad) {
            int cant = obj->lista.cantidad;
            double pedido = idx->numero;
            valor_destruir(obj); valor_destruir(idx); valor_destruir(val);
            return error_rt("ErrorRango", n->linea,
                "Índice %.0f fuera de rango. La lista tiene %d elementos.", pedido, cant);
        }
        valor_destruir(obj->lista.elementos[i]);
        obj->lista.elementos[i] = val;
        /* Reflejar en el entorno */
        if (n->asignacion_indice.objeto->tipo == NODO_IDENTIFICADOR) {
            entorno_asignar(e, n->asignacion_indice.objeto->identificador, obj, n->linea);
        }
        valor_destruir(idx);
        return valor_nulo();
    }
    if (obj->tipo == VAL_DICCIONARIO) {
        if (idx->tipo != VAL_TEXTO) {
            valor_destruir(obj); valor_destruir(idx); valor_destruir(val);
            return error_rt("ErrorTipo", n->linea, "La clave de un diccionario debe ser texto.");
        }
        {
            int i = valor_diccionario_buscar(obj, idx->texto);
            if (i >= 0) {
                valor_destruir(obj->diccionario.valores[i]);
                obj->diccionario.valores[i] = val;
                if (n->asignacion_indice.objeto->tipo == NODO_IDENTIFICADOR)
                    entorno_asignar(e, n->asignacion_indice.objeto->identificador, obj, n->linea);
                valor_destruir(idx);
                return valor_nulo();
            }
        }
        /* Clave nueva */
        valor_diccionario_asegurar(obj, obj->diccionario.cantidad + 1);
        int i = obj->diccionario.cantidad;
        obj->diccionario.claves[i]  = strdup(idx->texto);
        obj->diccionario.valores[i] = val;
        obj->diccionario.cantidad++;
        if (n->asignacion_indice.objeto->tipo == NODO_IDENTIFICADOR)
            entorno_asignar(e, n->asignacion_indice.objeto->identificador, obj, n->linea);
        valor_destruir(idx);
        return valor_nulo();
    }
    valor_destruir(obj); valor_destruir(idx); valor_destruir(val);
    return error_rt("ErrorTipo", n->linea,
        "Solo puedes asignar por índice en listas o diccionarios.");
}

/* ── Métodos de BYTES ──
   leer_u8/i8/u16/i16/u32/i32(pos[, grande]) y escribir_...(pos, valor[, grande]):
   enteros sin signo (u) o con signo (i) de 1, 2 o 4 bytes. Por defecto el byte
   menos significativo va primero (como en WAV, BMP o los PC); con 'grande' =
   verdadero, el más significativo (como en PNG o en la red). */
static Valor *metodo_bytes(Valor *obj, const char *met, Nodo *n, Entorno *e) {
    int nargs = n->metodo.num_argumentos;
    Valor *a[4] = {NULL, NULL, NULL, NULL};
    for (int j = 0; j < nargs && j < 4; j++) {
        a[j] = ejecutar(n->metodo.argumentos[j], e);
        if (hay_error) { for (int k = 0; k <= j; k++) valor_destruir(a[k]); valor_destruir(obj); return valor_nulo(); }
    }
    Valor *r = NULL;
    int cant = obj->bytes.cantidad;
    unsigned char *d = obj->bytes.datos;
    #define NUM(j) (j < nargs && a[j] && a[j]->tipo == VAL_NUMERO)
    if (strcmp(met, "longitud") == 0) r = valor_numero(cant);
    else if (strcmp(met, "agregar") == 0) {
        if (!NUM(0) || a[0]->numero < 0 || a[0]->numero > 255 || a[0]->numero != floor(a[0]->numero))
            r = error_rt("ErrorTipo", n->linea, "agregar(): un byte es un número entero de 0 a 255.");
        else {
            if (obj->bytes.cantidad == obj->bytes.capacidad) {
                obj->bytes.capacidad *= 2;
                obj->bytes.datos = realloc(obj->bytes.datos, (size_t)obj->bytes.capacidad);
            }
            obj->bytes.datos[obj->bytes.cantidad++] = (unsigned char)a[0]->numero;
            r = valor_nulo();
        }
    } else if (strcmp(met, "parte") == 0) {
        int desde = NUM(0) ? (int)a[0]->numero : 0, hasta = NUM(1) ? (int)a[1]->numero : cant;
        if (desde < 0) desde += cant;
        if (hasta < 0) hasta += cant;
        if (desde < 0) desde = 0;
        if (hasta > cant) hasta = cant;
        if (hasta < desde) hasta = desde;
        r = valor_bytes_crear(hasta - desde);
        memcpy(r->bytes.datos, d + desde, (size_t)(hasta - desde));
    } else if (strcmp(met, "a_lista") == 0) {
        r = valor_lista_crear();
        for (int i = 0; i < cant; i++) lista_agregar(r, valor_numero(d[i]));
    } else if (strcmp(met, "a_texto") == 0) {
        char *t = malloc((size_t)cant + 1);
        memcpy(t, d, (size_t)cant);
        t[cant] = '\0';
        r = valor_texto_propio(t);
    } else if (strncmp(met, "leer_", 5) == 0 || strncmp(met, "escribir_", 9) == 0) {
        int escribir = met[0] == 'e';
        const char *tipo = met + (escribir ? 9 : 5);
        int con_signo = tipo[0] == 'i', tam = 0;
        if (strcmp(tipo + 1, "8") == 0) tam = 1;
        else if (strcmp(tipo + 1, "16") == 0) tam = 2;
        else if (strcmp(tipo + 1, "32") == 0) tam = 4;
        if (!tam || (tipo[0] != 'u' && tipo[0] != 'i')) goto desconocido;
        int ig = escribir ? 2 : 1;                        /* posición del argumento 'grande' */
        int grande = ig < nargs && a[ig] && a[ig]->tipo == VAL_BOOLEANO && a[ig]->booleano;
        if (!NUM(0) || (escribir && !NUM(1))) {
            r = error_rt("ErrorTipo", n->linea, escribir ? "%s() espera (posición, valor[, grande])"
                                                         : "%s() espera (posición[, grande])", met);
        } else {
            int pos = (int)a[0]->numero;
            if (pos < 0 || pos + tam > cant)
                r = error_rt("ErrorRango", n->linea, "%s(): la posición %d se sale (hay %d bytes).", met, pos, cant);
            else if (escribir) {
                double v = a[1]->numero;
                double min = con_signo ? -ldexp(1, tam * 8 - 1) : 0, max = con_signo ? ldexp(1, tam * 8 - 1) - 1 : ldexp(1, tam * 8) - 1;
                if (v != floor(v) || v < min || v > max)
                    r = error_rt("ErrorRango", n->linea, "%s(): %.0f no cabe en %d byte(s)%s.", met, v, tam, con_signo ? " con signo" : "");
                else {
                    unsigned long long u = (unsigned long long)(long long)v;
                    for (int k = 0; k < tam; k++)
                        d[pos + (grande ? tam - 1 - k : k)] = (unsigned char)(u >> (8 * k));
                    r = valor_nulo();
                }
            } else {
                unsigned long long u = 0;
                for (int k = 0; k < tam; k++)
                    u |= (unsigned long long)d[pos + (grande ? tam - 1 - k : k)] << (8 * k);
                double v = (double)u;
                if (con_signo && (u >> (tam * 8 - 1)) & 1) v -= ldexp(1, tam * 8);
                r = valor_numero(v);
            }
        }
    } else {
desconocido:
        r = error_rt("Error", n->linea,
            "Los bytes no tienen el método '%s'. Tienen: longitud, agregar, parte, a_lista, a_texto, "
            "leer_u8, leer_i8, leer_u16, leer_i16, leer_u32, leer_i32 y los escribir_ correspondientes.", met);
    }
    #undef NUM
    for (int j = 0; j < 4; j++) valor_destruir(a[j]);
    valor_destruir(obj);
    return r;
}

/* Una función de biblioteca (juego.combo...) llamada desde el programa: como
   una función Lince normal (con sus valores por defecto), pero un error dentro
   se cuenta en la línea de quien llama y con el nombre de la función delante,
   que es lo que entiende quien la usa: sus líneas no están en su programa. */
static Valor *llamar_biblioteca(Valor *vf, Valor **args, int nargs, int linea) {
    FuncionLince *f = vf->funcion;
    int req = 0;
    for (int i = 0; i < f->num_parametros; i++)
        if (!f->parametros[i].valor_defecto) req++;
    if (nargs < req || nargs > f->num_parametros) {
        for (int i = 0; i < nargs; i++) valor_destruir(args[i]);
        if (req == f->num_parametros)
            return error_rt("ErrorArgumento", linea, "%s() espera %d argumento%s y recibió %d.",
                            f->nombre, req, req == 1 ? "" : "s", nargs);
        return error_rt("ErrorArgumento", linea, "%s() espera de %d a %d argumentos y recibió %d.",
                        f->nombre, req, f->num_parametros, nargs);
    }
    if (++profundidad > MAX_PROFUNDIDAD) {
        profundidad = 0;
        for (int i = 0; i < nargs; i++) valor_destruir(args[i]);
        return error_rt("ErrorRecursion", linea, "Demasiadas llamadas anidadas en '%s'.", f->nombre);
    }
    vf->refs++;
    Entorno *fn_e = entorno_crear(f->entorno_closure);
    _en_biblioteca++;
    for (int i = 0; i < f->num_parametros && !hay_error; i++) {
        Valor *arg = i < nargs ? args[i] : ejecutar(f->parametros[i].valor_defecto, fn_e);
        if (i < nargs) args[i] = NULL;
        validar_tipo(arg, f->parametros[i].tipo, "parámetro", f->parametros[i].nombre, linea);
        entorno_definir_i(fn_e, PI(&f->parametros[i]), arg, 0);
    }
    for (int i = 0; i < nargs; i++) if (args[i]) valor_destruir(args[i]);
    if (!hay_error) valor_destruir(ejecutar_cuerpo(f, fn_e));
    _en_biblioteca--;
    Valor *r = tomar_retorno();
    if (!hay_error) validar_tipo(r, f->tipo_retorno, "retorno de función", f->nombre, linea);
    entorno_destruir(fn_e);
    profundidad--;
    valor_destruir(vf);
    if (hay_error && valor_error && valor_error->tipo == VAL_ERROR &&
        strcmp(valor_error->error->tipo, ERROR_RECARGA) != 0) {
        ErrorLince *er = valor_error->error;
        er->linea = linea;
        size_t ln = strlen(f->nombre);
        if (strncmp(er->mensaje, f->nombre, ln) != 0 || er->mensaje[ln] != '(') {
            char *m = malloc(ln + strlen(er->mensaje) + 8);
            sprintf(m, "%s(): %s", f->nombre, er->mensaje);
            free(er->mensaje);
            er->mensaje = m;
        }
    }
    if (hay_error) { valor_destruir(r); return valor_nulo(); }
    return r;
}

/* 'src' con cada 'pat' (no vacío) cambiado por 'rep', en memoria nueva */
static char *texto_reemplazar(const char *src, const char *pat, const char *rep) {
    size_t lp = strlen(pat), lr = strlen(rep), veces = 0;
    for (const char *q = src; (q = strstr(q, pat)); q += lp) veces++;
    char *r = malloc(strlen(src) + veces * lr + 1), *w = r;
    const char *pos;
    while ((pos = strstr(src, pat))) {
        memcpy(w, src, (size_t)(pos - src)); w += pos - src;
        memcpy(w, rep, lr); w += lr;
        src = pos + lp;
    }
    strcpy(w, src);
    return r;
}

static Valor *exec_metodo(Nodo *n, Entorno *e) {
    Valor *obj = ejecutar(n->metodo.objeto, e);
    const char *met = n->metodo.metodo;

    /* ── Módulo (diccionario con funciones nativas) ── */
    if (obj->tipo == VAL_DICCIONARIO && obj->es_modulo) {
        const char *met = n->metodo.metodo;
        {
            /* El nodo recuerda en qué posición del módulo estaba la función
               (comprobando el nombre, por si es otro módulo) */
            Valor *vf = NULL;
            int i = n->cache_i;
            if (i < obj->diccionario.cantidad && strcmp(obj->diccionario.claves[i], met) == 0)
                vf = obj->diccionario.valores[i];
            else if ((i = valor_diccionario_buscar(obj, met)) >= 0) {
                vf = obj->diccionario.valores[i];
                n->cache_i = i;
            }
            if (vf) {
                if (vf->tipo == VAL_CLASE) {
                    Valor *cls = valor_retener(vf);
                    valor_destruir(obj);
                    return instanciar_clase(cls, n->metodo.argumentos,
                                            n->metodo.num_argumentos, e, n->linea);
                }
                if (vf->tipo != VAL_FUNCION) {
                    valor_destruir(obj);
                    return error_rt("ErrorTipo", n->linea,
                        "'%s' no es una función del módulo.", met);
                }
                FuncionLince *f = vf->funcion;

                int nargs = n->metodo.num_argumentos;
                Valor *args_pila[8];
                Valor **args = nargs < 8 ? args_pila : malloc(sizeof(Valor*) * (nargs + 1));
                for (int j = 0; j < nargs; j++)
                    args[j] = ejecutar(n->metodo.argumentos[j], e);

                Valor *resultado;
                if (f->de_biblioteca) {
                    resultado = llamar_biblioteca(vf, args, nargs, n->linea);
                } else if (f->entorno_closure == NULL) {
                    /* Nativa: 'cuerpo' guarda el puntero a la función C */
                    typedef Valor *(*FnNativa)(Valor**, int);
                    FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
                    resultado = fn(args, nargs);
                    for (int j = 0; j < nargs; j++)
                        valor_destruir(args[j]);
                } else {
                    /* Función Lince. Hasta ahora todos los módulos eran
                       nativos y esto casteaba 'cuerpo' a puntero de
                       función sin mirar; con 'importar ... como' hay
                       módulos cuyas funciones son cierres de verdad, y
                       ese cast ejecutaba el AST como código — segfault
                       limpio. interprete_llamar_funcion se queda con
                       los argumentos y valida aridad y tipos. */
                    resultado = interprete_llamar_funcion(vf, args, nargs);
                }
                if (args != args_pila) free(args);
                valor_destruir(obj);
                return resultado ? resultado : valor_nulo();
            }
        }
        valor_destruir(obj);
        return error_rt("Error", n->linea, "El módulo no tiene la función '%s'.", met);
    }

    /* ── Métodos ESTÁTICOS: Clase.metodo(...) ── */
    if (obj->tipo == VAL_CLASE) {
        ClaseLince   *cls = obj->clase;
        FuncionLince *f   = buscar_metodo(cls, met);
        Valor *err = NULL;
        if (!f)
            err = error_rt("Error", n->linea,
                "La clase '%s' no tiene el método '%s'.", cls->nombre, met);
        else if (!f->estatico)
            err = error_rt("ErrorTipo", n->linea,
                "'%s' es un método de instancia de '%s': llámalo sobre un objeto.", met, cls->nombre);
        else if (!acceso_permitido(f->vis, f->duena, e))
            err = error_rt("ErrorAcceso", n->linea,
                "El método estático '%s' de '%s' es %s.", met, cls->nombre, nombre_vis(f->vis));
        if (err) { valor_destruir(obj); return err; }

        int nargs = n->metodo.num_argumentos;
        Valor **args = malloc(sizeof(Valor*) * (nargs + 1));
        for (int j = 0; j < nargs; j++) args[j] = ejecutar(n->metodo.argumentos[j], e);
        Valor *r = llamar_metodo(NULL, cls, f, args, nargs, n->linea);
        free(args);
        valor_destruir(obj);
        return r;
    }

    /* ── Métodos de OBJETO ── */
    if (obj->tipo == VAL_OBJETO) {
        ClaseLince   *cls       = obj->objeto->clase;
        const char   *met_nombre = n->metodo.metodo;
        if (n->cache_clase != cls) {                /* el método, recordado por clase */
            n->cache_cd    = buscar_metodo(cls, met_nombre);
            n->cache_clase = cls;
        }
        FuncionLince *f          = (FuncionLince *)n->cache_cd;

        if (!f) {
            Valor *err = error_rt("Error", n->linea,
                "La clase '%s' no tiene el método '%s'.", cls->nombre, met_nombre);
            valor_destruir(obj);
            return err;
        }
        if (f->estatico) {
            Valor *err = error_rt("ErrorTipo", n->linea,
                "'%s' es un método estático: llámalo como %s.%s(...).",
                met_nombre, cls->nombre, met_nombre);
            valor_destruir(obj);
            return err;
        }
        if (f->vis != VIS_PUBLICO && !acceso_permitido(f->vis, f->duena, e)) {
            Valor *err = error_rt("ErrorAcceso", n->linea,
                "El método '%s' de '%s' es %s: no se puede llamar desde aquí.",
                met_nombre, f->duena ? f->duena->nombre : cls->nombre, nombre_vis(f->vis));
            valor_destruir(obj);
            return err;
        }

        if (n->metodo.num_argumentos != f->num_parametros) {
            Valor *err = error_rt("ErrorArgumento", n->linea,
                "El método '%s' espera %d argumento(s), recibió %d.",
                met_nombre, f->num_parametros, n->metodo.num_argumentos);
            valor_destruir(obj);
            return err;
        }

        if (profundidad >= 0 && profundidad + 1 < PILA_MAX) { _pila_nombre[profundidad + 1] = met_nombre; _pila_linea[profundidad + 1] = n->linea; }
        if (++profundidad > MAX_PROFUNDIDAD) {
            profundidad = 0;
            char msg[128];
            snprintf(msg, sizeof(msg),
                "Demasiadas llamadas anidadas en método '%s'.", met_nombre);
            valor_error = valor_error_nuevo("ErrorRecursion", msg, n->linea);
            hay_error   = 1;
            valor_destruir(obj);
            return valor_nulo();
        }
        Entorno *fn_e = entorno_crear(f->entorno_closure);
        for (int j = 0; j < f->num_parametros; j++) {
            Valor *arg = ejecutar(n->metodo.argumentos[j], e);
            validar_tipo(arg, f->parametros[j].tipo, "parámetro", f->parametros[j].nombre, n->linea);
            entorno_definir_i(fn_e, PI(&f->parametros[j]), arg, 0);
        }
        Valor *esto_anterior   = esto_actual;
        ClaseLince *cls_ant    = clase_actual;
        esto_actual  = obj;
        clase_actual = cls;
        valor_destruir(ejecutar_cuerpo(f, fn_e));   /* ver la nota de NODO_LLAMADA */
        Valor *retorno = tomar_retorno();
        esto_actual  = esto_anterior;
        clase_actual = cls_ant;
        entorno_destruir(fn_e);
        valor_destruir(obj);
        profundidad--;
        return retorno;
    }

    if (obj->tipo == VAL_BYTES) return metodo_bytes(obj, met, n, e);
    if (obj->tipo == VAL_TAREA) return metodo_tarea(obj, met, n, e);

    /* ── Métodos de LISTA ── */
    if (obj->tipo == VAL_LISTA) {
        if (strcmp(met, "longitud") == 0) {
            int len = obj->lista.cantidad;
            valor_destruir(obj);
            return valor_numero(len);
        }
        if (strcmp(met, "agregar") == 0) {
            if (n->metodo.num_argumentos != 1) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea, "'agregar' necesita exactamente 1 argumento.");
            }
            Valor *elem = ejecutar(n->metodo.argumentos[0], e);
            lista_agregar(obj, elem);
            if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR)
                entorno_asignar(e, n->metodo.objeto->identificador, obj, n->linea);
            else valor_destruir(obj);
            return valor_nulo();
        }
        if (strcmp(met, "eliminar") == 0) {
            if (n->metodo.num_argumentos != 1) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea,
                    "'eliminar' necesita exactamente 1 argumento (índice).");
            }
            Valor *vidx = ejecutar(n->metodo.argumentos[0], e);
            int idx = (int)vidx->numero;
            valor_destruir(vidx);
            if (idx < 0 || idx >= obj->lista.cantidad) {
                int cant = obj->lista.cantidad;
                valor_destruir(obj);
                return error_rt("ErrorRango", n->linea,
                    "Índice %d fuera de rango. La lista tiene %d elementos.", idx, cant);
            }
            valor_destruir(obj->lista.elementos[idx]);
            for (int i = idx; i < obj->lista.cantidad - 1; i++)
                obj->lista.elementos[i] = obj->lista.elementos[i+1];
            obj->lista.cantidad--;
            if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR)
                entorno_asignar(e, n->metodo.objeto->identificador, obj, n->linea);
            else valor_destruir(obj);
            return valor_nulo();
        }
        if (strcmp(met, "contiene") == 0) {
            if (n->metodo.num_argumentos != 1) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea, "'contiene' necesita exactamente 1 argumento.");
            }
            Valor *buscado = ejecutar(n->metodo.argumentos[0], e);
            int encontrado = 0;
            for (int i = 0; i < obj->lista.cantidad; i++) {
                Valor *el = obj->lista.elementos[i];
                if (el->tipo == VAL_NUMERO && buscado->tipo == VAL_NUMERO && el->numero == buscado->numero) { encontrado = 1; break; }
                if (el->tipo == VAL_TEXTO  && buscado->tipo == VAL_TEXTO  && strcmp(el->texto, buscado->texto) == 0) { encontrado = 1; break; }
                if (iguales_enum(el, buscado) == 1) { encontrado = 1; break; }
            }
            valor_destruir(buscado); valor_destruir(obj);
            return valor_booleano(encontrado);
        }
        if (strcmp(met, "ordenar") == 0) {
            /* Devuelve una lista nueva: la original no se toca.
               Sin argumentos usa el orden natural; con una
               función, la usa como comparador (negativo si el
               primero va antes, positivo si va después). */
            CtxOrden ctx = { NULL };
            Valor *vfn = NULL;
            if (n->metodo.num_argumentos >= 1) {
                vfn = ejecutar(n->metodo.argumentos[0], e);
                if (vfn->tipo != VAL_FUNCION) {
                    valor_destruir(vfn); valor_destruir(obj);
                    hay_error = 1;
                    valor_error = valor_error_nuevo("ErrorTipo",
                        "'ordenar' espera una función comparadora.", n->linea);
                    return valor_nulo();
                }
                ctx.fn = vfn;
            }
            int len = obj->lista.cantidad;
            Valor *res = valor_lista_crear();
            for (int i = 0; i < len; i++)
                lista_agregar(res, valor_copiar(obj->lista.elementos[i]));
            if (len > 1) {
                Valor **tmp = malloc(sizeof(Valor*) * len);
                orden_merge(res->lista.elementos, tmp, 0, len, &ctx);
                free(tmp);
            }
            if (vfn) valor_destruir(vfn);
            valor_destruir(obj);
            return res;
        }
        if (strcmp(met, "invertir") == 0) {
            /* Lista nueva, del final al principio. */
            Valor *res = valor_lista_crear();
            for (int i = obj->lista.cantidad - 1; i >= 0; i--)
                lista_agregar(res, valor_copiar(obj->lista.elementos[i]));
            valor_destruir(obj);
            return res;
        }
        if (strcmp(met, "copiar") == 0) {
            /* Copia superficial: útil para no modificar la
               original, porque las listas se pasan compartidas. */
            Valor *res = valor_lista_crear();
            for (int i = 0; i < obj->lista.cantidad; i++)
                lista_agregar(res, valor_copiar(obj->lista.elementos[i]));
            valor_destruir(obj);
            return res;
        }
        if (strcmp(met, "posicion") == 0) {
            if (n->metodo.num_argumentos != 1) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea, "'posicion' necesita exactamente 1 argumento.");
            }
            Valor *buscado = ejecutar(n->metodo.argumentos[0], e);
            int donde = -1;
            for (int i = 0; i < obj->lista.cantidad; i++) {
                if (valor_comparar_natural(obj->lista.elementos[i], buscado) == 0) {
                    donde = i; break;
                }
            }
            valor_destruir(buscado); valor_destruir(obj);
            return valor_numero(donde);
        }
        if (strcmp(met, "insertar") == 0) {
            if (n->metodo.num_argumentos != 2) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea,
                    "'insertar' necesita 2 argumentos (índice y valor).");
            }
            Valor *vidx = ejecutar(n->metodo.argumentos[0], e);
            Valor *elem = ejecutar(n->metodo.argumentos[1], e);
            int idx = (int)vidx->numero;
            valor_destruir(vidx);
            /* Insertar al final es válido, de ahí el <=. */
            if (idx < 0 || idx > obj->lista.cantidad) {
                valor_destruir(elem); valor_destruir(obj);
                hay_error = 1;
                valor_error = valor_error_nuevo("ErrorRango",
                    "Índice fuera de rango en 'insertar'.", n->linea);
                return valor_nulo();
            }
            /* Se añade al final para que crezca y luego se corre
               todo un hueco a la derecha. */
            lista_agregar(obj, elem);
            for (int i = obj->lista.cantidad - 1; i > idx; i--)
                obj->lista.elementos[i] = obj->lista.elementos[i-1];
            obj->lista.elementos[idx] = elem;
            if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR)
                entorno_asignar(e, n->metodo.objeto->identificador, obj, n->linea);
            else valor_destruir(obj);
            return valor_nulo();
        }
        valor_destruir(obj);
        return error_rt("Error", n->linea, "Las listas no tienen el método '%s'.", met);
    }

    /* ── Métodos de TEXTO ── */
    if (obj->tipo == VAL_TEXTO) {
        if (strcmp(met, "longitud") == 0) {
            /* Caracteres, no bytes: "niño" mide 4. */
            texto_medir(obj);
            int len = obj->nchars;
            valor_destruir(obj);
            return valor_numero(len);
        }
        if (strcmp(met, "mayusculas") == 0) {
            char *s = utf8_mayusculas(obj->texto);
            Valor *r = valor_texto(s); free(s); valor_destruir(obj);
            return r;
        }
        if (strcmp(met, "minusculas") == 0) {
            char *s = utf8_minusculas(obj->texto);
            Valor *r = valor_texto(s); free(s); valor_destruir(obj);
            return r;
        }
        if (strcmp(met, "contiene") == 0) {
            if (n->metodo.num_argumentos != 1) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea, "'contiene' necesita exactamente 1 argumento.");
            }
            Valor *sub = ejecutar(n->metodo.argumentos[0], e);
            int ok = strstr(obj->texto, sub->texto) != NULL;
            valor_destruir(sub); valor_destruir(obj);
            return valor_booleano(ok);
        }
        if (strcmp(met, "reemplazar") == 0) {
            if (n->metodo.num_argumentos != 2) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea,
                    "'reemplazar' necesita 2 argumentos: buscar y reemplazo.");
            }
            Valor *buscar = ejecutar(n->metodo.argumentos[0], e);
            Valor *repla  = ejecutar(n->metodo.argumentos[1], e);
            Valor *r;
            if (hay_error) r = valor_nulo();
            else if (buscar->tipo != VAL_TEXTO || repla->tipo != VAL_TEXTO)
                r = error_rt("ErrorTipo", n->linea, "'reemplazar' necesita dos textos: buscar y reemplazo.");
            else if (!buscar->texto[0])
                r = error_rt("ErrorArgumento", n->linea, "'reemplazar': el texto que se busca no puede estar vacío.");
            else r = valor_texto_propio(texto_reemplazar(obj->texto, buscar->texto, repla->texto));
            valor_destruir(buscar); valor_destruir(repla); valor_destruir(obj);
            return r;
        }
        if (strcmp(met, "recortar") == 0) {
            char *s = strdup(obj->texto);
            int i = 0, j = strlen(s) - 1;
            while (s[i] && isspace((unsigned char)s[i])) i++;
            while (j >= i && isspace((unsigned char)s[j])) j--;
            s[j+1] = '\0';
            Valor *r = valor_texto(s + i); free(s); valor_destruir(obj);
            return r;
        }
        valor_destruir(obj);
        return error_rt("Error", n->linea, "El texto no tiene el método '%s'.", met);
    }

    /* ── Métodos de DICCIONARIO ── */
    if (obj->tipo == VAL_DICCIONARIO) {
        if (strcmp(met, "contiene") == 0) {
            if (n->metodo.num_argumentos != 1) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea, "'contiene' necesita exactamente 1 argumento.");
            }
            Valor *clave = ejecutar(n->metodo.argumentos[0], e);
            int ok = 0;
            ok = valor_diccionario_buscar(obj, clave->texto) >= 0;
            valor_destruir(clave); valor_destruir(obj);
            return valor_booleano(ok);
        }
        if (strcmp(met, "longitud") == 0) {
            int len = obj->diccionario.cantidad;
            valor_destruir(obj);
            return valor_numero(len);
        }
        if (strcmp(met, "eliminar") == 0) {
            if (n->metodo.num_argumentos != 1) {
                valor_destruir(obj);
                return error_rt("ErrorArgumento", n->linea,
                    "'eliminar' necesita exactamente 1 argumento (clave).");
            }
            Valor *clave = ejecutar(n->metodo.argumentos[0], e);
            {
                int i = valor_diccionario_buscar(obj, clave->texto);
                if (i >= 0) {
                    free(obj->diccionario.claves[i]);
                    valor_destruir(obj->diccionario.valores[i]);
                    for (int j = i; j < obj->diccionario.cantidad - 1; j++) {
                        obj->diccionario.claves[j]  = obj->diccionario.claves[j+1];
                        obj->diccionario.valores[j] = obj->diccionario.valores[j+1];
                    }
                    obj->diccionario.cantidad--;
                    obj->diccionario.indice_n = -1;     /* las posiciones cambiaron */
                }
            }
            if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR)
                entorno_asignar(e, n->metodo.objeto->identificador, obj, n->linea);
            valor_destruir(clave);
            return valor_nulo();
        }
        valor_destruir(obj);
        return error_rt("Error", n->linea, "Los diccionarios no tienen el método '%s'.", met);
    }

    valor_destruir(obj);
    return error_rt("Error", n->linea, "El tipo actual no tiene el método '%s'.", met);
}

static Valor *exec_elegir(Nodo *n, Entorno *e) {
    Valor *sujeto = ejecutar(n->elegir.sujeto, e);
    int    encontrado = 0;

    for (int i = 0; i < n->elegir.num_casos && !hay_error && !hay_retorno; i++) {
        Valor *caso_val = ejecutar(n->elegir.valores[i], e);

        /* Comparación por igualdad */
        int iguales = iguales_enum(sujeto, caso_val);
        if (iguales >= 0) {}
        else if (sujeto->tipo == VAL_NUMERO && caso_val->tipo == VAL_NUMERO)
            iguales = (sujeto->numero == caso_val->numero);
        else if (sujeto->tipo == VAL_TEXTO && caso_val->tipo == VAL_TEXTO)
            iguales = (strcmp(sujeto->texto, caso_val->texto) == 0);
        else if (sujeto->tipo == VAL_BOOLEANO && caso_val->tipo == VAL_BOOLEANO)
            iguales = (sujeto->booleano == caso_val->booleano);
        else if (sujeto->tipo == VAL_NULO && caso_val->tipo == VAL_NULO)
            iguales = 1;

        valor_destruir(caso_val);

        if (iguales) {
            encontrado = 1;
            valor_destruir(ejecutar_en_ambito(n->elegir.cuerpos[i], e));
            break;
        }
    }

    /* Si no coincidió ningún caso, ejecutar otro */
    if (!encontrado && n->elegir.otro && !hay_error && !hay_retorno) {
        valor_destruir(ejecutar_en_ambito(n->elegir.otro, e));
    }

    valor_destruir(sujeto);
    return valor_nulo();
}

static Valor *exec_padre(Nodo *n, Entorno *e) {
    if (!esto_actual || !clase_actual) {
        return error_rt("Error", n->linea,
            "'padre()' solo se puede usar dentro del método 'crear' de una clase.");
    }
    ClaseLince *cls_padre = clase_actual->padre;
    if (!cls_padre) {
        return error_rt("Error", n->linea,
            "Esta clase no extiende ninguna otra — no hay 'padre' al que llamar.");
    }
    FuncionLince *f = buscar_metodo(cls_padre, "crear");
    if (!f) return valor_nulo(); /* padre sin crear — ok */

    if (n->padre.num_argumentos != f->num_parametros) {
        return error_rt("ErrorArgumento", n->linea,
            "'padre()' espera %d argumento(s), recibió %d.",
            f->num_parametros, n->padre.num_argumentos);
    }

    Entorno *fn_e = entorno_crear(f->entorno_closure);
    for (int i = 0; i < f->num_parametros; i++) {
        Valor *arg = ejecutar(n->padre.argumentos[i], e);
        validar_tipo(arg, f->parametros[i].tipo, "parámetro", f->parametros[i].nombre, n->linea);
        entorno_definir_i(fn_e, PI(&f->parametros[i]), arg, 0);
    }

    ClaseLince *cls_ant = clase_actual;
    clase_actual = cls_padre;
    valor_destruir(ejecutar_cuerpo(f, fn_e));   /* ver la nota de NODO_LLAMADA */
    clase_actual  = cls_ant;
    if (hay_retorno) valor_destruir(valor_retorno);
    hay_retorno   = 0;
    valor_retorno = NULL;
    entorno_destruir(fn_e);
    return valor_nulo();
}

static Valor *exec_intentar(Nodo *n, Entorno *e) {
    /* Ejecutar el bloque intentar */
    Entorno *e_try = entorno_crear(e);
    valor_destruir(ejecutar(n->intentar.cuerpo, e_try));   /* ver la nota de NODO_LLAMADA */
    entorno_destruir(e_try);

    /* La recarga en caliente usa un error para deshacer el programa que corre:
       ese no lo captura nadie. */
    if (hay_error && !es_tarea_parada() &&
        !(valor_error && valor_error->tipo == VAL_ERROR &&
          strcmp(valor_error->error->tipo, ERROR_RECARGA) == 0)) {
        Valor *err = valor_error;
        hay_error   = 0;
        valor_error = NULL;

        int capturado = 0;
        for (int i = 0; i < n->intentar.num_capturas && !capturado; i++) {
            const char *tipo_cap = n->intentar.tipos_captura[i];

            /* "error" captura cualquier tipo */
            int coincide_tipo = (strcmp(tipo_cap, "error") == 0) ||
                                (err->tipo == VAL_ERROR &&
                                 strcmp(err->error->tipo, tipo_cap) == 0);

            if (coincide_tipo) {
                /* Crear entorno con la variable del error */
                Entorno *e_cap = entorno_crear(e);

                /* Exponer el error como objeto con campos */
                ObjetoLince *obj = calloc(1, sizeof(ObjetoLince));
                obj->clase      = NULL;
                obj->num_campos = 3;
                obj->campos_nombres[0] = strdup("mensaje");
                obj->campos_valores[0] = valor_texto(err->error->mensaje);
                obj->campos_nombres[1] = strdup("tipo");
                obj->campos_valores[1] = valor_texto(err->error->tipo);
                obj->campos_nombres[2] = strdup("linea");
                obj->campos_valores[2] = valor_numero(err->error->linea);

                entorno_definir(e_cap, n->intentar.vars_captura[i],
                                valor_objeto(obj), 0);

                valor_destruir(ejecutar(n->intentar.cuerpos_captura[i], e_cap));   /* ver la nota de NODO_LLAMADA */
                entorno_destruir(e_cap);
                capturado = 1;
            }
        }

        if (!capturado) {
            /* Ningún capturar coincidió — propagar */
            hay_error   = 1;
            valor_error = err;
        } else {
            valor_destruir(err);
        }
    }

    /* finalmente — siempre se ejecuta */
    if (n->intentar.finalmente) {
        int error_guardado   = hay_error;
        Valor *err_guardado  = valor_error;
        int retorno_guardado = hay_retorno;
        Valor *ret_guardado  = valor_retorno;
        int salto_guardado   = hay_salto;

        hay_error   = 0;
        valor_error = NULL;
        hay_retorno = 0;
        hay_salto   = SALTO_NINGUNO;

        Entorno *e_fin = entorno_crear(e);
        valor_destruir(ejecutar(n->intentar.finalmente, e_fin));   /* ver la nota de NODO_LLAMADA */
        entorno_destruir(e_fin);

        /* Restaurar señales. Si el propio 'finalmente' ha puesto un
           error o ha hecho 'devolver', gana lo suyo — pero entonces lo
           que veníamos guardando hay que soltarlo, que si no se pierde
           con su tipo y su mensaje. */
        if (!hay_error) {
            hay_error   = error_guardado;
            valor_error = err_guardado;
        } else {
            valor_destruir(err_guardado);
        }
        if (!hay_retorno) {
            hay_retorno   = retorno_guardado;
            valor_retorno = ret_guardado;
        } else {
            valor_destruir(ret_guardado);
        }
        if (!hay_salto && !hay_retorno && !hay_error) hay_salto = salto_guardado;
    }

    return valor_nulo();
}

/* Posición de 'campo' en el objeto (-1 si no está), probando antes la del
   último acceso desde este nodo: casi siempre acierta. */
static inline int campo_objeto(Nodo *n, ObjetoLince *o, const char *campo) {
    int k = n->cache_i;
    if (k < o->num_campos && strcmp(o->campos_nombres[k], campo) == 0) return k;
    for (int i = 0; i < o->num_campos; i++)
        if (strcmp(o->campos_nombres[i], campo) == 0) { n->cache_i = i; return i; }
    return -1;
}

/* buscar_campo(clase, campo, 0) recordado por nodo y clase */
static inline CampoClase *campo_declarado(Nodo *n, ClaseLince *c, const char *campo) {
    if (n->cache_clase != c) {
        n->cache_cd = buscar_campo(c, campo, 0);
        n->cache_clase = c;
    }
    return (CampoClase *)n->cache_cd;
}

/* obj.campo con obj ya calculado (se queda con él). Lo usan la lectura y
   'obj.campo += v'. */
static Valor *acceso_en(Nodo *n, Valor *obj, const char *campo, Entorno *e) {
    /* Estado.Quieto.nombre · .valor · .indice */
    if (obj->tipo == VAL_ENUMERADO) {
        MiembroEnum *m = obj->miembro;
        Valor *r = NULL;
        if (strcmp(campo, "nombre") == 0)      r = valor_texto(m->texto + m->largo_enum + 1);
        else if (strcmp(campo, "valor") == 0)  r = valor_copiar(m->valor);
        else if (strcmp(campo, "indice") == 0) r = valor_numero(m->indice);
        valor_destruir(obj);
        if (r) return r;
        return error_rt("Error", n->linea,
            "Un valor de una enumeración tiene 'nombre', 'valor' e 'indice'; no tiene '%s'.", campo);
    }
    /* Acceso a constante de módulo */
    if (obj->tipo == VAL_DICCIONARIO && obj->es_modulo) {
        {
            int i = valor_diccionario_buscar(obj, campo);
            if (i >= 0) {
                Valor *r = valor_copiar(obj->diccionario.valores[i]);
                valor_destruir(obj);
                return r;
            }
        }
        if (obj->es_modulo == ES_ENUMERACION && obj->diccionario.cantidad > 0) {
            /* "Estado no tiene 'Volando'. Tiene Quieto, Andando y Saltando." */
            MiembroEnum *m0 = obj->diccionario.valores[0]->miembro;
            char lista[400] = "";
            size_t l = 0;
            int cant = obj->diccionario.cantidad;
            for (int i = 0; i < cant && l < sizeof lista - 40; i++)
                l += (size_t)snprintf(lista + l, sizeof lista - l, "%s%s",
                                      i == 0 ? "" : i == cant - 1 ? " y " : ", ", obj->diccionario.claves[i]);
            Valor *err = error_rt("Error", n->linea, "La enumeración %.*s no tiene '%s'. Tiene %s.",
                                  m0->largo_enum, m0->texto, campo, lista);
            valor_destruir(obj);
            return err;
        }
        valor_destruir(obj);
        return error_rt("Error", n->linea, "El módulo no tiene el campo '%s'.", campo);
    }
    if (obj->tipo == VAL_OBJETO) {
        int i = campo_objeto(n, obj->objeto, campo);
        if (i >= 0) {
            {
                CampoClase *cd = campo_declarado(n, obj->objeto->clase, campo);
                if (cd && cd->vis != VIS_PUBLICO && !acceso_permitido(cd->vis, cd->duena, e)) {
                    Valor *err = error_rt("ErrorAcceso", n->linea,
                        "El campo '%s' de '%s' es %s: no se puede leer desde aquí.",
                        campo, cd->duena->nombre, nombre_vis(cd->vis));
                    valor_destruir(obj);
                    return err;
                }
                Valor *r = valor_copiar(obj->objeto->campos_valores[i]);
                valor_destruir(obj);
                return r;
            }
        }
        /* Sin campo con ese nombre: ¿una propiedad 'obtener'? */
        char pn[256];
        snprintf(pn, sizeof(pn), "obtener %s", campo);
        FuncionLince *g = buscar_metodo(obj->objeto->clase, pn);
        if (g) {
            if (g->vis != VIS_PUBLICO && !acceso_permitido(g->vis, g->duena, e)) {
                Valor *err = error_rt("ErrorAcceso", n->linea,
                    "La propiedad '%s' de '%s' es %s.", campo, g->duena->nombre, nombre_vis(g->vis));
                valor_destruir(obj);
                return err;
            }
            Valor *r = llamar_metodo(obj, obj->objeto->clase, g, NULL, 0, n->linea);
            valor_destruir(obj);
            return r;
        }
        valor_destruir(obj);
        return error_rt("Error", n->linea, "El objeto no tiene el campo '%s'.", campo);
    }
    /* Campo estático: Clase.campo */
    if (obj->tipo == VAL_CLASE) {
        CampoClase *cd = buscar_campo(obj->clase, campo, 1);
        Valor *err = NULL;
        if (!cd)
            err = error_rt("Error", n->linea,
                "La clase '%s' no tiene el campo estático '%s'.", obj->clase->nombre, campo);
        else if (!acceso_permitido(cd->vis, cd->duena, e))
            err = error_rt("ErrorAcceso", n->linea,
                "El campo estático '%s' de '%s' es %s: no se puede leer desde aquí.",
                campo, cd->duena->nombre, nombre_vis(cd->vis));
        if (err) { valor_destruir(obj); return err; }
        Valor *r = valor_copiar(cd->valor);
        valor_destruir(obj);
        return r;
    }
    valor_destruir(obj);
    return error_rt("ErrorTipo", n->linea, "Acceso a campo en un tipo que no es objeto.");
}

static Valor *exec_acceso(Nodo *n, Entorno *e) {
    return acceso_en(n, ejecutar(n->acceso.objeto, e), n->acceso.campo, e);
}

/* Crea el FuncionLince de un método de clase a partir de su nodo. */
static FuncionLince *metodo_desde_nodo(Nodo *mn, ClaseLince *c, Entorno *ce) {
    FuncionLince *f = calloc(1, sizeof(FuncionLince));
    f->nombre           = strdup(mn->funcion.nombre);
    f->parametros       = mn->funcion.parametros;
    f->num_parametros   = mn->funcion.num_parametros;
    f->tipo_retorno     = mn->funcion.tipo_retorno;
    f->cuerpo           = mn->funcion.cuerpo;
    f->entorno_closure  = ce;
    marcar_tarea(f, mn);
    f->vis              = mn->funcion.vis;
    f->estatico         = mn->funcion.estatico;
    f->abstracto        = mn->funcion.abstracto;
    f->prop             = mn->funcion.prop;
    f->duena            = c;
    return f;
}

static void clase_agregar_metodo(ClaseLince *c, FuncionLince *f) {
    c->metodos = realloc(c->metodos, sizeof(FuncionLince*) * (c->num_metodos + 1));
    c->metodos[c->num_metodos++] = f;
}

static Valor *exec_clase(Nodo *n, Entorno *e) {
    ClaseLince *c = calloc(1, sizeof(ClaseLince));
    c->nombre         = strdup(n->clase.nombre);
    c->num_metodos    = n->clase.num_metodos;
    c->metodos        = malloc(sizeof(FuncionLince*) * (c->num_metodos + 1));
    c->padre          = NULL;
    c->interfaces     = malloc(sizeof(InterfazLince*) * 16);
    c->num_interfaces = 0;
    c->abstracta      = n->clase.abstracta;

    /* Resolver clase padre */
    if (n->clase.padre) {
        Valor *vpadre = entorno_obtener(e, n->clase.padre, n->linea);
        if (vpadre->tipo != VAL_CLASE) {
            valor_destruir(vpadre);
            return error_rt("ErrorTipo", n->linea,
                "'%s' no es una clase — no se puede extender.", n->clase.padre);
        }
        c->padre = vpadre->clase;
        valor_destruir(vpadre);
    }

    /* Ámbito de la clase: el código de sus métodos (y de las lambdas que
       contengan) lleva anotado a qué clase pertenece, y de ahí sale la
       visibilidad privada / protegida. */
    Entorno *ce = entorno_crear(e);
    ce->dueno = c;
    c->entorno = ce;

    /* Registrar métodos */
    for (int i = 0; i < c->num_metodos; i++)
        c->metodos[i] = metodo_desde_nodo(n->clase.metodos[i], c, ce);

    /* 'sobrescribe' tiene que sobrescribir algo */
    for (int i = 0; i < c->num_metodos; i++) {
        if (!n->clase.metodos[i]->funcion.sobrescribe) continue;
        if (!c->padre || !buscar_metodo(c->padre, c->metodos[i]->nombre))
            return error_rt("Error", n->linea,
                "'%s.%s' dice 'sobrescribe', pero ninguna clase padre tiene un método '%s'.",
                c->nombre, c->metodos[i]->nombre, c->metodos[i]->nombre);
    }

    /* Campos declarados */
    c->num_campos = n->clase.num_campos;
    c->campos     = calloc(c->num_campos + 1, sizeof(CampoClase));
    for (int i = 0; i < c->num_campos; i++) {
        Nodo *cn = n->clase.campos[i];
        CampoClase *cd = &c->campos[i];
        cd->nombre    = strdup(cn->declaracion.nombre);
        cd->vis       = cn->declaracion.vis;
        cd->estatico  = cn->declaracion.estatico;
        cd->constante = cn->declaracion.constante;
        cd->inicial   = cn->declaracion.valor;
        cd->duena     = c;
        cd->valor     = valor_nulo();
    }

    /* Verificar interfaces */
    for (int i = 0; i < n->clase.num_interfaces; i++) {
        Valor *viface = entorno_obtener(e, n->clase.interfaces[i], n->linea);
        if (!viface || viface->tipo != VAL_DICCIONARIO || viface->es_modulo != 2) {
            valor_destruir(viface);
            return error_rt("ErrorTipo", n->linea,
                "'%s' no es una interfaz.", n->clase.interfaces[i]);
        }
        InterfazLince *iface = (InterfazLince*)(uintptr_t)
            (long long)viface->diccionario.valores[0]->numero;
        c->interfaces[c->num_interfaces++] = iface;

        for (int j = 0; j < iface->num_metodos; j++) {
            /* Un método con cuerpo en la interfaz es el de por defecto: lo
               recibe la clase si no trae uno propio (ni heredado). */
            if (iface->funciones && iface->funciones[j]) {
                if (!buscar_metodo(c, iface->metodos[j]))
                    clase_agregar_metodo(c, metodo_desde_nodo(iface->funciones[j], c, ce));
                continue;
            }
            if (!c->abstracta && !buscar_metodo(c, iface->metodos[j])) {
                Valor *err = error_rt("Error", n->linea,
                    "La clase '%s' dice implementar '%s' pero le falta el método '%s'.",
                    c->nombre, iface->nombre, iface->metodos[j]);
                valor_destruir(viface);
                return err;
            }
        }
        valor_destruir(viface);
    }

    /* Una clase concreta tiene que implementar todo lo abstracto que hereda */
    if (!c->abstracta) {
        for (ClaseLince *a = c->padre; a; a = a->padre) {
            for (int i = 0; i < a->num_metodos; i++) {
                if (!a->metodos[i]->abstracto) continue;
                FuncionLince *real = buscar_metodo(c, a->metodos[i]->nombre);
                if (!real || real->abstracto)
                    return error_rt("Error", n->linea,
                        "La clase '%s' no es abstracta pero no implementa '%s', abstracto en '%s'.",
                        c->nombre, a->metodos[i]->nombre, a->nombre);
            }
            for (int k = 0; k < a->num_interfaces; k++) {
                InterfazLince *ia = a->interfaces[k];
                for (int j = 0; j < ia->num_metodos; j++) {
                    if (ia->funciones && ia->funciones[j]) continue;
                    FuncionLince *real = buscar_metodo(c, ia->metodos[j]);
                    if (!real || real->abstracto)
                        return error_rt("Error", n->linea,
                            "La clase '%s' no implementa '%s', que exige la interfaz '%s'.",
                            c->nombre, ia->metodos[j], ia->nombre);
                }
            }
        }
    }

    if (redefinido(e, c->nombre, "esta clase", 0, n->linea))
        return valor_nulo();
    entorno_definir(e, c->nombre, valor_clase(c), 0);

    /* Campos estáticos: se inicializan al declarar la clase, ya visible por su nombre */
    for (int i = 0; i < c->num_campos; i++) {
        CampoClase *cd = &c->campos[i];
        if (!cd->estatico || !cd->inicial) continue;
        Entorno *ie = entorno_crear(ce);
        valor_destruir(cd->valor);
        cd->valor = ejecutar(cd->inicial, ie);
        entorno_destruir(ie);
        if (hay_error) return valor_nulo();
    }
    return valor_nulo();
}

static Valor *exec_asignacion_campo(Nodo *n, Entorno *e) {
    Valor *val = ejecutar(n->asignacion_campo.valor, e);
    Valor *obj;
    int propio = n->asignacion_campo.objeto->tipo != NODO_ESTO;   /* ¿hay que soltar obj? */

    if (!propio) {
        obj = esto_actual;
    } else {
        obj = ejecutar(n->asignacion_campo.objeto, e);
    }
    const char *campo = n->asignacion_campo.campo;

    if (n->asignacion_campo.op[0] && !hay_error) {            /* obj.campo += v */
        if (!obj)
            valor_destruir(error_rt("Error", n->linea,
                "'mi' solo se puede usar dentro de un método de una clase."));
        else {
            Valor *antes = acceso_en(n, valor_retener(obj), campo, e);
            val = operar(antes, val, n->asignacion_campo.op, n->linea);
        }
    }
    if (hay_error) {
        valor_destruir(val);
        if (propio && obj) valor_destruir(obj);
        return valor_nulo();
    }

    /* Campo estático: Clase.campo = valor */
    if (obj && obj->tipo == VAL_CLASE) {
        CampoClase *cd = buscar_campo(obj->clase, campo, 1);
        Valor *err = NULL;
        if (!cd)
            err = error_rt("Error", n->linea,
                "La clase '%s' no tiene el campo estático '%s'.", obj->clase->nombre, campo);
        else if (!acceso_permitido(cd->vis, cd->duena, e))
            err = error_rt("ErrorAcceso", n->linea,
                "El campo estático '%s' de '%s' es %s: no se puede modificar desde aquí.",
                campo, cd->duena->nombre, nombre_vis(cd->vis));
        else if (cd->constante)
            err = error_rt("ErrorTipo", n->linea,
                "'%s.%s' es constante ('fijo'): no se puede modificar.", cd->duena->nombre, campo);
        if (err) { valor_destruir(val); if (propio) valor_destruir(obj); return err; }
        valor_destruir(cd->valor);
        cd->valor = val;
        if (propio) valor_destruir(obj);
        return valor_nulo();
    }

    if (!obj || obj->tipo != VAL_OBJETO) {
        valor_destruir(val);
        if (obj && propio) valor_destruir(obj);
        return error_rt("ErrorTipo", n->linea, "Solo puedes asignar campos a objetos.");
    }

    /* Buscar si el campo ya existe */
    int i = campo_objeto(n, obj->objeto, campo);
    if (i >= 0) {
        {
            CampoClase *cd = campo_declarado(n, obj->objeto->clase, campo);
            Valor *err = NULL;
            if (cd && cd->vis != VIS_PUBLICO && !acceso_permitido(cd->vis, cd->duena, e))
                err = error_rt("ErrorAcceso", n->linea,
                    "El campo '%s' de '%s' es %s: no se puede modificar desde aquí.",
                    campo, cd->duena->nombre, nombre_vis(cd->vis));
            else if (cd && cd->constante && cd->inicial)
                err = error_rt("ErrorTipo", n->linea,
                    "El campo '%s' es constante ('fijo'): no se puede modificar.", campo);
            if (err) { valor_destruir(val); if (propio) valor_destruir(obj); return err; }
            valor_destruir(obj->objeto->campos_valores[i]);
            obj->objeto->campos_valores[i] = val;
            if (propio) valor_destruir(obj);
            return valor_nulo();
        }
    }

    /* Sin campo con ese nombre: ¿una propiedad 'fijar'? */
    char pn[256];
    snprintf(pn, sizeof(pn), "fijar %s", campo);
    FuncionLince *st = buscar_metodo(obj->objeto->clase, pn);
    if (st) {
        Valor *err = NULL;
        if (st->vis != VIS_PUBLICO && !acceso_permitido(st->vis, st->duena, e))
            err = error_rt("ErrorAcceso", n->linea,
                "La propiedad '%s' de '%s' es %s: no se puede modificar desde aquí.",
                campo, st->duena->nombre, nombre_vis(st->vis));
        if (err) { valor_destruir(val); if (propio) valor_destruir(obj); return err; }
        Valor *args[1] = { val };
        Valor *r = llamar_metodo(obj, obj->objeto->clase, st, args, 1, n->linea);
        valor_destruir(r);
        if (propio) valor_destruir(obj);
        return valor_nulo();
    }
    snprintf(pn, sizeof(pn), "obtener %s", campo);
    if (buscar_metodo(obj->objeto->clase, pn)) {
        valor_destruir(val);
        if (propio) valor_destruir(obj);
        return error_rt("ErrorTipo", n->linea,
            "La propiedad '%s' es de solo lectura (tiene 'obtener' pero no 'fijar').", campo);
    }

    /* Campo nuevo */
    if (!objeto_poner_campo(obj->objeto, campo, val)) {
        valor_destruir(val);
        if (propio) valor_destruir(obj);
        return error_rt("Error", n->linea, "Un objeto no puede tener más de %d campos.", MAX_CAMPOS);
    }
    if (propio) valor_destruir(obj);
    return valor_nulo();
}

static Valor *ejecutar(Nodo *n, Entorno *e) {
    if (!n || hay_retorno || hay_error || hay_salto) return valor_nulo();

    switch (n->tipo) {

        /* Un número o un texto escrito en el programa vale siempre lo mismo:
           se crea la primera vez y luego solo se comparte (inmortal, así que
           nadie lo cambia en su sitio: eso solo se hace con refs == 1) */
        case NODO_NUMERO:
        case NODO_TEXTO:
            if (!n->valor_fijo) {
                Valor *v = n->tipo == NODO_NUMERO ? valor_numero(n->numero) : valor_texto(n->texto);
                v->refs = REFS_INMORTAL;
                n->valor_fijo = v;
            }
            return valor_retener((Valor *)n->valor_fijo);

        case NODO_BOOLEANO:
            return valor_booleano(n->booleano);

        case NODO_NULO:
            return valor_nulo();

        case NODO_IDENTIFICADOR:
            return obtener_var(n, e, NI(n, n->identificador), n->identificador);

        case NODO_BINARIO:
            return exec_binario(n, e);

        case NODO_UNARIO: {
            Valor *v = ejecutar(n->unario.operando, e);
            if (strcmp(n->unario.operador, "-") == 0 && v->tipo == VAL_NUMERO) {
                if (v->refs == 1) { v->numero = -v->numero; return v; }
                Valor *neg = valor_numero(-v->numero);   /* compartido con una variable: no se toca */
                valor_destruir(v);
                return neg;
            }
            if (strcmp(n->unario.operador, "no") == 0) {
                int b = !es_verdadero(v);
                valor_destruir(v);
                return valor_booleano(b);
            }
            return v;
        }

        case NODO_DECLARACION:
            return exec_declaracion(n, e);


        case NODO_ROMPER:
            hay_salto = SALTO_ROMPER;
            return valor_nulo();

        case NODO_CONTINUAR:
            hay_salto = SALTO_CONTINUAR;
            return valor_nulo();

        case NODO_PLANTILLA:
            return exec_plantilla(n, e);

        case NODO_DESESTRUCTURAR:
            return exec_desestructurar(n, e);

        case NODO_SI_EXPR: {
            Valor *c = ejecutar(n->si.condicion, e);
            int ok = es_verdadero(c);
            valor_destruir(c);
            if (hay_error) return valor_nulo();
            return ejecutar(ok ? n->si.entonces : n->si.sino, e);
        }

        case NODO_ASIGNACION: {
            Valor *v = ejecutar(n->asignacion.valor, e);
            if (n->asignacion.op[0] && !hay_error && v->tipo == VAL_NUMERO &&
                (n->asignacion.op[0] == '+' || n->asignacion.op[0] == '-' || n->asignacion.op[0] == '*')) {
                EntradaVar *ev = buscar_var(n, e, NI(n, n->asignacion.nombre));
                if (ev && !ev->constante && ev->valor->tipo == VAL_NUMERO && ev->valor->refs == 1) {
                    char o = n->asignacion.op[0];
                    ev->valor->numero = o == '+' ? ev->valor->numero + v->numero
                                      : o == '-' ? ev->valor->numero - v->numero
                                                 : ev->valor->numero * v->numero;
                    valor_destruir(v);
                    return valor_nulo();
                }
            }
            if (n->asignacion.op[0] && !hay_error) {      /* x += v */
                Valor *antes = obtener_var(n, e, NI(n, n->asignacion.nombre), n->asignacion.nombre);
                v = operar(antes, v, n->asignacion.op, n->linea);
            }
            if (hay_error) { valor_destruir(v); return valor_nulo(); }
            asignar_var(n, e, NI(n, n->asignacion.nombre), n->asignacion.nombre, v);
            return valor_nulo();
        }

        case NODO_INCREMENTO: {
            /* Si la variable es la única dueña de su número, se cambia ahí */
            EntradaVar *ev = buscar_var(n, e, NI(n, n->incremento.nombre));
            if (ev && !ev->constante && ev->valor->tipo == VAL_NUMERO && ev->valor->refs == 1) {
                ev->valor->numero += n->incremento.operador[0] == '+' ? 1 : -1;
                return valor_nulo();
            }
            Valor *v = obtener_var(n, e, NI(n, n->incremento.nombre), n->incremento.nombre);
            /* Si la variable no existe, entorno_obtener ya ha puesto ese
               error: se propaga, no se le añade encima uno de "no contiene un
               número" que sería la consecuencia y no la causa. */
            if (hay_error) { valor_destruir(v); return valor_nulo(); }
            if (v->tipo != VAL_NUMERO) {
                valor_destruir(v);
                return error_rt("ErrorTipo", n->linea,
                    "Solo puedes usar '++' o '--' con variables numéricas. "
                    "'%s' no contiene un número.", n->incremento.nombre);
            }
            Valor *nuevo;
            if (strcmp(n->incremento.operador, "++") == 0)
                nuevo = valor_numero(v->numero + 1);
            else
                nuevo = valor_numero(v->numero - 1);
            /* 'v' es una copia hecha por entorno_obtener y nadie la soltaba:
               cada 'x++' suelto fugaba una asignación. */
            valor_destruir(v);
            asignar_var(n, e, NI(n, n->incremento.nombre), n->incremento.nombre, nuevo);
            return valor_nulo();
        }

        case NODO_ESCRIBIR: {
            Valor *v = ejecutar(n->escribir, e);
            /* Si la expresión falló, no se imprime nada: antes salía un 'nulo'
               por pantalla y sólo después se propagaba el error, así que la
               salida mentía sobre lo que había pasado. */
            if (hay_error) {
                valor_destruir(v);
                return valor_nulo();
            }
            char  *s = valor_a_texto(v);
            printf("%s\n", s);
            free(s);
            valor_destruir(v);
            return valor_nulo();
        }

        case NODO_BLOQUE: {
            Valor *ultimo = valor_nulo();
            for (int i = 0; i < n->bloque.cantidad && !hay_retorno && !hay_error && !hay_salto; i++) {
                valor_destruir(ultimo);
                if (_hook_sentencia && !_en_biblioteca) _hook_sentencia(n->bloque.sentencias[i], e);
                ultimo = ejecutar(n->bloque.sentencias[i], e);
            }
            return ultimo;
        }

        case NODO_SI:
            return exec_si(n, e);


        case NODO_MIENTRAS: {
            Valor *r = valor_nulo();
            while (!hay_retorno) {
                Valor *cond = ejecutar(n->mientras.condicion, e);
                int    ok   = es_verdadero(cond);
                valor_destruir(cond);
                if (!ok) break;
                valor_destruir(r);
                r = ejecutar_en_ambito(n->mientras.cuerpo, e);
                if (hay_salto && fin_de_vuelta()) break;
            }
            return r;
        }

        case NODO_HACER: {
            Valor *r = valor_nulo();
            do {
                valor_destruir(r);
                r = ejecutar_en_ambito(n->hacer.cuerpo, e);
                if (hay_salto && fin_de_vuelta()) break;
                if (hay_retorno) break;
                Valor *cond = ejecutar(n->hacer.condicion, e);
                int    ok   = es_verdadero(cond);
                valor_destruir(cond);
                if (!ok) break;
            } while (1);
            return r;
        }

        case NODO_PARA:
            return exec_para(n, e);


        case NODO_PARA_CADA:
            return exec_para_cada(n, e);


        case NODO_FUNCION: {
            FuncionLince *f = calloc(1, sizeof(FuncionLince));
            f->nombre           = strdup(n->funcion.nombre);
            f->parametros       = n->funcion.parametros;
            f->num_parametros   = n->funcion.num_parametros;
            f->tipo_retorno     = n->funcion.tipo_retorno;
            f->cuerpo           = n->funcion.cuerpo;
            f->entorno_closure  = e;
            marcar_tarea(f, n);
            e->refs++;  /* el closure retiene el entorno */
            if (redefinido(e, n->funcion.nombre, "esta función", 0, n->linea))
                return valor_nulo();
            entorno_definir(e, n->funcion.nombre, valor_funcion(f), 0);
            return valor_nulo();
        }

        case NODO_LLAMADA:
            return exec_llamada(n, e);


        case NODO_DEVOLVER: {
            Valor *v = ejecutar(n->devolver, e);
            /* Si la expresión falló, propagar el error en vez de marcar
               retorno: con 'hay_retorno' puesto, el bloque 'capturar'
               que envuelva a este 'devolver' se saltaría sus sentencias
               y el error escaparía del intentar/capturar como nulo. */
            if (hay_error) {
                valor_destruir(v);
                return valor_nulo();
            }
            valor_retorno = v;
            hay_retorno   = 1;
            return valor_nulo();
        }

        case NODO_LISTA: {
            Valor *lista = valor_lista_crear();
            for (int i = 0; i < n->lista.cantidad; i++)
                lista_agregar(lista, ejecutar(n->lista.elementos[i], e));
            return lista;
        }

        case NODO_DICCIONARIO: {
            Valor *dic = valor_diccionario_crear();
            valor_diccionario_asegurar(dic, n->diccionario.cantidad);
            for (int i = 0; i < n->diccionario.cantidad; i++) {
                dic->diccionario.claves[i]  = strdup(n->diccionario.claves[i]);
                dic->diccionario.valores[i] = ejecutar(n->diccionario.valores[i], e);
                dic->diccionario.cantidad++;
            }
            return dic;
        }

        case NODO_INDICE:
            return exec_indice(n, e);


        case NODO_ASIGNACION_INDICE:
            return exec_asignacion_indice(n, e);


        case NODO_METODO:
            return exec_metodo(n, e);


        case NODO_ELEGIR:
            return exec_elegir(n, e);


        case NODO_GENERADOR: {
            /* Registra el generador como función especial en el entorno */
            FuncionLince *f = calloc(1, sizeof(FuncionLince));
            f->nombre           = strdup(n->generador.nombre);
            f->parametros       = n->generador.parametros;
            f->num_parametros   = n->generador.num_parametros;
            f->tipo_retorno     = TIPO_LISTA; /* siempre devuelve lista */
            f->cuerpo           = n->generador.cuerpo;
            f->entorno_closure  = e;
            e->refs++;
            /* Marcamos que es generador con tipo_retorno especial */
            Valor *vf = valor_funcion(f);
            vf->es_modulo = 99; /* marca: es generador */
            if (redefinido(e, f->nombre, "este generador", 0, n->linea)) {
                valor_destruir(vf);
                return valor_nulo();
            }
            entorno_definir(e, f->nombre, vf, 0);
            return valor_nulo();
        }

        case NODO_PRODUCIR:
            return exec_producir(n, e);

        case NODO_INTERFAZ: {
            /* Registra la interfaz en el entorno como un valor especial */
            InterfazLince *iface = malloc(sizeof(InterfazLince));
            iface->nombre      = strdup(n->interfaz.nombre);
            iface->num_metodos = n->interfaz.num_metodos;
            iface->metodos     = malloc(sizeof(char*) * n->interfaz.num_metodos);
            iface->tipos_retorno = malloc(sizeof(char*) * n->interfaz.num_metodos);
            iface->funciones   = n->interfaz.funciones;
            for (int i = 0; i < n->interfaz.num_metodos; i++) {
                iface->metodos[i]      = strdup(n->interfaz.metodos[i]);
                iface->tipos_retorno[i] = strdup(n->interfaz.tipos_retorno[i]);
            }
            /* Guardamos la interfaz como un valor de texto especial en el entorno */
            /* El valor real es un puntero empaquetado — usamos diccionario de interfaz */
            Valor *viface = valor_diccionario_crear();
            viface->es_modulo = 2; /* 2 = interfaz */
            /* Guardamos el puntero en el primer valor */
            viface->diccionario.claves[0]  = strdup("__interfaz__");
            viface->diccionario.valores[0] = valor_numero((double)(uintptr_t)iface);
            viface->diccionario.cantidad   = 1;
            if (redefinido(e, iface->nombre, "esta interfaz", 0, n->linea)) {
                valor_destruir(viface);
                return valor_nulo();
            }
            entorno_definir(e, iface->nombre, viface, 1);
            return valor_nulo();
        }

        case NODO_ENUMERACION: {
            /* Un diccionario con sus valores, que no es un diccionario más
               (se recorre con 'para cada' y no se puede cambiar) */
            Valor *dic = valor_diccionario_crear();
            dic->es_modulo = ES_ENUMERACION;
            int largo = (int)strlen(n->enumeracion.nombre);
            for (int i = 0; i < n->enumeracion.num_valores && !hay_error; i++) {
                Nodo *ex = n->enumeracion.expresiones ? n->enumeracion.expresiones[i] : NULL;
                Valor *v = ex ? ejecutar(ex, e) : valor_numero(i);
                MiembroEnum *m = calloc(1, sizeof(MiembroEnum));
                size_t tam = (size_t)largo + strlen(n->enumeracion.valores[i]) + 2;
                m->texto = malloc(tam);
                snprintf(m->texto, tam, "%s.%s", n->enumeracion.nombre, n->enumeracion.valores[i]);
                m->largo_enum = largo;
                m->indice = i;
                m->valor  = v;
                Valor *mv = valor_alloc();
                mv->tipo = VAL_ENUMERADO;
                mv->refs = 1; mv->es_modulo = 0;
                mv->miembro = m;
                valor_diccionario_agregar(dic, n->enumeracion.valores[i], mv);
            }
            if (hay_error || redefinido(e, n->enumeracion.nombre, "esta enumeración", 0, n->linea)) {
                valor_destruir(dic);
                return valor_nulo();
            }
            entorno_definir(e, n->enumeracion.nombre, dic, 1); /* constante */
            return valor_nulo();
        }

        case NODO_IMPORTAR:
            return ejecutar_importar(n, e);

        case NODO_LAMBDA: {
            /* Crea una función anónima y la retorna como valor */
            FuncionLince *f = calloc(1, sizeof(FuncionLince));
            f->nombre           = strdup("<lambda>");
            f->parametros       = n->lambda.parametros;
            f->num_parametros   = n->lambda.num_parametros;
            f->tipo_retorno     = n->lambda.tipo_retorno;
            f->cuerpo           = n->lambda.cuerpo;
            f->entorno_closure  = e;
            e->refs++;  /* el closure retiene el entorno */
            if (n->lambda.es_tarea) {
                free(f->nombre);
                f->nombre       = strdup("sin nombre");
                f->es_tarea     = 1;
                f->tipo_tarea   = f->tipo_retorno;
                f->tipo_retorno = TIPO_CUALQUIERA;
            }
            return valor_funcion(f);
        }

        case NODO_PADRE:
            return exec_padre(n, e);


        case NODO_LANZAR: {            Valor *vobj = ejecutar(n->lanzar, e);

            if (vobj->tipo == VAL_ERROR) {
                /* Ya es un error — lanzar directamente */
                valor_error = vobj;
            } else if (vobj->tipo == VAL_OBJETO) {
                /* Objeto de clase propia — envolver como error */
                char *msg  = NULL;
                char *tipo = strdup(vobj->objeto->clase->nombre);
                for (int i = 0; i < vobj->objeto->num_campos; i++) {
                    if (strcmp(vobj->objeto->campos_nombres[i], "mensaje") == 0) {
                        msg = valor_a_texto(vobj->objeto->campos_valores[i]);
                        break;
                    }
                }
                valor_error = valor_error_nuevo(tipo, msg ? msg : "sin mensaje", 0);
                free(tipo);
                if (msg) free(msg);
                valor_destruir(vobj);
            } else {
                char *s = valor_a_texto(vobj);
                valor_error = valor_error_nuevo("Error", s, 0);
                free(s);
                valor_destruir(vobj);
            }
            hay_error = 1;
            return valor_nulo();
        }

        case NODO_INTENTAR:
            return exec_intentar(n, e);


        case NODO_ACCESO:
            return exec_acceso(n, e);


        case NODO_ESTO: {
            if (!esto_actual) {
                return error_rt("Error", n->linea,
                    "'esto' solo se puede usar dentro de un método de una clase.");
            }
            return valor_retener(esto_actual);
        }

        case NODO_CLASE:
            return exec_clase(n, e);


        case NODO_INSTANCIA: {
            Valor *vcls = entorno_obtener(e, n->instancia.clase, n->linea);
            if (vcls->tipo != VAL_CLASE) {
                valor_destruir(vcls);
                return error_rt("ErrorTipo", n->linea,
                    "'%s' no es una clase.", n->instancia.clase);
            }
            return instanciar_clase(vcls, n->instancia.argumentos,
                                    n->instancia.num_argumentos, e, n->linea);
        }

        case NODO_ASIGNACION_CAMPO:
            return exec_asignacion_campo(n, e);


        default:
            fprintf(stderr, "\n❌ Error interno: tipo de nodo desconocido (%d).\n\n", n->tipo);
            exit(1);
    }
}

/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
/* Crea una clase de error predefinida */
/* Función nativa global — leer() */
static Valor *fn_leer(Valor **a, int n) {
    if (n >= 1 && a[0]->tipo == VAL_TEXTO)
        fprintf(stdout, "%s", a[0]->texto);
    fflush(stdout);
    char buf[4096] = "";
    if (fgets(buf, sizeof(buf), stdin)) {
        /* Quitar salto de línea */
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    }
    return valor_texto(buf);
}

/* Argumentos de línea de comandos — se rellenan desde main */
static Valor *args_globales = NULL;

void interprete_set_args(int argc, char *argv[]) {
    if (args_globales) return;
    args_globales = valor_lista_crear();
    for (int i = 0; i < argc; i++)
        lista_agregar(args_globales, valor_texto(argv[i]));
}

static Valor *fn_argumentos(Valor **a, int n) {
    (void)a; (void)n;
    if (!args_globales) return valor_lista_crear();
    return valor_retener(args_globales);
}

/* Función nativa global — mapear() */
static Valor *fn_mapear(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_LISTA || a[1]->tipo != VAL_FUNCION) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "mapear() espera una lista y una función", 0);
        return valor_nulo();
    }
    Valor        *lista     = a[0];
    FuncionLince *f         = a[1]->funcion;
    Valor        *resultado = valor_lista_crear();

    for (int i = 0; i < lista->lista.cantidad && !hay_error; i++) {
        Valor *elem = valor_copiar(lista->lista.elementos[i]);

        if (f->entorno_closure == NULL) {
            /* Función nativa */
            typedef Valor *(*FnNativa)(Valor**, int);
            FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
            Valor *r = fn(&elem, 1);
            valor_destruir(elem);
            lista_agregar(resultado, r ? r : valor_nulo());
        } else {
            Entorno *fn_e = entorno_crear(f->entorno_closure);
            if (f->num_parametros >= 1)
                entorno_definir(fn_e, f->parametros[0].nombre, elem, 0);
            else
                valor_destruir(elem);   /* la lambda no declara parámetro */
            valor_destruir(ejecutar_cuerpo(f, fn_e));   /* ver la nota de NODO_LLAMADA */
            Valor *r = tomar_retorno();
            entorno_destruir(fn_e);
            lista_agregar(resultado, r);
        }
    }
    return resultado;
}

/* Función nativa global — filtrar() */
static Valor *fn_filtrar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_LISTA || a[1]->tipo != VAL_FUNCION) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "filtrar() espera una lista y una función", 0);
        return valor_nulo();
    }
    Valor        *lista     = a[0];
    FuncionLince *f         = a[1]->funcion;
    Valor        *resultado = valor_lista_crear();

    for (int i = 0; i < lista->lista.cantidad && !hay_error; i++) {
        Valor *elem = lista->lista.elementos[i];
        int    ok   = 0;

        if (f->entorno_closure == NULL) {
            typedef Valor *(*FnNativa)(Valor**, int);
            FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
            Valor *r = fn(&elem, 1);
            ok = es_verdadero(r);
            valor_destruir(r);
        } else {
            Entorno *fn_e = entorno_crear(f->entorno_closure);
            if (f->num_parametros >= 1)
                entorno_definir(fn_e, f->parametros[0].nombre,
                    valor_copiar(elem), 0);
            valor_destruir(ejecutar_cuerpo(f, fn_e));   /* ver la nota de NODO_LLAMADA */
            Valor *r = tomar_retorno();
            ok = es_verdadero(r);
            valor_destruir(r);
            entorno_destruir(fn_e);
        }

        if (ok)
            lista_agregar(resultado, valor_copiar(elem));
    }
    return resultado;
}

/* Función nativa global — reducir() */
static Valor *fn_reducir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_LISTA || a[2]->tipo != VAL_FUNCION) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "reducir() espera una lista, valor inicial y una función", 0);
        return valor_nulo();
    }
    Valor        *lista = a[0];
    Valor        *acc   = valor_copiar(a[1]);
    FuncionLince *f     = a[2]->funcion;

    for (int i = 0; i < lista->lista.cantidad && !hay_error; i++) {
        Valor *elem = lista->lista.elementos[i]; /* no copiamos */
        Valor *nuevo_acc;

        if (f->entorno_closure == NULL) {
            typedef Valor *(*FnNativa)(Valor**, int);
            FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
            Valor *args2[2] = { acc, elem };
            nuevo_acc = fn(args2, 2);
        } else {
            Entorno *fn_e = entorno_crear(f->entorno_closure);
            if (f->num_parametros >= 1)
                entorno_definir(fn_e, f->parametros[0].nombre,
                    valor_copiar(acc), 0);
            if (f->num_parametros >= 2)
                entorno_definir(fn_e, f->parametros[1].nombre,
                    valor_copiar(elem), 0);
            valor_destruir(ejecutar_cuerpo(f, fn_e));   /* ver la nota de NODO_LLAMADA */
            nuevo_acc = tomar_retorno();
            entorno_destruir(fn_e);
        }
        valor_destruir(acc);
        acc = nuevo_acc;
    }
    return acc;
}
/* rango() — genera una secuencia de números */
static Valor *fn_rango(Valor **a, int n) {
    double inicio = 0, fin = 0, paso = 1;
    int bien = n >= 1 && n <= 3;
    for (int i = 0; bien && i < n; i++) bien = a[i]->tipo == VAL_NUMERO && isfinite(a[i]->numero);
    if (!bien)
        return error_rt("ErrorArgumento", linea_llamada(),
            "rango() espera de 1 a 3 números: rango(fin), rango(desde, fin) o rango(desde, fin, paso).");
    if (n == 1) {
        fin = a[0]->numero;
    } else if (n == 2) {
        inicio = a[0]->numero;
        fin    = a[1]->numero;
    } else if (n >= 3) {
        inicio = a[0]->numero;
        fin    = a[1]->numero;
        paso   = a[2]->numero;
    }
    if (paso == 0) {
        valor_error = valor_error_nuevo("Error", "El paso de rango() no puede ser cero.", 0);
        hay_error = 1;
        return valor_nulo();
    }
    /* Los números se calculan según se recorren: rango(1000000) no ocupa nada */
    Valor *gen = valor_generador_crear();
    gen->generador.desde = inicio;
    gen->generador.hasta = fin;
    gen->generador.paso  = paso;
    return gen;
}

static Valor *fn_conv_numero(Valor **a, int n) {
    (void)n;
    Valor *v = a[0];
    switch (v->tipo) {
        case VAL_NUMERO:   return valor_numero(v->numero);
        case VAL_BOOLEANO: return valor_numero(v->booleano ? 1.0 : 0.0);
        case VAL_TEXTO: {
            char *fin;
            double d = strtod(v->texto, &fin);
            if (fin == v->texto || *fin != '\0') {
                hay_error = 1;
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "No se puede convertir \"%s\" a numero.", v->texto);
                valor_error = valor_error_nuevo("ErrorTipo", msg, 0);
                return valor_nulo();
            }
            return valor_numero(d);
        }
        case VAL_NULO: return valor_numero(0);
        default: {
            hay_error = 1;
            valor_error = valor_error_nuevo("ErrorTipo",
                "numero() no puede convertir este tipo.", 0);
            return valor_nulo();
        }
    }
}

/* Función nativa global — conversión texto() */
static Valor *fn_conv_texto(Valor **a, int n) {
    (void)n;
    Valor *v = a[0];
    char buf[256];
    switch (v->tipo) {
        case VAL_TEXTO:    return valor_texto(v->texto);
        case VAL_NUMERO:
            if (v->numero == (long long)v->numero)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->numero);
            else
                snprintf(buf, sizeof(buf), "%g", v->numero);
            return valor_texto(buf);
        case VAL_BOOLEANO: return valor_texto(v->booleano ? "verdadero" : "falso");
        case VAL_NULO:     return valor_texto("nulo");
        default: {
            hay_error = 1;
            valor_error = valor_error_nuevo("ErrorTipo",
                "texto() no puede convertir este tipo.", 0);
            return valor_nulo();
        }
    }
}

/* Función nativa global — conversión logico() */
static Valor *fn_conv_logico(Valor **a, int n) {
    (void)n;
    Valor *v = a[0];
    switch (v->tipo) {
        case VAL_BOOLEANO: return valor_booleano(v->booleano);
        case VAL_NUMERO:   return valor_booleano(v->numero != 0);
        case VAL_TEXTO:
            return valor_booleano(
                strcmp(v->texto, "verdadero") == 0 ||
                strcmp(v->texto, "true") == 0 ||
                (strlen(v->texto) > 0 && strcmp(v->texto, "falso") != 0 &&
                 strcmp(v->texto, "false") != 0 && strcmp(v->texto, "0") != 0));
        case VAL_NULO:     return valor_booleano(0);
        default:           return valor_booleano(1);
    }
}

/* Registra una función nativa en el entorno global */
static void registrar_fn_nativa(Entorno *e, const char *nombre,
                                 void *fn_ptr, int num_params) {
    FuncionLince *f = calloc(1, sizeof(FuncionLince));
    f->nombre         = strdup(nombre);
    f->num_parametros = num_params;
    f->cuerpo         = (Nodo*)(uintptr_t)fn_ptr;
    f->entorno_closure = NULL; /* señal de función nativa */
    entorno_definir(e, nombre, valor_funcion(f), 0);
}

static void registrar_error_predefinido(Entorno *e, const char *nombre) {
    ClaseLince *c = calloc(1, sizeof(ClaseLince));
    c->nombre      = strdup(nombre);
    c->num_metodos = 0;
    c->metodos     = NULL;
    c->padre       = NULL;
    entorno_definir(e, nombre, valor_clase(c), 0);
}

Interprete *interprete_crear(void) {
    Interprete *interp = malloc(sizeof(Interprete));
    interp->global = entorno_crear(NULL);

    /* Registrar tipos de error predefinidos */
    registrar_error_predefinido(interp->global, "Error");
    registrar_error_predefinido(interp->global, "ErrorTipo");
    registrar_error_predefinido(interp->global, "ErrorRango");
    registrar_error_predefinido(interp->global, "ErrorNulo");
    registrar_error_predefinido(interp->global, "ErrorMatematico");
    registrar_error_predefinido(interp->global, "ErrorArgumento");
    registrar_error_predefinido(interp->global, "ErrorAcceso");
    registrar_error_predefinido(interp->global, "ErrorJuego");

    /* Registrar funciones de conversión de tipos */
    registrar_fn_nativa(interp->global, "a_numero", fn_conv_numero, 1);
    registrar_fn_nativa(interp->global, "a_texto",  fn_conv_texto,  1);
    registrar_fn_nativa(interp->global, "a_logico", fn_conv_logico, 1);

    /* Registrar funciones de entrada/salida y utilidades */
    registrar_fn_nativa(interp->global, "leer",        fn_leer,        -1);
    registrar_fn_nativa(interp->global, "argumentos",  fn_argumentos,   0);
    registrar_fn_nativa(interp->global, "mapear",      fn_mapear,       2);
    registrar_fn_nativa(interp->global, "filtrar",     fn_filtrar,      2);
    registrar_fn_nativa(interp->global, "reducir",     fn_reducir,      3);
    registrar_fn_nativa(interp->global, "rango",       fn_rango,       -1);
    registrar_fn_nativa(interp->global, "esperar",     fn_esperar,     -1);

    /* Entrada/salida */
    registrar_fn_nativa(interp->global, "leer",       fn_leer,       -1);
    registrar_fn_nativa(interp->global, "argumentos", fn_argumentos, 0);

    /* Funciones de orden superior */
    registrar_fn_nativa(interp->global, "mapear",  fn_mapear,  2);
    registrar_fn_nativa(interp->global, "filtrar", fn_filtrar, 2);
    registrar_fn_nativa(interp->global, "reducir", fn_reducir, 3);

    return interp;
}

void interprete_destruir(Interprete *interp) {
    entorno_destruir(interp->global);
    free(interp);
}

void interprete_ejecutar(Interprete *interp, Nodo *programa) {
    if (interprete_ejecutar_recargable(interp, programa, NULL, 0) != 0) exit(1);
}

int interprete_ejecutar_recargable(Interprete *interp, Nodo *programa, char *mensaje, size_t max) {
    valor_destruir(ejecutar(programa, interp->global));   /* ver la nota de NODO_LLAMADA */
    interprete_parar_tareas();
    if (!hay_error || !valor_error) { hay_error = 0; return 0; }
    ErrorLince *err = valor_error->error;
    int r = 1;
    if (strcmp(err->tipo, ERROR_RECARGA) == 0) r = 2;
    else {
        /* Con el número de línea cuando se conoce: el pilar del lenguaje es
           "errores literales", y los fallos que antes morían con su propio
           fprintf ya lo decían; no puede perderse al pasar a ser capturables. */
        if (err->linea > 0)
            fprintf(stderr, "\n❌ %s en línea %d: %s\n\n", err->tipo, err->linea, err->mensaje);
        else
            fprintf(stderr, "\n❌ %s: %s\n\n", err->tipo, err->mensaje);
        if (mensaje && max) {
            if (err->linea > 0) snprintf(mensaje, max, "%s en línea %d: %s", err->tipo, err->linea, err->mensaje);
            else snprintf(mensaje, max, "%s: %s", err->tipo, err->mensaje);
        }
    }
    valor_destruir(valor_error);
    valor_error = NULL;
    hay_error = 0;
    hay_retorno = 0;
    hay_salto = SALTO_NINGUNO;
    return r;
}

void interprete_olvidar_importados(void) {
    for (int i = 0; i < _num_importados; i++) free(_importados[i]);
    _num_importados = 0;
}

/* Expuesta para el REPL */
Valor *ejecutar_nodo(Nodo *n, Entorno *e) {
    return ejecutar(n, e);
}

/* Versión con comillas para textos — para el REPL */
char *valor_a_texto_repl(Valor *v) {
    if (!v) return strdup("nulo");
    if (v->tipo == VAL_TEXTO) {
        size_t len = strlen(v->texto) + 3;
        char *buf = malloc(len);
        snprintf(buf, len, "\"%s\"", v->texto);
        return buf;
    }
    return valor_a_texto(v);
}

/* ─────────────────────────────────────────
   API pública para llamar funciones desde módulos
───────────────────────────────────────── */

/* Quien llama cede la propiedad de args[]: los que adopta el entorno local
   de la función los libera entorno_destruir, pero los sobrantes (o todos, si
   ni siquiera llegamos a llamar) no los libera nadie. Importa porque estas
   llamadas salen de bucles: una vez por
   fotograma en el motor. Un callback declarado sin parámetros fugaba
   el delta de cada frame. */
static void soltar_args(Valor **args, int desde, int nargs) {
    for (int i = desde; i < nargs; i++) valor_destruir(args[i]);
}

/* El árbol de cada biblioteca se hace una vez y se guarda: las funciones que
   define lo usan mientras viva el programa. También su ámbito de la última
   vez, para sus variables al recargar en caliente y para llamar a sus
   funciones desde C (interprete_llamar_biblioteca). */
static struct { const char *codigo; Nodo *ast; Entorno *ambito; } _bibliotecas[8];
static int _nbibliotecas = 0;

void interprete_biblioteca(Entorno *entorno, const char *nombre_modulo, const char *codigo) {
    int k = -1;
    for (int i = 0; i < _nbibliotecas; i++) if (_bibliotecas[i].codigo == codigo) k = i;
    Nodo *ast = k >= 0 ? _bibliotecas[k].ast : NULL;
    if (!ast) {
        Lexer  *lx  = lexer_crear(codigo);
        int     cnt = 0;
        Token  *tok = lexer_tokenizar(lx, &cnt);
        Parser *pa  = parser_crear(tok, cnt);
        ast = parser_parsear(pa);
        parser_destruir(pa);
        lexer_destruir(lx);
        if (_nbibliotecas < 8) {
            k = _nbibliotecas++;
            _bibliotecas[k].codigo = codigo;
            _bibliotecas[k].ast    = ast;
            _bibliotecas[k].ambito = NULL;
        }
    }
    if (hay_error) return;
    Valor *mod = entorno_obtener(entorno, nombre_modulo, 0);
    if (hay_error || mod->tipo != VAL_DICCIONARIO) { valor_destruir(mod); return; }

    /* Su ámbito cuelga del de más fuera (donde están escribir, a_texto...) */
    Entorno *raiz = entorno;
    while (raiz->padre) raiz = raiz->padre;
    Entorno *bib = entorno_crear(raiz);
    entorno_definir(bib, nombre_modulo, valor_retener(mod), 0);
    /* Al recargar en caliente, sus 'sea' conservan el valor de la vez
       anterior, como los del programa (la escena en la que se estaba...) */
    Entorno *cons_de = _conservar_de, *cons_en = _conservar_en;
    Entorno *antes = k >= 0 ? _bibliotecas[k].ambito : NULL;
    _conservar_de = cons_de && antes ? antes : NULL;
    _conservar_en = bib;
    _en_biblioteca++;
    valor_destruir(ejecutar(ast, bib));
    _en_biblioteca--;
    _conservar_de = cons_de;
    _conservar_en = cons_en;
    for (int i = 0; i < bib->cantidad && !hay_error; i++) {
        Valor *v = bib->vars[i].valor;
        if (v->tipo != VAL_FUNCION || !v->funcion) continue;
        v->funcion->de_biblioteca = 1;
        const char *nombre = bib->vars[i].nombre;
        if (nombre[0] == '_') continue;
        int k = valor_diccionario_buscar(mod, nombre);
        if (k >= 0) { valor_destruir(mod->diccionario.valores[k]); mod->diccionario.valores[k] = valor_retener(v); }
        else valor_diccionario_agregar(mod, nombre, valor_retener(v));
    }
    if (k >= 0) {
        entorno_destruir(antes);
        _bibliotecas[k].ambito = bib;       /* se queda con su referencia */
    } else {
        entorno_destruir(bib);              /* sigue vivo: lo retienen sus funciones */
    }
    valor_destruir(mod);
}

Valor *interprete_llamar_biblioteca(const char *codigo, const char *nombre, Valor **args, int nargs) {
    Entorno *bib = NULL;
    for (int i = 0; i < _nbibliotecas; i++) if (_bibliotecas[i].codigo == codigo) bib = _bibliotecas[i].ambito;
    const char *id = bib ? intern_buscar(nombre) : NULL;
    Valor *vf = NULL;
    for (int i = 0; id && i < bib->cantidad; i++)
        if (bib->vars[i].nombre == id) vf = bib->vars[i].valor;
    if (!vf || vf->tipo != VAL_FUNCION) {
        for (int i = 0; i < nargs; i++) valor_destruir(args[i]);
        return error_rt("Error", linea_llamada(), "La biblioteca no tiene la función '%s'.", nombre);
    }
    return llamar_biblioteca(vf, args, nargs, linea_llamada());
}

Valor *interprete_llamar_funcion(Valor *fn, Valor **args, int nargs) {
    if (!fn || fn->tipo != VAL_FUNCION) { soltar_args(args, 0, nargs); return valor_nulo(); }
    FuncionLince *f = fn->funcion;
    if (!f) { soltar_args(args, 0, nargs); return valor_nulo(); }

    if (!f->entorno_closure && !f->cuerpo) { soltar_args(args, 0, nargs); return valor_nulo(); }

    if (f->entorno_closure == NULL) {
        /* Función nativa. Las nativas no se quedan con sus argumentos: la
           ruta normal del intérprete también los libera al volver (ver la
           llamada a método de módulo), así que aquí hacemos lo mismo. */
        typedef Valor *(*FnNativa)(Valor**, int);
        FnNativa fnnat = (FnNativa)(uintptr_t)f->cuerpo;
        Valor *r = fnnat ? fnnat(args, nargs) : valor_nulo();
        soltar_args(args, 0, nargs);
        return r;
    }

    /* Declarar más parámetros de los que el módulo va a pasar no es algo que
       se pueda salvar: los de más se quedarían sin definir y el cuerpo
       fallaría con "la variable X no está definida" una vez por frame, desde
       dentro del bucle de render y sin decir de dónde viene. Al revés sí se
       admite — un callback que no necesita el delta puede declararse
       'funcion(): nulo' — que es la comodidad que justifica esta ruta. */
    if (f->num_parametros > nargs) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "'%s' declara %d parámetro(s), pero se le llama desde un módulo "
            "con %d. Declara como mucho esos %d.",
            f->nombre ? f->nombre : "la función",
            f->num_parametros, nargs, nargs);
        valor_error = valor_error_nuevo("ErrorTipo", msg, 0);
        hay_error   = 1;
        soltar_args(args, 0, nargs);
        return valor_nulo();
    }

    /* Sostener la función mientras dura la llamada. Esta es la única ruta que
       ejecuta un cuerpo Lince sin tener referencia propia a la función: la de
       NODO_LLAMADA se apoya en su 'vfun'. Aquí quien llama puede ser el único
       dueño — el módulo motor guarda el callback de al_actualizar — y ese cuerpo puede soltarlo desde
       dentro:

           motor.al_actualizar(funcion(val numero d): nulo {
               motor.al_actualizar(otra)     # suelta la que se está ejecutando
           })

       Sin este refs++, 'f' quedaría colgando para el resto de la llamada, y
       la validación del tipo de retorno de más abajo lo leería ya liberado. */
    fn->refs++;

    Entorno *fn_e = entorno_crear(f->entorno_closure);
    for (int i = 0; i < f->num_parametros; i++) {
        /* Mismo trato que en las demás rutas de llamada: si el parámetro
           declara un tipo, se comprueba. Sin esto el tipo declarado era
           mentira — un 'val texto delta' recibía el número del frame. */
        validar_tipo(args[i], f->parametros[i].tipo, "parámetro",
                     f->parametros[i].nombre, 0);
        entorno_definir(fn_e, f->parametros[i].nombre, args[i], 0);
    }
    soltar_args(args, f->num_parametros, nargs);

    Valor *r = valor_nulo();
    if (!hay_error) {
        valor_destruir(ejecutar_cuerpo(f, fn_e));   /* ver la nota de NODO_LLAMADA */
        valor_destruir(r);
        r = tomar_retorno();
        if (!hay_error)
            validar_tipo(r, f->tipo_retorno, "retorno de función",
                         f->nombre ? f->nombre : "(anónima)", 0);
    }

    entorno_destruir(fn_e);
    valor_destruir(fn);
    return r;
}
