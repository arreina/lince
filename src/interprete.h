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
    VAL_BYTES,      /* datos binarios: cada elemento es un número de 0 a 255 */
    VAL_TAREA,      /* lo que devuelve llamar a una tarea (ver TareaLince en interprete.c) */
    VAL_ENUMERADO,  /* un valor de una enumeración: Estado.Quieto */
} TipoValor;

typedef struct Valor Valor;
typedef struct Entorno Entorno;

/* Un valor de una enumeración. Se escribe y se compara por su texto,
   "Estado.Quieto": así es igual al de antes de recargar el programa, y al
   texto que eran antes los valores de las enumeraciones. */
typedef struct {
    char  *texto;       /* "Estado.Quieto" */
    int    largo_enum;  /* cuánto mide "Estado" */
    int    indice;      /* su posición: 0, 1, 2... */
    Valor *valor;       /* el que se le dio (Arriba = [0, -1]), o su posición */
} MiembroEnum;

/* Función nativa de Lince */
typedef struct {
    char       *nombre;
    Parametro  *parametros;
    int         num_parametros;
    TipoDato    tipo_retorno;
    Nodo       *cuerpo;
    Entorno    *entorno_closure;
    /* Sólo para métodos de clase */
    int                 vis;        /* VIS_PUBLICO / VIS_PRIVADO / VIS_PROTEGIDO */
    int                 estatico;
    int                 abstracto;
    int                 prop;       /* 1 = obtener, 2 = fijar */
    struct ClaseLince  *duena;      /* clase que lo declara */
    int                 de_biblioteca;  /* escrita en Lince dentro de un módulo estándar */
    int                 es_tarea;       /* 'tarea': llamarla la empieza y da la tarea */
    TipoDato            tipo_tarea;     /* el tipo de lo que devuelve la tarea al acabar */
} FuncionLince;

enum { VIS_PUBLICO = 0, VIS_PRIVADO = 1, VIS_PROTEGIDO = 2 };

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
    Nodo  **funciones;      /* método por defecto (con cuerpo) o NULL */
} InterfazLince;

/* Campo declarado en el cuerpo de una clase. Los no declarados (los que se
   crean con 'mi.x = ...') siguen siendo públicos y dinámicos. */
typedef struct {
    char              *nombre;
    int                vis;
    int                estatico;
    int                constante;
    Nodo              *inicial;          /* expresión inicial, o NULL */
    Valor             *valor;            /* valor del campo estático */
    struct ClaseLince *duena;
} CampoClase;

typedef struct ClaseLince {
    char              *nombre;
    FuncionLince     **metodos;
    int                num_metodos;
    struct ClaseLince *padre;          /* NULL si no extiende */
    InterfazLince    **interfaces;     /* interfaces que implementa */
    int                num_interfaces;
    int                abstracta;
    CampoClase        *campos;         /* campos declarados con 'sea' / 'fijo' */
    int                num_campos;
    Entorno           *entorno;        /* ámbito léxico de la clase (dueño de sus métodos) */
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
    int       ascii;      /* texto: 1 si solo tiene ASCII, 0 si no, -1 sin calcular */
    int       nchars;     /* texto: número de caracteres, o -1 sin calcular */
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
            int           *indice;      /* tabla hash de posiciones (ver valor_diccionario_buscar) */
            int            indice_cap;
            int            indice_n;    /* cuántas entradas están indexadas; -1 = invalidada */
        } diccionario;
        ClaseLince  *clase;
        ObjetoLince *objeto;
        ErrorLince  *error;
        /* VAL_BYTES */
        struct {
            unsigned char *datos;
            int            cantidad;
            int            capacidad;
        } bytes;
        struct TareaLince *tarea;       /* VAL_TAREA */
        MiembroEnum       *miembro;     /* VAL_ENUMERADO */
        /* VAL_GENERADOR — da sus valores de uno en uno, según se recorre: los
           de un 'generador' los va produciendo su cuerpo, que corre aparte
           (como una tarea), y los de rango() se calculan */
        struct {
            struct TareaLince *productor;   /* NULL en rango() */
            double             desde, hasta, paso;
        } generador;
    };
};

/* ─────────────────────────────────────────
   ENTORNO (ámbito de variables)
───────────────────────────────────────── */
#define MAX_VARS 256

typedef struct EntradaVar {
    const char *nombre;   /* internado: se compara por puntero */
    Valor *valor;
    int    constante;
} EntradaVar;

