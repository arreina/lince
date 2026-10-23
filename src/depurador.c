/*
 * 🐆 Lince — depurador.c
 * Depurador sobre el hook de sentencias del intérprete. Dos modos:
 *   lince depurar prog.lince        para una persona, en la terminal
 *   lince depurar --ide prog.lince  para un programa (el IDE): los mismos
 *                                   comandos por stdin, y cada respuesta o
 *                                   evento en una línea "@lince {json}"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "depurador.h"
#include "lexer.h"
#include "parser.h"
#include "interprete.h"
#include "lsp.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

enum { M_PASO, M_SIGUIENTE, M_SALIR, M_CONTINUAR };

/* archivo: la ruta como la dio quien la puso, ya normalizada ("./libs/x.lince"),
   o NULL para cualquier fichero. Si no lleva '/', vale con el nombre. */
typedef struct { char *archivo; int linea; } Parada;
#define MAX_PARADAS 256
static Parada _paradas[MAX_PARADAS];
static int    _num_paradas = 0;

static int     _modo = M_PASO;
static int     _ref = 0;                /* profundidad de referencia de siguiente/salir */
static Entorno *_global = NULL;
static char    *_principal = NULL;      /* ruta del programa principal */
static int     _ide = 0;                /* --ide: respuestas en JSON */
static int     _primera = 1;            /* aún no se ha parado nunca */
static Valor  *_error_visto = NULL;     /* el error sin capturar donde ya se paró */

/* ── El sitio de cada llamada abierta ──
   El hook apunta, para cada profundidad, el fichero, la línea y el ámbito
   de la última sentencia: la de una profundidad menor que la actual es la
   que hizo la llamada. Las de una biblioteca estándar no pasan por el hook
   y no valen (no son del programa). */
#define MAX_MARCOS 520
typedef struct { const char *archivo; int linea; Entorno *e; int vale; } Sitio;
static Sitio _sitios[MAX_MARCOS];
static int   _sitios_max = -1;

typedef struct { const char *funcion; const char *archivo; int linea; Entorno *e; } Marco;

/* Los marcos, del más profundo (el 0, donde está parado) al principal */
static int marcos(Marco *m, int max) {
    int n = 0, prof = interprete_profundidad();
    if (prof >= MAX_MARCOS) prof = MAX_MARCOS - 1;
    for (int d = prof; d >= 0 && n < max; d--) {
        if (!_sitios[d].vale) continue;
        const char *nombre = NULL; int linea_llamada = 0;
        if (d > 0) interprete_pila(d, &nombre, &linea_llamada);
        m[n].funcion = d > 0 ? (nombre ? nombre : "?") : NULL;
        m[n].archivo = _sitios[d].archivo;
        m[n].linea   = _sitios[d].linea;
        /* Más exacta, la línea de la llamada (en una sentencia de varias líneas) */
        if (n > 0 && d + 1 <= prof) {
            const char *x; int l = 0;
            if (interprete_pila(d + 1, &x, &l) && l > 0) m[n].linea = l;
        }
        m[n].e = _sitios[d].e;
        n++;
    }
    return n;
}

/* ── Rutas ──
   Como en los errores, relativas a la carpeta del principal y con '/':
   "./prog.lince", "./libs/util.lince". */
static void normalizar(const char *ruta, char *out, size_t max) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", ruta);
    for (char *c = tmp; *c; c++) if (*c == '\\') *c = '/';
    /* Se trocea por '/' y se quitan "." y "x/.." */
    char *partes[128]; int np = 0, absoluta = tmp[0] == '/';
    for (char *t = strtok(tmp, "/"); t && np < 128; t = strtok(NULL, "/")) {
        if (!strcmp(t, ".")) continue;
        if (!strcmp(t, "..") && np > 0 && strcmp(partes[np - 1], "..")) { np--; continue; }
        partes[np++] = t;
    }
    size_t k = 0;
    out[0] = '\0';
    if (absoluta && k + 1 < max) { out[k++] = '/'; out[k] = '\0'; }
    for (int i = 0; i < np; i++) {
        k += snprintf(out + k, k < max ? max - k : 0, "%s%s", i ? "/" : "", partes[i]);
        if (k >= max) { out[max - 1] = '\0'; return; }
    }
}

static int es_absoluta(const char *r) {
    return r[0] == '/' || r[0] == '\\' || (r[0] && r[1] == ':');
}

static void con_punto(char *r, size_t max) {
    if (es_absoluta(r) || !strncmp(r, "../", 3)) return;
    size_t n = strlen(r);
    if (n + 3 > max) return;
    memmove(r + 2, r, n + 1);
    r[0] = '.'; r[1] = '/';
}

