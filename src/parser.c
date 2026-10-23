/*
 * 🐆 Lince — parser.c
 * Construye el Árbol Sintáctico (AST) a partir de los tokens.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

/* ─────────────────────────────────────────
   UTILIDADES INTERNAS
───────────────────────────────────────── */
static Token *actual(Parser *p) {
    return &p->tokens[p->pos];
}

static Token *ver_siguiente(Parser *p) {
    if (p->pos + 1 < p->cantidad)
        return &p->tokens[p->pos + 1];
    return &p->tokens[p->cantidad - 1]; /* EOF */
}

static int coincide(Parser *p, TipoToken tipo) {
    return actual(p)->tipo == tipo;
}

static Token *consumir(Parser *p, TipoToken tipo, const char *esperaba) {
    Token *t = actual(p);
    if (t->tipo != tipo) {
        fprintf(stderr,
            "\n❌ Error en línea %d%s:\n"
            "   Esperaba %s pero encontré '%s'.\n"
            "   Revisa la sintaxis en esa línea.\n\n",
            t->linea, lince_de_archivo(), esperaba,
            t->valor ? t->valor : "fin de archivo");
        lince_marcar_columna(t->columna, t->ancho);
        exit(1);
    }
    p->pos++;
    return t;
}

static Token *consumir_cualquiera(Parser *p) {
    Token *t = actual(p);
    p->pos++;
    return t;
}

static void error_sintaxis(Parser *p, const char *mensaje) {
    Token *t = actual(p);
    fprintf(stderr,
        "\n❌ Error en línea %d%s:\n"
        "   %s\n"
        "   Encontré: '%s'\n\n",
        t->linea, lince_de_archivo(), mensaje,
        t->valor ? t->valor : "fin de archivo");
    lince_marcar_columna(t->columna, t->ancho);
    exit(1);
}

/* ─────────────────────────────────────────
   CREACIÓN DE NODOS
───────────────────────────────────────── */
static Nodo *nodo_nuevo(TipoNodo tipo, Parser *p) {
    Nodo *n = calloc(1, sizeof(Nodo));
    n->tipo  = tipo;
    n->linea = !p ? 0 : (p->pos > 0 ? p->tokens[p->pos - 1].linea : actual(p)->linea);
    return n;
}

/* ─────────────────────────────────────────
   DECLARACIONES ANTICIPADAS
───────────────────────────────────────── */
static Nodo *sentencia(Parser *p);
static Nodo *expresion(Parser *p);
static Nodo *bloque(Parser *p);
static Nodo *postfijo(Parser *p, Nodo *base);
static Nodo *si_expresion(Parser *p);
static TipoDato parsear_tipo(Parser *p, char **tipo_clase_out);
static Nodo *sent_elegir(Parser *p);
static Nodo *sent_enumeracion(Parser *p);
static Nodo *sent_interfaz(Parser *p);
static Nodo *sent_generador(Parser *p);

/* ─────────────────────────────────────────
   POSTFIJO: obj[i], obj.metodo(), obj.campo
───────────────────────────────────────── */
/* El cuerpo de una función empieza sin bucles abiertos: un 'romper' dentro
   de una función no puede salir de un bucle de quien la llama. */
static Nodo *cuerpo_funcion(Parser *p) {
    int antes = p->bucles;
    p->bucles = 0;
    Nodo *c = bloque(p);
    p->bucles = antes;
    return c;
}

static Nodo *cuerpo_bucle(Parser *p) {
    p->bucles++;
    Nodo *c = bloque(p);
    p->bucles--;
    return c;
}

/* += -= *= /= %=: el operador que hay que aplicar ("+", "-"...), o NULL */
static const char *op_compuesto(TipoToken t) {
    switch (t) {
        case TOK_MAS_IGUAL:    return "+";
        case TOK_MENOS_IGUAL:  return "-";
        case TOK_POR_IGUAL:    return "*";
        case TOK_ENTRE_IGUAL:  return "/";
        case TOK_MODULO_IGUAL: return "%";
        default:               return NULL;
    }
}

/* 'tarea(' de una tarea sin nombre: tarea() { ... } o tarea(val ...) ...
   ('tarea' no es palabra reservada: tarea(x) puede ser llamar a una función) */
static int tarea_sin_nombre(Parser *p) {
    Token *t = &p->tokens[p->pos];
    if (t->tipo != TOK_IDENTIFICADOR || !t->valor || strcmp(t->valor, "tarea") != 0) return 0;
    if (p->pos + 2 >= p->cantidad || p->tokens[p->pos + 1].tipo != TOK_PAREN_IZQ) return 0;
    TipoToken dentro = p->tokens[p->pos + 2].tipo;
    if (dentro == TOK_VAL || dentro == TOK_REF) return 1;
    /* sin 'val': tarea(numero x) ... — un tipo seguido de un nombre */
    if (p->pos + 3 < p->cantidad && p->tokens[p->pos + 3].tipo == TOK_IDENTIFICADOR &&
        (dentro == TOK_TIPO_NUMERO || dentro == TOK_TIPO_TEXTO || dentro == TOK_TIPO_LOGICO ||
         dentro == TOK_TIPO_LISTA || dentro == TOK_TIPO_DICCIONARIO || dentro == TOK_TIPO_NULO ||
         dentro == TOK_LISTA || dentro == TOK_DICCIONARIO || dentro == TOK_IDENTIFICADOR)) return 1;
    if (dentro != TOK_PAREN_DER || p->pos + 3 >= p->cantidad) return 0;
    TipoToken tras = p->tokens[p->pos + 3].tipo;
    return tras == TOK_LLAVE_IZQ || tras == TOK_DOS_PUNTOS;
}

/* Guarda el nodo en una de las listas del parser (para los avisos del final) */
static void apuntar(Nodo ***lista, int *n, Nodo *nodo) {
    if ((*n & (*n - 1)) == 0) *lista = realloc(*lista, sizeof(Nodo*) * (size_t)(*n ? *n * 2 : 4));
    (*lista)[(*n)++] = nodo;
}

/* Cada rama de un 'si' que da valor: '{', una expresión y '}' */
static Nodo *rama_si(Parser *p) {
    consumir(p, TOK_LLAVE_IZQ, "'{'");
    if (coincide(p, TOK_LLAVE_DER))
        error_sintaxis(p, "Cada rama de un 'si' que da un valor lleva dentro ese valor: si c { 1 } sino { 2 }.");
    TipoToken t = actual(p)->tipo;
    if (t == TOK_ESCRIBIR || t == TOK_SEA || t == TOK_FIJO || t == TOK_DEVOLVER ||
        t == TOK_MIENTRAS || t == TOK_PARA || t == TOK_HACER || t == TOK_ROMPER || t == TOK_CONTINUAR ||
        t == TOK_LANZAR || t == TOK_INTENTAR || t == TOK_ELEGIR)
        error_sintaxis(p, "Cada rama de un 'si' que da un valor lleva solo una expresión, sin más sentencias.");
    Nodo *v = expresion(p);
    if (!coincide(p, TOK_LLAVE_DER))
        error_sintaxis(p, "Cada rama de un 'si' que da un valor lleva solo una expresión, sin más sentencias.");
    consumir(p, TOK_LLAVE_DER, "'}'");
    return v;
}

static Nodo *si_expresion(Parser *p) {
    consumir(p, TOK_SI, "'si'");
    Nodo *n = nodo_nuevo(NODO_SI_EXPR, p);
    n->si.condicion = expresion(p);
    n->si.entonces  = rama_si(p);
    if (!coincide(p, TOK_SINO))
        error_sintaxis(p, "Un 'si' que da un valor necesita su 'sino': sea x = si c { 1 } sino { 2 }.");
    consumir(p, TOK_SINO, "'sino'");
    n->si.sino = coincide(p, TOK_SI) ? si_expresion(p) : rama_si(p);
    return n;
}

static Nodo *postfijo(Parser *p, Nodo *base) {
    while (1) {
        /* Si el base es un NODO_ACCESO y viene '(' — convertir a NODO_METODO */
        if (base->tipo == NODO_ACCESO && coincide(p, TOK_PAREN_IZQ)) {
            consumir(p, TOK_PAREN_IZQ, "'('");
            Nodo **args = malloc(sizeof(Nodo*) * 64);
            int    nargs = 0;
            if (!coincide(p, TOK_PAREN_DER)) {
                args[nargs++] = expresion(p);
                while (coincide(p, TOK_COMA)) {
                    consumir(p, TOK_COMA, "','");
                    args[nargs++] = expresion(p);
                }
            }
            consumir(p, TOK_PAREN_DER, "')'");
            Nodo *n = nodo_nuevo(NODO_METODO, p);
            n->metodo.objeto         = base->acceso.objeto;
            n->metodo.metodo         = strdup(base->acceso.campo);
            n->metodo.argumentos     = args;
            n->metodo.num_argumentos = nargs;
            free(base->acceso.campo);
            free(base);
            base = n;
            continue;
        }

        /* Llamar a lo que ha salido: lista[0](1), (f)(1), f(1)(2). Solo en la
           misma línea: un '(' al principio de la siguiente es otra cosa */
        if (coincide(p, TOK_PAREN_IZQ) && p->pos > 0 &&
            p->tokens[p->pos - 1].linea == actual(p)->linea) {
            consumir(p, TOK_PAREN_IZQ, "'('");
            int cap = 8;
            Nodo **args = malloc(sizeof(Nodo*) * cap);
            int    nargs = 0;
            if (!coincide(p, TOK_PAREN_DER)) {
                do {
                    if (nargs > 0) consumir(p, TOK_COMA, "','");
                    if (nargs == cap) args = realloc(args, sizeof(Nodo*) * (cap *= 2));
                    args[nargs++] = expresion(p);
                } while (coincide(p, TOK_COMA));
            }
            consumir(p, TOK_PAREN_DER, "')'");
            Nodo *n = nodo_nuevo(NODO_LLAMADA, p);
            n->llamada.nombre         = strdup("(sin nombre)");
            n->llamada.argumentos     = args;
            n->llamada.num_argumentos = nargs;
            n->llamada.funcion        = base;
            base = n;
            continue;
        }

        /* Acceso por índice: base[expr] */
        if (coincide(p, TOK_CORCHETE_IZQ)) {
            consumir(p, TOK_CORCHETE_IZQ, "'['");
            Nodo *indice = expresion(p);
            consumir(p, TOK_CORCHETE_DER, "']'");
            Nodo *n = nodo_nuevo(NODO_INDICE, p);
            n->acceso_indice.objeto = base;
            n->acceso_indice.indice = indice;
            base = n;
            continue;
        }
        /* Acceso por punto: base.campo o base.metodo() */
        if (coincide(p, TOK_PUNTO)) {
            consumir(p, TOK_PUNTO, "'.'");
            /* Aceptar cualquier identificador O keyword como nombre de campo/método */
            Token *nombre;
            if (coincide(p, TOK_IDENTIFICADOR)   ||
                coincide(p, TOK_ESCRIBIR)         ||
                coincide(p, TOK_TIPO_TEXTO)       ||
                coincide(p, TOK_TIPO_NUMERO)      ||
                coincide(p, TOK_TIPO_LOGICO)      ||
                coincide(p, TOK_LISTA)            ||
                coincide(p, TOK_DICCIONARIO)) {
                nombre = consumir_cualquiera(p);
            } else {
                nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de campo o método");
            }
            /* ¿Es llamada a método? */
            if (coincide(p, TOK_PAREN_IZQ)) {
                consumir(p, TOK_PAREN_IZQ, "'('");
                Nodo **args = malloc(sizeof(Nodo*) * 64);
                int    nargs = 0;
                if (!coincide(p, TOK_PAREN_DER)) {
                    args[nargs++] = expresion(p);
                    while (coincide(p, TOK_COMA)) {
                        consumir(p, TOK_COMA, "','");
                        args[nargs++] = expresion(p);
                    }
                }
                consumir(p, TOK_PAREN_DER, "')'");
                Nodo *n = nodo_nuevo(NODO_METODO, p);
                n->metodo.objeto        = base;
                n->metodo.metodo        = strdup(nombre->valor);
                n->metodo.argumentos    = args;
                n->metodo.num_argumentos = nargs;
                base = n;
            } else {
                Nodo *n = nodo_nuevo(NODO_ACCESO, p);
                n->acceso.objeto = base;
                n->acceso.campo  = strdup(nombre->valor);
                base = n;
            }
            continue;
        }
        break;
    }
    return base;
}

