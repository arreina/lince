/*
 * 🐆 Lince — parser.h
 * Definición de nodos del AST
 */

#ifndef LINCE_PARSER_H
#define LINCE_PARSER_H

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
} Parametro;
typedef struct Nodo Nodo;

struct Nodo {
    TipoNodo tipo;
    int      linea;   /* línea del código fuente */
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
        } declaracion;

        /* NODO_ASIGNACION */
        struct {
            char *nombre;
            Nodo *valor;
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
            Nodo       *cuerpo;
        } funcion;

        /* NODO_LLAMADA */
        struct {
            char  *nombre;
            Nodo **argumentos;
            int    num_argumentos;
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
        } asignacion_indice;

        /* NODO_METODO: objeto.metodo(args) */
        struct {
            Nodo  *objeto;
            char  *metodo;
            Nodo **argumentos;
            int    num_argumentos;
        } metodo;

        /* NODO_ACCESO: objeto.campo */
        struct {
            Nodo *objeto;
            char *campo;
        } acceso;

        /* NODO_CLASE */
        struct {
            char   *nombre;
            char   *padre;         /* nombre de la clase padre, NULL si no extiende */
            char  **interfaces;    /* interfaces que implementa */
            int     num_interfaces;
            Nodo  **metodos;
            int     num_metodos;
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
        } lambda;

        /* NODO_ENUMERACION */
        struct {
            char  *nombre;
            char **valores;
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
} Parser;

/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
Parser *parser_crear(Token *tokens, int cantidad);
void    parser_destruir(Parser *p);
Nodo   *parser_parsear(Parser *p);
void    nodo_destruir(Nodo *n);
void    nodo_imprimir(Nodo *n, int nivel);

#endif /* LINCE_PARSER_H */