/* La ruta de un fichero que corre, para enseñarla */
static void ruta_ide(const char *archivo, char *out, size_t max) {
    char dir[1024], norm[1024];
    normalizar(_principal, dir, sizeof dir);
    char *barra = strrchr(dir, '/');
    if (barra) *barra = '\0'; else dir[0] = '\0';
    normalizar(archivo, norm, sizeof norm);
    size_t ld = strlen(dir);
    if (ld && !strncmp(norm, dir, ld) && norm[ld] == '/') snprintf(out, max, "%s", norm + ld + 1);
    else snprintf(out, max, "%s", norm);
    con_punto(out, max);
}

/* La ruta que da quien pone una parada: relativa a la carpeta del principal */
static void ruta_dada(const char *ruta, char *out, size_t max) {
    if (es_absoluta(ruta)) { ruta_ide(ruta, out, max); return; }
    normalizar(ruta, out, max);
    if (_ide || strchr(ruta, '/') || strchr(ruta, '\\')) con_punto(out, max);
}

static const char *base(const char *ruta) {
    const char *b = strrchr(ruta, '/');
    return b ? b + 1 : ruta;
}

/* ── Código fuente de los archivos, para mostrar líneas ── */
typedef struct { char *ruta; char *texto; } Fuente;
static Fuente _fuentes[16];
static int    _num_fuentes = 0;

static const char *fuente_de(const char *ruta) {
    for (int i = 0; i < _num_fuentes; i++)
        if (strcmp(_fuentes[i].ruta, ruta) == 0) return _fuentes[i].texto;
    if (_num_fuentes >= 16) return NULL;
    char *t = interprete_leer_fuente(ruta);
    if (!t) return NULL;
    _fuentes[_num_fuentes].ruta = strdup(ruta);
    _fuentes[_num_fuentes].texto = t;
    return _fuentes[_num_fuentes++].texto;
}

/* Imprime la línea 'n' (1-based) del texto; devuelve 0 si no existe */
static int linea_de(const char *texto, int n, char *out, size_t max) {
    if (!texto || n < 1) return 0;
    const char *p = texto;
    for (int i = 1; i < n; i++) {
        p = strchr(p, '\n');
        if (!p) return 0;
        p++;
    }
    size_t k = 0;
    while (p[k] && p[k] != '\n' && k + 1 < max) { out[k] = p[k]; k++; }
    out[k] = '\0';
    return 1;
}

static void mostrar_linea(const char *archivo, int linea) {
    char txt[512];
    const char *src = fuente_de(archivo);
    if (linea_de(src, linea, txt, sizeof txt))
        printf("→ %s:%d   %s\n", base(archivo), linea, txt);
    else
        printf("→ %s:%d\n", base(archivo), linea);
}

static void listar(const char *archivo, int centro) {
    const char *src = fuente_de(archivo);
    char txt[512];
    for (int i = centro - 5; i <= centro + 5; i++) {
        if (i < 1 || !linea_de(src, i, txt, sizeof txt)) continue;
        printf("%s %4d  %s\n", i == centro ? "→" : " ", i, txt);
    }
}

/* ── Texto para el IDE ── */
typedef struct { char *p; size_t n, cap; } Buf;

static void b_mem(Buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        b->cap = (b->n + n + 1) * 2;
        b->p = realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}
static void b_txt(Buf *b, const char *s) { b_mem(b, s, strlen(s)); }

/* Un texto entre comillas de JSON */
static void b_json(Buf *b, const char *s) {
    if (!s) { b_txt(b, "null"); return; }
    b_txt(b, "\"");
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        char e[8];
        if (*c == '"' || *c == '\\') { e[0] = '\\'; e[1] = (char)*c; b_mem(b, e, 2); }
        else if (*c == '\n') b_txt(b, "\\n");
        else if (*c == '\r') b_txt(b, "\\r");
        else if (*c == '\t') b_txt(b, "\\t");
        else if (*c < 0x20) { snprintf(e, sizeof e, "\\u%04x", *c); b_txt(b, e); }
        else b_mem(b, (const char *)c, 1);
    }
    b_txt(b, "\"");
}

static void b_ruta(Buf *b, const char *archivo) {
    char r[1024];
    if (!archivo) { b_txt(b, "null"); return; }
    ruta_ide(archivo, r, sizeof r);
    b_json(b, r);
}

/* Envía una línea al IDE y la suelta */
static void emitir(Buf *b) {
    fflush(stdout);
    printf("@lince %s\n", b->p ? b->p : "{}");
    fflush(stdout);
    free(b->p);
    b->p = NULL; b->n = b->cap = 0;
}

static void responder_error(const char *msg) {
    Buf b = {0};
    b_txt(&b, "{\"respuesta\":\"error\",\"mensaje\":");
    b_json(&b, msg);
    b_txt(&b, "}");
    emitir(&b);
}

/* ── Valores escritos como en el código, en una línea ── */
#define LARGO_VALOR 200