/* ─────────────────────────────────────────
   EXPRESIONES (por precedencia)
───────────────────────────────────────── */
static Nodo *primario(Parser *p) {
    Token *t = actual(p);

    /* Lambda: funcion(params): tipo { cuerpo }, o una tarea sin nombre:
       tarea(params) { cuerpo }, con el tipo opcional */
    int es_tarea = tarea_sin_nombre(p);
    if (coincide(p, TOK_FUNCION) || es_tarea) {
        consumir_cualquiera(p);
        consumir(p, TOK_PAREN_IZQ, "'('");

        Parametro *params     = calloc(64, sizeof(Parametro));
        int        num_params = 0;

        if (!coincide(p, TOK_PAREN_DER)) {
            do {
                if (num_params > 0) consumir(p, TOK_COMA, "','");
                int por_ref = 0;
                if (coincide(p, TOK_VAL))      { consumir_cualquiera(p); por_ref = 0; }
                else if (coincide(p, TOK_REF)) { consumir_cualquiera(p); por_ref = 1; }
                /* sin 'val' ni 'ref': val */
                char *tipo_clase = NULL;
                TipoDato tipo = parsear_tipo(p, &tipo_clase);
                Token *pnom = consumir(p, TOK_IDENTIFICADOR, "nombre del parámetro");
                Nodo *vdef = NULL;
                if (coincide(p, TOK_IGUAL)) {
                    consumir(p, TOK_IGUAL, "'='");
                    vdef = expresion(p);
                }
                params[num_params].nombre        = strdup(pnom->valor);
                params[num_params].tipo          = tipo;
                params[num_params].tipo_clase    = tipo_clase;
                params[num_params].por_ref       = por_ref;
                params[num_params].valor_defecto = vdef;
                num_params++;
            } while (coincide(p, TOK_COMA));
        }
        consumir(p, TOK_PAREN_DER, "')'");
        char *tipo_ret_clase = NULL;
        TipoDato tipo_retorno = TIPO_CUALQUIERA;
        if (!es_tarea || coincide(p, TOK_DOS_PUNTOS)) {
            consumir(p, TOK_DOS_PUNTOS, "':'");
            tipo_retorno = parsear_tipo(p, &tipo_ret_clase);
        }
        Nodo *cuerpo = cuerpo_funcion(p);

        Nodo *n = nodo_nuevo(NODO_LAMBDA, p);
        n->lambda.es_tarea           = es_tarea;
        n->lambda.parametros         = params;
        n->lambda.num_parametros     = num_params;
        n->lambda.tipo_retorno       = tipo_retorno;
        n->lambda.tipo_retorno_clase = tipo_ret_clase;
        n->lambda.cuerpo             = cuerpo;
        return n;
    }

    /* esto (mi) */
    if (coincide(p, TOK_MI)) {
        consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_ESTO, p);
        return postfijo(p, n);
    }

    /* Tipos usados como identificadores (ej: texto.dividir) O como funciones numero(), texto(), logico() */
    if (coincide(p, TOK_TIPO_TEXTO) || coincide(p, TOK_TIPO_NUMERO) ||
        coincide(p, TOK_TIPO_LOGICO) || coincide(p, TOK_LISTA) ||
        coincide(p, TOK_DICCIONARIO)) {
        Token *t2 = consumir_cualquiera(p);
        /* ¿Es llamada a función? numero(...) texto(...) logico(...) */
        if (coincide(p, TOK_PAREN_IZQ)) {
            consumir(p, TOK_PAREN_IZQ, "'('");
            Nodo **args = malloc(sizeof(Nodo*) * 64);
            int nargs = 0;
            if (!coincide(p, TOK_PAREN_DER)) {
                args[nargs++] = expresion(p);
                while (coincide(p, TOK_COMA)) {
                    consumir(p, TOK_COMA, "','");
                    args[nargs++] = expresion(p);
                }
            }
            consumir(p, TOK_PAREN_DER, "')'");
            Nodo *n = nodo_nuevo(NODO_LLAMADA, p);
            n->llamada.nombre         = strdup(t2->valor);
            n->llamada.argumentos     = args;
            n->llamada.num_argumentos = nargs;
            return postfijo(p, n);
        }
        Nodo *n = nodo_nuevo(NODO_IDENTIFICADOR, p);
        n->identificador = strdup(t2->valor);
        return postfijo(p, n);
    }

    /* Número */
    if (coincide(p, TOK_NUMERO)) {
        consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_NUMERO, p);
        n->numero = atof(t->valor);
        return n;
    }

    /* si c { a } sino { b }: el valor de una de las dos ramas */
    if (coincide(p, TOK_SI)) return si_expresion(p);

    /* Texto con valores dentro: "Vidas: {vidas}". El lexer deja los trozos
       alternos: TEXTO, expresión, TEXTO, ..., TEXTO. */
    if (coincide(p, TOK_PLANTILLA_INICIO)) {
        consumir_cualquiera(p);
        int cap = 8, cant = 0;
        Nodo **partes = malloc(sizeof(Nodo*) * cap);
        for (;;) {
            Token *tt = consumir(p, TOK_TEXTO, "'}' para cerrar el valor metido en el texto");
            if (cant + 2 > cap) { cap *= 2; partes = realloc(partes, sizeof(Nodo*) * cap); }
            if (tt->valor[0]) {
                Nodo *tx = nodo_nuevo(NODO_TEXTO, p);
                tx->texto = strdup(tt->valor);
                partes[cant++] = tx;
            }
            if (coincide(p, TOK_PLANTILLA_FIN)) { consumir_cualquiera(p); break; }
            partes[cant++] = expresion(p);
        }
        Nodo *n = nodo_nuevo(NODO_PLANTILLA, p);
        n->lista.elementos = partes;
        n->lista.cantidad  = cant;
        return postfijo(p, n);
    }

    /* Texto */
    if (coincide(p, TOK_TEXTO)) {
        consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_TEXTO, p);
        n->texto = strdup(t->valor);
        return n;
    }

    /* verdadero */
    if (coincide(p, TOK_VERDADERO)) {
        consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BOOLEANO, p);
        n->booleano = 1;
        return n;
    }

    /* falso */
    if (coincide(p, TOK_FALSO)) {
        consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BOOLEANO, p);
        n->booleano = 0;
        return n;
    }

    /* nulo */
    if (coincide(p, TOK_NULO)) {
        consumir_cualquiera(p);
        return nodo_nuevo(NODO_NULO, p);
    }

    /* Lista: [1, 2, 3] */
    if (coincide(p, TOK_CORCHETE_IZQ)) {
        consumir(p, TOK_CORCHETE_IZQ, "'['");
        Nodo **elems = malloc(sizeof(Nodo*) * 256);
        int    cant  = 0;
        if (!coincide(p, TOK_CORCHETE_DER)) {
            elems[cant++] = expresion(p);
            while (coincide(p, TOK_COMA)) {
                consumir(p, TOK_COMA, "','");
                if (coincide(p, TOK_CORCHETE_DER)) break;
                elems[cant++] = expresion(p);
            }
        }
        consumir(p, TOK_CORCHETE_DER, "']'");
        Nodo *n = nodo_nuevo(NODO_LISTA, p);
        n->lista.elementos = elems;
        n->lista.cantidad  = cant;
        return postfijo(p, n);
    }

    /* Diccionario: {"clave": valor} */
    if (coincide(p, TOK_LLAVE_IZQ)) {
        consumir(p, TOK_LLAVE_IZQ, "'{'");
        char **claves  = malloc(sizeof(char*) * 256);
        Nodo **valores = malloc(sizeof(Nodo*)  * 256);
        int    cant    = 0;
        if (!coincide(p, TOK_LLAVE_DER)) {
            Token *clave = consumir(p, TOK_TEXTO, "clave del diccionario entre comillas");
            claves[cant] = strdup(clave->valor);
            consumir(p, TOK_DOS_PUNTOS, "':'");
            valores[cant] = expresion(p);
            cant++;
            while (coincide(p, TOK_COMA)) {
                consumir(p, TOK_COMA, "','");
                if (coincide(p, TOK_LLAVE_DER)) break;
                clave = consumir(p, TOK_TEXTO, "clave del diccionario entre comillas");
                claves[cant] = strdup(clave->valor);
                consumir(p, TOK_DOS_PUNTOS, "':'");
                valores[cant] = expresion(p);
                cant++;
            }
        }
        consumir(p, TOK_LLAVE_DER, "'}'");
        Nodo *n = nodo_nuevo(NODO_DICCIONARIO, p);
        n->diccionario.claves   = claves;
        n->diccionario.valores  = valores;
        n->diccionario.cantidad = cant;
        return postfijo(p, n);
    }

    /* Identificador, llamada a función, instancia de clase o acceso a método */
    if (coincide(p, TOK_IDENTIFICADOR)) {
        char *nombre = strdup(t->valor);
        consumir_cualquiera(p);

        /* f(...) con el '(' en la misma línea: uno al principio de la
           siguiente empieza otra cosa */
        if (coincide(p, TOK_PAREN_IZQ) && actual(p)->linea == t->linea) {
            consumir(p, TOK_PAREN_IZQ, "'('");
            Nodo **args = malloc(sizeof(Nodo*) * 64);
            int num_args = 0;
            if (!coincide(p, TOK_PAREN_DER)) {
                args[num_args++] = expresion(p);
                while (coincide(p, TOK_COMA)) {
                    consumir(p, TOK_COMA, "','");
                    args[num_args++] = expresion(p);
                }
            }
            consumir(p, TOK_PAREN_DER, "')'");

            /* Si empieza con mayúscula es instanciación de clase */
            Nodo *base;
            if (nombre[0] >= 'A' && nombre[0] <= 'Z') {
                base = nodo_nuevo(NODO_INSTANCIA, p);
                base->instancia.clase          = nombre;
                base->instancia.argumentos     = args;
                base->instancia.num_argumentos = num_args;
            } else {
                base = nodo_nuevo(NODO_LLAMADA, p);
                base->llamada.nombre          = nombre;
                base->llamada.argumentos      = args;
                base->llamada.num_argumentos  = num_args;
            }
            return postfijo(p, base);
        }

        Nodo *n = nodo_nuevo(NODO_IDENTIFICADOR, p);
        n->identificador = nombre;
        return postfijo(p, n);
    }

    /* Expresión entre paréntesis */
    if (coincide(p, TOK_PAREN_IZQ)) {
        consumir(p, TOK_PAREN_IZQ, "'('");
        Nodo *n = expresion(p);
        consumir(p, TOK_PAREN_DER, "')'");
        return postfijo(p, n);
    }

    error_sintaxis(p, "Expresión no reconocida.");
    return NULL;
}

