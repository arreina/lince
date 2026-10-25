/*
 * 🐆 Lince — parser.h
 * Definición de nodos del AST
 */

#ifndef LINCE_PARSER_H
#define LINCE_PARSER_H

#include <stdio.h>
#include "lexer.h"

/* ─────────────────────────────────────────
   TIPOS DE NODO
───────────────────────────────────────── */
typedef enum {
    NODO_NUMERO,
    NODO_TEXTO,
    NODO_BOOLEANO,
    NODO_NULO,
    NODO_IDENTIFICADOR,
    NODO_BINARIO,
    NODO_UNARIO,
    NODO_DECLARACION,
    NODO_ASIGNACION,
    NODO_ASIGNACION_INDICE,  /* lista[i] = valor */
    NODO_INCREMENTO,
    NODO_ESCRIBIR,
    NODO_BLOQUE,
    NODO_SI,
    NODO_MIENTRAS,
    NODO_HACER,
    NODO_PARA,
    NODO_PARA_CADA,          /* para cada tipo var en coleccion */
    NODO_FUNCION,
    NODO_LLAMADA,
    NODO_DEVOLVER,
    NODO_LISTA,              /* [1, 2, 3] */
    NODO_DICCIONARIO,        /* {"a": 1}  */
    NODO_INDICE,             /* lista[i]  */
    NODO_METODO,             /* obj.metodo(args) */
    NODO_ACCESO,             /* obj.campo */
    NODO_CLASE,              /* definición de clase */
    NODO_INSTANCIA,          /* Persona("Ana", 22) */
    NODO_ESTO,               /* esto */
    NODO_ASIGNACION_CAMPO,   /* esto.campo = valor  o  obj.campo = valor */
    NODO_PADRE,              /* padre(args) — llama al constructor del padre */
    NODO_INTENTAR,           /* intentar / capturar / finalmente */
    NODO_LANZAR,             /* lanzar Error("msg") */
    NODO_IMPORTAR,           /* importar "matematica" */
    NODO_LAMBDA,             /* funcion(params): tipo { cuerpo } */
    NODO_ELEGIR,             /* elegir expr { caso v { } ... otro { } } */
    NODO_ENUMERACION,        /* enumeracion Color { Rojo Verde Azul } */
    NODO_INTERFAZ,           /* interfaz Forma { funcion area(): numero } */
    NODO_GENERADOR,          /* generador contar(val numero n): numero { producir i } */
    NODO_PRODUCIR,           /* producir valor */
    NODO_ROMPER,             /* romper: sale del bucle */
    NODO_CONTINUAR,          /* continuar: pasa a la vuelta siguiente */
    NODO_PLANTILLA,          /* "Vidas: {vidas}": usa 'lista' (trozos de texto y expresiones) */
    NODO_DESESTRUCTURAR,     /* sea [x, y] = pos  /  sea {x, y} = d  /  [a, b] = [b, a] */
    NODO_SI_EXPR,            /* si c { a } sino { b } como valor: usa 'si' (entonces y sino son expresiones) */
} TipoNodo;

/* ─────────────────────────────────────────
   TIPOS DE DATO (para parámetros y retorno)
───────────────────────────────────────── */
typedef enum {
    TIPO_NUMERO,
    TIPO_TEXTO,
    TIPO_LOGICO,
    TIPO_LISTA,
    TIPO_DICCIONARIO,
    TIPO_NULO,
    TIPO_CLASE,      /* nombre de clase como tipo */
    TIPO_FUNCION,    /* función / lambda */
    TIPO_CUALQUIERA, /* sin tipo declarado */
    TIPO_TAREA,      /* val tarea t */
} TipoDato;

/* ─────────────────────────────────────────
   PARÁMETRO
───────────────────────────────────────── */
typedef struct {
    char     *nombre;
    TipoDato  tipo;
    char     *tipo_clase;     /* si tipo == TIPO_CLASE */
    int       por_ref;        /* 0=val, 1=ref */
    struct Nodo *valor_defecto; /* NULL si no tiene valor por defecto */
    const char *nombre_i;       /* nombre internado (lo rellena el intérprete) */
} Parametro;
typedef struct Nodo Nodo;

