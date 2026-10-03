/*
 * 🐆 Lince — interprete.h
 */

#ifndef LINCE_INTERPRETE_H
#define LINCE_INTERPRETE_H

#include "parser.h"

/* ─────────────────────────────────────────
   TIPOS DE VALOR
───────────────────────────────────────── */
typedef enum {
    VAL_NUMERO,
    VAL_TEXTO,
    VAL_BOOLEANO,
    VAL_NULO,
    VAL_FUNCION,
    VAL_LISTA,
    VAL_DICCIONARIO,
    VAL_CLASE,
    VAL_OBJETO,
    VAL_ERROR,
    VAL_GENERADOR,
} TipoValor;

typedef struct Valor Valor;
typedef struct Entorno Entorno;

/* Función nativa de Lince */
typedef struct {
    char       *nombre;
    Parametro  *parametros;
    int         num_parametros;
    TipoDato    tipo_retorno;
    Nodo       *cuerpo;
    Entorno    *entorno_closure;
} FuncionLince;

/* Error de Lince */
typedef struct {
    char *tipo;     /* "Error", "ErrorTipo", etc. */
    char *mensaje;
    int   linea;
} ErrorLince;

/* Interfaz de Lince */
typedef struct {
    char   *nombre;
    char  **metodos;
    char  **tipos_retorno;
    int     num_metodos;
} InterfazLince;

typedef struct ClaseLince {
    char              *nombre;
    FuncionLince     **metodos;
    int                num_metodos;
    struct ClaseLince *padre;          /* NULL si no extiende */
    InterfazLince    **interfaces;     /* interfaces que implementa */
    int                num_interfaces;
} ClaseLince;

/* Objeto (instancia de una clase) */
#define MAX_CAMPOS 64
typedef struct {
    ClaseLince *clase;
    char       *campos_nombres[MAX_CAMPOS];
    Valor      *campos_valores[MAX_CAMPOS];
    int         num_campos;
} ObjetoLince;

struct Valor {
    TipoValor tipo;
    int       refs;
    int       es_modulo;  /* 1 si es un módulo importado */
    union {
        double        numero;
        char         *texto;
        int           booleano;
        FuncionLince *funcion;
        struct {
            struct Valor **elementos;
            int            cantidad;
            int            capacidad;
        } lista;
        struct {
            char         **claves;
            struct Valor **valores;
            int            cantidad;
            int            capacidad;
        } diccionario;
        ClaseLince  *clase;
        ObjetoLince *objeto;
        ErrorLince  *error;
        /* VAL_GENERADOR — lista de valores producidos */
        struct {
            struct Valor **elementos;
            int            cantidad;
            int            capacidad;
        } generador;
    };
};

/* ─────────────────────────────────────────
   ENTORNO (ámbito de variables)
───────────────────────────────────────── */
#define MAX_VARS 256

typedef struct EntradaVar {
    char  *nombre;
    Valor *valor;
    int    constante;
} EntradaVar;

struct Entorno {
    EntradaVar vars[MAX_VARS];
    int        cantidad;
    Entorno   *padre;
    int        refs;    /* conteo de referencias — para closures */
};

/* ─────────────────────────────────────────
   API PÚBLICA DE VALORES
   (usada desde modulos.c)
───────────────────────────────────────── */
Valor *valor_numero(double n);
Valor *valor_texto(const char *s);
Valor *valor_booleano(int b);
Valor *valor_nulo(void);
Valor *valor_lista_crear(void);
/* ── UTF-8 ──
   El texto se guarda en UTF-8 y estas ayudas permiten trabajar con
   caracteres en vez de bytes: "niño" mide 4, no 5. */
int   utf8_tam(const char *s);             /* bytes del carácter en 's' */
int   utf8_longitud(const char *s);        /* número de caracteres */
int   utf8_desplazamiento(const char *s, int i); /* byte del carácter i, o -1 */
char *utf8_mayusculas(const char *s);      /* cadena nueva, con malloc */
char *utf8_minusculas(const char *s);      /* cadena nueva, con malloc */
int   utf8_es_letra(const char *s);

Valor *valor_diccionario_crear(void);
/* Reserva hueco para 'n' entradas en un diccionario (crece si hace falta). */
void   valor_diccionario_asegurar(Valor *dic, int n);
/* Añade clave/valor al final del diccionario, creciendo si hace falta. */
void   valor_diccionario_agregar(Valor *dic, const char *clave, Valor *valor);
void   lista_agregar(Valor *lista, Valor *elem);
Valor *valor_crear_error(const char *tipo, const char *mensaje, int linea);
void   entorno_definir(Entorno *e, const char *nombre, Valor *valor, int constante);

extern int    hay_error;
extern Valor *valor_error;

/* Pone un error nuevo soltando el pendiente. Usa esto, no la asignación
   directa a valor_error, que fuga el anterior. */
Valor *valor_error_nuevo(const char *tipo, const char *mensaje, int linea);

/* ─────────────────────────────────────────
   INTÉRPRETE
───────────────────────────────────────── */
typedef struct {
    Entorno *global;
} Interprete;

/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
Interprete *interprete_crear(void);
void        interprete_destruir(Interprete *interp);
void        interprete_ejecutar(Interprete *interp, Nodo *programa);
void        interprete_set_args(int argc, char *argv[]);
Valor      *ejecutar_nodo(Nodo *n, Entorno *e);
char       *valor_a_texto_repl(Valor *v);
void        valor_destruir(Valor *v);

#endif /* LINCE_INTERPRETE_H */

/* Llamar a una función Lince desde C externo */
/* Llama a una función Lince desde un módulo nativo. Se queda con la
   propiedad de args[]. */
Valor *interprete_llamar_funcion(Valor *fn, Valor **args, int nargs);

/* Ruta del fichero que se va a ejecutar. Sirve para resolver los
   'importar "./x"' junto al fichero que los escribe, no junto al directorio
   de trabajo de quien lanza el programa. */
void interprete_fijar_archivo(const char *ruta);
