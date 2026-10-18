/*
 * 🐆 Lince — depurador.c
 * Depurador interactivo de línea de comandos sobre el hook de sentencias del
 * intérprete.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "depurador.h"
#include "lexer.h"
#include "parser.h"
#include "interprete.h"
#include "lsp.h"

enum { M_PASO, M_SIGUIENTE, M_SALIR, M_CONTINUAR };

typedef struct { char *archivo; int linea; } Parada;
#define MAX_PARADAS 64
static Parada _paradas[MAX_PARADAS];
static int    _num_paradas = 0;

static int     _modo = M_PASO;
static int     _ref = 0;                /* profundidad de referencia de siguiente/salir */
static Entorno *_global = NULL;
static char    *_principal = NULL;      /* ruta del programa principal */

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

static const char *base(const char *ruta) {
    const char *b = strrchr(ruta, '/');
    return b ? b + 1 : ruta;
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

/* ── Puntos de parada ── */
static int hay_parada(const char *archivo, int linea) {
    for (int i = 0; i < _num_paradas; i++)
        if (_paradas[i].linea == linea &&
            (!_paradas[i].archivo || strcmp(base(_paradas[i].archivo), base(archivo)) == 0))
            return 1;
    return 0;
}

static void poner_parada(const char *arg) {
    if (_num_paradas >= MAX_PARADAS) { printf("Demasiados puntos de parada.\n"); return; }
    char *copia = strdup(arg);
    char *dos = strrchr(copia, ':');
    int linea;
    const char *archivo = NULL;
    if (dos) { *dos = '\0'; archivo = copia; linea = atoi(dos + 1); }
    else      linea = atoi(copia);
    if (linea < 1) { printf("Uso: b <línea>  o  b <archivo>:<línea>\n"); free(copia); return; }
    _paradas[_num_paradas].archivo = archivo ? strdup(archivo) : NULL;
    _paradas[_num_paradas].linea = linea;
    _num_paradas++;
    printf("Punto de parada %d: %s%s%d\n", _num_paradas,
           archivo ? archivo : "", archivo ? ":" : "", linea);
    free(copia);
}

/* ── Inspección ── */
static void mostrar_valor(const char *nombre, Valor *v) {
    char *t = valor_a_texto_repl(v);
    printf("  %s = %s\n", nombre, t);
    free(t);
}

static void mostrar_variables(Entorno *e) {
    int hubo = 0;
    for (Entorno *x = e; x && x != _global; x = x->padre)
        for (int i = 0; i < x->cantidad; i++) {
            mostrar_valor(x->vars[i].nombre, x->vars[i].valor);
            hubo = 1;
        }
    if (!hubo) printf("  (sin variables locales)\n");
}

static void mostrar_globales(void) {
    int hubo = 0;
    for (int i = 0; i < _global->cantidad; i++) {
        Valor *v = _global->vars[i].valor;
        if (v->tipo == VAL_FUNCION || v->tipo == VAL_CLASE || v->es_modulo) continue;
        mostrar_valor(_global->vars[i].nombre, v);
        hubo = 1;
    }
    if (!hubo) printf("  (sin variables globales)\n");
}

static void pila(int linea_actual, const char *archivo) {
    int p = interprete_profundidad();
    printf("  #%d  (aquí)  %s:%d\n", p, base(archivo), linea_actual);
    for (int n = p; n >= 1; n--) {
        const char *nombre; int linea;
        if (interprete_pila(n, &nombre, &linea))
            printf("  #%d  %s() llamada desde la línea %d\n", n - 1, nombre ? nombre : "?", linea);
    }
}

static void hook(Nodo *s, Entorno *e);

/* Evalúa una expresión en el entorno actual y la muestra */
static void evaluar(const char *texto, Entorno *e) {
    int linea = 0;
    char msg[256];
    if (lsp_comprobar_texto(texto, &linea, msg, sizeof msg)) {
        printf("  Error de sintaxis: %s\n", msg);
        return;
    }
    Lexer  *lx = lexer_crear(texto);
    int     cnt = 0;
    Token  *tok = lexer_tokenizar(lx, &cnt);
    Parser *pa = parser_crear(tok, cnt);
    Nodo   *ast = parser_parsear(pa);

    /* Sin el hook: lo que se evalúa no debe disparar paradas */
    interprete_fijar_hook(NULL);
    Valor *r = ejecutar_nodo(ast, e);
    interprete_fijar_hook(hook);
    if (hay_error) {
        printf("  %s: %s\n", valor_error->error->tipo, valor_error->error->mensaje);
        hay_error = 0;
        valor_destruir(valor_error);
        valor_error = NULL;
    } else if (r && r->tipo != VAL_NULO) {
        char *t = valor_a_texto_repl(r);
        printf("  %s\n", t);
        free(t);
    }
    valor_destruir(r);
    /* No se destruye el árbol: lo definido en la expresión (funciones) puede usarlo */
    parser_destruir(pa);
    lexer_destruir(lx);
}

static void hook(Nodo *s, Entorno *e);

static void ayuda(void) {
    printf("  s, paso          ejecuta la siguiente línea (entrando en las funciones)\n"
           "  n, siguiente     ejecuta la siguiente línea sin entrar en las funciones\n"
           "  o, salir         sigue hasta salir de la función actual\n"
           "  c, continuar     sigue hasta el siguiente punto de parada\n"
           "  b <línea>        punto de parada (o b <archivo>:<línea>)\n"
           "  d <n>            borra el punto de parada n;  bs  los lista\n"
           "  l [línea]        muestra el código alrededor de la línea\n"
           "  p <expresión>    evalúa una expresión (también asigna: p x = 5)\n"
           "  v                variables locales;  g  variables globales\n"
           "  bt               pila de llamadas\n"
           "  q                termina el programa\n");
}

static void hook(Nodo *s, Entorno *e) {
    const char *archivo = interprete_archivo_actual();
    if (!archivo[0]) archivo = _principal;
    int linea = s->linea_ini ? s->linea_ini : s->linea, prof = interprete_profundidad();

    int parar = 0;
    if (_modo == M_PASO) parar = 1;
    else if (_modo == M_SIGUIENTE && prof <= _ref) parar = 1;
    else if (_modo == M_SALIR && prof < _ref) parar = 1;
    if (!parar && !hay_parada(archivo, linea)) return;
    if (!parar) printf("\nPunto de parada en %s:%d\n", base(archivo), linea);

    mostrar_linea(archivo, linea);
    char entrada[1024];
    for (;;) {
        printf("(lince-dbg) ");
        fflush(stdout);
        if (!fgets(entrada, sizeof entrada, stdin)) {      /* sin entrada: seguir hasta el final */
            printf("\n");
            _modo = M_CONTINUAR;
            _num_paradas = 0;
            return;
        }
        entrada[strcspn(entrada, "\r\n")] = '\0';
        char *cmd = entrada;
        while (*cmd == ' ') cmd++;
        char *arg = cmd;
        while (*arg && *arg != ' ') arg++;
        if (*arg) { *arg++ = '\0'; while (*arg == ' ') arg++; }

        if (!*cmd) continue;
        if (!strcmp(cmd, "s") || !strcmp(cmd, "paso"))              { _modo = M_PASO; return; }
        if (!strcmp(cmd, "n") || !strcmp(cmd, "siguiente"))         { _modo = M_SIGUIENTE; _ref = prof; return; }
        if (!strcmp(cmd, "o") || !strcmp(cmd, "salir"))             { _modo = M_SALIR; _ref = prof; return; }
        if (!strcmp(cmd, "c") || !strcmp(cmd, "continuar"))         { _modo = M_CONTINUAR; return; }
        if (!strcmp(cmd, "b"))  { poner_parada(arg); continue; }
        if (!strcmp(cmd, "bs")) {
            if (!_num_paradas) printf("  (sin puntos de parada)\n");
            for (int i = 0; i < _num_paradas; i++)
                printf("  %d: %s%s%d\n", i + 1, _paradas[i].archivo ? _paradas[i].archivo : "",
                       _paradas[i].archivo ? ":" : "", _paradas[i].linea);
            continue;
        }
        if (!strcmp(cmd, "d")) {
            int k = atoi(arg);
            if (k < 1 || k > _num_paradas) { printf("  No existe ese punto de parada.\n"); continue; }
            free(_paradas[k - 1].archivo);
            for (int i = k - 1; i < _num_paradas - 1; i++) _paradas[i] = _paradas[i + 1];
            _num_paradas--;
            continue;
        }
        if (!strcmp(cmd, "l"))  { listar(archivo, *arg ? atoi(arg) : linea); continue; }
        if (!strcmp(cmd, "p"))  { if (*arg) evaluar(arg, e); else printf("  Uso: p <expresión>\n"); continue; }
        if (!strcmp(cmd, "v"))  { mostrar_variables(e); continue; }
        if (!strcmp(cmd, "g"))  { mostrar_globales(); continue; }
        if (!strcmp(cmd, "bt")) { pila(linea, archivo); continue; }
        if (!strcmp(cmd, "q") || !strcmp(cmd, "salir!")) { printf("Programa terminado.\n"); exit(0); }
        if (!strcmp(cmd, "h") || !strcmp(cmd, "ayuda")) { ayuda(); continue; }
        printf("  Comando desconocido: %s (h para ver la ayuda)\n", cmd);
    }
}

int depurador_ejecutar(const char *archivo, int argc, char *argv[]) {
    char *codigo = interprete_leer_fuente(archivo);
    if (!codigo) {
        fprintf(stderr, "\n❌ No se encontró el archivo '%s'.\n\n", archivo);
        return 1;
    }
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

    printf("🐆 Depurador de Lince — h para ver la ayuda\n\n");
    interprete_fijar_hook(hook);
    interprete_ejecutar(interp, programa);
    interprete_fijar_hook(NULL);
    printf("\nPrograma terminado.\n");

    interprete_destruir(interp);
    nodo_destruir(programa);
    parser_destruir(parser);
    lexer_destruir(lexer);
    free(codigo);
    return 0;
}