static const char *nombre_tipo(Valor *v) {
    switch (v->tipo) {
        case VAL_NUMERO:      return "numero";
        case VAL_TEXTO:       return "texto";
        case VAL_BOOLEANO:    return "logico";
        case VAL_NULO:        return "nulo";
        case VAL_FUNCION:     return "funcion";
        case VAL_LISTA:       return "lista";
        case VAL_DICCIONARIO: return v->es_modulo ? "modulo" : "diccionario";
        case VAL_CLASE:       return "clase";
        case VAL_OBJETO:      return v->objeto->clase ? v->objeto->clase->nombre : "objeto";
        case VAL_ERROR:       return "error";
        case VAL_GENERADOR:   return "generador";
        case VAL_BYTES:       return "bytes";
        case VAL_TAREA:       return "tarea";
        case VAL_ENUMERADO:   return "enumeracion";
    }
    return "?";
}

static void b_texto_lince(Buf *b, const char *s) {
    b_txt(b, "\"");
    for (const char *c = s; *c && b->n <= LARGO_VALOR; c++) {
        if (*c == '"')       b_txt(b, "\\\"");
        else if (*c == '\\') b_txt(b, "\\\\");
        else if (*c == '\n') b_txt(b, "\\n");
        else if (*c == '\t') b_txt(b, "\\t");
        else if (*c == '\r') b_txt(b, "\\r");
        else b_mem(b, c, 1);
    }
    b_txt(b, "\"");
}

static void b_valor(Buf *b, Valor *v) {
    if (b->n > LARGO_VALOR) return;
    if (!v) { b_txt(b, "nulo"); return; }
    switch (v->tipo) {
        case VAL_TEXTO: b_texto_lince(b, v->texto); return;
        case VAL_LISTA:
            b_txt(b, "[");
            for (int i = 0; i < v->lista.cantidad && b->n <= LARGO_VALOR; i++) {
                if (i) b_txt(b, ", ");
                b_valor(b, v->lista.elementos[i]);
            }
            b_txt(b, "]");
            return;
        case VAL_DICCIONARIO:
            b_txt(b, "{");
            for (int i = 0; i < v->diccionario.cantidad && b->n <= LARGO_VALOR; i++) {
                if (i) b_txt(b, ", ");
                b_texto_lince(b, v->diccionario.claves[i]);
                b_txt(b, ": ");
                b_valor(b, v->diccionario.valores[i]);
            }
            b_txt(b, "}");
            return;
        case VAL_OBJETO: {
            ObjetoLince *o = v->objeto;
            b_txt(b, o->clase ? o->clase->nombre : "objeto");
            b_txt(b, " {");
            for (int i = 0; i < o->num_campos && b->n <= LARGO_VALOR; i++) {
                if (i) b_txt(b, ", ");
                b_txt(b, o->campos_nombres[i]);
                b_txt(b, ": ");
                b_valor(b, o->campos_valores[i]);
            }
            b_txt(b, "}");
            return;
        }
        case VAL_FUNCION:
            b_txt(b, "funcion ");
            b_txt(b, v->funcion && v->funcion->nombre ? v->funcion->nombre : "(anónima)");
            return;
        case VAL_CLASE:
            b_txt(b, "clase ");
            b_txt(b, v->clase->nombre);
            return;
        case VAL_ERROR:
            b_txt(b, v->error->tipo);
            b_txt(b, "(");
            b_texto_lince(b, v->error->mensaje);
            b_txt(b, ")");
            return;
        default: {
            char *t = valor_a_texto_repl(v);
            b_txt(b, t);
            free(t);
        }
    }
}

/* El valor en una línea, cortado hacia los 200 caracteres (sin partir una letra) */
static char *valor_corto(Valor *v) {
    Buf b = {0};
    b_valor(&b, v);
    if (!b.p) return strdup("");
    if (b.n > LARGO_VALOR) {
        size_t k = LARGO_VALOR;
        while (k > 0 && ((unsigned char)b.p[k] & 0xC0) == 0x80) k--;
        b.p[k] = '\0';
        b.n = k;
        b_txt(&b, "…");
    }
    return b.p;
}

static void b_variable(Buf *b, const char *nombre, Valor *v, int *primera) {
    if (!*primera) b_txt(b, ",");
    *primera = 0;
    char *t = valor_corto(v);
    b_txt(b, "{\"nombre\":"); b_json(b, nombre);
    b_txt(b, ",\"tipo\":");   b_json(b, nombre_tipo(v));
    b_txt(b, ",\"valor\":");  b_json(b, t);
    b_txt(b, "}");
    free(t);
}

/* ── Puntos de parada ── */
static int hay_parada(const char *archivo, int linea) {
    char rel[1024]; int hecha = 0;
    for (int i = 0; i < _num_paradas; i++) {
        if (_paradas[i].linea != linea) continue;
        const char *pa = _paradas[i].archivo;
        if (!pa) return 1;
        if (!hecha) { ruta_ide(archivo, rel, sizeof rel); hecha = 1; }
        if (strchr(pa, '/') ? !strcmp(pa, rel) : !strcmp(pa, base(rel))) return 1;
    }
    return 0;
}