static Nodo *unario(Parser *p) {
    if (coincide(p, TOK_NO) || coincide(p, TOK_MENOS)) {
        Token *op = consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_UNARIO, p);
        strncpy(n->unario.operador, op->valor, 3);
        n->unario.operando = unario(p);
        return n;
    }
    return primario(p);
}

static Nodo *multiplicacion(Parser *p) {
    Nodo *izq = unario(p);
    while (coincide(p, TOK_ASTERISCO) || coincide(p, TOK_BARRA) || coincide(p, TOK_MODULO)) {
        Token *op = consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BINARIO, p);
        strncpy(n->binario.operador, op->valor, 3);
        n->binario.izquierda = izq;
        n->binario.derecha   = unario(p);
        izq = n;
    }
    return izq;
}

static Nodo *suma(Parser *p) {
    Nodo *izq = multiplicacion(p);
    while (coincide(p, TOK_MAS) || coincide(p, TOK_MENOS)) {
        Token *op = consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BINARIO, p);
        strncpy(n->binario.operador, op->valor, 3);
        n->binario.izquierda = izq;
        n->binario.derecha   = multiplicacion(p);
        izq = n;
    }
    return izq;
}

/* 'x es Tipo': ¿viene un 'es' seguido de un nombre de clase, interfaz o tipo? */
static int viene_es(Parser *p) {
    if (!(coincide(p, TOK_IDENTIFICADOR) && actual(p)->valor &&
          strcmp(actual(p)->valor, "es") == 0)) return 0;
    TipoToken t = ver_siguiente(p)->tipo;
    return t == TOK_IDENTIFICADOR || t == TOK_TIPO_NUMERO || t == TOK_TIPO_TEXTO ||
           t == TOK_TIPO_LOGICO || t == TOK_TIPO_LISTA || t == TOK_TIPO_DICCIONARIO ||
           t == TOK_TIPO_NULO || t == TOK_NULO || t == TOK_LISTA || t == TOK_DICCIONARIO || t == TOK_FUNCION;
}

static Nodo *comparacion(Parser *p) {
    Nodo *izq = suma(p);
    while (coincide(p, TOK_MENOR) || coincide(p, TOK_MAYOR) ||
           coincide(p, TOK_MENOR_IGUAL) || coincide(p, TOK_MAYOR_IGUAL) ||
           viene_es(p)) {
        if (viene_es(p)) {
            consumir_cualquiera(p);                       /* es */
            Token *tipo = consumir_cualquiera(p);         /* Nombre */
            Nodo *r = nodo_nuevo(NODO_IDENTIFICADOR, p);
            r->identificador = strdup(tipo->valor);
            Nodo *n = nodo_nuevo(NODO_BINARIO, p);
            strcpy(n->binario.operador, "es");
            n->binario.izquierda = izq;
            n->binario.derecha   = r;
            izq = n;
            continue;
        }
        Token *op = consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BINARIO, p);
        strncpy(n->binario.operador, op->valor, 3);
        n->binario.izquierda = izq;
        n->binario.derecha   = suma(p);
        izq = n;
    }
    return izq;
}

static Nodo *igualdad(Parser *p) {
    Nodo *izq = comparacion(p);
    while (coincide(p, TOK_IGUAL_IGUAL) || coincide(p, TOK_DIFERENTE)) {
        Token *op = consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BINARIO, p);
        strncpy(n->binario.operador, op->valor, 3);
        n->binario.izquierda = izq;
        n->binario.derecha   = comparacion(p);
        izq = n;
    }
    return izq;
}

static int es_palabra(Parser *p, const char *w);

/* 'y' y 'o' son palabras contextuales: entre dos valores son el operador; en
   cualquier otro sitio, un nombre más (x, y). Tras un valor, 'y' es el
   operador si está en la misma línea, o si empieza una línea pero no sigue un
   '=', '.', '[' o '(' (así 'y = 3' en la línea siguiente es otra sentencia). */
static int es_operador_logico(Parser *p, const char *w) {
    if (!es_palabra(p, w) || p->pos == 0) return 0;
    if (p->tokens[p->pos - 1].linea == actual(p)->linea) return 1;
    switch (ver_siguiente(p)->tipo) {
        case TOK_IGUAL: case TOK_MAS_IGUAL: case TOK_MENOS_IGUAL: case TOK_POR_IGUAL:
        case TOK_ENTRE_IGUAL: case TOK_MODULO_IGUAL: case TOK_MAS_MAS: case TOK_MENOS_MENOS:
        case TOK_PUNTO: case TOK_CORCHETE_IZQ: case TOK_PAREN_IZQ: case TOK_EOF:
            return 0;
        default:
            return 1;
    }
}

static Nodo *logica_y(Parser *p) {
    Nodo *izq = igualdad(p);
    while (es_operador_logico(p, "y")) {
        consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BINARIO, p);
        strcpy(n->binario.operador, "y");
        n->binario.izquierda = izq;
        n->binario.derecha   = igualdad(p);
        izq = n;
    }
    return izq;
}

static Nodo *logica_o(Parser *p) {
    Nodo *izq = logica_y(p);
    while (es_operador_logico(p, "o")) {
        consumir_cualquiera(p);
        Nodo *n = nodo_nuevo(NODO_BINARIO, p);
        strcpy(n->binario.operador, "o");
        n->binario.izquierda = izq;
        n->binario.derecha   = logica_y(p);
        izq = n;
    }
    return izq;
}

static Nodo *expresion(Parser *p) {
    return logica_o(p);
}

/* ─────────────────────────────────────────
   SENTENCIAS
───────────────────────────────────────── */
static Nodo *bloque(Parser *p) {
    consumir(p, TOK_LLAVE_IZQ, "'{'");
    int cap = 16;
    Nodo **sentencias = malloc(sizeof(Nodo*) * cap);
    int cantidad = 0;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        if (cantidad == cap) { cap *= 2; sentencias = realloc(sentencias, sizeof(Nodo*) * cap); }
        sentencias[cantidad++] = sentencia(p);
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_BLOQUE, p);
    n->bloque.sentencias = sentencias;
    n->bloque.cantidad   = cantidad;
    /* Un bloque que no define nada no necesita ámbito propio: el intérprete
       se ahorra crearlo en cada vuelta de un bucle */
    for (int i = 0; i < cantidad; i++) {
        Nodo *s = sentencias[i];
        if (!s) continue;
        switch (s->tipo) {
            case NODO_DECLARACION: case NODO_FUNCION: case NODO_CLASE: case NODO_ENUMERACION:
            case NODO_INTERFAZ: case NODO_GENERADOR: case NODO_IMPORTAR:
                n->bloque.declara = 1; break;
            case NODO_DESESTRUCTURAR:
                if (s->desestructurar.declarar) n->bloque.declara = 1;
                break;
            default: break;
        }
    }
    return n;
}

/* [x, y] = valor  o  {x, y} = valor, ya pasado el 'sea' o el 'fijo' (si lo
   hay). Los nombres van sueltos, sin tipo, separados por comas. */
static Nodo *desestructurar(Parser *p, int constante, int declarar) {
    int dic = coincide(p, TOK_LLAVE_IZQ);
    TipoToken cierre = dic ? TOK_LLAVE_DER : TOK_CORCHETE_DER;
    consumir_cualquiera(p);
    int cap = 4, num = 0;
    char **nombres = malloc(sizeof(char*) * cap);
    do {
        if (num > 0) consumir(p, TOK_COMA, "','");
        Token *t = consumir(p, TOK_IDENTIFICADOR, dic ? "el nombre de una clave" : "un nombre de variable");
        for (int i = 0; i < num; i++)
            if (strcmp(nombres[i], t->valor) == 0) {
                p->pos--;
                error_sintaxis(p, "Ese nombre ya está en la lista: cada uno puede ir una sola vez.");
            }
        if (num == cap) { cap *= 2; nombres = realloc(nombres, sizeof(char*) * cap); }
        nombres[num++] = strdup(t->valor);
    } while (coincide(p, TOK_COMA));
    consumir(p, cierre, dic ? "'}'" : "']'");
    consumir(p, TOK_IGUAL, "'='");

    Nodo *n = nodo_nuevo(NODO_DESESTRUCTURAR, p);
    n->desestructurar.nombres        = nombres;
    n->desestructurar.num_nombres    = num;
    n->desestructurar.de_diccionario = dic;
    n->desestructurar.constante      = constante;
    n->desestructurar.declarar       = declarar;
    n->desestructurar.valor          = expresion(p);
    return n;
}

/* ¿Empieza aquí un '[a, b, ...] ='? (si no, el '[' es una lista) */
static int viene_desestructurar(Parser *p) {
    int i = p->pos;
    if (p->tokens[i].tipo != TOK_CORCHETE_IZQ) return 0;
    for (i++; i < p->cantidad; i += 2) {
        if (p->tokens[i].tipo != TOK_IDENTIFICADOR) return 0;
        if (i + 1 >= p->cantidad) return 0;
        if (p->tokens[i + 1].tipo == TOK_CORCHETE_DER)
            return i + 2 < p->cantidad && p->tokens[i + 2].tipo == TOK_IGUAL;
        if (p->tokens[i + 1].tipo != TOK_COMA) return 0;
    }
    return 0;
}

static Nodo *sent_declaracion(Parser *p, int constante) {
    consumir_cualquiera(p); /* sea / fijo */

    /* ¿Tipo explícito? */
    int es_lista = 0, es_diccionario = 0;
    if (coincide(p, TOK_LISTA)) {
        es_lista = 1;
        consumir(p, TOK_LISTA, "'lista'");
    } else if (coincide(p, TOK_DICCIONARIO)) {
        es_diccionario = 1;
        consumir(p, TOK_DICCIONARIO, "'diccionario'");
    }

    if (!es_lista && !es_diccionario &&
        (coincide(p, TOK_CORCHETE_IZQ) || coincide(p, TOK_LLAVE_IZQ)))
        return desestructurar(p, constante, 1);

    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de variable");
    consumir(p, TOK_IGUAL, "'='");
    Nodo *valor = expresion(p);

    /* El tipo declarado se comprueba en ejecución (ver NODO_DECLARACION
       en interprete.c). Aquí no se puede: el parser sólo ve la forma de
       la expresión, no su tipo, y exigir un literal rechazaba cosas
       perfectamente válidas como 'sea lista l = datos.ordenar()'. */

    Nodo *n = nodo_nuevo(NODO_DECLARACION, p);
    n->declaracion.nombre        = strdup(nombre->valor);
    n->declaracion.valor         = valor;
    n->declaracion.constante     = constante;
    n->declaracion.es_lista      = es_lista;
    n->declaracion.es_diccionario = es_diccionario;
    return n;
}

