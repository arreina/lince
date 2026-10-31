/*
 * 🐆 Lince — formatear.c
 * 'lince formatear': el estilo único de Lince (ver formatear.h).
 *
 * Trabaja sobre los tokens del lexer, que saben dónde empiezan y acaban en el
 * código: lo que hay entre dos tokens (espacios, saltos y comentarios) se
 * vuelve a escribir, y cada token se copia tal cual. Un texto con huecos
 * ("Vidas: {v}") se copia entero, de comilla a comilla.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "formatear.h"

typedef struct { char *d; size_t n, cap; } Salida;

static void poner_n(Salida *s, const char *t, size_t n) {
    if (s->n + n + 1 > s->cap) {
        s->cap = (s->n + n + 1) * 2;
        s->d   = realloc(s->d, s->cap);
    }
    memcpy(s->d + s->n, t, n);
    s->n += n;
    s->d[s->n] = '\0';
}
static void poner(Salida *s, const char *t) { poner_n(s, t, strlen(t)); }

/* Quita los espacios del final de la línea que se está escribiendo */
static void recortar(Salida *s) {
    while (s->n > 0 && (s->d[s->n - 1] == ' ' || s->d[s->n - 1] == '\t')) s->n--;
    if (s->d) s->d[s->n] = '\0';
}

static void sangrar(Salida *s, int nivel) {
    for (int i = 0; i < nivel; i++) poner(s, "    ");
}

/* ── Clases de tokens ── */
static int es_abre(TipoToken t)   { return t == TOK_PAREN_IZQ || t == TOK_CORCHETE_IZQ || t == TOK_LLAVE_IZQ; }
static int es_cierra(TipoToken t) { return t == TOK_PAREN_DER || t == TOK_CORCHETE_DER || t == TOK_LLAVE_DER; }

static int es_binario(TipoToken t) {
    switch (t) {
        case TOK_MAS: case TOK_MENOS: case TOK_ASTERISCO: case TOK_BARRA: case TOK_MODULO:
        case TOK_IGUAL: case TOK_IGUAL_IGUAL: case TOK_DIFERENTE: case TOK_MENOR: case TOK_MAYOR:
        case TOK_MENOR_IGUAL: case TOK_MAYOR_IGUAL: case TOK_MAS_IGUAL: case TOK_MENOS_IGUAL:
        case TOK_POR_IGUAL: case TOK_ENTRE_IGUAL: case TOK_MODULO_IGUAL: case TOK_NULO_O:
            return 1;
        default: return 0;
    }
}

/* Tras esto, un '-' es una resta y un '(' o '[' es una llamada o un índice */
static int acaba_valor(TipoToken t) {
    switch (t) {
        case TOK_IDENTIFICADOR: case TOK_NUMERO: case TOK_TEXTO: case TOK_PLANTILLA_FIN:
        case TOK_PAREN_DER: case TOK_CORCHETE_DER: case TOK_VERDADERO: case TOK_FALSO:
        case TOK_NULO: case TOK_MI: case TOK_PADRE: case TOK_MAS_MAS: case TOK_MENOS_MENOS:
            return 1;
        default: return 0;
    }
}

/* 'y', 'o', 'en', 'es' y 'como' son nombres para el lexer (alguien puede
   llamar 'y' a una variable): tras ellas, un '(' o un '-' se dejan como estaban */
static int es_dudosa(const Token *t) {
    static const char *d[] = { "y", "o", "en", "es", "como", NULL };
    if (t->tipo != TOK_IDENTIFICADOR) return 0;
    for (int i = 0; d[i]; i++) if (strcmp(t->valor, d[i]) == 0) return 1;
    return 0;
}

static int es_y_o(const Token *t) {
    return t && t->tipo == TOK_IDENTIFICADOR && (strcmp(t->valor, "y") == 0 || strcmp(t->valor, "o") == 0);
}

/* Tras un 'y' a principio de línea, ¿empieza otra sentencia (y = 3, y.x…)? */
static int empieza_sentencia(TipoToken t) {
    switch (t) {
        case TOK_IGUAL: case TOK_MAS_IGUAL: case TOK_MENOS_IGUAL: case TOK_POR_IGUAL: case TOK_ENTRE_IGUAL:
        case TOK_MODULO_IGUAL: case TOK_MAS_MAS: case TOK_MENOS_MENOS: case TOK_PUNTO:
        case TOK_PUNTO_OPCIONAL: case TOK_CORCHETE_IZQ: case TOK_PAREN_IZQ:
            return 1;
        default: return 0;
    }
}

/* Una palabra (clave o nombre): entre dos palabras va un espacio */
static int es_palabra(const Token *t) {
    return t->valor && (t->valor[0] == '_' || ((t->valor[0] | 32) >= 'a' && (t->valor[0] | 32) <= 'z')) &&
           t->tipo != TOK_TEXTO;
}