/* "ruta:línea" o "línea" (en 'archivo'). 0 si no se entiende. */
static int leer_sitio(const char *arg, const char *archivo, char *ruta, size_t max, int *linea) {
    char copia[1024];
    snprintf(copia, sizeof copia, "%s", arg);
    char *dos = strrchr(copia, ':');
    ruta[0] = '\0';
    if (dos) {
        *dos = '\0';
        *linea = atoi(dos + 1);
        if (!copia[0]) return 0;
        ruta_dada(copia, ruta, max);
    } else {
        *linea = atoi(copia);
        if (archivo) ruta_ide(archivo, ruta, max);
    }
    return *linea >= 1;
}

static void poner_parada(const char *arg, const char *archivo) {
    char ruta[1024]; int linea;
    /* En la terminal, "b 12" vale en cualquier fichero, como siempre */
    if (!leer_sitio(arg, _ide ? archivo : NULL, ruta, sizeof ruta, &linea)) {
        if (_ide) responder_error("Uso: b <ruta>:<línea>");
        else printf("Uso: b <línea>  o  b <archivo>:<línea>\n");
        return;
    }
    if (_num_paradas >= MAX_PARADAS) {
        if (_ide) responder_error("Demasiados puntos de parada.");
        else printf("Demasiados puntos de parada.\n");
        return;
    }
    _paradas[_num_paradas].archivo = ruta[0] ? strdup(ruta) : NULL;
    _paradas[_num_paradas].linea = linea;
    _num_paradas++;
    if (_ide) {
        Buf b = {0};
        b_txt(&b, "{\"respuesta\":\"parada\",\"archivo\":");
        b_json(&b, ruta[0] ? ruta : NULL);
        char l[32]; snprintf(l, sizeof l, ",\"linea\":%d}", linea);
        b_txt(&b, l);
        emitir(&b);
    } else {
        printf("Punto de parada %d: %s%s%d\n", _num_paradas, ruta, ruta[0] ? ":" : "", linea);
    }
}

static void quitar_parada_n(int k) {
    free(_paradas[k].archivo);
    for (int i = k; i < _num_paradas - 1; i++) _paradas[i] = _paradas[i + 1];
    _num_paradas--;
}

static void quitar_parada(const char *arg, const char *archivo) {
    if (!_ide) {
        int k = atoi(arg);
        if (k < 1 || k > _num_paradas) { printf("  No existe ese punto de parada.\n"); return; }
        quitar_parada_n(k - 1);
        return;
    }
    char ruta[1024]; int linea;
    if (!leer_sitio(arg, archivo, ruta, sizeof ruta, &linea)) {
        responder_error("Uso: d <ruta>:<línea>");
        return;
    }
    int quitadas = 0;
    for (int i = _num_paradas - 1; i >= 0; i--)
        if (_paradas[i].linea == linea && _paradas[i].archivo && !strcmp(_paradas[i].archivo, ruta)) {
            quitar_parada_n(i);
            quitadas++;
        }
    if (!quitadas) { responder_error("No hay un punto de parada ahí."); return; }
    Buf b = {0};
    b_txt(&b, "{\"respuesta\":\"quitada\",\"archivo\":");
    b_json(&b, ruta);
    char l[32]; snprintf(l, sizeof l, ",\"linea\":%d}", linea);
    b_txt(&b, l);
    emitir(&b);
}

/* ── Inspección ── */
static void mostrar_valor(const char *nombre, Valor *v) {
    char *t = valor_a_texto_repl(v);
    printf("  %s = %s\n", nombre, t);
    free(t);
}

static int visto_antes(Entorno *desde, Entorno *hasta, int j, const char *nombre) {
    for (Entorno *x = desde; x; x = x->padre) {
        int fin = x == hasta ? j : x->cantidad;
        for (int i = 0; i < fin; i++) if (!strcmp(x->vars[i].nombre, nombre)) return 1;
        if (x == hasta) break;
    }
    return 0;
}

/* Las locales, de dentro afuera; una tapada por otra de más dentro no sale */
static void mostrar_variables(Entorno *e) {
    Buf b = {0};
    int primera = 1, hubo = 0;
    if (_ide) b_txt(&b, "{\"respuesta\":\"variables\",\"variables\":[");
    for (Entorno *x = e; x && x != _global; x = x->padre)
        for (int i = 0; i < x->cantidad; i++) {
            if (visto_antes(e, x, i, x->vars[i].nombre)) continue;
            if (_ide) b_variable(&b, x->vars[i].nombre, x->vars[i].valor, &primera);
            else mostrar_valor(x->vars[i].nombre, x->vars[i].valor);
            hubo = 1;
        }
    if (_ide) { b_txt(&b, "]}"); emitir(&b); }
    else if (!hubo) printf("  (sin variables locales)\n");
}