struct Nodo {
    TipoNodo tipo;
    int      linea;   /* línea del código fuente */
    const char *nombre_i;   /* nombre internado, lo rellena el intérprete al usarlo */
    int      linea_ini; /* sentencias: línea donde empiezan (0 si no es una sentencia) */
    /* Cachés del intérprete para obj.campo (los rellena él): posición del campo
       en el último objeto, y el campo declarado de la última clase vista */
    int         cache_i;
    const void *cache_clase;
    void       *cache_cd;
    /* Caché de variables: dónde se encontró la última vez el nombre que usa
       este nodo (ámbitos hacia fuera y posición, +1; 0 = sin caché) */
    int         var_saltos;
    int         var_pos;
    /* NODO_NUMERO y NODO_TEXTO: su valor, creado una vez (lo crea el intérprete) */
    void       *valor_fijo;
    union {
        /* NODO_NUMERO */
        double numero;

        /* NODO_TEXTO */
        char *texto;

        /* NODO_BOOLEANO */
        int booleano;

        /* NODO_IDENTIFICADOR */
        char *identificador;

        /* NODO_BINARIO */
        struct {
            Nodo *izquierda;
            char  operador[4];
            Nodo *derecha;
        } binario;

        /* NODO_UNARIO */
        struct {
            char  operador[4];
            Nodo *operando;
        } unario;

        /* NODO_DECLARACION */
        struct {
            char *nombre;
            Nodo *valor;
            int   constante;
            int   es_lista;        /* sea lista x = [...] */
            int   es_diccionario;  /* sea diccionario x = {...} */
            int   vis;             /* campo de clase: 0 público, 1 privado, 2 protegido */
            int   estatico;        /* campo de clase estático */
            int   tipado;          /* sea numero x = ...: lleva tipo (en 'tipo' y 'tipo_clase') */
            TipoDato tipo;
            char *tipo_clase;      /* si tipo == TIPO_CLASE: "Punto" o "geo.Punto" */
        } declaracion;

        /* NODO_DESESTRUCTURAR */
        struct {
            char **nombres;
            int    num_nombres;
            int    de_diccionario;  /* sea {x, y} = d: por clave */
            int    constante;       /* fijo [x, y] = ... */
            int    declarar;        /* 0 en '[a, b] = ...' (asigna a variables que ya existen) */
            Nodo  *valor;
            const char **nombres_i; /* internados (los rellena el intérprete) */
        } desestructurar;

        /* NODO_ASIGNACION */
        struct {
            char *nombre;
            Nodo *valor;
            char  op[4];           /* "" con '=', "+" con '+=', "-" con '-='... */
        } asignacion;

        /* NODO_INCREMENTO */
        struct {
            char *nombre;
            char  operador[4];
        } incremento;

        /* NODO_ESCRIBIR */
        Nodo *escribir;

        /* NODO_BLOQUE */
        struct {
            Nodo **sentencias;
            int    cantidad;
            int    declara;   /* 1 si define algo en su ámbito (sea, funcion, clase...) */
        } bloque;

        /* NODO_SI */
        struct {
            Nodo  *condicion;
            Nodo  *entonces;
            Nodo **condiciones_sino_si;
            Nodo **bloques_sino_si;
            int    cantidad_sino_si;
            Nodo  *sino;
        } si;

        /* NODO_MIENTRAS */
        struct {
            Nodo *condicion;
            Nodo *cuerpo;
        } mientras;

        /* NODO_HACER */
        struct {
            Nodo *cuerpo;
            Nodo *condicion;
        } hacer;

