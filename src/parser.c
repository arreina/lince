/*
 * 🐆 Lince — parser.c
 * Construye el Árbol Sintáctico (AST) a partir de los tokens.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
            "\n❌ Error en línea %d:\n"
            "   Esperaba %s pero encontré '%s'.\n"
            "   Revisa la sintaxis en esa línea.\n\n",
            t->linea, esperaba,
            t->valor ? t->valor : "fin de archivo");
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
        "\n❌ Error en línea %d:\n"
        "   %s\n"
        "   Encontré: '%s'\n\n",
        t->linea, mensaje,
        t->valor ? t->valor : "fin de archivo");
    exit(1);
}

/* ─────────────────────────────────────────
   CREACIÓN DE NODOS
───────────────────────────────────────── */
static Nodo *nodo_nuevo(TipoNodo tipo, Parser *p) {
    Nodo *n = calloc(1, sizeof(Nodo));
    n->tipo  = tipo;
    n->linea = p ? actual(p)->linea : 0;
    return n;
}

/* ─────────────────────────────────────────
   DECLARACIONES ANTICIPADAS
───────────────────────────────────────── */
static Nodo *sentencia(Parser *p);
static Nodo *expresion(Parser *p);
static Nodo *bloque(Parser *p);
static Nodo *postfijo(Parser *p, Nodo *base);
static TipoDato parsear_tipo(Parser *p, char **tipo_clase_out);
static Nodo *sent_elegir(Parser *p);
static Nodo *sent_enumeracion(Parser *p);
static Nodo *sent_interfaz(Parser *p);
static Nodo *sent_generador(Parser *p);