static void mostrar_globales(void) {
    Buf b = {0};
    int primera = 1, hubo = 0;
    if (_ide) b_txt(&b, "{\"respuesta\":\"variables\",\"variables\":[");
    for (int i = 0; i < _global->cantidad; i++) {
        Valor *v = _global->vars[i].valor;
        if (v->tipo == VAL_FUNCION || v->tipo == VAL_CLASE || v->es_modulo) continue;
        if (_ide) b_variable(&b, _global->vars[i].nombre, v, &primera);
        else mostrar_valor(_global->vars[i].nombre, v);
        hubo = 1;
    }
    if (_ide) { b_txt(&b, "]}"); emitir(&b); }
    else if (!hubo) printf("  (sin variables globales)\n");
}

static void pila(void) {
    Marco m[MAX_MARCOS];
    int n = marcos(m, MAX_MARCOS);
    if (!_ide) {
        for (int i = 0; i < n; i++)
            printf("  #%d  %s%s  %s:%d\n", i, m[i].funcion ? m[i].funcion : "(principal)",
                   m[i].funcion ? "()" : "", base(m[i].archivo), m[i].linea);
        return;
    }
    Buf b = {0};
    b_txt(&b, "{\"respuesta\":\"pila\",\"marcos\":[");
    for (int i = 0; i < n; i++) {
        b_txt(&b, i ? ",{\"funcion\":" : "{\"funcion\":");
        b_json(&b, m[i].funcion);
        b_txt(&b, ",\"archivo\":");
        b_ruta(&b, m[i].archivo);
        char l[32]; snprintf(l, sizeof l, ",\"linea\":%d}", m[i].linea);
        b_txt(&b, l);
    }
    b_txt(&b, "]}");
    emitir(&b);
}

static void hook(Nodo *s, Entorno *e);
static void hook_error(Nodo *s, Entorno *e);

/* Evalúa una expresión en el entorno actual y la muestra */
static void evaluar(const char *texto, Entorno *e) {
    int linea = 0;
    char msg[256];
    /* Lo que no es una sentencia (1 + 1, [a, b], "x") se evalúa como el valor
       de una declaración: sea _ = expresión */
    char *envuelto = NULL;
    if (lsp_comprobar_texto(texto, &linea, msg, sizeof msg)) {
        size_t n = strlen(texto) + 16;
        envuelto = malloc(n);
        snprintf(envuelto, n, "sea _p_ = %s", texto);
        int l2; char m2[256];
        if (lsp_comprobar_texto(envuelto, &l2, m2, sizeof m2)) { free(envuelto); envuelto = NULL; }
    }
    if (!envuelto && lsp_comprobar_texto(texto, &linea, msg, sizeof msg)) {
        if (_ide) {
            Buf b = {0};
            char t[300]; snprintf(t, sizeof t, "ErrorSintaxis: %s", msg);
            b_txt(&b, "{\"respuesta\":\"valor\",\"error\":"); b_json(&b, t); b_txt(&b, "}");
            emitir(&b);
        } else printf("  Error de sintaxis: %s\n", msg);
        return;
    }
    Lexer  *lx = lexer_crear(envuelto ? envuelto : texto);
    int     cnt = 0;
    Token  *tok = lexer_tokenizar(lx, &cnt);
    Parser *pa = parser_crear(tok, cnt);
    Nodo   *ast = parser_parsear(pa);
    if (envuelto && ast->bloque.cantidad == 1) ast = ast->bloque.sentencias[0]->declaracion.valor;

    /* Parado en un error sin capturar: se aparta mientras tanto */
    int    error_antes = hay_error;
    Valor *valor_antes = valor_error;
    hay_error = 0;
    valor_error = NULL;

    /* Sin los hooks: lo que se evalúa no debe disparar paradas */
    interprete_fijar_hook(NULL);
    interprete_fijar_hook_error(NULL);
    Valor *r = ejecutar_nodo(ast, e);
    interprete_fijar_hook(hook);
    interprete_fijar_hook_error(hook_error);
    Buf b = {0};
    if (hay_error) {
        if (_ide) {
            char t[600];
            snprintf(t, sizeof t, "%s: %s", valor_error->error->tipo, valor_error->error->mensaje);
            b_txt(&b, "{\"respuesta\":\"valor\",\"error\":"); b_json(&b, t); b_txt(&b, "}");
            emitir(&b);
        } else printf("  %s: %s\n", valor_error->error->tipo, valor_error->error->mensaje);
        hay_error = 0;
        valor_destruir(valor_error);
        valor_error = NULL;
    } else if (_ide) {
        char *t = valor_corto(r);
        b_txt(&b, "{\"respuesta\":\"valor\",\"tipo\":"); b_json(&b, nombre_tipo(r));
        b_txt(&b, ",\"valor\":"); b_json(&b, t); b_txt(&b, "}");
        free(t);
        emitir(&b);
    } else if (r && r->tipo != VAL_NULO) {
        char *t = valor_a_texto_repl(r);
        printf("  %s\n", t);
        free(t);
    }
    valor_destruir(r);
    hay_error = error_antes;
    valor_error = valor_antes;
    /* No se destruye el árbol: lo definido en la expresión (funciones) puede usarlo */
    parser_destruir(pa);
    lexer_destruir(lx);
}

