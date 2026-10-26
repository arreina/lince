/*
 * 🐆 Lince — migrar.c
 * 'lince migrar' (ver migrar.h). Trabaja sobre los tokens del lexer: cada
 * cambio sustituye un trozo [desde, hasta) del código por otro texto, y lo
 * demás (comentarios, espacios, saltos de línea) se queda como estaba. Los
 * argumentos que se mueven se copian tal cual, con lo que haya entre ellos.
 *
 * Se hace por vueltas: en cada una se buscan los cambios en el código de la
 * vuelta anterior y se aplican los que no se pisan (uno dentro de otro, como
 * juego.texto(texto.extraer(t, 0, 3), 1, 2, 3), se resuelve en dos vueltas).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif
#include "lexer.h"
#include "migrar.h"

typedef struct { int desde, hasta, linea; char *nuevo; char *que; } Cambio;

typedef struct {
    const char *src;
    Token *tk;
    int n;
    Cambio *c;
    int nc, capc;
    MigrarNota *mano;
    int nm, capm;
    int notas;                       /* apuntar lo que hay que mirar a mano (solo en la primera vuelta) */
    char juego[64], texto[64];       /* cómo se llaman los módulos en este archivo */
    char fuentes[64][64];            /* lo que se guardó de juego.fuente(...) */
    int nfuentes;
    char camaras[64][64];            /* lo que se guardó de juego.camara_actual() */
    int ncamaras;
} Mig;

/* ── Texto que crece ── */
typedef struct { char *d; size_t n, cap; } Buf;
static void b_pon(Buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2 + 64; b->d = realloc(b->d, b->cap); }
    memcpy(b->d + b->n, s, n);
    b->n += n;
    b->d[b->n] = '\0';
}
static void b_txt(Buf *b, const char *s) { b_pon(b, s, strlen(s)); }