static Nodo *sent_asignacion(Parser *p) {
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de variable");
    const char *op = op_compuesto(consumir_cualquiera(p)->tipo);   /* '=' o '+='... */
    Nodo *valor = expresion(p);

    Nodo *n = nodo_nuevo(NODO_ASIGNACION, p);
    n->asignacion.nombre = strdup(nombre->valor);
    n->asignacion.valor  = valor;
    if (op) strcpy(n->asignacion.op, op);
    return n;
}

static Nodo *sent_escribir(Parser *p) {
    consumir(p, TOK_ESCRIBIR, "'escribir'");
    consumir(p, TOK_PAREN_IZQ, "'('");
    Nodo *expr = expresion(p);
    consumir(p, TOK_PAREN_DER, "')'");

    Nodo *n = nodo_nuevo(NODO_ESCRIBIR, p);
    n->escribir = expr;
    return n;
}

static Nodo *sent_si(Parser *p) {
    consumir(p, TOK_SI, "'si'");
    Nodo *condicion = expresion(p);
    Nodo *entonces  = bloque(p);

    Nodo **conds_sino_si  = malloc(sizeof(Nodo*) * 32);
    Nodo **bloqs_sino_si  = malloc(sizeof(Nodo*) * 32);
    int    cant_sino_si   = 0;
    Nodo  *sino           = NULL;

    while (coincide(p, TOK_SINO)) {
        consumir(p, TOK_SINO, "'sino'");
        if (coincide(p, TOK_SI)) {
            consumir(p, TOK_SI, "'si'");
            conds_sino_si[cant_sino_si] = expresion(p);
            bloqs_sino_si[cant_sino_si] = bloque(p);
            cant_sino_si++;
        } else {
            sino = bloque(p);
            break;
        }
    }

    Nodo *n = nodo_nuevo(NODO_SI, p);
    n->si.condicion          = condicion;
    n->si.entonces           = entonces;
    n->si.condiciones_sino_si = conds_sino_si;
    n->si.bloques_sino_si    = bloqs_sino_si;
    n->si.cantidad_sino_si   = cant_sino_si;
    n->si.sino               = sino;
    return n;
}

static Nodo *sent_mientras(Parser *p) {
    consumir(p, TOK_MIENTRAS, "'mientras'");
    Nodo *condicion = expresion(p);
    Nodo *cuerpo    = cuerpo_bucle(p);

    Nodo *n = nodo_nuevo(NODO_MIENTRAS, p);
    n->mientras.condicion = condicion;
    n->mientras.cuerpo    = cuerpo;
    return n;
}

static Nodo *sent_hacer(Parser *p) {
    consumir(p, TOK_HACER, "'hacer'");
    Nodo *cuerpo = cuerpo_bucle(p);
    consumir(p, TOK_MIENTRAS, "'mientras'");
    consumir(p, TOK_PAREN_IZQ, "'('");
    Nodo *condicion = expresion(p);
    consumir(p, TOK_PAREN_DER, "')'");

    Nodo *n = nodo_nuevo(NODO_HACER, p);
    n->hacer.cuerpo    = cuerpo;
    n->hacer.condicion = condicion;
    return n;
}

static Nodo *sent_para(Parser *p) {
    consumir(p, TOK_PARA, "'para'");

    /* ¿Es 'para cada'? */
    if (coincide(p, TOK_CADA)) {
        consumir(p, TOK_CADA, "'cada'");

        /* tipo */
        char     *tipo_clase = NULL;
        TipoDato  tipo       = parsear_tipo(p, &tipo_clase);

        /* nombre de variable */
        Token *var = consumir(p, TOK_IDENTIFICADOR, "nombre de variable");

        /* 'en' — lo tratamos como identificador ya que no es keyword */
        Token *en = consumir(p, TOK_IDENTIFICADOR, "'en'");
        if (strcmp(en->valor, "en") != 0) {
            fprintf(stderr,
                "\n❌ Error en línea %d%s:\n"
                "   Se esperaba 'en' después del nombre de variable.\n"
                "   Ejemplo: para cada numero n en lista\n\n",
                en->linea, lince_de_archivo());
            lince_marcar_columna(en->columna, en->ancho);
            exit(1);
        }

        /* colección */
        Nodo *coleccion = expresion(p);
        Nodo *cuerpo    = cuerpo_bucle(p);

        Nodo *n = nodo_nuevo(NODO_PARA_CADA, p);
        n->para_cada.tipo       = tipo;
        n->para_cada.tipo_clase = tipo_clase;
        n->para_cada.variable   = strdup(var->valor);
        n->para_cada.coleccion  = coleccion;
        n->para_cada.cuerpo     = cuerpo;
        return n;
    }

    /* Para clásico: para (i = 0; i < 10; i++) */
    consumir(p, TOK_PAREN_IZQ, "'('");
    Token *var = consumir(p, TOK_IDENTIFICADOR, "variable del bucle");
    consumir(p, TOK_IGUAL, "'='");
    Nodo *val_inicio = expresion(p);
    consumir(p, TOK_PUNTO_COMA, "';'");
    Nodo *condicion = expresion(p);
    consumir(p, TOK_PUNTO_COMA, "';'");
    Token *var_inc = consumir(p, TOK_IDENTIFICADOR, "variable de incremento");
    Token *op_inc;
    Nodo  *paso = NULL;
    if (coincide(p, TOK_MAS_MAS) || coincide(p, TOK_MENOS_MENOS)) {
        op_inc = consumir_cualquiera(p);
    } else if (op_compuesto(actual(p)->tipo)) {
        /* para (i = 0; i < 10; i += 2) */
        op_inc = consumir_cualquiera(p);
        paso = nodo_nuevo(NODO_ASIGNACION, p);
        paso->asignacion.nombre = strdup(var_inc->valor);
        strcpy(paso->asignacion.op, op_compuesto(op_inc->tipo));
        paso->asignacion.valor = expresion(p);
    } else {
        error_sintaxis(p, "Se esperaba '++', '--' o algo como '+= 2' en el incremento del bucle 'para'.");
        return NULL;
    }
    consumir(p, TOK_PAREN_DER, "')'");
    Nodo *cuerpo = cuerpo_bucle(p);

    Nodo *n = nodo_nuevo(NODO_PARA, p);
    n->para.var_inicio     = strdup(var->valor);
    n->para.val_inicio     = val_inicio;
    n->para.condicion      = condicion;
    n->para.var_incremento = strdup(var_inc->valor);
    strncpy(n->para.op_incremento, op_inc->valor, 3);
    n->para.paso           = paso;
    n->para.cuerpo         = cuerpo;
    return n;
}

/* Parsea un tipo de dato: numero, texto, logico, lista, diccionario, nulo, o NombreClase */
static TipoDato parsear_tipo(Parser *p, char **tipo_clase_out) {
    if (tipo_clase_out) *tipo_clase_out = NULL;
    if (coincide(p, TOK_TIPO_NUMERO))                            { consumir_cualquiera(p); return TIPO_NUMERO; }
    if (coincide(p, TOK_TIPO_TEXTO))                             { consumir_cualquiera(p); return TIPO_TEXTO; }
    if (coincide(p, TOK_TIPO_LOGICO))                            { consumir_cualquiera(p); return TIPO_LOGICO; }
    if (coincide(p, TOK_LISTA)  || coincide(p, TOK_TIPO_LISTA)) { consumir_cualquiera(p); return TIPO_LISTA; }
    if (coincide(p, TOK_DICCIONARIO) || coincide(p, TOK_TIPO_DICCIONARIO)) { consumir_cualquiera(p); return TIPO_DICCIONARIO; }
    if (coincide(p, TOK_NULO)   || coincide(p, TOK_TIPO_NULO))  { consumir_cualquiera(p); return TIPO_NULO; }
    if (coincide(p, TOK_FUNCION))                                { consumir_cualquiera(p); return TIPO_FUNCION; }
    if (coincide(p, TOK_IDENTIFICADOR)) {
        Token *t = consumir_cualquiera(p);
        /* 'cualquiera': acepta un valor de cualquier tipo (no es palabra
           reservada, solo significa eso donde va un tipo) */
        if (strcmp(t->valor, "cualquiera") == 0) return TIPO_CUALQUIERA;
        if (strcmp(t->valor, "tarea") == 0)      return TIPO_TAREA;
        if (tipo_clase_out) *tipo_clase_out = strdup(t->valor);
        return TIPO_CLASE;
    }
    error_sintaxis(p, "Se esperaba un tipo (numero, texto, logico, lista, diccionario, nulo, funcion, o nombre de clase).");
    return TIPO_CUALQUIERA;
}

/* Desde el '(' de la lista de parámetros en adelante. 'sin_cuerpo_ok' admite
   una firma sin '{' (método abstracto): cuerpo NULL. */
static Nodo *funcion_desde_parametros(Parser *p, Token *nombre, int sin_cuerpo_ok) {
    int tarea = p->tarea;       /* solo para esta: no para las que lleve dentro */
    p->tarea = 0;
    consumir(p, TOK_PAREN_IZQ, "'('");

    Parametro *params    = calloc(64, sizeof(Parametro));
    int        num_params = 0;

    if (!coincide(p, TOK_PAREN_DER)) {
        do {
            if (num_params > 0) consumir(p, TOK_COMA, "','");

            /* val o ref */
            int por_ref = 0;
            if (coincide(p, TOK_VAL)) {
                consumir(p, TOK_VAL, "'val'");
                por_ref = 0;
            } else if (coincide(p, TOK_REF)) {
                consumir(p, TOK_REF, "'ref'");
                por_ref = 1;
            }                       /* sin 'val' ni 'ref': val */

            /* tipo */
            char *tipo_clase = NULL;
            TipoDato tipo = parsear_tipo(p, &tipo_clase);

            /* nombre del parámetro */
            Token *pnombre = consumir(p, TOK_IDENTIFICADOR, "nombre del parámetro");

            /* ¿valor por defecto? */
            Nodo *valor_defecto = NULL;
            if (coincide(p, TOK_IGUAL)) {
                consumir(p, TOK_IGUAL, "'='");
                valor_defecto = expresion(p);
            }

            params[num_params].nombre        = strdup(pnombre->valor);
            params[num_params].tipo          = tipo;
            params[num_params].tipo_clase    = tipo_clase;
            params[num_params].por_ref       = por_ref;
            params[num_params].valor_defecto = valor_defecto;
            num_params++;
        } while (coincide(p, TOK_COMA));
    }
    consumir(p, TOK_PAREN_DER, "')'");

    /* tipo de retorno: ): tipo. Una tarea puede no llevarlo: es lo que da
       esperar(tarea) cuando acaba, y casi nunca hace falta. */
    char *tipo_ret_clase = NULL;
    TipoDato tipo_retorno = TIPO_CUALQUIERA;
    if (!tarea || coincide(p, TOK_DOS_PUNTOS)) {
        consumir(p, TOK_DOS_PUNTOS, "':' seguido del tipo de retorno");
        tipo_retorno = parsear_tipo(p, &tipo_ret_clase);
    }

    Nodo *cuerpo = (sin_cuerpo_ok && !coincide(p, TOK_LLAVE_IZQ)) ? NULL : cuerpo_funcion(p);

    Nodo *n = nodo_nuevo(NODO_FUNCION, p);
    n->funcion.nombre             = strdup(nombre->valor);
    n->funcion.parametros         = params;
    n->funcion.num_parametros     = num_params;
    n->funcion.tipo_retorno       = tipo_retorno;
    n->funcion.tipo_retorno_clase = tipo_ret_clase;
    n->funcion.cuerpo             = cuerpo;
    n->funcion.es_tarea           = tarea;
    return n;
}