struct Entorno {
    EntradaVar vars[MAX_VARS];
    int        cantidad;
    Entorno   *padre;
    int        refs;    /* conteo de referencias — para closures */
    struct ClaseLince *dueno;   /* clase en cuyo cuerpo se escribió este código */
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
/* Bytes con 'n' ceros (los datos se pueden rellenar después) */
Valor *valor_bytes_crear(int n);
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
/* Posición de la clave en el diccionario, o -1 */
int    valor_diccionario_buscar(Valor *dic, const char *clave);
void   lista_agregar(Valor *lista, Valor *elem);
Valor *valor_crear_error(const char *tipo, const char *mensaje, int linea);
void   entorno_definir_i(Entorno *e, const char *nombre_internado, Valor *valor, int constante);
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
/* Como interprete_ejecutar, pero sin terminar el proceso: 0 si acaba bien,
   1 si hubo un error (ya escrito en stderr y copiado en 'mensaje'), 2 si se
   cortó para recargar el programa (error de tipo ERROR_RECARGA). */
int         interprete_ejecutar_recargable(Interprete *interp, Nodo *programa,
                                           char *mensaje, size_t max);
/* Recarga en caliente: mientras corre 'nuevo', sus 'sea' del nivel superior
   toman el valor que tenían en el global 'anterior' (NULL lo desactiva). */
void        interprete_conservar(Entorno *anterior, Interprete *nuevo);
/* Olvida qué archivos se importaron, para que el programa recargado los
   vuelva a importar. */
void        interprete_olvidar_importados(void);
#define ERROR_RECARGA "RecargaLince"
void        interprete_set_args(int argc, char *argv[]);
Valor      *ejecutar_nodo(Nodo *n, Entorno *e);
char       *valor_a_texto_repl(Valor *v);
void        valor_destruir(Valor *v);

#endif /* LINCE_INTERPRETE_H */

/* Llamar a una función Lince desde C externo */
/* Llama a una función Lince desde un módulo nativo. Se queda con la
   propiedad de args[]. */
Valor *interprete_llamar_funcion(Valor *fn, Valor **args, int nargs);

/* La parte de un módulo estándar escrita en Lince: ejecuta 'codigo' en un
   ámbito propio (donde 'nombre_modulo' es el propio módulo, ya definido en
   'entorno') y añade al módulo cada función cuyo nombre no empiece por '_'.
   'codigo' tiene que vivir siempre (el árbol se guarda la primera vez). */
void interprete_biblioteca(Entorno *entorno, const char *nombre_modulo, const char *codigo);
/* Llama a la función 'nombre' de esa biblioteca (también a las que empiezan
   por '_', que no están en el módulo) como si se llamara desde la línea que
   corre ahora. Se queda con la propiedad de args[]. */
Valor *interprete_llamar_biblioteca(const char *codigo, const char *nombre, Valor **args, int nargs);

/* Tareas (tarea ... { esperar(1) }). Quien tiene un bucle de fotogramas
   (juego.presentar) da su reloj y las avanza una vez por fotograma; sin
   reloj, esperar() fuera de una tarea es quien las avanza, con el reloj real.
   Al acabar el programa se paran las que queden. */
void interprete_reloj_tareas(double (*reloj)(void));
void interprete_avanzar_tareas(void);
void interprete_parar_tareas(void);

/* Ruta del fichero que se va a ejecutar. Sirve para resolver los
   'importar "./x"' junto al fichero que los escribe, no junto al directorio
   de trabajo de quien lanza el programa. */
void interprete_fijar_archivo(const char *ruta);
const char *interprete_archivo_actual(void);
/* El programa que se ejecuta (no el archivo importado que corre ahora): las
   partidas guardadas van a una carpeta con su nombre */
void interprete_fijar_principal(const char *ruta);
const char *interprete_archivo_principal(void);

/* Depuración: el hook se llama antes de cada sentencia de un bloque. */
void interprete_fijar_hook(void (*hook)(Nodo *sentencia, Entorno *e));
int  interprete_profundidad(void);
/* Llamada activa 'nivel' (1 = la más externa): nombre y línea desde la que se hizo */
int  interprete_pila(int nivel, const char **nombre, int *linea);

/* Lector de archivos 'importar' alternativo al disco (programas empaquetados):
   devuelve el contenido de la ruta o NULL si no es suyo. */
void interprete_fijar_lector(const char *(*lector)(const char *ruta, size_t *largo));

/* Como interprete_leer_fuente, pero también devuelve los bytes leídos (binarios) */
char *interprete_leer_bytes(const char *ruta, size_t *largo);

/* Contenido (malloc) del archivo que se importaría con esa ruta, o NULL */
char *interprete_leer_fuente(const char *ruta);

/* Resuelve 'importar "./x"' desde un fichero, como el intérprete. 1 si existe */
int interprete_resolver_import(const char *archivo_actual, const char *nombre,
                               char *ruta, size_t max);