static void ayuda(void) {
    printf("  s, paso          ejecuta la siguiente línea (entrando en las funciones)\n"
           "  n, siguiente     ejecuta la siguiente línea sin entrar en las funciones\n"
           "  o, salir         sigue hasta salir de la función actual\n"
           "  c, continuar     sigue hasta el siguiente punto de parada\n"
           "  b <línea>        punto de parada (o b <archivo>:<línea>)\n"
           "  d <n>            borra el punto de parada n;  bs  los lista\n"
           "  l [línea]        muestra el código alrededor de la línea\n"
           "  p <expresión>    evalúa una expresión (también asigna: p x = 5)\n"
           "  v [n]            variables locales (del marco n de bt);  g  globales\n"
           "  bt               pila de llamadas\n"
           "  q                termina el programa\n");
}

/* ── La entrada del IDE ──
   Un hilo lee stdin sin parar y deja las líneas en una cola: así «pausa»
   llega aunque el programa esté corriendo (en Windows no hay señales que
   sirvan). El hook mira en cada sentencia si hay algo en la cola. */
typedef struct Linea { char *texto; struct Linea *sig; } Linea;
static Linea *_cola = NULL, **_cola_fin = &_cola;
static volatile int _en_cola = 0;
static int _fin_entrada = 0;

#ifdef _WIN32
static CRITICAL_SECTION   _cerrojo;
static CONDITION_VARIABLE _hay_linea;
#define CERRAR()  EnterCriticalSection(&_cerrojo)
#define ABRIR()   LeaveCriticalSection(&_cerrojo)
#define AVISAR()  WakeConditionVariable(&_hay_linea)
#define ESPERAR() SleepConditionVariableCS(&_hay_linea, &_cerrojo, INFINITE)
#else
static pthread_mutex_t _cerrojo = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  _hay_linea = PTHREAD_COND_INITIALIZER;
#define CERRAR()  pthread_mutex_lock(&_cerrojo)
#define ABRIR()   pthread_mutex_unlock(&_cerrojo)
#define AVISAR()  pthread_cond_signal(&_hay_linea)
#define ESPERAR() pthread_cond_wait(&_hay_linea, &_cerrojo)
#endif

/* texto NULL: se acabó la entrada */
static void encolar(char *texto) {
    Linea *l = malloc(sizeof *l);
    l->texto = texto;
    l->sig = NULL;
    CERRAR();
    *_cola_fin = l;
    _cola_fin = &l->sig;
    __atomic_add_fetch(&_en_cola, 1, __ATOMIC_SEQ_CST);
    AVISAR();
    ABRIR();
}

#ifdef _WIN32
static DWORD WINAPI lector(LPVOID arg)
#else
static void *lector(void *arg)
#endif
{
    (void)arg;
    char linea[4096];
    while (fgets(linea, sizeof linea, stdin)) {
        linea[strcspn(linea, "\r\n")] = '\0';
        encolar(strdup(linea));
    }
    encolar(NULL);
    return 0;
}

static void lector_empezar(void) {
#ifdef _WIN32
    InitializeCriticalSection(&_cerrojo);
    InitializeConditionVariable(&_hay_linea);
    HANDLE h = CreateThread(NULL, 0, lector, NULL, 0, NULL);
    if (h) CloseHandle(h);
#else
    pthread_t h;
    if (pthread_create(&h, NULL, lector, NULL) == 0) pthread_detach(h);
#endif
}

/* La siguiente línea de la cola; espera si 'esperar'. 0 si no hay o se acabó. */
static int desencolar(char *out, size_t max, int esperar) {
    if (_fin_entrada) return 0;
    CERRAR();
    while (!_cola && esperar) ESPERAR();
    Linea *l = _cola;
    if (l) {
        _cola = l->sig;
        if (!_cola) _cola_fin = &_cola;
        __atomic_sub_fetch(&_en_cola, 1, __ATOMIC_SEQ_CST);
    }
    ABRIR();
    if (!l) return 0;
    if (!l->texto) { _fin_entrada = 1; free(l); return 0; }
    snprintf(out, max, "%s", l->texto);
    free(l->texto);
    free(l);
    return 1;
}