static int es_palabra(Parser *p, const char *w) {
    return coincide(p, TOK_IDENTIFICADOR) && actual(p)->valor &&
           strcmp(actual(p)->valor, w) == 0;
}

/* 'tarea nombre(' — 'tarea' no es palabra reservada: solo cuenta así */
static int viene_tarea(Parser *p) {
    return es_palabra(p, "tarea") && ver_siguiente(p)->tipo == TOK_IDENTIFICADOR &&
           p->pos + 2 < p->cantidad && p->tokens[p->pos + 2].tipo == TOK_PAREN_IZQ;
}

/* tarea nombre(parámetros)[: tipo] { ... } — una función que, al llamarla,
   empieza a correr aparte y puede pararse con esperar() */
static Nodo *sent_tarea(Parser *p) {
    consumir_cualquiera(p);
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de la tarea");
    p->tarea = 1;
    return funcion_desde_parametros(p, nombre, 0);
}

static Nodo *sent_funcion(Parser *p) {
    consumir(p, TOK_FUNCION, "'funcion'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de función");
    return funcion_desde_parametros(p, nombre, 0);
}

static Nodo *sent_clase(Parser *p) {
    int abstracta = 0;
    if (es_palabra(p, "abstracta")) { consumir_cualquiera(p); abstracta = 1; }
    consumir(p, TOK_CLASE, "'clase'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de clase");

    /* ¿Extiende otra clase? */
    char *padre = NULL;
    if (coincide(p, TOK_EXTIENDE)) {
        consumir(p, TOK_EXTIENDE, "'extiende'");
        Token *tnombre_padre = consumir(p, TOK_IDENTIFICADOR, "nombre de clase padre");
        padre = strdup(tnombre_padre->valor);
    }

    /* ¿Implementa interfaces? */
    char **interfaces    = malloc(sizeof(char*) * 16);
    int    num_interfaces = 0;
    if (coincide(p, TOK_IMPLEMENTA)) {
        consumir(p, TOK_IMPLEMENTA, "'implementa'");
        Token *ti = consumir(p, TOK_IDENTIFICADOR, "nombre de interfaz");
        interfaces[num_interfaces++] = strdup(ti->valor);
        while (coincide(p, TOK_COMA)) {
            consumir(p, TOK_COMA, "','");
            ti = consumir(p, TOK_IDENTIFICADOR, "nombre de interfaz");
            interfaces[num_interfaces++] = strdup(ti->valor);
        }
    }

    consumir(p, TOK_LLAVE_IZQ, "'{'");

    Nodo **metodos    = malloc(sizeof(Nodo*) * 64);
    int    num_metodos = 0;
    Nodo **campos     = malloc(sizeof(Nodo*) * 64);
    int    num_campos  = 0;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        /* Modificadores: publico/privado/protegido, estatico, abstracto,
           sobrescribe. Son palabras contextuales: fuera de una clase siguen
           siendo identificadores corrientes. */
        int vis = 0, estatico = 0, abstracto = 0, sobrescribe = 0;
        int linea_mod = actual(p)->linea;
        while (1) {
            if      (es_palabra(p, "publico"))   { vis = 0; }
            else if (es_palabra(p, "privado"))   { vis = 1; }
            else if (es_palabra(p, "protegido")) { vis = 2; }
            else if (es_palabra(p, "estatico"))  { estatico = 1; }
            else if (es_palabra(p, "abstracto")) { abstracto = 1; }
            else if (es_palabra(p, "sobrescribe")) { sobrescribe = 1; }
            else break;
            consumir_cualquiera(p);
        }

        if (coincide(p, TOK_FUNCION) ||
            ((es_palabra(p, "obtener") || es_palabra(p, "fijar") || es_palabra(p, "tarea")) &&
             ver_siguiente(p)->tipo == TOK_IDENTIFICADOR)) {
            int prop = 0;
            if (es_palabra(p, "tarea")) {
                consumir_cualquiera(p);
                if (abstracto || estatico)
                    error_sintaxis(p, "Una tarea no puede ser 'abstracto' ni 'estatico'.");
                if (es_palabra(p, "crear"))
                    error_sintaxis(p, "'crear' no puede ser una tarea: el objeto tiene que estar hecho al acabar crear(). "
                                      "Empieza la tarea desde crear(), llamándola.");
                p->tarea = 1;
            } else if (!coincide(p, TOK_FUNCION)) {
                prop = es_palabra(p, "obtener") ? 1 : 2;
                consumir_cualquiera(p);
            } else {
                consumir(p, TOK_FUNCION, "'funcion'");
            }
            Token *mn = consumir(p, TOK_IDENTIFICADOR, "nombre del método");
            Nodo *m = funcion_desde_parametros(p, mn, abstracto);
            if (abstracto && m->funcion.cuerpo) {
                fprintf(stderr, "\n❌ Error en línea %d%s:\n"
                    "   El método abstracto '%s' no puede tener cuerpo.\n\n", mn->linea, lince_de_archivo(), mn->valor);
                exit(1);
            }
            if (!abstracto && !m->funcion.cuerpo) {
                fprintf(stderr, "\n❌ Error en línea %d%s:\n"
                    "   El método '%s' no tiene cuerpo. ¿Querías marcarlo 'abstracto'?\n\n",
                    mn->linea, lince_de_archivo(), mn->valor);
                exit(1);
            }
            m->funcion.vis = vis; m->funcion.estatico = estatico;
            m->funcion.abstracto = abstracto; m->funcion.sobrescribe = sobrescribe;
            m->funcion.prop = prop;
            if (prop) {
                /* Se guarda como método 'obtener edad' / 'fijar edad' */
                char buf[256];
                snprintf(buf, sizeof(buf), "%s %s", prop == 1 ? "obtener" : "fijar", mn->valor);
                free(m->funcion.nombre);
                m->funcion.nombre = strdup(buf);
            }
            metodos[num_metodos++] = m;
        } else if (coincide(p, TOK_SEA) || coincide(p, TOK_FIJO)) {
            int constante = coincide(p, TOK_FIJO);
            consumir_cualquiera(p);
            int es_lista = 0, es_dic = 0;
            if (coincide(p, TOK_LISTA)) { es_lista = 1; consumir_cualquiera(p); }
            else if (coincide(p, TOK_DICCIONARIO)) { es_dic = 1; consumir_cualquiera(p); }
            Token *cn = consumir(p, TOK_IDENTIFICADOR, "nombre del campo");
            Nodo *valor = NULL;
            if (coincide(p, TOK_IGUAL)) { consumir(p, TOK_IGUAL, "'='"); valor = expresion(p); }
            Nodo *c = nodo_nuevo(NODO_DECLARACION, p);
            c->linea = cn->linea;
            c->declaracion.nombre = strdup(cn->valor);
            c->declaracion.valor = valor;
            c->declaracion.constante = constante;
            c->declaracion.es_lista = es_lista;
            c->declaracion.es_diccionario = es_dic;
            c->declaracion.vis = vis;
            c->declaracion.estatico = estatico;
            campos[num_campos++] = c;
        } else {
            (void)linea_mod;
            fprintf(stderr,
                "\n❌ Error en línea %d%s:\n"
                "   Dentro de una clase solo puede haber métodos ('funcion'), campos\n"
                "   ('sea' / 'fijo') y propiedades ('obtener' / 'fijar').\n"
                "   Encontré '%s' en su lugar.\n\n",
                actual(p)->linea, lince_de_archivo(), actual(p)->valor ? actual(p)->valor : "?");
            lince_marcar_columna(actual(p)->columna, actual(p)->ancho);
            exit(1);
        }
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_CLASE, p);
    n->clase.nombre         = strdup(nombre->valor);
    n->clase.padre          = padre;
    n->clase.interfaces     = interfaces;
    n->clase.num_interfaces = num_interfaces;
    n->clase.metodos        = metodos;
    n->clase.num_metodos    = num_metodos;
    n->clase.abstracta      = abstracta;
    n->clase.campos         = campos;
    n->clase.num_campos     = num_campos;
    return n;
}

static Nodo *sent_devolver(Parser *p) {
    consumir(p, TOK_DEVOLVER, "'devolver'");
    Nodo *valor = expresion(p);
    Nodo *n = nodo_nuevo(NODO_DEVOLVER, p);
    n->devolver = valor;
    return n;
}

static Nodo *sent_lanzar(Parser *p) {
    consumir(p, TOK_LANZAR, "'lanzar'");
    Nodo *expr = expresion(p);
    Nodo *n = nodo_nuevo(NODO_LANZAR, p);
    n->lanzar = expr;
    return n;
}

static Nodo *sent_elegir(Parser *p) {
    consumir(p, TOK_ELEGIR, "'elegir'");
    Nodo *sujeto = expresion(p);
    consumir(p, TOK_LLAVE_IZQ, "'{'");

    int    cap      = 16;
    Nodo **valores  = malloc(sizeof(Nodo*) * cap);
    Nodo **cuerpos  = malloc(sizeof(Nodo*) * cap);
    int    num      = 0;
    Nodo  *otro     = NULL;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        if (coincide(p, TOK_CASO)) {
            consumir(p, TOK_CASO, "'caso'");
            if (num == cap) {
                cap *= 2;
                valores = realloc(valores, sizeof(Nodo*) * cap);
                cuerpos = realloc(cuerpos, sizeof(Nodo*) * cap);
            }
            valores[num] = expresion(p);
            cuerpos[num] = bloque(p);
            num++;
        } else if (coincide(p, TOK_OTRO)) {
            consumir(p, TOK_OTRO, "'otro'");
            otro = bloque(p);
        } else {
            fprintf(stderr,
                "\n❌ Error en línea %d%s:\n"
                "   Dentro de 'elegir' solo puede haber 'caso' u 'otro'.\n\n",
                actual(p)->linea, lince_de_archivo());
            lince_marcar_columna(actual(p)->columna, actual(p)->ancho);
            exit(1);
        }
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_ELEGIR, p);
    n->elegir.sujeto    = sujeto;
    n->elegir.valores   = valores;
    n->elegir.cuerpos   = cuerpos;
    n->elegir.num_casos = num;
    n->elegir.otro      = otro;
    apuntar(&p->elegires, &p->nelegires, n);
    return n;
}

/* enumeracion Estado { Quieto Andando Saltando }, cada uno con un valor si
   se quiere: enumeracion Direccion { Arriba = [0, -1], Abajo = [0, 1] }. Se
   separan con espacios, saltos de línea o comas. */