/* Una línea que acaba así sigue en la siguiente */
static int deja_a_medias(TipoToken t) {
    return es_binario(t) || t == TOK_PUNTO ||
           t == TOK_PUNTO_OPCIONAL || t == TOK_Y || t == TOK_O;
}

/* Una línea que empieza así continúa la anterior */
static int continua(TipoToken t) {
    return t == TOK_PUNTO || t == TOK_PUNTO_OPCIONAL || t == TOK_Y || t == TOK_O ||
           (es_binario(t) && t != TOK_IGUAL && t != TOK_MAS_IGUAL && t != TOK_MENOS_IGUAL &&
            t != TOK_POR_IGUAL && t != TOK_ENTRE_IGUAL && t != TOK_MODULO_IGUAL);
}

/* ¿Cuántos espacios entre 'a' y 'b', en la misma línea? 'habia' es lo que
   había escrito; 'a_unario' dice si 'a' es un signo (-x) y no una resta. */
static int espacios(const Token *a, const Token *b, int a_unario, int b_unario, int habia,
                    int a_operador, int llave_bloque) {
    TipoToken ta = a->tipo, tb = b->tipo;
    int conserva = habia > 0;              /* lo dudoso: 0 si no había, 1 si había */
    if (ta == TOK_LLAVE_IZQ && tb == TOK_LLAVE_DER) return 0;                    /* {} */
    if (tb == TOK_COMA || tb == TOK_PUNTO_COMA || tb == TOK_PAREN_DER || tb == TOK_CORCHETE_DER ||
        tb == TOK_PUNTO || tb == TOK_PUNTO_OPCIONAL || tb == TOK_DOS_PUNTOS)
        return 0;
    if (ta == TOK_PAREN_IZQ || ta == TOK_CORCHETE_IZQ || ta == TOK_PUNTO || ta == TOK_PUNTO_OPCIONAL)
        return 0;
    /* { escribir(x) } pero {"a": 1} */
    if (ta == TOK_LLAVE_IZQ || tb == TOK_LLAVE_DER) return llave_bloque;
    if (a_operador && (tb == TOK_PAREN_IZQ || tb == TOK_CORCHETE_IZQ)) return 1;  /* x y (…) */
    if (ta == TOK_NO && tb == TOK_PAREN_IZQ) return 1;                            /* no (…) */
    if (ta == TOK_COMA || ta == TOK_PUNTO_COMA || ta == TOK_DOS_PUNTOS) return 1;
    if (es_dudosa(a) && (tb == TOK_PAREN_IZQ || tb == TOK_CORCHETE_IZQ)) return conserva;
    if ((tb == TOK_MAS_MAS || tb == TOK_MENOS_MENOS) && acaba_valor(ta)) return 0;   /* i++ */
    if ((ta == TOK_MAS_MAS || ta == TOK_MENOS_MENOS) && !a_unario) return conserva;
    if (a_unario) return 0;                                                       /* -x, ++i */
    if (tb == TOK_PAREN_IZQ) {
        if (acaba_valor(ta) || ta == TOK_FUNCION || ta == TOK_GENERADOR || ta == TOK_ESCRIBIR) return 0;
        if (es_binario(ta)) return 1;
        return conserva;                                                          /* si (x), tarea(…) */
    }
    if (tb == TOK_CORCHETE_IZQ && acaba_valor(ta)) return 0;                      /* l[0] */
    if (es_binario(tb) && !b_unario) return 1;
    if (es_binario(ta)) return 1;
    if (b_unario && (ta == TOK_CORCHETE_DER || ta == TOK_PAREN_DER)) return 1;
    if (tb == TOK_MAS_MAS || tb == TOK_MENOS_MENOS) return 1;                     /* devolver ++i */
    if (tb == TOK_LLAVE_IZQ) return 1;
    if (es_palabra(a) || es_palabra(b) || acaba_valor(ta)) return 1;
    return conserva;
}

/* ¿Escriben los dos el mismo programa? Mismos tokens, y los que iban en la
   misma línea siguen juntos (el parser mira las líneas). */
static int mismo_programa(const Token *a, int na, const Token *b, int nb) {
    if (na != nb) return 0;
    for (int i = 0; i < na; i++) {
        if (a[i].tipo != b[i].tipo) return 0;
        if ((a[i].valor == NULL) != (b[i].valor == NULL)) return 0;
        if (a[i].valor && strcmp(a[i].valor, b[i].valor) != 0) return 0;
        if (i > 0 && (a[i].linea == a[i - 1].linea) != (b[i].linea == b[i - 1].linea)) return 0;
    }
    return 1;
}

typedef struct { int nivel, linea, bloque; } Abierto;   /* la sangría de lo de dentro y la de su línea; ¿es un bloque? */