static char *formato(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *r = malloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(r, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return r;
}

/* ── Los tokens ── */
static int es(Mig *m, int i, TipoToken t) { return i >= 0 && i < m->n && m->tk[i].tipo == t; }
static int es_nombre(Mig *m, int i, const char *s) {
    return i >= 0 && i < m->n && m->tk[i].valor && !strcmp(m->tk[i].valor, s) &&
           (m->tk[i].tipo == TOK_IDENTIFICADOR || m->tk[i].tipo == TOK_TIPO_TEXTO);
}
static int es_juego(Mig *m, int i) { return es_nombre(m, i, m->juego) && !es(m, i - 1, TOK_PUNTO); }
static int es_texto(Mig *m, int i) { return es_nombre(m, i, m->texto) && !es(m, i - 1, TOK_PUNTO); }
static int abre(TipoToken t) { return t == TOK_PAREN_IZQ || t == TOK_CORCHETE_IZQ || t == TOK_LLAVE_IZQ || t == TOK_PLANTILLA_INICIO; }
static int cierra(TipoToken t) { return t == TOK_PAREN_DER || t == TOK_CORCHETE_DER || t == TOK_LLAVE_DER || t == TOK_PLANTILLA_FIN; }

/* El trozo del código de los tokens a..b (ambos incluidos) */
static void trozo(Mig *m, Buf *b, int a, int z) { b_pon(b, m->src + m->tk[a].desde, (size_t)(m->tk[z].hasta - m->tk[a].desde)); }
/* Lo que hay entre el final del token a y el principio del b (la coma y los espacios) */
static void entre(Mig *m, Buf *b, int a, int z) { b_pon(b, m->src + m->tk[a].hasta, (size_t)(m->tk[z].desde - m->tk[a].hasta)); }

/* Los argumentos de la llamada cuyo '(' es el token 'p': cuántos, y dónde
   empieza y acaba cada uno; *fin, el ')' (o -1 si no está cerrada) */
#define MAX_ARGS 16
static int argumentos(Mig *m, int p, int *ini, int *fin, int *cierre) {
    int prof = 0, n = 0, desde = p + 1;
    *cierre = -1;
    for (int i = p + 1; i < m->n; i++) {
        TipoToken t = m->tk[i].tipo;
        if (t == TOK_EOF) return -1;
        if (abre(t)) prof++;
        else if (cierra(t)) {
            if (prof == 0) {
                if (i > desde) { if (n < MAX_ARGS) { ini[n] = desde; fin[n] = i - 1; } n++; }
                *cierre = i;
                return n > MAX_ARGS ? -1 : n;
            }
            prof--;
        } else if (t == TOK_COMA && prof == 0) {
            if (n < MAX_ARGS) { ini[n] = desde; fin[n] = i - 1; }
            n++;
            desde = i + 1;
        }
    }
    return -1;
}

/* ¿Es el argumento un número escrito tal cual (0.5, -1)? */
static int numero_suelto(Mig *m, int a, int z) {
    if (a == z) return es(m, a, TOK_NUMERO);
    return z == a + 1 && es(m, a, TOK_MENOS) && es(m, z, TOK_NUMERO);
}

/* ¿Se puede poner un .metodo() detrás sin paréntesis? Un nombre o un literal
   seguido de .campo, [índice] o (argumentos) */
static int simple(Mig *m, int a, int z) {
    int i = a;
    TipoToken t = m->tk[i].tipo;
    if (t == TOK_PLANTILLA_INICIO || t == TOK_PAREN_IZQ || t == TOK_CORCHETE_IZQ) {
        int prof = 0;
        for (; i <= z; i++) {
            if (abre(m->tk[i].tipo)) prof++;
            else if (cierra(m->tk[i].tipo) && --prof == 0) break;
        }
        if (t == TOK_PAREN_IZQ && i == z) return 1;
    } else if (t != TOK_IDENTIFICADOR && t != TOK_TEXTO && t != TOK_NUMERO && t != TOK_MI) return 0;
    for (i++; i <= z; i++) {
        t = m->tk[i].tipo;
        if (t == TOK_PUNTO && i + 1 <= z) { i++; continue; }
        if (t == TOK_PAREN_IZQ || t == TOK_CORCHETE_IZQ) {
            int prof = 0;
            for (; i <= z; i++) {
                if (abre(m->tk[i].tipo)) prof++;
                else if (cierra(m->tk[i].tipo) && --prof == 0) break;
            }
            continue;
        }
        return 0;
    }
    return 1;
}
static void operando(Mig *m, Buf *b, int a, int z) {
    int p = !simple(m, a, z);
    if (p) b_txt(b, "(");
    trozo(m, b, a, z);
    if (p) b_txt(b, ")");
}

/* ── Apuntar ── */
static void cambiar(Mig *m, int desde, int hasta, int linea, char *nuevo, const char *fmt, ...) {
    if (m->nc == m->capc) { m->capc = m->capc ? m->capc * 2 : 32; m->c = realloc(m->c, sizeof(Cambio) * (size_t)m->capc); }
    va_list ap;
    va_start(ap, fmt);
    char que[600];
    vsnprintf(que, sizeof que, fmt, ap);
    va_end(ap);
    Cambio *c = &m->c[m->nc++];
    c->desde = desde; c->hasta = hasta; c->linea = linea; c->nuevo = nuevo; c->que = strdup(que);
}
static void a_mano(Mig *m, int linea, const char *fmt, ...) {
    if (!m->notas) return;
    if (m->nm == m->capm) { m->capm = m->capm ? m->capm * 2 : 16; m->mano = realloc(m->mano, sizeof(MigrarNota) * (size_t)m->capm); }
    va_list ap;
    va_start(ap, fmt);
    char que[600];
    vsnprintf(que, sizeof que, fmt, ap);
    va_end(ap);
    m->mano[m->nm].linea = linea;
    m->mano[m->nm].que = strdup(que);
    m->nm++;
}

/* ── Los nombres que cambian ── */
static const char *const RENOMBRES[][2] = {
    {"cuerpo", "fisica.cuerpo"}, {"quitar_cuerpo", "fisica.quitar"}, {"avanzar_fisica", "fisica.avanzar"},
    {"tocando", "fisica.tocando"}, {"empujar", "fisica.empujar"}, {"bisagra", "fisica.bisagra"},
    {"cuerda", "fisica.cuerda"}, {"quitar_union", "fisica.quitar_union"}, {"puntos_union", "fisica.puntos_union"},
    {"centro_cuerpo", "fisica.centro"}, {"puntos_cuerpo", "fisica.puntos"}, {"dibujar_cuerpo", "fisica.dibujar"},
    {"mapa_tiled", "mapa.tiled"}, {"casilla", "mapa.casilla"}, {"poner_casilla", "mapa.poner"},
    {"solido", "mapa.solido"}, {"casilla_en", "mapa.casilla_en"}, {"buscar_en_mapa", "mapa.buscar"},
    {"mover_en_mapa", "mapa.mover"}, {"dibujar_mapa", "mapa.dibujar"}, {"dibujar_tiled", "mapa.dibujar_tiled"},
    {"camino", "mapa.camino"},
    {"guardar", "guardar_partida"}, {"cargar", "cargar_partida"}, {"olvidar", "olvidar_partida"},
    {"borrar_particulas", "quitar_particulas"},
    {NULL, NULL}};

/* Las funciones del módulo texto que pasan a ser métodos del texto */
static const char *const METODOS_TEXTO[] = {
    "dividir", "invertir", "a_lista", "posicion", "contar", "extraer", "es_numero", "es_letra",
    "empieza_con", "termina_con", "rellenar_izq", "rellenar_der", "centrar", NULL};

static int en(const char *const *lista, const char *s) {
    for (int i = 0; lista[i]; i++) if (!strcmp(lista[i], s)) return 1;
    return 0;
}

/* ── Lo que hay que saber del archivo antes de cambiar nada ── */
static void conocer(Mig *m) {
    snprintf(m->juego, sizeof m->juego, "juego");
    snprintf(m->texto, sizeof m->texto, "texto");
    for (int i = 0; i + 1 < m->n; i++) {
        /* importar "juego" como j */
        if (es(m, i, TOK_IMPORTAR) && es(m, i + 1, TOK_TEXTO) && es_nombre(m, i + 2, "como") && es(m, i + 3, TOK_IDENTIFICADOR)) {
            if (!strcmp(m->tk[i + 1].valor, "juego")) snprintf(m->juego, sizeof m->juego, "%s", m->tk[i + 3].valor);
            if (!strcmp(m->tk[i + 1].valor, "texto")) snprintf(m->texto, sizeof m->texto, "%s", m->tk[i + 3].valor);
        }
    }
    for (int i = 0; i + 4 < m->n; i++) {
        /* sea/fijo NOMBRE = juego.fuente(  y  sea c = juego.camara_actual() */
        if (!(es(m, i, TOK_SEA) || es(m, i, TOK_FIJO))) continue;
        int k = i + 1;
        if (!es(m, k, TOK_IDENTIFICADOR)) continue;
        if (es(m, k + 1, TOK_IDENTIFICADOR)) k++;                    /* sea Tipo nombre */
        if (!es(m, k + 1, TOK_IGUAL) || !es_juego(m, k + 2) || !es(m, k + 3, TOK_PUNTO)) continue;
        if (es_nombre(m, k + 4, "fuente") && m->nfuentes < 64)
            snprintf(m->fuentes[m->nfuentes++], 64, "%s", m->tk[k].valor);
        if (es_nombre(m, k + 4, "camara_actual") && m->ncamaras < 64)
            snprintf(m->camaras[m->ncamaras++], 64, "%s", m->tk[k].valor);
    }
}

static int es_fuente(Mig *m, int a, int z) {
    if (a == z && es(m, a, TOK_IDENTIFICADOR))
        for (int i = 0; i < m->nfuentes; i++) if (!strcmp(m->fuentes[i], m->tk[a].valor)) return 1;
    return z >= a + 3 && es_juego(m, a) && es(m, a + 1, TOK_PUNTO) && es_nombre(m, a + 2, "fuente");
}

/* ── Las reglas ── */

/* juego.algo(...) */
static void regla_juego(Mig *m, int i) {
    const Token *nom = &m->tk[i + 2];
    const char *f = nom->valor;
    int llamada = es(m, i + 3, TOK_PAREN_IZQ);
    if (es(m, i + 3, TOK_PUNTO)) return;                       /* juego.fisica.cuerpo: ya es la nueva */
    for (int k = 0; RENOMBRES[k][0]; k++)
        if (!strcmp(f, RENOMBRES[k][0])) {
            cambiar(m, nom->desde, nom->hasta, nom->linea, strdup(RENOMBRES[k][1]), "%s.%s → %s.%s",
                    m->juego, f, m->juego, RENOMBRES[k][1]);
            return;
        }
    if (!strcmp(f, "fisica") || !strcmp(f, "mapa")) {
        const char *nuevo = !strcmp(f, "fisica") ? "fisica.mundo" : "mapa.crear";
        cambiar(m, nom->desde, nom->hasta, nom->linea, strdup(nuevo), "%s.%s → %s.%s", m->juego, f, m->juego, nuevo);
        return;
    }
    if (!llamada) return;
    int ini[MAX_ARGS], fin[MAX_ARGS], cie;
    int n = argumentos(m, i + 3, ini, fin, &cie);
    if (n < 0) return;
    int linea = nom->linea;
    Buf b = {0};

    if (!strcmp(f, "reproducir") || !strcmp(f, "musica")) {
        if (n < 2 || es(m, ini[1], TOK_LLAVE_IZQ) || es(m, ini[1], TOK_NULO)) return;
        int bucle = !strcmp(f, "reproducir") && n >= 3;
        if (!bucle && !numero_suelto(m, ini[1], fin[1])) {
            a_mano(m, linea, "%s.%s(..., %s): si es el volumen, ahora va en las opciones: {\"volumen\": ...}",
                   m->juego, f, m->tk[ini[1]].valor ? m->tk[ini[1]].valor : "...");
            return;
        }
        b_txt(&b, "{\"volumen\": ");
        trozo(m, &b, ini[1], fin[1]);
        if (bucle) { b_txt(&b, ", \"bucle\": "); trozo(m, &b, ini[2], fin[2]); }
        b_txt(&b, "}");
        int hasta = bucle ? fin[2] : fin[1];
        cambiar(m, m->tk[ini[1]].desde, m->tk[hasta].hasta, linea, b.d, "%s.%s(s, %s) → %s.%s(s, {\"volumen\"%s})",
                m->juego, f, bucle ? "volumen, bucle" : "volumen", m->juego, f, bucle ? ", \"bucle\"" : "");
        return;
    }
    if (!strcmp(f, "texto")) {
        if (n < 4 || es(m, ini[3], TOK_LLAVE_IZQ) || es(m, ini[3], TOK_NULO)) return;
        if (n == 4 && es_fuente(m, ini[3], fin[3])) {
            b_txt(&b, "{\"fuente\": ");
            trozo(m, &b, ini[3], fin[3]);
            b_txt(&b, "}");
            cambiar(m, m->tk[ini[3]].desde, m->tk[fin[3]].hasta, linea, b.d, "%s.texto(t, x, y, FUENTE) → %s.texto(t, x, y, {\"fuente\": FUENTE})",
                    m->juego, m->juego);
            return;
        }
        if (n == 4 && !numero_suelto(m, ini[3], fin[3])) {
            a_mano(m, linea, "%s.texto(t, x, y, %s): la escala (o la fuente) ahora va en las opciones: {\"escala\": ...} o {\"fuente\": ...}",
                   m->juego, m->tk[ini[3]].valor ? m->tk[ini[3]].valor : "...");
            return;
        }
        b_txt(&b, "{\"escala\": ");
        trozo(m, &b, ini[3], fin[3]);
        if (n >= 5) { b_txt(&b, ", \"color\": "); trozo(m, &b, ini[4], fin[4]); }
        b_txt(&b, "}");
        cambiar(m, m->tk[ini[3]].desde, m->tk[fin[n >= 5 ? 4 : 3]].hasta, linea, b.d,
                "%s.texto(t, x, y, escala%s) → %s.texto(t, x, y, {\"escala\"%s})", m->juego, n >= 5 ? ", color" : "",
                m->juego, n >= 5 ? ", \"color\"" : "");
        return;
    }
    if (!strcmp(f, "colision")) {
        if (n != 8) return;
        b_txt(&b, "[");
        for (int k = 0; k < 8; k++) {
            trozo(m, &b, ini[k], fin[k]);
            if (k == 3) { b_txt(&b, "]"); entre(m, &b, fin[k], ini[k + 1]); b_txt(&b, "["); }
            else if (k < 7) entre(m, &b, fin[k], ini[k + 1]);
        }
        b_txt(&b, "]");
        cambiar(m, m->tk[ini[0]].desde, m->tk[fin[7]].hasta, linea, b.d,
                "%s.colision(x1, y1, ancho1, alto1, x2, ...) → %s.colision([x1, y1, ancho1, alto1], [x2, ...])", m->juego, m->juego);
        return;
    }
    if (!strcmp(f, "colision_circulos") || !strcmp(f, "colision_circulo_rect") || !strcmp(f, "dentro")) {
        int esperado = !strcmp(f, "colision_circulos") ? 6 : !strcmp(f, "dentro") ? 6 : 7;
        if (!strcmp(f, "dentro") && n == 2) {
            cambiar(m, nom->desde, nom->hasta, linea, strdup("colision"), "%s.dentro(punto, caja) → %s.colision(punto, caja)", m->juego, m->juego);
            return;
        }
        if (n != esperado) {
            a_mano(m, linea, "%s.%s: ahora es %s.colision(a, b), con un punto [x, y], una caja [x, y, ancho, alto] o un círculo {\"centro\": [x, y], \"radio\": r}",
                   m->juego, f, m->juego);
            return;
        }
        b_txt(&b, "colision(");
        if (!strcmp(f, "dentro")) {
            b_txt(&b, "["); trozo(m, &b, ini[0], fin[0]); entre(m, &b, fin[0], ini[1]); trozo(m, &b, ini[1], fin[1]);
            b_txt(&b, "]"); entre(m, &b, fin[1], ini[2]); b_txt(&b, "[");
            for (int k = 2; k < 6; k++) { trozo(m, &b, ini[k], fin[k]); if (k < 5) entre(m, &b, fin[k], ini[k + 1]); }
            b_txt(&b, "]");
        } else {
            b_txt(&b, "{\"centro\": ["); trozo(m, &b, ini[0], fin[0]); entre(m, &b, fin[0], ini[1]); trozo(m, &b, ini[1], fin[1]);
            b_txt(&b, "], \"radio\": "); trozo(m, &b, ini[2], fin[2]); b_txt(&b, "}"); entre(m, &b, fin[2], ini[3]);
            if (!strcmp(f, "colision_circulos")) {
                b_txt(&b, "{\"centro\": ["); trozo(m, &b, ini[3], fin[3]); entre(m, &b, fin[3], ini[4]); trozo(m, &b, ini[4], fin[4]);
                b_txt(&b, "], \"radio\": "); trozo(m, &b, ini[5], fin[5]); b_txt(&b, "}");
            } else {
                b_txt(&b, "[");
                for (int k = 3; k < 7; k++) { trozo(m, &b, ini[k], fin[k]); if (k < 6) entre(m, &b, fin[k], ini[k + 1]); }
                b_txt(&b, "]");
            }
        }
        b_txt(&b, ")");
        cambiar(m, nom->desde, m->tk[cie].hasta, linea, b.d, "%s.%s(...) → %s.colision(...)", m->juego, f, m->juego);
        return;
    }
    if (!strcmp(f, "posicion_camara") && n == 0) {
        char *nuevo = formato("[%s.camara_actual()[\"x\"], %s.camara_actual()[\"y\"]]", m->juego, m->juego);
        cambiar(m, m->tk[i].desde, m->tk[cie].hasta, linea, nuevo, "%s.posicion_camara() → %s.camara_actual()", m->juego, m->juego);
        return;
    }
    if (!strcmp(f, "camara")) {                                  /* {"giro": ...} → {"angulo": ...} */
        for (int a = 0; a < n; a++) {
            if (!es(m, ini[a], TOK_LLAVE_IZQ)) continue;
            int prof = 0;
            for (int k = ini[a]; k <= fin[a]; k++) {
                if (abre(m->tk[k].tipo)) prof++;
                else if (cierra(m->tk[k].tipo)) prof--;
                else if (prof == 1 && es(m, k, TOK_TEXTO) && !strcmp(m->tk[k].valor, "giro") && es(m, k + 1, TOK_DOS_PUNTOS))
                    cambiar(m, m->tk[k].desde, m->tk[k].hasta, m->tk[k].linea, strdup("\"angulo\""),
                            "%s.camara(x, y, {\"giro\": ...}) → {\"angulo\": ...}", m->juego);
            }
        }
    }
}

/* texto.algo(t, ...) */
static void regla_texto(Mig *m, int i) {
    if (!es(m, i + 1, TOK_PUNTO) || !es(m, i + 2, TOK_IDENTIFICADOR) || !es(m, i + 3, TOK_PAREN_IZQ)) return;
    const char *f = m->tk[i + 2].valor;
    int ini[MAX_ARGS], fin[MAX_ARGS], cie;
    int n = argumentos(m, i + 3, ini, fin, &cie);
    if (n < 1) return;
    int linea = m->tk[i].linea;
    Buf b = {0};
    if (en(METODOS_TEXTO, f)) {
        operando(m, &b, ini[0], fin[0]);
        b_txt(&b, ".");
        b_txt(&b, f);
        b_txt(&b, "(");
        if (n > 1) b_pon(&b, m->src + m->tk[ini[1]].desde, (size_t)(m->tk[fin[n - 1]].hasta - m->tk[ini[1]].desde));
        b_txt(&b, ")");
        cambiar(m, m->tk[i].desde, m->tk[cie].hasta, linea, b.d, "%s.%s(t, ...) → t.%s(...)", m->texto, f, f);
        return;
    }
    if ((!strcmp(f, "a_numero") || !strcmp(f, "de_numero")) && n == 1) {
        b_txt(&b, !strcmp(f, "a_numero") ? "a_numero(" : "a_texto(");
        trozo(m, &b, ini[0], fin[0]);
        b_txt(&b, ")");
        cambiar(m, m->tk[i].desde, m->tk[cie].hasta, linea, b.d, "%s.%s(...) → %s(...)", m->texto, f,
                !strcmp(f, "a_numero") ? "a_numero" : "a_texto");
        return;
    }
    if (!strcmp(f, "repetir") && n == 2) {
        b_txt(&b, "(");
        operando(m, &b, ini[0], fin[0]);
        b_txt(&b, " * ");
        operando(m, &b, ini[1], fin[1]);
        b_txt(&b, ")");
        cambiar(m, m->tk[i].desde, m->tk[cie].hasta, linea, b.d, "%s.repetir(t, n) → t * n", m->texto);
        return;
    }
    if (!strcmp(f, "es_vacio") && n == 1) {
        b_txt(&b, "(");
        operando(m, &b, ini[0], fin[0]);
        b_txt(&b, " == \"\")");
        cambiar(m, m->tk[i].desde, m->tk[cie].hasta, linea, b.d, "%s.es_vacio(t) → t == \"\"", m->texto);
    }
}

/* c["giro"] de una cámara guardada con camara_actual() → c["angulo"] */
static void regla_giro(Mig *m, int i) {
    if (!es(m, i, TOK_CORCHETE_IZQ) || !es(m, i + 1, TOK_TEXTO) || strcmp(m->tk[i + 1].valor, "giro") || !es(m, i + 2, TOK_CORCHETE_DER))
        return;
    int ok = 0;
    if (es(m, i - 1, TOK_IDENTIFICADOR))
        for (int k = 0; k < m->ncamaras; k++) if (!strcmp(m->camaras[k], m->tk[i - 1].valor)) ok = 1;
    if (es(m, i - 1, TOK_PAREN_DER) && es(m, i - 2, TOK_PAREN_IZQ) && es_nombre(m, i - 3, "camara_actual") && es(m, i - 4, TOK_PUNTO))
        ok = 1;
    if (ok) cambiar(m, m->tk[i + 1].desde, m->tk[i + 1].hasta, m->tk[i + 1].linea, strdup("\"angulo\""),
                    "[\"giro\"] de la cámara → [\"angulo\"]");
    else a_mano(m, m->tk[i + 1].linea, "[\"giro\"]: si es de una cámara (camara_actual()), ahora es [\"angulo\"]");
}

static void buscar_cambios(Mig *m) {
    int vals = 0, primera_val = 0;
    for (int i = 0; i < m->n; i++) {
        if (es(m, i, TOK_VAL)) {                                 /* val: fuera, con su espacio */
            int hasta = m->tk[i].hasta;
            while (m->src[hasta] == ' ' || m->src[hasta] == '\t') hasta++;
            if (!vals++) primera_val = m->tk[i].linea;
            cambiar(m, m->tk[i].desde, hasta, m->tk[i].linea, strdup(""), "val");
            continue;
        }
        if (es_juego(m, i) && es(m, i + 1, TOK_PUNTO) && i + 2 < m->n && m->tk[i + 2].valor) regla_juego(m, i);
        else if (es_texto(m, i) && es(m, i + 1, TOK_PUNTO)) regla_texto(m, i);
        else if (es(m, i, TOK_CORCHETE_IZQ)) regla_giro(m, i);
    }
    (void)primera_val;
}

static int por_desde(const void *a, const void *b) {
    const Cambio *x = a, *y = b;
    return x->desde != y->desde ? (x->desde < y->desde ? -1 : 1) : (x->hasta < y->hasta ? -1 : x->hasta > y->hasta);
}

/* Una vuelta: los cambios que no se pisan, aplicados. Devuelve el código
   nuevo, o NULL si no había nada que cambiar. */
static char *vuelta(const char *codigo, MigrarInforme *inf, int primera) {
    Lexer *lx = lexer_crear(codigo);
    Mig m;
    memset(&m, 0, sizeof m);
    m.src = codigo;
    m.tk = lexer_tokenizar(lx, &m.n);
    m.notas = primera && inf;
    conocer(&m);
    buscar_cambios(&m);
    char *r = NULL;
    if (m.nc > 0) {
        qsort(m.c, (size_t)m.nc, sizeof(Cambio), por_desde);
        Buf b = {0};
        int pos = 0;
        for (int k = 0; k < m.nc; k++) {
            Cambio *c = &m.c[k];
            if (c->desde < pos) continue;                        /* pisa a otro: en la vuelta siguiente */
            b_pon(&b, codigo + pos, (size_t)(c->desde - pos));
            b_txt(&b, c->nuevo);
            pos = c->hasta;
            if (inf && primera) {
                inf->cambios = realloc(inf->cambios, sizeof(MigrarNota) * (size_t)(inf->ncambios + 1));
                inf->cambios[inf->ncambios].linea = c->linea;
                inf->cambios[inf->ncambios].que = strdup(c->que);
                inf->ncambios++;
            }
        }
        b_txt(&b, codigo + pos);
        r = b.d;
    }
    if (inf && primera) {
        inf->mano = m.mano;
        inf->nmano = m.nm;
    } else {
        for (int k = 0; k < m.nm; k++) free(m.mano[k].que);
        free(m.mano);
    }
    for (int k = 0; k < m.nc; k++) { free(m.c[k].nuevo); free(m.c[k].que); }
    free(m.c);
    lexer_destruir(lx);
    return r;
}

char *migrar_codigo(const char *codigo, MigrarInforme *inf) {
    if (inf) memset(inf, 0, sizeof *inf);
    char *actual = strdup(codigo);
    for (int v = 0; v < 8; v++) {
        char *nuevo = vuelta(actual, inf, v == 0);
        if (!nuevo) break;
        free(actual);
        actual = nuevo;
    }
    return actual;
}

void migrar_informe_liberar(MigrarInforme *inf) {
    for (int i = 0; i < inf->ncambios; i++) free(inf->cambios[i].que);
    for (int i = 0; i < inf->nmano; i++) free(inf->mano[i].que);
    free(inf->cambios);
    free(inf->mano);
    memset(inf, 0, sizeof *inf);
}

void migrar_avisos(const char *codigo, void (*avisar)(int linea, const char *mensaje)) {
    MigrarInforme inf;
    char *nuevo = migrar_codigo(codigo, &inf);
    free(nuevo);
    int vals = 0, primera_val = 0;
    for (int i = 0; i < inf.ncambios; i++) {
        if (!strcmp(inf.cambios[i].que, "val")) { if (!vals++) primera_val = inf.cambios[i].linea; continue; }
        char msg[800];
        snprintf(msg, sizeof msg, "obsoleto: %s. En la 2.0 ya no valdrá; «lince migrar» lo cambia solo.", inf.cambios[i].que);
        avisar(inf.cambios[i].linea, msg);
    }
    if (vals) {
        char msg[300];
        snprintf(msg, sizeof msg, "obsoleto: 'val' ya no hace falta (es lo mismo que no poner nada; está %d %s en este archivo). "
                 "En la 2.0 ya no valdrá; «lince migrar» lo quita solo.", vals, vals == 1 ? "vez" : "veces");
        avisar(primera_val, msg);
    }
    /* Lo dudoso (reproducir(s, vol): ¿vol es el volumen o unas opciones?) no se
       avisa aquí: sería un aviso para siempre en el código que ya está bien. Lo
       avisa el programa al ejecutarse, que sí sabe lo que hay en 'vol'. */
    migrar_informe_liberar(&inf);
}

/* ── Los archivos ── */
static int es_carpeta(const char *r) {
    struct stat st;
    return stat(r, &st) == 0 && S_ISDIR(st.st_mode);
}
static int acaba_en_lince(const char *n) {
    size_t l = strlen(n);
    return l > 6 && !strcmp(n + l - 6, ".lince");
}
typedef struct { char **v; int n, cap; } Lista;
static void poner(Lista *l, const char *s) {
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 16; l->v = realloc(l->v, sizeof(char *) * (size_t)l->cap); }
    l->v[l->n++] = strdup(s);
}
static void recorrer(const char *carpeta, Lista *out, int prof) {
    if (prof > 12) return;
#ifdef _WIN32
    char patron[1100];
    snprintf(patron, sizeof patron, "%s\\*", carpeta);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(patron, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const char *nom = fd.cFileName;
        if (nom[0] == '.') continue;
        char r[2048];
        snprintf(r, sizeof r, "%s/%s", carpeta, nom);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) recorrer(r, out, prof + 1);
        else if (acaba_en_lince(nom)) poner(out, r);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(carpeta);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char r[2048];
        snprintf(r, sizeof r, "%s/%s", carpeta, e->d_name);
        if (es_carpeta(r)) recorrer(r, out, prof + 1);
        else if (acaba_en_lince(e->d_name)) poner(out, r);
    }
    closedir(d);
#endif
}
static int comparar(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int migrar_buscar(const char *ruta, char ***archivos) {
    Lista l = {0};
    if (es_carpeta(ruta)) recorrer(ruta, &l, 0);
    else poner(&l, ruta);
    if (l.n > 1) qsort(l.v, (size_t)l.n, sizeof(char *), comparar);
    *archivos = l.v;
    return l.n;
}