static Nodo *sent_enumeracion(Parser *p) {
    consumir(p, TOK_ENUMERACION, "'enumeracion'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de la enumeración");
    consumir(p, TOK_LLAVE_IZQ, "'{'");

    int    cap         = 16;
    char **valores     = malloc(sizeof(char*) * cap);
    Nodo **expresiones = malloc(sizeof(Nodo*) * cap);
    int    num_valores = 0;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        Token *val = consumir(p, TOK_IDENTIFICADOR, "nombre del valor");
        for (int i = 0; i < num_valores; i++)
            if (strcmp(valores[i], val->valor) == 0)
                error_sintaxis(p, "Ese valor ya está en la enumeración: cada uno tiene que tener un nombre distinto.");
        if (num_valores == cap) {
            cap *= 2;
            valores     = realloc(valores, sizeof(char*) * cap);
            expresiones = realloc(expresiones, sizeof(Nodo*) * cap);
        }
        valores[num_valores] = strdup(val->valor);
        expresiones[num_valores] = NULL;
        if (coincide(p, TOK_IGUAL)) {
            consumir_cualquiera(p);
            expresiones[num_valores] = expresion(p);
        }
        num_valores++;
        if (coincide(p, TOK_COMA)) consumir_cualquiera(p);
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_ENUMERACION, p);
    n->enumeracion.nombre      = strdup(nombre->valor);
    n->enumeracion.valores     = valores;
    n->enumeracion.expresiones = expresiones;
    n->enumeracion.num_valores = num_valores;
    apuntar(&p->enums, &p->nenums, n);
    return n;
}

static Nodo *sent_interfaz(Parser *p) {
    consumir(p, TOK_INTERFAZ, "'interfaz'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de la interfaz");
    consumir(p, TOK_LLAVE_IZQ, "'{'");

    char  **metodos      = malloc(sizeof(char*)  * 64);
    char  **tipos_ret    = malloc(sizeof(char*)  * 64);
    Nodo  **funciones    = calloc(64, sizeof(Nodo*));
    int     num_metodos  = 0;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        /* Firmas: funcion nombre(params): tipo — o, con cuerpo, un método
           por defecto que las clases reciben si no tienen uno propio. */
        consumir(p, TOK_FUNCION, "'funcion'");
        Token *nom_met = consumir(p, TOK_IDENTIFICADOR, "nombre del método");
        int pos_parens = p->pos;
        consumir(p, TOK_PAREN_IZQ, "'('");
        /* Ignorar parámetros — solo importa la firma */
        int prof = 1;
        while (prof > 0 && !coincide(p, TOK_EOF)) {
            if (coincide(p, TOK_PAREN_IZQ))  { consumir_cualquiera(p); prof++; }
            else if (coincide(p, TOK_PAREN_DER)) { consumir_cualquiera(p); prof--; }
            else consumir_cualquiera(p);
        }
        /* Tipo de retorno */
        char *tipo_ret_str = strdup("nulo");
        if (coincide(p, TOK_DOS_PUNTOS)) {
            consumir(p, TOK_DOS_PUNTOS, "':'");
            char *tipo_clase = NULL;
            TipoDato tipo = parsear_tipo(p, &tipo_clase);
            (void)tipo;
            if (tipo_clase) { free(tipo_ret_str); tipo_ret_str = tipo_clase; }
        }
        metodos[num_metodos]   = strdup(nom_met->valor);
        tipos_ret[num_metodos] = tipo_ret_str;
        if (coincide(p, TOK_LLAVE_IZQ)) {
            /* Tiene cuerpo: se vuelve al '(' y se parsea como una función */
            p->pos = pos_parens;
            funciones[num_metodos] = funcion_desde_parametros(p, nom_met, 0);
        }
        num_metodos++;
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_INTERFAZ, p);
    n->interfaz.nombre       = strdup(nombre->valor);
    n->interfaz.metodos      = metodos;
    n->interfaz.tipos_retorno = tipos_ret;
    n->interfaz.num_metodos  = num_metodos;
    n->interfaz.funciones    = funciones;
    return n;
}

static Nodo *sent_generador(Parser *p) {
    consumir(p, TOK_GENERADOR, "'generador'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre del generador");
    consumir(p, TOK_PAREN_IZQ, "'('");

    Parametro *params     = calloc(64, sizeof(Parametro));
    int        num_params = 0;

    if (!coincide(p, TOK_PAREN_DER)) {
        do {
            if (num_params > 0) consumir(p, TOK_COMA, "','");
            int por_ref = 0;
            if (coincide(p, TOK_VAL))      { consumir_cualquiera(p); por_ref = 0; }
            else if (coincide(p, TOK_REF)) { consumir_cualquiera(p); por_ref = 1; }
            /* sin 'val' ni 'ref': val */
            char *tipo_clase = NULL;
            TipoDato tipo = parsear_tipo(p, &tipo_clase);
            Token *pnom = consumir(p, TOK_IDENTIFICADOR, "nombre del parámetro");
            Nodo *vdef = NULL;
            if (coincide(p, TOK_IGUAL)) {
                consumir(p, TOK_IGUAL, "'='");
                vdef = expresion(p);
            }
            params[num_params].nombre        = strdup(pnom->valor);
            params[num_params].tipo          = tipo;
            params[num_params].tipo_clase    = tipo_clase;
            params[num_params].por_ref       = por_ref;
            params[num_params].valor_defecto = vdef;
            num_params++;
        } while (coincide(p, TOK_COMA));
    }
    consumir(p, TOK_PAREN_DER, "')'");
    consumir(p, TOK_DOS_PUNTOS, "':'");
    char *tipo_clase = NULL;
    TipoDato tipo_retorno = parsear_tipo(p, &tipo_clase);
    (void)tipo_clase;
    Nodo *cuerpo = cuerpo_funcion(p);

    Nodo *n = nodo_nuevo(NODO_GENERADOR, p);
    n->generador.nombre         = strdup(nombre->valor);
    n->generador.parametros     = params;
    n->generador.num_parametros = num_params;
    n->generador.tipo_retorno   = tipo_retorno;
    n->generador.cuerpo         = cuerpo;
    return n;
}

static Nodo *sent_importar(Parser *p) {
    consumir(p, TOK_IMPORTAR, "'importar'");
    Token *nombre = consumir(p, TOK_TEXTO, "nombre del módulo entre comillas");
    Nodo *n = nodo_nuevo(NODO_IMPORTAR, p);
    n->importar.nombre = strdup(nombre->valor);
    n->importar.alias  = NULL;

    /* 'como' se reconoce aquí y no en el lexer a propósito: así no pasa a ser
       palabra reservada y el código que ya use 'como' de nombre sigue
       valiendo. */
    if (coincide(p, TOK_IDENTIFICADOR) && strcmp(actual(p)->valor, "como") == 0) {
        consumir_cualquiera(p);   /* el 'como' */
        Token *alias = consumir(p, TOK_IDENTIFICADOR, "un nombre después de 'como'");
        n->importar.alias = strdup(alias->valor);
    }
    return n;
}

static Nodo *sent_intentar(Parser *p) {
    consumir(p, TOK_INTENTAR, "'intentar'");
    Nodo *cuerpo = bloque(p);

    char **tipos  = malloc(sizeof(char*) * 16);
    char **vars   = malloc(sizeof(char*) * 16);
    Nodo **cuerpos = malloc(sizeof(Nodo*) * 16);
    int    num    = 0;

    while (coincide(p, TOK_CAPTURAR)) {
        consumir(p, TOK_CAPTURAR, "'capturar'");
        consumir(p, TOK_PAREN_IZQ, "'('");

        /* tipo del error — identificador (ErrorTipo, error, etc.) */
        Token *tipo = consumir(p, TOK_IDENTIFICADOR, "tipo de error");
        /* nombre de la variable */
        Token *var  = consumir(p, TOK_IDENTIFICADOR, "nombre de variable de error");

        consumir(p, TOK_PAREN_DER, "')'");
        Nodo *cuerpo_captura = bloque(p);

        tipos[num]   = strdup(tipo->valor);
        vars[num]    = strdup(var->valor);
        cuerpos[num] = cuerpo_captura;
        num++;
    }

    if (num == 0) {
        fprintf(stderr,
            "\n❌ Error:\n"
            "   'intentar' necesita al menos un bloque 'capturar'.\n\n");
        exit(1);
    }

    Nodo *finalmente = NULL;
    if (coincide(p, TOK_FINALMENTE)) {
        consumir(p, TOK_FINALMENTE, "'finalmente'");
        finalmente = bloque(p);
    }

    Nodo *n = nodo_nuevo(NODO_INTENTAR, p);
    n->intentar.cuerpo          = cuerpo;
    n->intentar.tipos_captura   = tipos;
    n->intentar.vars_captura    = vars;
    n->intentar.cuerpos_captura = cuerpos;
    n->intentar.num_capturas    = num;
    n->intentar.finalmente      = finalmente;
    return n;
}

/* Una cadena ya leída (lista[i], a[i][j], obj.campo, mi.a.b...): si le sigue
   '=' o '+='..., su último eslabón es lo que se asigna; si no, es una
   expresión usada como sentencia. */
static Nodo *asignar_a_cadena(Parser *p, Nodo *base) {
    int paso = coincide(p, TOK_MAS_MAS) || coincide(p, TOK_MENOS_MENOS);
    if (!coincide(p, TOK_IGUAL) && !op_compuesto(actual(p)->tipo) && !paso) return base;
    const char *op;
    Nodo *valor;
    if (paso) {                     /* mi.x++ y lista[i]-- son mi.x += 1 y lista[i] -= 1 */
        op = coincide(p, TOK_MAS_MAS) ? "+" : "-";
        consumir_cualquiera(p);
        valor = nodo_nuevo(NODO_NUMERO, p);
        valor->numero = 1;
    } else {
        op = op_compuesto(consumir_cualquiera(p)->tipo);
        valor = expresion(p);
    }
    if (base->tipo == NODO_INDICE) {
        Nodo *n = nodo_nuevo(NODO_ASIGNACION_INDICE, p);
        n->asignacion_indice.objeto = base->acceso_indice.objeto;
        n->asignacion_indice.indice = base->acceso_indice.indice;
        n->asignacion_indice.valor  = valor;
        if (op) strcpy(n->asignacion_indice.op, op);
        free(base);
        return n;
    }
    if (base->tipo == NODO_ACCESO) {
        Nodo *n = nodo_nuevo(NODO_ASIGNACION_CAMPO, p);
        n->asignacion_campo.objeto = base->acceso.objeto;
        n->asignacion_campo.campo  = base->acceso.campo;
        n->asignacion_campo.valor  = valor;
        if (op) strcpy(n->asignacion_campo.op, op);
        free(base);
        return n;
    }
    error_sintaxis(p, paso
        ? "Solo se puede sumar o restar uno ('++', '--') a una variable, a un elemento (lista[i]) o a un campo (objeto.campo)."
        : "A esto no se le puede asignar un valor: solo a una variable, a un elemento (lista[i]) o a un campo (objeto.campo).");
    return NULL;
}