/* ¿Abre un bloque (si x {, clase P {, sino {) y no un diccionario (= {, ({)? */
static int abre_bloque(const Token *previo) {
    if (!previo) return 0;
    switch (previo->tipo) {
        case TOK_SINO: case TOK_HACER: case TOK_INTENTAR: case TOK_FINALMENTE: case TOK_OTRO:
        case TOK_TIPO_NUMERO: case TOK_TIPO_TEXTO: case TOK_TIPO_LOGICO: case TOK_TIPO_LISTA:
        case TOK_TIPO_DICCIONARIO: case TOK_TIPO_NULO: case TOK_FUNCION: case TOK_LISTA: case TOK_DICCIONARIO:
            return 1;
        default:
            return acaba_valor(previo->tipo) && !es_dudosa(previo);
    }
}

char *formatear_codigo(const char *codigo) {
    Lexer *lx = lexer_crear(codigo);
    int n = 0;
    Token *tk = lexer_tokenizar(lx, &n);

    Salida s = {0};
    poner(&s, "");
    Abierto *pila = malloc(sizeof(Abierto) * (size_t)(n + 1));
    int alto = 0;
    int nivel_linea = 0;          /* la sangría de la línea de código que se escribe */
    int base = 0;                 /* la misma, sin lo que suma por continuar la anterior */
    int nivel_actual = 0;         /* baja si la línea cierra algo abierto más arriba */
    int primero = 1;              /* aún no se ha escrito nada */
    const Token *previo = NULL, *antes = NULL;
    int previo_unario = 0;
    size_t desde = 0;             /* por dónde va lo que hay entre tokens */

    for (int i = 0; i < n; i++) {
        const Token *t = &tk[i];
        size_t hasta = t->tipo == TOK_EOF ? strlen(codigo) : (size_t)t->desde;

        /* ── Lo de antes del token: espacios, saltos y comentarios ── */
        int saltos = 0, blancos = 0;
        for (size_t k = desde; k < hasta; ) {
            char c = codigo[k];
            if (c == ' ' || c == '\t') { blancos++; k++; continue; }
            if (c == '\r') { k++; continue; }
            if (c == '\n') { saltos++; blancos = 0; k++; continue; }
            if (c != '#') { k++; continue; }                     /* no pasa: el lexer solo salta esto */
            size_t fin = k;
            while (fin < hasta && codigo[fin] != '\n') fin++;
            size_t f2 = fin;
            while (f2 > k && (codigo[f2 - 1] == ' ' || codigo[f2 - 1] == '\t' || codigo[f2 - 1] == '\r')) f2--;
            if (saltos == 0 && !primero) {
                /* comentario al final de una línea con código: con su hueco */
                for (int b = 0; b < (blancos > 0 ? blancos : 1); b++) poner(&s, " ");
            } else {
                if (!primero) {
                    recortar(&s);
                    for (int b = 0; b < (saltos > 3 ? 3 : saltos); b++) poner(&s, "\n");
                }
                sangrar(&s, alto ? pila[alto - 1].nivel : 0);
            }
            poner_n(&s, codigo + k, f2 - k);
            primero = 0;
            saltos = blancos = 0;
            k = fin;
        }
        if (t->tipo == TOK_EOF) break;

        /* ¿Es un signo (-x) o un ++ delante? */
        int unario = (t->tipo == TOK_MENOS || t->tipo == TOK_MAS || t->tipo == TOK_MAS_MAS ||
                      t->tipo == TOK_MENOS_MENOS) && !(previo && acaba_valor(previo->tipo) &&
                      !((previo->tipo == TOK_MAS_MAS || previo->tipo == TOK_MENOS_MENOS) && previo_unario));
        if ((t->tipo == TOK_MENOS || t->tipo == TOK_MAS) && previo && es_dudosa(previo) && i + 1 < n)
            /* "a y -b" o "y - 1": como estaba escrito */
            unario = hasta > desde && tk[i + 1].desde == t->hasta;

        /* ── El token: en una línea nueva, con su sangría, o tras su espacio ── */
        if (primero || saltos > 0) {
            if (!primero) {
                recortar(&s);
                int sl = saltos > 0 ? saltos : 1;
                for (int b = 0; b < (sl > 3 ? 3 : sl); b++) poner(&s, "\n");
            }
            if (es_cierra(t->tipo) && alto > 0) base = nivel_linea = pila[alto - 1].linea;
            else {
                base = nivel_linea = alto ? pila[alto - 1].nivel : 0;
                /* ¿sigue la sentencia de la línea anterior? */
                int sigue = 0;
                if (previo && deja_a_medias(previo->tipo) && !es_dudosa(previo)) sigue = 1;
                else if (es_y_o(previo) && antes && acaba_valor(antes->tipo)) sigue = 1;
                else if (previo && acaba_valor(previo->tipo) && !unario && continua(t->tipo)) sigue = 1;
                else if (previo && acaba_valor(previo->tipo) && es_y_o(t) && i + 1 < n &&
                         tk[i + 1].linea == t->linea && !empieza_sentencia(tk[i + 1].tipo)) sigue = 1;
                nivel_linea += sigue;
            }
            if (nivel_linea < 0) nivel_linea = base = 0;
            nivel_actual = nivel_linea;
            sangrar(&s, nivel_linea);
        } else if (previo) {
            int habia = (int)(hasta - desde);
            int bloque = t->tipo == TOK_LLAVE_IZQ ? abre_bloque(previo)
                       : (t->tipo == TOK_LLAVE_DER && alto > 0) ? pila[alto - 1].bloque : 0;
            if (previo->tipo == TOK_LLAVE_IZQ && alto > 0) bloque = pila[alto - 1].bloque;
            /* 'o' / 'y' son el operador entre dos valores; pero al principio de
               una línea y seguidos de [, ., =... empiezan otra sentencia: o[k] = 1 */
            int operador = es_dudosa(previo) && antes && acaba_valor(antes->tipo) &&
                           !(antes->linea != previo->linea && empieza_sentencia(t->tipo));
            int e = espacios(previo, t, previo_unario, unario, habia, operador, bloque);
            for (int b = 0; b < e; b++) poner(&s, " ");
        }

        /* Un texto con huecos se copia entero, hasta su comilla final */
        int j = i;
        if (t->tipo == TOK_PLANTILLA_INICIO) {
            int prof = 0;
            for (; j < n; j++) {
                if (tk[j].tipo == TOK_PLANTILLA_INICIO) prof++;
                else if (tk[j].tipo == TOK_PLANTILLA_FIN && --prof == 0) break;
            }
            if (j >= n) j = n - 1;
        }
        poner_n(&s, codigo + t->desde, (size_t)(tk[j].hasta - t->desde));
        desde = (size_t)tk[j].hasta;

        if (es_abre(t->tipo)) {
            /* lo de dentro, un nivel más que la línea donde se abre; el cuerpo
               de un bloque, uno más que donde empieza su sentencia (aunque
               ocupe varias líneas: 'funcion f(a,\n b) {' o 'si a y\n b {') */
            int primero_de_linea = saltos > 0 || !previo;
            /* los parámetros de 'funcion f(…' que siguen en otra línea, dos
               niveles más: así no se confunden con el cuerpo */
            int cabecera = t->tipo == TOK_PAREN_IZQ && previo && previo->tipo == TOK_IDENTIFICADOR && antes &&
                           (antes->tipo == TOK_FUNCION || antes->tipo == TOK_GENERADOR ||
                            (antes->tipo == TOK_IDENTIFICADOR && (!strcmp(antes->valor, "tarea") ||
                             !strcmp(antes->valor, "obtener") || !strcmp(antes->valor, "fijar"))));
            pila[alto].bloque = t->tipo == TOK_LLAVE_IZQ && abre_bloque(previo);
            pila[alto].linea = t->tipo == TOK_LLAVE_IZQ && !primero_de_linea ? base : nivel_actual;
            pila[alto++].nivel = (t->tipo == TOK_LLAVE_IZQ && !primero_de_linea ? base : nivel_actual) + 1 + cabecera;
        } else if (es_cierra(t->tipo) && alto > 0) {
            int donde = pila[--alto].linea;   /* la sangría de la línea donde se abrió */
            if (donde < nivel_actual) nivel_actual = donde;
            if (donde < base) base = donde;
        }
        antes = previo;
        previo = &tk[j];
        previo_unario = unario;
        primero = 0;
        i = j;
    }
    recortar(&s);
    /* sin líneas en blanco al final, y acaba en un salto */
    while (s.n > 0 && s.d[s.n - 1] == '\n') s.n--;
    s.d[s.n] = '\0';
    if (s.n > 0) poner(&s, "\n");
    free(pila);

    /* La prueba: el resultado tiene que ser el mismo programa */
    Lexer *lx2 = lexer_crear(s.d);
    int n2 = 0;
    Token *tk2 = lexer_tokenizar(lx2, &n2);
    int bien = mismo_programa(tk, n, tk2, n2);
    lexer_destruir(lx2);
    lexer_destruir(lx);
    if (!bien) { free(s.d); return NULL; }

    /* Si el archivo usaba \r\n (Windows), el resultado también */
    const char *salto = strchr(codigo, '\n');
    if (salto && salto > codigo && salto[-1] == '\r') {
        Salida w = {0};
        poner(&w, "");
        for (const char *p = s.d; *p; p++) {
            if (*p == '\n' && (p == s.d || p[-1] != '\r')) poner(&w, "\r");
            poner_n(&w, p, 1);
        }
        free(s.d);
        return w.d;
    }
    return s.d;
}