        /* NODO_PARA */
        struct {
            char *var_inicio;
            Nodo *val_inicio;
            Nodo *condicion;
            char *var_incremento;
            char  op_incremento[4];
            Nodo *paso;            /* con 'i += 2': la asignación; NULL con ++ y -- */
            Nodo *cuerpo;
        } para;

        /* NODO_PARA_CADA */
        struct {
            TipoDato  tipo;
            char     *tipo_clase;
            char     *variable;
            Nodo     *coleccion;
            Nodo     *cuerpo;
        } para_cada;

        /* NODO_FUNCION */
        struct {
            char       *nombre;
            Parametro  *parametros;
            int         num_parametros;
            TipoDato    tipo_retorno;
            char       *tipo_retorno_clase;
            Nodo       *cuerpo;       /* NULL en un método abstracto */
            /* Modificadores de método (sólo dentro de una clase) */
            int         vis;          /* 0 público, 1 privado, 2 protegido */
            int         estatico;
            int         abstracto;
            int         sobrescribe;
            int         prop;         /* 1 = obtener, 2 = fijar */
            int         es_tarea;     /* 'tarea' en vez de 'funcion' */
        } funcion;

        /* NODO_LLAMADA */
        struct {
            char  *nombre;
            Nodo **argumentos;
            int    num_argumentos;
            Nodo  *funcion;     /* lista[0](1), (f)(1)...: lo que se llama, si no es un nombre */
        } llamada;

        /* NODO_DEVOLVER */
        Nodo *devolver;

        /* NODO_LISTA */
        struct {
            Nodo **elementos;
            int    cantidad;
        } lista;

        /* NODO_DICCIONARIO */
        struct {
            char **claves;
            Nodo **valores;
            int    cantidad;
        } diccionario;

        /* NODO_INDICE: expr[indice] */
        struct {
            Nodo *objeto;
            Nodo *indice;
        } acceso_indice;

        /* NODO_ASIGNACION_INDICE: objeto[indice] = valor */
        struct {
            Nodo *objeto;
            Nodo *indice;
            Nodo *valor;
            char  op[4];           /* como en 'asignacion' */
        } asignacion_indice;

        /* NODO_METODO: objeto.metodo(args) */
        struct {
            Nodo  *objeto;
            char  *metodo;
            Nodo **argumentos;
            int    num_argumentos;
            int    opcional;       /* objeto?.metodo(): nulo si el objeto es nulo */
        } metodo;

        /* NODO_ACCESO: objeto.campo */
        struct {
            Nodo *objeto;
            char *campo;
            int   opcional;        /* objeto?.campo */
        } acceso;

        /* NODO_CLASE */
        struct {
            char   *nombre;
            char   *padre;         /* nombre de la clase padre, NULL si no extiende */
            char  **interfaces;    /* interfaces que implementa */
            int     num_interfaces;
            Nodo  **metodos;
            int     num_metodos;
            int     abstracta;     /* 'abstracta clase X' */
            Nodo  **campos;        /* NODO_DECLARACION de los campos declarados */
            int     num_campos;
        } clase;

        /* NODO_INSTANCIA: Persona("Ana", 22) */
        struct {
            char  *clase;
            Nodo **argumentos;
            int    num_argumentos;
        } instancia;

        /* NODO_ASIGNACION_CAMPO: obj.campo = valor */
        struct {
            Nodo *objeto;
            char *campo;
            Nodo *valor;
            char  op[4];           /* como en 'asignacion' */
        } asignacion_campo;

        /* NODO_PADRE: padre(args) */
        struct {
            Nodo **argumentos;
            int    num_argumentos;
        } padre;

        /* NODO_INTERFAZ */
        struct {
            char   *nombre;
            char  **metodos;        /* nombres de métodos requeridos */
            char  **tipos_retorno;  /* tipo de retorno de cada método */
            int     num_metodos;
            Nodo  **funciones;      /* método con cuerpo (por defecto) o NULL */
        } interfaz;