static Nodo *sentencia_interna(Parser *p) {
    if (coincide(p, TOK_SEA))      return sent_declaracion(p, 0);
    if (coincide(p, TOK_FIJO))     return sent_declaracion(p, 1);
    if (coincide(p, TOK_ESCRIBIR)) return sent_escribir(p);
    if (coincide(p, TOK_SI))       return sent_si(p);
    if (coincide(p, TOK_MIENTRAS)) return sent_mientras(p);
    if (coincide(p, TOK_HACER))    return sent_hacer(p);
    if (coincide(p, TOK_PARA))     return sent_para(p);
    if (coincide(p, TOK_FUNCION))  return sent_funcion(p);
    if (viene_tarea(p))            return sent_tarea(p);
    if (coincide(p, TOK_DEVOLVER)) return sent_devolver(p);
    if (coincide(p, TOK_CLASE))    return sent_clase(p);
    if (es_palabra(p, "abstracta") && ver_siguiente(p)->tipo == TOK_CLASE) return sent_clase(p);
    if (coincide(p, TOK_ELEGIR))   return sent_elegir(p);

    if (coincide(p, TOK_ROMPER) || coincide(p, TOK_CONTINUAR)) {
        int romper = coincide(p, TOK_ROMPER);
        if (p->bucles == 0)
            error_sintaxis(p, romper
                ? "'romper' solo puede ir dentro de un bucle (mientras, hacer, para o para cada)."
                : "'continuar' solo puede ir dentro de un bucle (mientras, hacer, para o para cada).");
        consumir_cualquiera(p);
        return nodo_nuevo(romper ? NODO_ROMPER : NODO_CONTINUAR, p);
    }

    if (viene_desestructurar(p)) return desestructurar(p, 0, 0);

    if (coincide(p, TOK_INTENTAR))    return sent_intentar(p);
    if (coincide(p, TOK_LANZAR))      return sent_lanzar(p);
    if (coincide(p, TOK_IMPORTAR))    return sent_importar(p);
    if (coincide(p, TOK_ELEGIR))      return sent_elegir(p);
    if (coincide(p, TOK_ENUMERACION)) return sent_enumeracion(p);
    if (coincide(p, TOK_INTERFAZ))    return sent_interfaz(p);
    if (coincide(p, TOK_GENERADOR))   return sent_generador(p);

    /* producir valor */
    if (coincide(p, TOK_PRODUCIR)) {
        consumir(p, TOK_PRODUCIR, "'producir'");
        Nodo *n = nodo_nuevo(NODO_PRODUCIR, p);
        n->producir = expresion(p);
        return n;
    }

    /* padre(args) — llama al constructor del padre */
    if (coincide(p, TOK_PADRE)) {
        consumir(p, TOK_PADRE, "'padre'");
        consumir(p, TOK_PAREN_IZQ, "'('");
        Nodo **args = malloc(sizeof(Nodo*) * 64);
        int    nargs = 0;
        if (!coincide(p, TOK_PAREN_DER)) {
            args[nargs++] = expresion(p);
            while (coincide(p, TOK_COMA)) {
                consumir(p, TOK_COMA, "','");
                args[nargs++] = expresion(p);
            }
        }
        consumir(p, TOK_PAREN_DER, "')'");
        Nodo *n = nodo_nuevo(NODO_PADRE, p);
        n->padre.argumentos     = args;
        n->padre.num_argumentos = nargs;
        return n;
    }
    /* esto.campo = valor */
    if (coincide(p, TOK_MI)) {
        consumir(p, TOK_MI, "'mi'");
        consumir(p, TOK_PUNTO, "'.'");
        Token *campo = consumir(p, TOK_IDENTIFICADOR, "nombre de campo");
        Nodo *obj = nodo_nuevo(NODO_ESTO, p);
        Nodo *acc = nodo_nuevo(NODO_ACCESO, p);
        acc->acceso.objeto = obj;
        acc->acceso.campo  = strdup(campo->valor);
        return asignar_a_cadena(p, postfijo(p, acc));
    }

    /* Asignación, incremento, o asignación a índice */
    if (coincide(p, TOK_IDENTIFICADOR)) {
        TipoToken sig = ver_siguiente(p)->tipo;
        if (sig == TOK_IGUAL || op_compuesto(sig))         return sent_asignacion(p);
        if (sig == TOK_MAS_MAS || sig == TOK_MENOS_MENOS) {
            Token *var = consumir_cualquiera(p);
            Token *op  = consumir_cualquiera(p);
            Nodo *n = nodo_nuevo(NODO_INCREMENTO, p);
            n->incremento.nombre = strdup(var->valor);
            strncpy(n->incremento.operador, op->valor, 3);
            return n;
        }
        /* Cadenas que empiezan por una variable: lista[i] = v, a[i][j] = v,
           obj.campo = v, obj.a.b = v, f["camino"].eliminar(0)... Se lee la
           cadena entera y, si sigue un '=', su último eslabón es lo que se
           asigna; si no, es una expresión usada como sentencia. */
        if (sig == TOK_CORCHETE_IZQ || sig == TOK_PUNTO) {
            Token *var = consumir_cualquiera(p);
            Nodo *base = nodo_nuevo(NODO_IDENTIFICADOR, p);
            base->identificador = strdup(var->valor);
            return asignar_a_cadena(p, postfijo(p, base));
        }
    }

    /* Fallback: cualquier expresión puede ser sentencia (llamadas a métodos,
       (tarea() { ... })(), etc.) */
    if (coincide(p, TOK_IDENTIFICADOR) || coincide(p, TOK_PAREN_IZQ)) {
        Nodo *expr = expresion(p);
        return expr;
    }

    error_sintaxis(p, "Sentencia no reconocida.");
    return NULL;
}

/* Cada sentencia recuerda la línea donde EMPIEZA (el depurador la necesita:
   'linea' es la del token que sigue a lo ya leído al crear el nodo). */
static Nodo *sentencia(Parser *p) {
    int inicio = actual(p)->linea;
    Nodo *n = sentencia_interna(p);
    if (n) n->linea_ini = inicio;
    return n;
}
/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
Parser *parser_crear(Token *tokens, int cantidad) {
    Parser *p = malloc(sizeof(Parser));
    p->tokens   = tokens;
    p->cantidad = cantidad;
    p->pos      = 0;
    p->bucles   = 0;
    p->tarea    = 0;
    p->enums    = p->elegires = p->enums_fuera = NULL;
    p->nenums   = p->nelegires = p->nenums_fuera = 0;
    return p;
}

/* ── Enumeraciones de otros archivos ──
   Para el aviso de 'elegir': las enumeraciones de un archivo importado se
   leen de su texto con este escáner sencillo (el lexer termina el proceso si
   ese archivo tiene un error, y aquí no se puede). */
static int ch_ident(unsigned char c) { return isalnum(c) || c == '_' || c >= 0x80; }

static const char *saltar_blancos(const char *s) {
    for (;;) {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        if (*s != '#') return s;
        while (*s && *s != '\n') s++;
    }
}

static const char *saltar_texto(const char *s) {       /* s en la comilla de apertura */
    for (s++; *s && *s != '"'; s++) if (*s == '\\' && s[1]) s++;
    return *s ? s + 1 : s;
}

/* Un trozo de expresión: lo que va entre corchetes, llaves o paréntesis, un
   texto, o una palabra o número (con '.' y '-' dentro) */
static const char *saltar_termino(const char *s) {
    s = saltar_blancos(s);
    if (*s == '"') return saltar_texto(s);
    if (*s == '[' || *s == '{' || *s == '(') {
        int prof = 0;
        while (*s) {
            if (*s == '"') { s = saltar_texto(s); continue; }
            if (*s == '[' || *s == '{' || *s == '(') prof++;
            else if (*s == ']' || *s == '}' || *s == ')') { if (--prof == 0) return s + 1; }
            s++;
        }
        return s;
    }
    if (*s == '-') s++;
    while (ch_ident((unsigned char)*s) || *s == '.') s++;
    return s;
}

void parser_conocer_enumeraciones(Parser *p, const char *texto,
                                  void (*importa)(const char *nombre, void *ctx), void *ctx) {
    const char *s = texto;
    while (*s) {
        if (*s == '#') { while (*s && *s != '\n') s++; continue; }
        if (*s == '"') { s = saltar_texto(s); continue; }
        if (!ch_ident((unsigned char)*s)) { s++; continue; }
        const char *w = s;
        while (ch_ident((unsigned char)*s)) s++;
        size_t lw = (size_t)(s - w);
        if (lw == 8 && strncmp(w, "importar", 8) == 0 && importa) {
            const char *q = saltar_blancos(s);
            if (*q == '"') {
                const char *fin = saltar_texto(q);
                size_t l = (size_t)(fin - q) - 2;
                char *nombre = malloc(l + 1);
                memcpy(nombre, q + 1, l);
                nombre[l] = '\0';
                importa(nombre, ctx);
                free(nombre);
                s = fin;
            }
            continue;
        }
        if (lw != 11 || strncmp(w, "enumeracion", 11) != 0 || (w > texto && ch_ident((unsigned char)w[-1]))) continue;
        const char *q = saltar_blancos(s), *nom = q;
        while (ch_ident((unsigned char)*q)) q++;
        size_t ln = (size_t)(q - nom);
        q = saltar_blancos(q);
        if (!ln || *q != '{') continue;
        q++;
        int cap = 16, num = 0;
        char **valores = malloc(sizeof(char*) * cap);
        for (;;) {
            q = saltar_blancos(q);
            if (*q == ',') { q++; continue; }
            if (!ch_ident((unsigned char)*q)) break;            /* '}' o algo raro: se acaba */
            const char *v = q;
            while (ch_ident((unsigned char)*q)) q++;
            if (num == cap) valores = realloc(valores, sizeof(char*) * (cap *= 2));
            valores[num] = malloc((size_t)(q - v) + 1);
            memcpy(valores[num], v, (size_t)(q - v));
            valores[num][q - v] = '\0';
            num++;
            const char *r = saltar_blancos(q);
            if (*r == '=') {                                  /* = valor: se salta */
                q = saltar_termino(r + 1);
                for (;;) {
                    const char *o = q;
                    while (*o == ' ' || *o == '\t') o++;
                    if (!strchr("+-*/%", *o) || !*o) break;
                    q = saltar_termino(o + 1);
                }
            }
        }
        Nodo *n = nodo_nuevo(NODO_ENUMERACION, NULL);
        n->enumeracion.nombre = malloc(ln + 1);
        memcpy(n->enumeracion.nombre, nom, ln);
        n->enumeracion.nombre[ln] = '\0';
        n->enumeracion.valores = valores;
        n->enumeracion.num_valores = num;
        apuntar(&p->enums, &p->nenums, n);
        apuntar(&p->enums_fuera, &p->nenums_fuera, n);
        s = q;
    }
}

void parser_destruir(Parser *p) {
    for (int i = 0; i < p->nenums_fuera; i++) {
        Nodo *n = p->enums_fuera[i];
        for (int k = 0; k < n->enumeracion.num_valores; k++) free(n->enumeracion.valores[k]);
        free(n->enumeracion.valores);
        free(n->enumeracion.nombre);
        free(n);
    }
    free(p->enums_fuera);
    free(p->enums);
    free(p->elegires);
    free(p);
}

/* Un 'elegir' sin 'otro' cuyos casos son todos de la misma enumeración
   (declarada en este archivo) y no la cubren entera: casi siempre es que se
   ha añadido un valor nuevo y se ha olvidado aquí. */