/* Una línea de órdenes. 0 si se acabó la entrada. */
static int leer_orden(char *out, size_t max) {
    if (_ide) return desencolar(out, max, 1);
    printf("(lince-dbg) ");
    fflush(stdout);
    if (!fgets(out, (int)max, stdin)) { printf("\n"); _fin_entrada = 1; return 0; }
    out[strcspn(out, "\r\n")] = '\0';
    return 1;
}

static void terminar(int codigo) {
    if (_ide) {
        Buf b = {0};
        char t[64]; snprintf(t, sizeof t, "{\"evento\":\"terminado\",\"codigo\":%d}", codigo);
        b_txt(&b, t);
        emitir(&b);
    } else printf("\nPrograma terminado.\n");
}

static void separar(char *entrada, char **cmd, char **arg) {
    char *c = entrada;
    while (*c == ' ') c++;
    char *a = c;
    while (*a && *a != ' ') a++;
    if (*a) { *a++ = '\0'; while (*a == ' ') a++; }
    *cmd = c; *arg = a;
}

/* Parado en una sentencia: atiende órdenes hasta que una hace seguir */
static void parado(const char *motivo, const char *mensaje, const char *archivo, int linea,
                   Entorno *e, int prof) {
    _primera = 0;
    if (_ide) {
        Buf b = {0};
        b_txt(&b, "{\"evento\":\"parado\",\"motivo\":"); b_json(&b, motivo);
        b_txt(&b, ",\"archivo\":"); b_ruta(&b, archivo);
        char l[32]; snprintf(l, sizeof l, ",\"linea\":%d", linea);
        b_txt(&b, l);
        if (mensaje) { b_txt(&b, ",\"mensaje\":"); b_json(&b, mensaje); }
        b_txt(&b, "}");
        emitir(&b);
    } else {
        if (!strcmp(motivo, "parada")) printf("\nPunto de parada en %s:%d\n", base(archivo), linea);
        if (mensaje) printf("\n❌ %s (sin capturar: al seguir, el programa acaba)\n", mensaje);
        mostrar_linea(archivo, linea);
    }

    char entrada[4096];
    for (;;) {
        if (!leer_orden(entrada, sizeof entrada)) {   /* sin entrada: seguir hasta el final */
            _modo = M_CONTINUAR;
            _num_paradas = 0;
            return;
        }
        char *cmd, *arg;
        separar(entrada, &cmd, &arg);

        if (!*cmd) continue;
        if (!strcmp(cmd, "s") || !strcmp(cmd, "paso"))              { _modo = M_PASO; return; }
        if (!strcmp(cmd, "n") || !strcmp(cmd, "siguiente"))         { _modo = M_SIGUIENTE; _ref = prof; return; }
        if (!strcmp(cmd, "o") || !strcmp(cmd, "salir"))             { _modo = M_SALIR; _ref = prof; return; }
        if (!strcmp(cmd, "c") || !strcmp(cmd, "continuar"))         { _modo = M_CONTINUAR; return; }
        if (!strcmp(cmd, "pausa")) continue;                        /* ya lo está */
        if (!strcmp(cmd, "b"))  { poner_parada(arg, archivo); continue; }
        if (!strcmp(cmd, "bs") && !_ide) {
            if (!_num_paradas) printf("  (sin puntos de parada)\n");
            for (int i = 0; i < _num_paradas; i++)
                printf("  %d: %s%s%d\n", i + 1, _paradas[i].archivo ? _paradas[i].archivo : "",
                       _paradas[i].archivo ? ":" : "", _paradas[i].linea);
            continue;
        }
        if (!strcmp(cmd, "d"))  { quitar_parada(arg, archivo); continue; }
        if (!strcmp(cmd, "l") && !_ide) { listar(archivo, *arg ? atoi(arg) : linea); continue; }
        if (!strcmp(cmd, "p")) {
            if (*arg) evaluar(arg, e);
            else if (_ide) responder_error("Uso: p <expresión>");
            else printf("  Uso: p <expresión>\n");
            continue;
        }
        if (!strcmp(cmd, "v")) {
            Entorno *ev = e;
            if (*arg) {
                Marco m[MAX_MARCOS];
                int n = marcos(m, MAX_MARCOS), k = atoi(arg);
                if (k < 0 || k >= n) {
                    if (_ide) responder_error("No existe ese marco de la pila.");
                    else printf("  No existe ese marco de la pila (mira bt).\n");
                    continue;
                }
                ev = m[k].e;
            }
            mostrar_variables(ev);
            continue;
        }
        if (!strcmp(cmd, "g"))  { mostrar_globales(); continue; }
        if (!strcmp(cmd, "bt")) { pila(); continue; }
        if (!strcmp(cmd, "q") || !strcmp(cmd, "salir!")) { terminar(0); exit(0); }
        if (!strcmp(cmd, "h") || !strcmp(cmd, "ayuda")) {
            if (_ide) responder_error("En el modo --ide no hay ayuda: mira ESPECIFICACION.md.");
            else ayuda();
            continue;
        }
        if (_ide) {
            char t[300]; snprintf(t, sizeof t, "Comando desconocido: %.200s", cmd);
            responder_error(t);
        } else printf("  Comando desconocido: %s (h para ver la ayuda)\n", cmd);
    }
}