        /* NODO_INTENTAR */
        struct {
            Nodo   *cuerpo;
            char  **tipos_captura;    /* tipo de error por cada rama */
            char  **vars_captura;     /* nombre de variable por cada rama */
            Nodo  **cuerpos_captura;  /* bloque por cada rama */
            int     num_capturas;
            Nodo   *finalmente;       /* puede ser NULL */
        } intentar;

        /* NODO_LANZAR */
        Nodo *lanzar;

        /* NODO_IMPORTAR */
        struct {
            char *nombre;  /* módulo, ./archivo o paquete:nombre */
            char *alias;   /* 'importar "x" como alias', o NULL si no lleva */
        } importar;

        /* NODO_LAMBDA */
        struct {
            Parametro *parametros;
            int        num_parametros;
            TipoDato   tipo_retorno;
            char      *tipo_retorno_clase;
            Nodo      *cuerpo;
            int        es_tarea;     /* tarea(...) { } */
        } lambda;

        /* NODO_ENUMERACION */
        struct {
            char  *nombre;
            char **valores;
            Nodo **expresiones;    /* el valor de cada uno (Arriba = [0, -1]), o NULL */
            int    num_valores;
        } enumeracion;

        /* NODO_GENERADOR */
        struct {
            char      *nombre;
            Parametro *parametros;
            int        num_parametros;
            TipoDato   tipo_retorno;
            Nodo      *cuerpo;
        } generador;

        /* NODO_PRODUCIR */
        Nodo *producir;

        /* NODO_ELEGIR */
        struct {
            Nodo   *sujeto;        /* expresión a evaluar */
            Nodo  **valores;       /* valor de cada caso */
            Nodo  **cuerpos;       /* cuerpo de cada caso */
            int     num_casos;
            Nodo   *otro;          /* bloque otro { } — puede ser NULL */
        } elegir;
    };
};

/* ─────────────────────────────────────────
   PARSER
───────────────────────────────────────── */
typedef struct {
    Token *tokens;
    int    cantidad;
    int    pos;
    int    bucles;   /* bucles abiertos en la función actual (para 'romper') */
    int    tarea;    /* la función que viene es una tarea: el tipo de retorno es opcional */
    Nodo **enums, **elegires;   /* para parser_avisos */
    int    nenums, nelegires;
    Nodo **enums_fuera;         /* las de otros archivos (son suyas) */
    int    nenums_fuera;
} Parser;

/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
Parser *parser_crear(Token *tokens, int cantidad);
void    parser_destruir(Parser *p);
Nodo   *parser_parsear(Parser *p);
/* Avisos de lo ya leído que no es un error pero casi seguro es un descuido
   (un 'elegir' al que le faltan valores de una enumeración) */
void    parser_avisos(Parser *p, void (*avisar)(int linea, const char *mensaje));
/* Apunta las enumeraciones del texto de otro archivo (uno importado), para
   esos avisos, y llama a 'importa' con cada 'importar "..."' que lleve */
void    parser_conocer_enumeraciones(Parser *p, const char *texto,
                                     void (*importa)(const char *nombre, void *ctx), void *ctx);
void    nodo_destruir(Nodo *n);
void    nodo_imprimir(Nodo *n, int nivel);

/* Un número como clave de diccionario ({1: "uno"}, d[3]) se guarda como
   texto con este byte delante, para no confundirlo con la clave "1"; al
   sacarlo (claves(), para cada, escribir) vuelve a ser un número. */
#define LINCE_CLAVE_NUMERO '\x01'
static inline void lince_clave_numerica(double d, char *buf, size_t tam) {
    if (d == 0) d = 0;                           /* -0 y 0, la misma clave */
    if (d == (double)(long long)d && d > -1e15 && d < 1e15)
        snprintf(buf, tam, "%c%lld", LINCE_CLAVE_NUMERO, (long long)d);
    else
        snprintf(buf, tam, "%c%.17g", LINCE_CLAVE_NUMERO, d);
}

#endif /* LINCE_PARSER_H */