void parser_avisos(Parser *p, void (*avisar)(int linea, const char *mensaje)) {
    for (int i = 0; i < p->nelegires; i++) {
        Nodo *el = p->elegires[i];
        if (el->elegir.otro || el->elegir.num_casos == 0) continue;
        const char *nombre = NULL;
        int todos = 1;
        for (int c = 0; c < el->elegir.num_casos && todos; c++) {
            /* Estado.Quieto, o alias.Estado.Quieto si se importó 'como' */
            Nodo *v = el->elegir.valores[c];
            const char *este = NULL;
            if (v->tipo == NODO_ACCESO && v->acceso.objeto->tipo == NODO_IDENTIFICADOR)
                este = v->acceso.objeto->identificador;
            else if (v->tipo == NODO_ACCESO && v->acceso.objeto->tipo == NODO_ACCESO &&
                     v->acceso.objeto->acceso.objeto->tipo == NODO_IDENTIFICADOR)
                este = v->acceso.objeto->acceso.campo;
            todos = este && (!nombre || strcmp(nombre, este) == 0);
            if (todos) nombre = este;
        }
        Nodo *en = NULL;
        for (int k = 0; todos && k < p->nenums && !en; k++)
            if (strcmp(p->enums[k]->enumeracion.nombre, nombre) == 0) en = p->enums[k];
        if (!en) continue;
        char msg[600];
        size_t m = 0;
        int faltan = 0, ultimo = -1;
        for (int k = 0; k < en->enumeracion.num_valores; k++) {
            int esta = 0;
            for (int c = 0; c < el->elegir.num_casos && !esta; c++)
                esta = strcmp(el->elegir.valores[c]->acceso.campo, en->enumeracion.valores[k]) == 0;
            if (!esta) { faltan++; ultimo = k; }
        }
        if (!faltan) continue;
        m += (size_t)snprintf(msg + m, sizeof msg - m, "este 'elegir' no trata ");
        int puestos = 0;
        for (int k = 0; k < en->enumeracion.num_valores && m < sizeof msg - 80; k++) {
            int esta = 0;
            for (int c = 0; c < el->elegir.num_casos && !esta; c++)
                esta = strcmp(el->elegir.valores[c]->acceso.campo, en->enumeracion.valores[k]) == 0;
            if (esta) continue;
            m += (size_t)snprintf(msg + m, sizeof msg - m, "%s%s.%s",
                                  puestos == 0 ? "" : k == ultimo ? " ni " : ", ",
                                  nombre, en->enumeracion.valores[k]);
            puestos++;
        }
        snprintf(msg + m, sizeof msg - m, ": %s, o pon 'otro { }' si no hace falta.",
                 faltan == 1 ? "añádelo" : "añádelos");
        avisar(el->linea_ini ? el->linea_ini : el->linea, msg);
    }
}

Nodo *parser_parsear(Parser *p) {
    int    cap        = 64;
    Nodo **sentencias = malloc(sizeof(Nodo*) * cap);
    int    cantidad   = 0;

    while (!coincide(p, TOK_EOF)) {
        if (cantidad == cap) { cap *= 2; sentencias = realloc(sentencias, sizeof(Nodo*) * cap); }
        sentencias[cantidad++] = sentencia(p);
    }

    Nodo *programa = nodo_nuevo(NODO_BLOQUE, p);
    programa->bloque.sentencias = sentencias;
    programa->bloque.cantidad   = cantidad;
    return programa;
}

/* ─────────────────────────────────────────
   DESTRUCCIÓN DE NODOS
───────────────────────────────────────── */
void nodo_destruir(Nodo *n) {
    if (!n) return;
    switch (n->tipo) {
        case NODO_TEXTO:         free(n->texto); break;
        case NODO_IDENTIFICADOR: free(n->identificador); break;
        case NODO_BINARIO:
            nodo_destruir(n->binario.izquierda);
            nodo_destruir(n->binario.derecha);
            break;
        case NODO_UNARIO:
            nodo_destruir(n->unario.operando);
            break;
        case NODO_DECLARACION:
            free(n->declaracion.nombre);
            nodo_destruir(n->declaracion.valor);
            break;
        case NODO_ASIGNACION:
            free(n->asignacion.nombre);
            nodo_destruir(n->asignacion.valor);
            break;
        case NODO_INCREMENTO:
            free(n->incremento.nombre);
            break;
        case NODO_ESCRIBIR:
            nodo_destruir(n->escribir);
            break;
        case NODO_BLOQUE:
            for (int i = 0; i < n->bloque.cantidad; i++)
                nodo_destruir(n->bloque.sentencias[i]);
            free(n->bloque.sentencias);
            break;
        case NODO_SI:
            nodo_destruir(n->si.condicion);
            nodo_destruir(n->si.entonces);
            for (int i = 0; i < n->si.cantidad_sino_si; i++) {
                nodo_destruir(n->si.condiciones_sino_si[i]);
                nodo_destruir(n->si.bloques_sino_si[i]);
            }
            free(n->si.condiciones_sino_si);
            free(n->si.bloques_sino_si);
            nodo_destruir(n->si.sino);
            break;
        case NODO_MIENTRAS:
            nodo_destruir(n->mientras.condicion);
            nodo_destruir(n->mientras.cuerpo);
            break;
        case NODO_PARA:
            free(n->para.var_inicio);
            nodo_destruir(n->para.val_inicio);
            nodo_destruir(n->para.condicion);
            free(n->para.var_incremento);
            nodo_destruir(n->para.paso);
            nodo_destruir(n->para.cuerpo);
            break;
        case NODO_PLANTILLA:
            for (int i = 0; i < n->lista.cantidad; i++)
                nodo_destruir(n->lista.elementos[i]);
            free(n->lista.elementos);
            break;
        case NODO_DESESTRUCTURAR:
            for (int i = 0; i < n->desestructurar.num_nombres; i++)
                free(n->desestructurar.nombres[i]);
            free(n->desestructurar.nombres);
            free(n->desestructurar.nombres_i);
            nodo_destruir(n->desestructurar.valor);
            break;
        case NODO_SI_EXPR:
            nodo_destruir(n->si.condicion);
            nodo_destruir(n->si.entonces);
            nodo_destruir(n->si.sino);
            break;
        case NODO_FUNCION:
            free(n->funcion.nombre);
            for (int i = 0; i < n->funcion.num_parametros; i++) {
                free(n->funcion.parametros[i].nombre);
                if (n->funcion.parametros[i].tipo_clase)
                    free(n->funcion.parametros[i].tipo_clase);
            }
            free(n->funcion.parametros);
            nodo_destruir(n->funcion.cuerpo);
            break;
        case NODO_LLAMADA:
            free(n->llamada.nombre);
            nodo_destruir(n->llamada.funcion);
            for (int i = 0; i < n->llamada.num_argumentos; i++)
                nodo_destruir(n->llamada.argumentos[i]);
            free(n->llamada.argumentos);
            break;
        case NODO_DEVOLVER:
            nodo_destruir(n->devolver);
            break;
        default: break;
    }
    free(n);
}

/* ─────────────────────────────────────────
   DEPURACIÓN — imprimir AST
───────────────────────────────────────── */
static void indentar(int nivel) {
    for (int i = 0; i < nivel * 2; i++) printf(" ");
}

void nodo_imprimir(Nodo *n, int nivel) {
    if (!n) return;
    indentar(nivel);
    switch (n->tipo) {
        case NODO_NUMERO:
            printf("Numero(%.6g)\n", n->numero); break;
        case NODO_TEXTO:
            printf("Texto(\"%s\")\n", n->texto); break;
        case NODO_BOOLEANO:
            printf("Booleano(%s)\n", n->booleano ? "verdadero" : "falso"); break;
        case NODO_NULO:
            printf("Nulo\n"); break;
        case NODO_IDENTIFICADOR:
            printf("Identificador(%s)\n", n->identificador); break;
        case NODO_BINARIO:
            printf("Binario(%s)\n", n->binario.operador);
            nodo_imprimir(n->binario.izquierda, nivel + 1);
            nodo_imprimir(n->binario.derecha,   nivel + 1);
            break;
        case NODO_UNARIO:
            printf("Unario(%s)\n", n->unario.operador);
            nodo_imprimir(n->unario.operando, nivel + 1);
            break;
        case NODO_DECLARACION:
            printf("%s %s =\n", n->declaracion.constante ? "fijo" : "sea", n->declaracion.nombre);
            nodo_imprimir(n->declaracion.valor, nivel + 1);
            break;
        case NODO_ASIGNACION:
            printf("Asignacion(%s %s=)\n", n->asignacion.nombre, n->asignacion.op);
            nodo_imprimir(n->asignacion.valor, nivel + 1);
            break;
        case NODO_INCREMENTO:
            printf("Incremento(%s%s)\n", n->incremento.nombre, n->incremento.operador); break;
        case NODO_ESCRIBIR:
            printf("Escribir\n");
            nodo_imprimir(n->escribir, nivel + 1);
            break;
        case NODO_BLOQUE:
            printf("Bloque(%d sentencias)\n", n->bloque.cantidad);
            for (int i = 0; i < n->bloque.cantidad; i++)
                nodo_imprimir(n->bloque.sentencias[i], nivel + 1);
            break;
        case NODO_SI:
            printf("Si\n");
            indentar(nivel + 1); printf("condicion:\n");
            nodo_imprimir(n->si.condicion, nivel + 2);
            indentar(nivel + 1); printf("entonces:\n");
            nodo_imprimir(n->si.entonces, nivel + 2);
            if (n->si.sino) {
                indentar(nivel + 1); printf("sino:\n");
                nodo_imprimir(n->si.sino, nivel + 2);
            }
            break;
        case NODO_MIENTRAS:
            printf("Mientras\n");
            nodo_imprimir(n->mientras.condicion, nivel + 1);
            nodo_imprimir(n->mientras.cuerpo,    nivel + 1);
            break;
        case NODO_PARA:
            printf("Para(%s = ...; ...; %s%s)\n",
                n->para.var_inicio, n->para.var_incremento, n->para.op_incremento);
            nodo_imprimir(n->para.cuerpo, nivel + 1);
            break;
        case NODO_FUNCION:
            printf("Funcion(%s, %d params)\n", n->funcion.nombre, n->funcion.num_parametros);
            nodo_imprimir(n->funcion.cuerpo, nivel + 1);
            break;
        case NODO_LLAMADA:
            printf("Llamada(%s, %d args)\n", n->llamada.nombre, n->llamada.num_argumentos);
            break;
        case NODO_DEVOLVER:
            printf("Devolver\n");
            nodo_imprimir(n->devolver, nivel + 1);
            break;
        case NODO_ROMPER:    printf("Romper\n"); break;
        case NODO_DESESTRUCTURAR:
            printf("Desestructurar(%d nombres%s)\n", n->desestructurar.num_nombres,
                   n->desestructurar.de_diccionario ? ", por clave" : "");
            nodo_imprimir(n->desestructurar.valor, nivel + 1);
            break;
        case NODO_SI_EXPR:
            printf("SiValor\n");
            nodo_imprimir(n->si.condicion, nivel + 1);
            nodo_imprimir(n->si.entonces, nivel + 1);
            nodo_imprimir(n->si.sino, nivel + 1);
            break;
        case NODO_CONTINUAR: printf("Continuar\n"); break;
        case NODO_PLANTILLA:
            printf("Plantilla(%d trozos)\n", n->lista.cantidad);
            for (int i = 0; i < n->lista.cantidad; i++)
                nodo_imprimir(n->lista.elementos[i], nivel + 1);
            break;
        default:
            printf("Nodo desconocido\n"); break;
    }
}