/* Con el programa corriendo, lo que manda el IDE: 1 si pide una pausa */
static int ordenes_corriendo(const char *archivo) {
    char entrada[4096];
    int pausa = 0;
    while (desencolar(entrada, sizeof entrada, 0)) {
        char *cmd, *arg;
        separar(entrada, &cmd, &arg);
        if (!*cmd) continue;
        if (!strcmp(cmd, "pausa")) pausa = 1;
        else if (!strcmp(cmd, "b")) poner_parada(arg, archivo);
        else if (!strcmp(cmd, "d")) quitar_parada(arg, archivo);
        else if (!strcmp(cmd, "q") || !strcmp(cmd, "salir!")) { terminar(0); exit(0); }
        else if (!strcmp(cmd, "c") || !strcmp(cmd, "continuar")) continue;   /* ya sigue */
        else responder_error("El programa está corriendo: manda «pausa» antes.");
    }
    if (_fin_entrada) _num_paradas = 0;   /* nadie más va a mandar nada */
    return pausa;
}

static const char *archivo_ahora(void) {
    const char *a = interprete_archivo_en_curso();
    return a && a[0] ? a : _principal;
}

static void hook(Nodo *s, Entorno *e) {
    const char *archivo = archivo_ahora();
    int linea = s->linea_ini ? s->linea_ini : s->linea, prof = interprete_profundidad();
    _error_visto = NULL;

    if (prof < MAX_MARCOS) {
        for (int d = prof + 1; d <= _sitios_max && d < MAX_MARCOS; d++) _sitios[d].vale = 0;
        _sitios[prof] = (Sitio){ archivo, linea, e, 1 };
        _sitios_max = prof;
    }

    const char *motivo = NULL;
    if (_modo == M_PASO) motivo = _primera ? "inicio" : "paso";
    else if (_modo == M_SIGUIENTE && prof <= _ref) motivo = "paso";
    else if (_modo == M_SALIR && prof < _ref) motivo = "paso";
    if (!motivo && _ide && _en_cola && ordenes_corriendo(archivo)) motivo = "pausa";
    if (!motivo && _num_paradas && hay_parada(archivo, linea)) motivo = "parada";
    if (!motivo) return;
    parado(motivo, NULL, archivo, linea, e, prof);
}

/* Un error que no va a capturar nadie: se para antes de que acabe el programa */
static void hook_error(Nodo *s, Entorno *e) {
    if (!valor_error || valor_error == _error_visto || valor_error->tipo != VAL_ERROR) return;
    _error_visto = valor_error;
    ErrorLince *err = valor_error->error;
    if (!strcmp(err->tipo, ERROR_RECARGA)) return;
    if (_modo == M_CONTINUAR && _fin_entrada) return;
    char msg[1200];
    snprintf(msg, sizeof msg, "%s: %s", err->tipo, err->mensaje);
    const char *archivo = archivo_ahora();
    int linea = s->linea_ini ? s->linea_ini : s->linea;
    parado("error", msg, archivo, linea, e, interprete_profundidad());
}

int depurador_ejecutar(const char *archivo, int argc, char *argv[], int ide) {
    char *codigo = interprete_leer_fuente(archivo);
    if (!codigo) {
        fprintf(stderr, "\n❌ No se encontró el archivo '%s'.\n\n", archivo);
        return 1;
    }
    _ide = ide;
    _principal = strdup(archivo);
    Lexer      *lexer    = lexer_crear(codigo);
    int         cantidad = 0;
    Token      *tokens   = lexer_tokenizar(lexer, &cantidad);
    Parser     *parser   = parser_crear(tokens, cantidad);
    Nodo       *programa = parser_parsear(parser);
    Interprete *interp   = interprete_crear();
    _global = interp->global;

    interprete_fijar_archivo(archivo);
    interprete_fijar_principal(archivo);
    interprete_set_args(argc, argv);

    if (_ide) lector_empezar();
    else printf("🐆 Depurador de Lince — h para ver la ayuda\n\n");
    interprete_fijar_hook(hook);
    interprete_fijar_hook_error(hook_error);
    int r = interprete_ejecutar_recargable(interp, programa, NULL, 0);
    interprete_fijar_hook(NULL);
    interprete_fijar_hook_error(NULL);
    terminar(r ? 1 : 0);

    interprete_destruir(interp);
    nodo_destruir(programa);
    parser_destruir(parser);
    lexer_destruir(lexer);
    free(codigo);
    return r ? 1 : 0;
}
