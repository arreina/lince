/*
 * 🐆 Lince — documentar.c
 * La ayuda sacada de los comentarios (ver documentar.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "documentar.h"

typedef struct { char *d; size_t n, cap; } Texto;

static void tx_n(Texto *t, const char *s, size_t n) {
    if (t->n + n + 1 > t->cap) {
        t->cap = (t->n + n + 1) * 2;
        t->d   = realloc(t->d, t->cap);
    }
    memcpy(t->d + t->n, s, n);
    t->n += n;
    t->d[t->n] = '\0';
}
static void tx(Texto *t, const char *s) { tx_n(t, s, strlen(s)); }

/* ── Las líneas del código, con lo que hace falta saber de cada una ── */
typedef struct {
    const char *ini;      /* la línea, sin el salto */
    int         largo;
    int         nivel;    /* llaves abiertas al empezar la línea */
    int         en_texto; /* empieza dentro de un texto de varias líneas */
    int         cambio;   /* cuántas llaves abre (+) o cierra (-) */
    int         parens;   /* paréntesis que deja abiertos */
} Linea;

/* Recorre el código saltando textos (con sus huecos {…}) y comentarios */
static int partir(const char *c, Linea **out) {
    int cap = 256, n = 0;
    Linea *ls = malloc(sizeof(Linea) * (size_t)cap);
    int pila[64], alto = 1;         /* 0 = código, 1 = texto; el fondo es código */
    int huecos[64];                 /* llaves abiertas dentro de cada hueco */
    pila[0] = 0; huecos[0] = 0;
    int nivel = 0;
    const char *p = c;
    while (*p) {
        if (n == cap) { cap *= 2; ls = realloc(ls, sizeof(Linea) * (size_t)cap); }
        Linea *l = &ls[n++];
        l->ini = p;
        l->nivel = nivel;
        l->en_texto = alto > 1;
        int parens = 0;
        while (*p && *p != '\n') {
            char ch = *p;
            if (pila[alto - 1] == 1) {                       /* dentro de un texto */
                if (ch == '\\' && p[1]) { p += 2; continue; }
                if (ch == '"') { alto--; p++; continue; }
                if (ch == '{' && p[1] == '}') { p += 2; continue; }
                if (ch == '{' && alto < 63) { pila[alto] = 0; huecos[alto] = 0; alto++; p++; continue; }
                p++;
                continue;
            }
            /* código: el de fuera (alto == 1) o el de un hueco */
            if (ch == '"' && alto < 63) { pila[alto] = 1; alto++; p++; continue; }
            if (ch == '#') { while (*p && *p != '\n') p++; break; }
            if (alto > 1) {
                if (ch == '{') huecos[alto - 1]++;
                else if (ch == '}') { if (huecos[alto - 1]-- == 0) alto--; }
            } else {
                if (ch == '{') nivel++;
                else if (ch == '}' && nivel > 0) nivel--;
                else if (ch == '(') parens++;
                else if (ch == ')') parens--;
            }
            p++;
        }
        l->largo  = (int)(p - l->ini);
        l->cambio = nivel - l->nivel;
        l->parens = parens;
        if (*p == '\n') p++;
    }
    *out = ls;
    return n;
}

static const char *saltar_blancos(const char *s, const char *fin) {
    while (s < fin && (*s == ' ' || *s == '\t' || *s == '\r')) s++;
    return s;
}

/* Lee una palabra (letras, números, _ y letras con tilde) */
static const char *palabra(const char *s, const char *fin, char *w, size_t max) {
    size_t k = 0;
    while (s < fin && (isalnum((unsigned char)*s) || *s == '_' || (unsigned char)*s >= 0x80)) {
        if (k + 1 < max) w[k++] = *s;
        s++;
    }
    w[k] = '\0';
    return s;
}

static int es_comentario(const Linea *l) {
    const char *s = saltar_blancos(l->ini, l->ini + l->largo);
    return !l->en_texto && s < l->ini + l->largo && *s == '#';
}
static int es_blanca(const Linea *l) {
    return !l->en_texto && saltar_blancos(l->ini, l->ini + l->largo) == l->ini + l->largo;
}

/* "# texto" → "texto"; una raya de adorno (# ────) no cuenta */
static void linea_de_ayuda(Texto *t, const Linea *l) {
    const char *fin = l->ini + l->largo;
    const char *s = saltar_blancos(l->ini, fin) + 1;
    if (s < fin && *s == '!') return;                /* #!/usr/bin/env lince */
    if (s < fin && *s == ' ') s++;
    while (fin > s && (fin[-1] == ' ' || fin[-1] == '\t' || fin[-1] == '\r')) fin--;
    /* sin letras ni números: una raya de adorno (los ─ son E2 94 80 en UTF-8;
       una letra con tilde empieza por C3) */
    int adorno = fin > s;
    for (const char *q = s; q < fin && adorno; q++)
        if (isalnum((unsigned char)*q) || ((unsigned char)*q >= 0xC2 && (unsigned char)*q <= 0xDF)) adorno = 0;
    if (adorno) return;
    tx_n(t, s, (size_t)(fin - s));
    tx(t, "\n");
}