/* ─────────────────────────────────────────
   POSTFIJO: obj[i], obj.metodo(), obj.campo
───────────────────────────────────────── */
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

    /* Lambda: funcion(params): tipo { cuerpo } */
    if (coincide(p, TOK_FUNCION)) {
        consumir(p, TOK_FUNCION, "'funcion'");
        consumir(p, TOK_PAREN_IZQ, "'('");

        Parametro *params     = malloc(sizeof(Parametro) * 64);
        int        num_params = 0;

        if (!coincide(p, TOK_PAREN_DER)) {
            do {
                if (num_params > 0) consumir(p, TOK_COMA, "','");
                int por_ref = 0;
                if (coincide(p, TOK_VAL))      { consumir_cualquiera(p); por_ref = 0; }
                else if (coincide(p, TOK_REF)) { consumir_cualquiera(p); por_ref = 1; }
                else {
                    fprintf(stderr,
                        "\n❌ Error en línea %d:\n"
                        "   Parámetro de lambda necesita 'val' o 'ref'.\n\n",
                        actual(p)->linea);
                    exit(1);
                }
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
        char *tipo_ret_clase = NULL;
        TipoDato tipo_retorno = parsear_tipo(p, &tipo_ret_clase);
        Nodo *cuerpo = bloque(p);

        Nodo *n = nodo_nuevo(NODO_LAMBDA, p);
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

        if (coincide(p, TOK_PAREN_IZQ)) {
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

static Nodo *comparacion(Parser *p) {
    Nodo *izq = suma(p);
    while (coincide(p, TOK_MENOR) || coincide(p, TOK_MAYOR) ||
           coincide(p, TOK_MENOR_IGUAL) || coincide(p, TOK_MAYOR_IGUAL)) {
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

static Nodo *logica_y(Parser *p) {
    Nodo *izq = igualdad(p);
    while (coincide(p, TOK_Y)) {
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
    while (coincide(p, TOK_O)) {
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
    Nodo **sentencias = malloc(sizeof(Nodo*) * 256);
    int cantidad = 0;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        sentencias[cantidad++] = sentencia(p);
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_BLOQUE, p);
    n->bloque.sentencias = sentencias;
    n->bloque.cantidad   = cantidad;
    return n;
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

    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de variable");
    consumir(p, TOK_IGUAL, "'='");
    Nodo *valor = expresion(p);

    /* Validar que el tipo declarado coincide con el valor */
    if (es_lista && valor->tipo != NODO_LISTA) {
        fprintf(stderr,
            "\n❌ Error en línea %d:\n"
            "   Declaraste '%s' como lista pero el valor no es una lista.\n"
            "   Una lista se escribe así: [1, 2, 3]\n\n",
            nombre->linea, nombre->valor);
        exit(1);
    }
    if (es_diccionario && valor->tipo != NODO_DICCIONARIO) {
        fprintf(stderr,
            "\n❌ Error en línea %d:\n"
            "   Declaraste '%s' como diccionario pero el valor no es un diccionario.\n"
            "   Un diccionario se escribe así: {\"clave\": valor}\n\n",
            nombre->linea, nombre->valor);
        exit(1);
    }

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
    consumir(p, TOK_IGUAL, "'='");
    Nodo *valor = expresion(p);

    Nodo *n = nodo_nuevo(NODO_ASIGNACION, p);
    n->asignacion.nombre = strdup(nombre->valor);
    n->asignacion.valor  = valor;
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
    Nodo *cuerpo    = bloque(p);

    Nodo *n = nodo_nuevo(NODO_MIENTRAS, p);
    n->mientras.condicion = condicion;
    n->mientras.cuerpo    = cuerpo;
    return n;
}

static Nodo *sent_hacer(Parser *p) {
    consumir(p, TOK_HACER, "'hacer'");
    Nodo *cuerpo = bloque(p);
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
                "\n❌ Error en línea %d:\n"
                "   Se esperaba 'en' después del nombre de variable.\n"
                "   Ejemplo: para cada numero n en lista\n\n",
                en->linea);
            exit(1);
        }

        /* colección */
        Nodo *coleccion = expresion(p);
        Nodo *cuerpo    = bloque(p);

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
    if (coincide(p, TOK_MAS_MAS) || coincide(p, TOK_MENOS_MENOS)) {
        op_inc = consumir_cualquiera(p);
    } else {
        error_sintaxis(p, "Se esperaba '++' o '--' en el incremento del bucle 'para'.");
        return NULL;
    }
    consumir(p, TOK_PAREN_DER, "')'");
    Nodo *cuerpo = bloque(p);

    Nodo *n = nodo_nuevo(NODO_PARA, p);
    n->para.var_inicio     = strdup(var->valor);
    n->para.val_inicio     = val_inicio;
    n->para.condicion      = condicion;
    n->para.var_incremento = strdup(var_inc->valor);
    strncpy(n->para.op_incremento, op_inc->valor, 3);
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
        if (tipo_clase_out) *tipo_clase_out = strdup(t->valor);
        return TIPO_CLASE;
    }
    error_sintaxis(p, "Se esperaba un tipo (numero, texto, logico, lista, diccionario, nulo, funcion, o nombre de clase).");
    return TIPO_CUALQUIERA;
}

static Nodo *sent_funcion(Parser *p) {
    consumir(p, TOK_FUNCION, "'funcion'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de función");
    consumir(p, TOK_PAREN_IZQ, "'('");

    Parametro *params    = malloc(sizeof(Parametro) * 64);
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
            } else {
                fprintf(stderr,
                    "\n❌ Error en línea %d:\n"
                    "   Cada parámetro debe indicar si es 'val' o 'ref'.\n"
                    "   Ejemplo: funcion f(val numero x, ref lista nums): nulo\n\n",
                    actual(p)->linea);
                exit(1);
            }

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

    /* tipo de retorno: ): tipo */
    consumir(p, TOK_DOS_PUNTOS, "':' seguido del tipo de retorno");
    char *tipo_ret_clase = NULL;
    TipoDato tipo_retorno = parsear_tipo(p, &tipo_ret_clase);

    Nodo *cuerpo = bloque(p);

    Nodo *n = nodo_nuevo(NODO_FUNCION, p);
    n->funcion.nombre             = strdup(nombre->valor);
    n->funcion.parametros         = params;
    n->funcion.num_parametros     = num_params;
    n->funcion.tipo_retorno       = tipo_retorno;
    n->funcion.tipo_retorno_clase = tipo_ret_clase;
    n->funcion.cuerpo             = cuerpo;
    return n;
}

static Nodo *sent_clase(Parser *p) {
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

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        if (!coincide(p, TOK_FUNCION)) {
            fprintf(stderr,
                "\n❌ Error en línea %d:\n"
                "   Dentro de una clase solo puede haber funciones.\n"
                "   Encontré '%s' en su lugar.\n\n",
                actual(p)->linea, actual(p)->valor ? actual(p)->valor : "?");
            exit(1);
        }
        metodos[num_metodos++] = sent_funcion(p);
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_CLASE, p);
    n->clase.nombre         = strdup(nombre->valor);
    n->clase.padre          = padre;
    n->clase.interfaces     = interfaces;
    n->clase.num_interfaces = num_interfaces;
    n->clase.metodos        = metodos;
    n->clase.num_metodos    = num_metodos;
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

    Nodo **valores  = malloc(sizeof(Nodo*) * 64);
    Nodo **cuerpos  = malloc(sizeof(Nodo*) * 64);
    int    num      = 0;
    Nodo  *otro     = NULL;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        if (coincide(p, TOK_CASO)) {
            consumir(p, TOK_CASO, "'caso'");
            valores[num] = expresion(p);
            cuerpos[num] = bloque(p);
            num++;
        } else if (coincide(p, TOK_OTRO)) {
            consumir(p, TOK_OTRO, "'otro'");
            otro = bloque(p);
        } else {
            fprintf(stderr,
                "\n❌ Error en línea %d:\n"
                "   Dentro de 'elegir' solo puede haber 'caso' u 'otro'.\n\n",
                actual(p)->linea);
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
    return n;
}

static Nodo *sent_enumeracion(Parser *p) {
    consumir(p, TOK_ENUMERACION, "'enumeracion'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de la enumeración");
    consumir(p, TOK_LLAVE_IZQ, "'{'");

    char **valores    = malloc(sizeof(char*) * 64);
    int    num_valores = 0;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        Token *val = consumir(p, TOK_IDENTIFICADOR, "nombre del valor");
        valores[num_valores++] = strdup(val->valor);
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_ENUMERACION, p);
    n->enumeracion.nombre     = strdup(nombre->valor);
    n->enumeracion.valores    = valores;
    n->enumeracion.num_valores = num_valores;
    return n;
}

static Nodo *sent_interfaz(Parser *p) {
    consumir(p, TOK_INTERFAZ, "'interfaz'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre de la interfaz");
    consumir(p, TOK_LLAVE_IZQ, "'{'");

    char  **metodos      = malloc(sizeof(char*)  * 64);
    char  **tipos_ret    = malloc(sizeof(char*)  * 64);
    int     num_metodos  = 0;

    while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
        /* Solo firmas: funcion nombre(params): tipo */
        consumir(p, TOK_FUNCION, "'funcion'");
        Token *nom_met = consumir(p, TOK_IDENTIFICADOR, "nombre del método");
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
        num_metodos++;
    }
    consumir(p, TOK_LLAVE_DER, "'}'");

    Nodo *n = nodo_nuevo(NODO_INTERFAZ, p);
    n->interfaz.nombre       = strdup(nombre->valor);
    n->interfaz.metodos      = metodos;
    n->interfaz.tipos_retorno = tipos_ret;
    n->interfaz.num_metodos  = num_metodos;
    return n;
}

static Nodo *sent_generador(Parser *p) {
    consumir(p, TOK_GENERADOR, "'generador'");
    Token *nombre = consumir(p, TOK_IDENTIFICADOR, "nombre del generador");
    consumir(p, TOK_PAREN_IZQ, "'('");

    Parametro *params     = malloc(sizeof(Parametro) * 64);
    int        num_params = 0;

    if (!coincide(p, TOK_PAREN_DER)) {
        do {
            if (num_params > 0) consumir(p, TOK_COMA, "','");
            int por_ref = 0;
            if (coincide(p, TOK_VAL))      { consumir_cualquiera(p); por_ref = 0; }
            else if (coincide(p, TOK_REF)) { consumir_cualquiera(p); por_ref = 1; }
            else {
                fprintf(stderr,
                    "\n❌ Error en línea %d:\n"
                    "   Cada parámetro debe indicar si es 'val' o 'ref'.\n\n",
                    actual(p)->linea);
                exit(1);
            }
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
    Nodo *cuerpo = bloque(p);

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
    n->importar = strdup(nombre->valor);
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

static Nodo *sentencia(Parser *p) {
    if (coincide(p, TOK_SEA))      return sent_declaracion(p, 0);
    if (coincide(p, TOK_FIJO))     return sent_declaracion(p, 1);
    if (coincide(p, TOK_ESCRIBIR)) return sent_escribir(p);
    if (coincide(p, TOK_SI))       return sent_si(p);
    if (coincide(p, TOK_MIENTRAS)) return sent_mientras(p);
    if (coincide(p, TOK_HACER))    return sent_hacer(p);
    if (coincide(p, TOK_PARA))     return sent_para(p);
    if (coincide(p, TOK_FUNCION))  return sent_funcion(p);
    if (coincide(p, TOK_DEVOLVER)) return sent_devolver(p);
    if (coincide(p, TOK_CLASE))    return sent_clase(p);
    if (coincide(p, TOK_ELEGIR)) {
        consumir(p, TOK_ELEGIR, "'elegir'");
        Nodo *sujeto = expresion(p);
        consumir(p, TOK_LLAVE_IZQ, "'{'");

        Nodo **valores   = malloc(sizeof(Nodo*) * 64);
        Nodo **cuerpos   = malloc(sizeof(Nodo*) * 64);
        int    num_casos = 0;
        Nodo  *otro      = NULL;

        while (!coincide(p, TOK_LLAVE_DER) && !coincide(p, TOK_EOF)) {
            if (coincide(p, TOK_CASO)) {
                consumir(p, TOK_CASO, "'caso'");
                valores[num_casos]   = expresion(p);
                cuerpos[num_casos++] = bloque(p);
            } else if (coincide(p, TOK_OTRO)) {
                consumir(p, TOK_OTRO, "'otro'");
                otro = bloque(p);
            } else {
                fprintf(stderr,
                    "\n❌ Error en línea %d:\n"
                    "   Dentro de 'elegir' solo puede haber 'caso' u 'otro'.\n\n",
                    actual(p)->linea);
                exit(1);
            }
        }
        consumir(p, TOK_LLAVE_DER, "'}'");

        Nodo *n = nodo_nuevo(NODO_ELEGIR, p);
        n->elegir.sujeto    = sujeto;
        n->elegir.valores   = valores;
        n->elegir.cuerpos   = cuerpos;
        n->elegir.num_casos = num_casos;
        n->elegir.otro      = otro;
        return n;
    }

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
        /* ¿Es asignación o llamada? */
        if (coincide(p, TOK_IGUAL)) {
            consumir(p, TOK_IGUAL, "'='");
            Nodo *valor = expresion(p);
            Nodo *obj = nodo_nuevo(NODO_ESTO, p);
            Nodo *n = nodo_nuevo(NODO_ASIGNACION_CAMPO, p);
            n->asignacion_campo.objeto = obj;
            n->asignacion_campo.campo  = strdup(campo->valor);
            n->asignacion_campo.valor  = valor;
            return n;
        }
        /* Es acceso a campo o método como sentencia */
        Nodo *obj = nodo_nuevo(NODO_ESTO, p);
        Nodo *acc = nodo_nuevo(NODO_ACCESO, p);
        acc->acceso.objeto = obj;
        acc->acceso.campo  = strdup(campo->valor);
        return postfijo(p, acc);
    }

    /* Asignación, incremento, o asignación a índice */
    if (coincide(p, TOK_IDENTIFICADOR)) {
        TipoToken sig = ver_siguiente(p)->tipo;
        if (sig == TOK_IGUAL)                              return sent_asignacion(p);
        if (sig == TOK_MAS_MAS || sig == TOK_MENOS_MENOS) {
            Token *var = consumir_cualquiera(p);
            Token *op  = consumir_cualquiera(p);
            Nodo *n = nodo_nuevo(NODO_INCREMENTO, p);
            n->incremento.nombre = strdup(var->valor);
            strncpy(n->incremento.operador, op->valor, 3);
            return n;
        }
        /* lista[i] = valor */
        if (sig == TOK_CORCHETE_IZQ) {
            Token *var = consumir_cualquiera(p);
            consumir(p, TOK_CORCHETE_IZQ, "'['");
            Nodo *indice = expresion(p);
            consumir(p, TOK_CORCHETE_DER, "']'");
            consumir(p, TOK_IGUAL, "'='");
            Nodo *valor = expresion(p);
            Nodo *obj = nodo_nuevo(NODO_IDENTIFICADOR, p);
            obj->identificador = strdup(var->valor);
            Nodo *n = nodo_nuevo(NODO_ASIGNACION_INDICE, p);
            n->asignacion_indice.objeto = obj;
            n->asignacion_indice.indice = indice;
            n->asignacion_indice.valor  = valor;
            return n;
        }
        /* obj.campo = valor  o  obj.metodo() como sentencia */
        if (sig == TOK_PUNTO) {
            Token *var = consumir_cualquiera(p); /* obj */
            consumir(p, TOK_PUNTO, "'.'");
            /* Aceptar keyword como nombre de campo */
            Token *campo;
            if (coincide(p, TOK_IDENTIFICADOR) || coincide(p, TOK_ESCRIBIR) ||
                coincide(p, TOK_TIPO_TEXTO) || coincide(p, TOK_TIPO_NUMERO) ||
                coincide(p, TOK_LISTA) || coincide(p, TOK_DICCIONARIO)) {
                campo = consumir_cualquiera(p);
            } else {
                campo = consumir(p, TOK_IDENTIFICADOR, "nombre de campo o método");
            }
            if (coincide(p, TOK_IGUAL)) {
                consumir(p, TOK_IGUAL, "'='");
                Nodo *valor = expresion(p);
                Nodo *obj = nodo_nuevo(NODO_IDENTIFICADOR, p);
                obj->identificador = strdup(var->valor);
                Nodo *n = nodo_nuevo(NODO_ASIGNACION_CAMPO, p);
                n->asignacion_campo.objeto = obj;
                n->asignacion_campo.campo  = strdup(campo->valor);
                n->asignacion_campo.valor  = valor;
                return n;
            }
            /* Es llamada a método u otro postfijo */
            Nodo *obj = nodo_nuevo(NODO_IDENTIFICADOR, p);
            obj->identificador = strdup(var->valor);
            Nodo *acc = nodo_nuevo(NODO_ACCESO, p);
            acc->acceso.objeto = obj;
            acc->acceso.campo  = strdup(campo->valor);
            return postfijo(p, acc);
        }
    }

    /* Fallback: cualquier expresión puede ser sentencia (llamadas a métodos, etc.) */
    if (coincide(p, TOK_IDENTIFICADOR)) {
        Nodo *expr = expresion(p);
        return expr;
    }

    error_sintaxis(p, "Sentencia no reconocida.");
    return NULL;
}
/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
Parser *parser_crear(Token *tokens, int cantidad) {
    Parser *p = malloc(sizeof(Parser));
    p->tokens   = tokens;
    p->cantidad = cantidad;
    p->pos      = 0;
    return p;
}

void parser_destruir(Parser *p) {
    free(p);
}

Nodo *parser_parsear(Parser *p) {
    Nodo **sentencias = malloc(sizeof(Nodo*) * 1024);
    int    cantidad   = 0;

    while (!coincide(p, TOK_EOF)) {
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
            nodo_destruir(n->para.cuerpo);
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
            printf("Asignacion(%s)\n", n->asignacion.nombre);
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
        default:
            printf("Nodo desconocido\n"); break;
    }
}