/* Los comentarios pegados encima de la línea i */
static char *ayuda_de(const Linea *ls, int i, int desde) {
    int a = i;
    while (a - 1 >= desde && es_comentario(&ls[a - 1])) a--;
    Texto t = {0};
    tx(&t, "");
    for (int k = a; k < i; k++) linea_de_ayuda(&t, &ls[k]);
    while (t.n > 0 && t.d[t.n - 1] == '\n') t.d[--t.n] = '\0';
    return t.d;
}

/* Las líneas de comentario del principio, si van separadas del código */
static int fin_cabecera(const Linea *ls, int n) {
    int k = 0;
    while (k < n && es_comentario(&ls[k])) k++;
    if (k == 0 || (k < n && !es_blanca(&ls[k]))) return 0;
    return k;
}

char *documentar_cabecera(const char *codigo) {
    Linea *ls;
    int n = partir(codigo, &ls);
    int k = fin_cabecera(ls, n);
    char *r = k ? ayuda_de(ls, k, 0) : NULL;
    free(ls);
    if (r && !r[0]) { free(r); r = NULL; }
    return r;
}

/* La línea que define, sin el cuerpo ni el comentario de detrás (y con las
   siguientes si los paréntesis siguen abiertos: funcion f(a,\n b)) */
static void firma_de(const Linea *ls, int n, int i, char *f, size_t max, int con_llaves) {
    Texto t = {0};
    tx(&t, "");
    int parens = 0;
    for (int k = i; k < n && k < i + 8; k++) {
        const char *s = saltar_blancos(ls[k].ini, ls[k].ini + ls[k].largo), *fin = ls[k].ini + ls[k].largo;
        const char *q = s;
        int en_texto = 0, cortar = 0;
        for (; q < fin; q++) {
            if (en_texto) { if (*q == '\\') q++; else if (*q == '"') en_texto = 0; continue; }
            if (*q == '"') en_texto = 1;
            else if (*q == '#') break;
            else if (*q == '(') parens++;
            else if (*q == ')') parens--;
            else if (*q == '{' && parens == 0 && !con_llaves) { cortar = 1; break; }
        }
        while (q > s && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\r')) q--;
        if (t.n) tx(&t, " ");
        tx_n(&t, s, (size_t)(q - s));
        if (cortar || parens <= 0) break;
    }
    snprintf(f, max, "%s", t.d);
    free(t.d);
}

static const char *MODIFICADORES[] = { "abstracta", "abstracto", "estatico", "privado", "protegido",
                                       "publico", "sobrescribe", NULL };
static const char *DEFINEN[] = { "funcion", "generador", "tarea", "clase", "interfaz", "enumeracion",
                                 "obtener", "fijar", "sea", "fijo", NULL };

static int en_lista(const char *w, const char **l) {
    for (int i = 0; l[i]; i++) if (strcmp(w, l[i]) == 0) return 1;
    return 0;
}

int documentar_escanear(const char *codigo, Definicion **defs) {
    Linea *ls;
    int n = partir(codigo, &ls);
    int cap = 64, nd = 0;
    Definicion *d = malloc(sizeof(Definicion) * (size_t)cap);
    /* las clases, interfaces y enumeraciones abiertas: su nombre y el nivel de dentro */
    struct { char nombre[96]; int nivel; char que[16]; } ctx[32];
    int nctx = 0;
    int cabecera = fin_cabecera(ls, n);

    for (int i = 0; i < n; i++) {
        Linea *l = &ls[i];
        while (nctx > 0 && l->nivel < ctx[nctx - 1].nivel) nctx--;
        if (l->en_texto) continue;
        int dentro = nctx > 0 && l->nivel == ctx[nctx - 1].nivel;
        if (l->nivel != 0 && !dentro) continue;
        if (dentro && strcmp(ctx[nctx - 1].que, "enumeracion") == 0) continue;

        const char *fin = l->ini + l->largo, *s = saltar_blancos(l->ini, fin);
        char w[96];
        int privado = 0;
        s = palabra(s, fin, w, sizeof w);
        while (w[0] && en_lista(w, MODIFICADORES)) {
            if (strcmp(w, "privado") == 0) privado = 1;
            s = palabra(saltar_blancos(s, fin), fin, w, sizeof w);
        }
        if (!w[0] || !en_lista(w, DEFINEN)) continue;
        char que[16];
        snprintf(que, sizeof que, "%s", w);

        /* el nombre: la palabra que sigue; en 'sea numero vida = 3', la última antes del = */
        char nombre[96] = "";
        const char *q = saltar_blancos(s, fin);
        if (strcmp(que, "sea") == 0 || strcmp(que, "fijo") == 0) {
            if (q < fin && *q == '[') continue;                       /* sea [a, b] = … */
            while (q < fin) {
                char x[96];
                const char *r = palabra(q, fin, x, sizeof x);
                if (!x[0]) break;
                snprintf(nombre, sizeof nombre, "%s", x);
                q = saltar_blancos(r, fin);
                if (q < fin && *q == '.') q = saltar_blancos(q + 1, fin);   /* sea geo.Punto p */
            }
        } else palabra(q, fin, nombre, sizeof nombre);
        if (!nombre[0]) continue;

        if (nd == cap) { cap *= 2; d = realloc(d, sizeof(Definicion) * (size_t)cap); }
        Definicion *e = &d[nd++];
        memset(e, 0, sizeof *e);
        snprintf(e->que, sizeof e->que, "%s", dentro && strcmp(que, "funcion") == 0 ? "metodo" : que);
        snprintf(e->nombre, sizeof e->nombre, "%s", nombre);
        snprintf(e->dentro, sizeof e->dentro, "%s", dentro ? ctx[nctx - 1].nombre : "");
        int abre_cuerpo = strcmp(que, "clase") == 0 || strcmp(que, "interfaz") == 0;
        firma_de(ls, n, i, e->firma, sizeof e->firma,
                 strcmp(que, "enumeracion") == 0 || strcmp(que, "sea") == 0 || strcmp(que, "fijo") == 0);
        e->ayuda = ayuda_de(ls, i, cabecera);
        e->linea = i;
        e->privado = privado || nombre[0] == '_';

        if ((abre_cuerpo || strcmp(que, "enumeracion") == 0) && nctx < 32) {
            /* lo que venga un nivel más adentro es suyo (el '{' puede ir en otra línea) */
            snprintf(ctx[nctx].nombre, sizeof ctx[nctx].nombre, "%s", nombre);
            snprintf(ctx[nctx].que, sizeof ctx[nctx].que, "%s", que);
            ctx[nctx].nivel = l->nivel + 1;
            nctx++;
        }
    }
    free(ls);
    *defs = d;
    return nd;
}

void documentar_liberar(Definicion *defs, int n) {
    for (int i = 0; i < n; i++) free(defs[i].ayuda);
    free(defs);
}

static void seccion(Texto *t, const Definicion *e, const char *almohadillas) {
    tx(t, "\n"); tx(t, almohadillas); tx(t, " `"); tx(t, e->firma); tx(t, "`\n");
    if (e->ayuda[0]) { tx(t, "\n"); tx(t, e->ayuda); tx(t, "\n"); }
}

char *documentar_markdown(const char *codigo, const char *titulo) {
    Texto t = {0};
    tx(&t, "# "); tx(&t, titulo); tx(&t, "\n");
    char *cab = documentar_cabecera(codigo);
    if (cab) { tx(&t, "\n"); tx(&t, cab); tx(&t, "\n"); free(cab); }
    Definicion *d;
    int n = documentar_escanear(codigo, &d);
    /* una variable 'sea' sin comentario es algo interno: no se cuenta */
    const char *dentro = NULL;
    int privada = 0;                 /* la clase de la que se saltan los miembros */
    for (int i = 0; i < n; i++) {
        Definicion *e = &d[i];
        int es_variable = strcmp(e->que, "sea") == 0;
        if (e->dentro[0]) {
            if (privada || !dentro || strcmp(dentro, e->dentro) != 0) continue;
            if (e->privado || (es_variable && !e->ayuda[0])) continue;
            seccion(&t, e, "###");
            continue;
        }
        dentro = NULL;
        privada = 0;
        if (e->privado || (es_variable && !e->ayuda[0])) {
            privada = 1;
            dentro = e->nombre;
            continue;
        }
        seccion(&t, e, "##");
        if (strcmp(e->que, "clase") == 0 || strcmp(e->que, "interfaz") == 0) dentro = e->nombre;
    }
    documentar_liberar(d, n);
    return t.d;
}

char *documentar_ayuda(const char *codigo, const char *nombre, int linea) {
    Definicion *d;
    int n = documentar_escanear(codigo, &d);
    Definicion *e = NULL;
    for (int i = 0; i < n && !e; i++)
        if (strcmp(d[i].nombre, nombre) == 0 && (linea < 0 || d[i].linea == linea)) e = &d[i];
    char *r = NULL;
    if (e) {
        Texto t = {0};
        tx(&t, "```lince\n"); tx(&t, e->firma); tx(&t, "\n```");
        if (e->dentro[0]) { tx(&t, "\n\nde `"); tx(&t, e->dentro); tx(&t, "`"); }
        if (e->ayuda[0]) { tx(&t, "\n\n"); tx(&t, e->ayuda); }
        r = t.d;
    }
    documentar_liberar(d, n);
    return r;
}
