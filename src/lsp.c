/*
 * 🐆 Lince — lsp.c
 *
 * Servidor de lenguaje mínimo y sin dependencias. Entiende:
 *   - diagnósticos: los errores de sintaxis salen subrayados al escribir
 *   - autocompletado: palabras clave, funciones y módulos estándar,
 *     lo definido en el propio archivo, y los miembros tras 'modulo.' / 'mi.'
 *   - símbolos del documento e ir a la definición
 *
 * El parser de Lince termina el proceso en el primer error (no se puede
 * reanudar), así que la comprobación se hace en un proceso hijo y su mensaje
 * se lee por una tubería. El resto trabaja sobre el texto con un escáner de
 * líneas, que sigue funcionando con el código a medias.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lsp.h"
#include "lexer.h"
#include "parser.h"
#include "interprete.h"
#include "version.h"

static const char *_ruta_analizada = NULL;   /* ver lsp_fijar_ruta */

/* ── Las enumeraciones de los archivos importados ('importar "./x"'), para
   el aviso de un 'elegir' incompleto. Se siguen también sus importaciones. */
typedef struct { Parser *p; char vistos[16][1024]; int nvistos; int nivel; char desde[1024]; } Importados;

static void seguir_import(const char *nombre, void *ctx) {
    Importados *im = ctx;
    if (strncmp(nombre, "./", 2) != 0 && strncmp(nombre, "../", 3) != 0) return;
    char ruta[1024];
    if (im->nivel >= 8 || im->nvistos >= 16 || !interprete_resolver_import(im->desde, nombre, ruta, sizeof ruta)) return;
    for (int i = 0; i < im->nvistos; i++) if (strcmp(im->vistos[i], ruta) == 0) return;
    snprintf(im->vistos[im->nvistos++], sizeof im->vistos[0], "%s", ruta);
    char *texto = interprete_leer_fuente(ruta);
    if (!texto) return;
    char desde[1024];
    memcpy(desde, im->desde, sizeof desde);
    snprintf(im->desde, sizeof im->desde, "%s", ruta);
    im->nivel++;
    parser_conocer_enumeraciones(im->p, texto, seguir_import, im);
    im->nivel--;
    memcpy(im->desde, desde, sizeof desde);
    free(texto);
}

void lsp_conocer_importados(Parser *p, const char *texto, const char *ruta) {
    if (!ruta || !ruta[0]) return;
    Importados *im = calloc(1, sizeof(Importados));
    im->p = p;
    snprintf(im->desde, sizeof im->desde, "%s", ruta);
    /* Del archivo principal interesan sus 'importar' (sus enumeraciones las
       vuelve a apuntar, pero ya estaban: se usa la primera) */
    parser_conocer_enumeraciones(p, texto, seguir_import, im);
    free(im);
}

void lsp_fijar_ruta(const char *ruta) { _ruta_analizada = ruta; }

static void avisar_stderr(int linea, const char *mensaje) {
    fprintf(stderr, "⚠️  Aviso en línea %d: %s\n", linea, mensaje);
}

void lsp_analizar(const char *texto) {
    Lexer  *l = lexer_crear(texto);
    int     n = 0;
    Token  *t = lexer_tokenizar(l, &n);
    Parser *p = parser_crear(t, n);
    parser_parsear(p);         /* termina el proceso si hay un error */
    lsp_conocer_importados(p, texto, _ruta_analizada);
    parser_avisos(p, avisar_stderr);
    fflush(stderr);
}

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif
#include "interprete.h"
#include "version.h"
#include "modulos.h"

/* ─────────────────────────────────────────
   Comprobación de sintaxis
   Se hace en otro proceso porque el parser termina en el primer error.
   Deja en buf lo que el parser escribió en stderr (el error, o si no lo hay,
   los avisos); devuelve 1 si hubo error de sintaxis, 0 si el texto es
   correcto o el proceso no terminó bien.
───────────────────────────────────────── */
#ifdef _WIN32
static int analizar_aparte(const char *texto, char *buf, size_t max) {
    /* Windows no tiene fork: el texto va a un archivo temporal y se comprueba
       con 'lince comprobar' en un proceso nuevo del propio ejecutable. */
    char dir[MAX_PATH], tmp[MAX_PATH], exe[MAX_PATH];
    if (!GetTempPathA(sizeof dir, dir) || !GetTempFileNameA(dir, "lnc", 0, tmp)) return 0;
    if (!GetModuleFileNameA(NULL, exe, sizeof exe)) return 0;
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    fwrite(texto, 1, strlen(texto), f);
    fclose(f);

    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE lee = NULL, escribe = NULL;
    if (!CreatePipe(&lee, &escribe, &sa, 1 << 16)) { DeleteFileA(tmp); return 0; }
    SetHandleInformation(lee, HANDLE_FLAG_INHERIT, 0);
    HANDLE nulo = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    char cmd[3 * MAX_PATH];
    if (_ruta_analizada) snprintf(cmd, sizeof cmd, "\"%s\" comprobar \"%s\" \"%s\"", exe, tmp, _ruta_analizada);
    else snprintf(cmd, sizeof cmd, "\"%s\" comprobar \"%s\"", exe, tmp);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = NULL;
    si.hStdOutput = nulo;
    si.hStdError = escribe;
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(escribe);
    if (nulo != INVALID_HANDLE_VALUE) CloseHandle(nulo);
    if (!ok) { CloseHandle(lee); DeleteFileA(tmp); return 0; }

    DWORD codigo = 0;
    if (WaitForSingleObject(pi.hProcess, 3000) == WAIT_TIMEOUT) {   /* parser colgado */
        TerminateProcess(pi.hProcess, 99);
        WaitForSingleObject(pi.hProcess, 1000);
    }
    GetExitCodeProcess(pi.hProcess, &codigo);
    size_t total = 0;
    DWORD r;
    while (total < max - 1 && ReadFile(lee, buf + total, (DWORD)(max - 1 - total), &r, NULL) && r > 0)
        total += r;
    buf[total] = '\0';
    CloseHandle(lee);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DeleteFileA(tmp);
    if (codigo == 0 || codigo == 99) return 0;
    return buf[0] != '\0';
}
#else
static int analizar_aparte(const char *texto, char *buf, size_t max) {
    int fds[2];
    if (pipe(fds) != 0) return 0;
    /* No fflush(NULL): esperaría a que acabe de leer stdin otro hilo (el del
       depurador del IDE) */
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return 0; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], 2);
        alarm(3);                         /* un parser colgado no cuelga al servidor */
        lsp_analizar(texto);
        _exit(0);
    }
    close(fds[1]);
    size_t total = 0;
    ssize_t r;
    while ((r = read(fds[0], buf + total, max - 1 - total)) > 0) {
        total += (size_t)r;
        if (total >= max - 1) break;
    }
    buf[total] = '\0';
    close(fds[0]);
    int estado = 0;
    waitpid(pid, &estado, 0);
    if (WIFEXITED(estado) && WEXITSTATUS(estado) == 0) return 0;
    /* muerto por señal (alarma, fallo del parser): no es un error de sintaxis */
    if (WIFSIGNALED(estado) && buf[0] == '\0') return 0;
    return 1;
}
#endif

static void quitar_retornos(char *buf) {    /* en Windows stderr sale con \r\n */
    char *w = buf;
    for (char *r = buf; *r; r++) if (*r != '\r') *w++ = *r;
    *w = '\0';
}

static void leer_error(char *buf, int *linea, char *mensaje, size_t max);

int lsp_comprobar_texto(const char *texto, int *linea, char *mensaje, size_t max) {
    char buf[4096];
    if (!analizar_aparte(texto, buf, sizeof buf)) return 0;
    quitar_retornos(buf);
    leer_error(buf, linea, mensaje, max);
    return 1;
}

/* "❌ Error en línea N:\n   mensaje..." → la línea y el mensaje en una sola línea */
static void leer_error(char *buf, int *linea, char *mensaje, size_t max) {
    /* "❌ Error en línea N:\n   mensaje\n   mensaje" */
    *linea = 1;
    const char *lp = strstr(buf, "línea ");
    if (lp) *linea = atoi(lp + strlen("línea "));
    if (*linea < 1) *linea = 1;

    /* Mensaje: todo lo que sigue a la primera línea, sin sangrías ni vacías */
    mensaje[0] = '\0';
    char *ini_buf = buf;
    while (*ini_buf == '\n' || *ini_buf == ' ') ini_buf++;
    memmove(buf, ini_buf, strlen(ini_buf) + 1);
    char *nl = strchr(buf, '\n');
    char *resto = nl ? nl + 1 : buf;
    {
        /* "❌ Error en línea 5: texto" o "❌ Error en línea 5:\n   texto" */
        const char *lp2 = strstr(buf, "línea ");
        const char *dp = lp2 ? strchr(lp2, ':') : NULL;
        if (dp && (!nl || dp < nl)) resto = (char *)(dp + 1);
    }
    size_t m = 0;
    for (char *s = resto; *s && m + 2 < max; s++) {
        if (*s == '\n') {
            while (m > 0 && mensaje[m-1] == ' ') m--;
            if (m > 0 && mensaje[m-1] != ' ') mensaje[m++] = ' ';
            while (s[1] == ' ' || s[1] == '\n') s++;
            continue;
        }
        if (m == 0 && (*s == ' ')) continue;
        mensaje[m++] = *s;
    }
    while (m > 0 && mensaje[m-1] == ' ') m--;
    mensaje[m] = '\0';
    if (!mensaje[0]) snprintf(mensaje, max, "Error de sintaxis.");
}

/* ─────────────────────────────────────────
   Búfer de texto creciente y JSON
───────────────────────────────────────── */
typedef struct { char *d; size_t n, cap; } Buf;

static void buf_bytes(Buf *b, const char *s, size_t k) {
    if (b->n + k + 1 > b->cap) {
        b->cap = (b->n + k + 1) * 2;
        b->d = realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, s, k);
    b->n += k;
    b->d[b->n] = '\0';
}
static void buf_str(Buf *b, const char *s) { buf_bytes(b, s, strlen(s)); }
static void buf_fmt(Buf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void buf_fmt(Buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    buf_str(b, tmp);
}
/* Un texto como cadena JSON (con comillas). */
static void buf_json(Buf *b, const char *s) {
    buf_bytes(b, "\"", 1);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"':  buf_str(b, "\\\""); break;
            case '\\': buf_str(b, "\\\\"); break;
            case '\n': buf_str(b, "\\n");  break;
            case '\r': buf_str(b, "\\r");  break;
            case '\t': buf_str(b, "\\t");  break;
            default:
                if (c < 0x20) buf_fmt(b, "\\u%04x", c);
                else buf_bytes(b, (const char *)&c, 1);
        }
    }
    buf_bytes(b, "\"", 1);
}

static const char *saltar_ws(const char *p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    return p;
}

/* Valor que sigue a la clave "clave" (un objeto de JSON), o NULL. Recorre el JSON
   saltando cadenas enteras, así que el texto del usuario nunca se toma por una
   clave. 'prof' es la profundidad exigida (1 = el objeto raíz) o -1 si da igual. */
static const char *json_buscar(const char *json, const char *clave, int prof) {
    size_t kl = strlen(clave);
    int d = 0;
    const char *p = json;
    while (*p) {
        if (*p == '{' || *p == '[') { d++; p++; }
        else if (*p == '}' || *p == ']') { d--; p++; }
        else if (*p == '"') {
            const char *s = ++p;
            while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
            if (!*p) return NULL;
            size_t n = (size_t)(p - s);
            p++;
            if (n == kl && strncmp(s, clave, kl) == 0) {
                const char *q = saltar_ws(p);
                if (*q == ':' && (prof < 0 || d == prof)) return saltar_ws(q + 1);
            }
        } else p++;
    }
    return NULL;
}
static const char *json_valor(const char *json, const char *clave) {
    return json_buscar(json, clave, -1);
}

static int hex4(const char *p) {
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return -1;
    }
    return v;
}
static void utf8_poner(Buf *b, unsigned cp) {
    char o[4]; size_t k;
    if (cp < 0x80)      { o[0] = (char)cp; k = 1; }
    else if (cp < 0x800){ o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 0x3F)); k = 2; }
    else if (cp < 0x10000) {
        o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (char)(0x80 | (cp & 0x3F)); k = 3;
    } else {
        o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        o[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); o[3] = (char)(0x80 | (cp & 0x3F)); k = 4;
    }
    buf_bytes(b, o, k);
}

/* Cadena JSON en 'p' (apunta a la comilla de apertura), decodificada. */
static char *json_cadena(const char *p) {
    if (!p || *p != '"') return NULL;
    p++;
    Buf b = {0};
    buf_bytes(&b, "", 0);
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case 'n': buf_bytes(&b, "\n", 1); break;
                case 't': buf_bytes(&b, "\t", 1); break;
                case 'r': buf_bytes(&b, "\r", 1); break;
                case 'b': buf_bytes(&b, "\b", 1); break;
                case 'f': buf_bytes(&b, "\f", 1); break;
                case 'u': {
                    int cp = hex4(p + 1);
                    if (cp < 0) break;
                    p += 4;
                    if (cp >= 0xD800 && cp < 0xDC00 && p[1] == '\\' && p[2] == 'u') {
                        int lo = hex4(p + 3);
                        if (lo >= 0xDC00 && lo < 0xE000) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            p += 6;
                        }
                    }
                    utf8_poner(&b, (unsigned)cp);
                    break;
                }
                default: if (*p) buf_bytes(&b, p, 1);
            }
            if (*p) p++;
        } else {
            buf_bytes(&b, p, 1);
            p++;
        }
    }
    return b.d;
}

/* ─────────────────────────────────────────
   Documentos abiertos
───────────────────────────────────────── */
#define MAX_DOCS 64
typedef struct { char *uri; char *texto; } Doc;
static Doc docs[MAX_DOCS];

static Doc *doc_buscar(const char *uri) {
    for (int i = 0; i < MAX_DOCS; i++)
        if (docs[i].uri && strcmp(docs[i].uri, uri) == 0) return &docs[i];
    return NULL;
}
static void doc_fijar(const char *uri, const char *texto) {
    Doc *d = doc_buscar(uri);
    if (!d) for (int i = 0; i < MAX_DOCS; i++) if (!docs[i].uri) { d = &docs[i]; d->uri = strdup(uri); break; }
    if (!d) return;
    free(d->texto);
    d->texto = strdup(texto);
}
static void doc_cerrar(const char *uri) {
    Doc *d = doc_buscar(uri);
    if (!d) return;
    free(d->uri); free(d->texto);
    d->uri = d->texto = NULL;
}

/* file:///ruta/a%20x → /ruta/a x */
static char *uri_a_ruta(const char *uri) {
    if (strncmp(uri, "file://", 7) != 0) return NULL;
    const char *p = uri + 7;
    char *r = malloc(strlen(p) + 1), *o = r;
    for (; *p; p++) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char h[3] = { p[1], p[2], 0 };
            *o++ = (char)strtol(h, NULL, 16);
            p += 2;
        } else *o++ = *p;
    }
    *o = '\0';
    return r;
}

/* ─────────────────────────────────────────
   Escáner de símbolos (por líneas: funciona con el código a medias)
───────────────────────────────────────── */
enum { SK_FUNCION = 12, SK_METODO = 6, SK_CLASE = 5, SK_INTERFAZ = 11,
       SK_ENUM = 10, SK_VARIABLE = 13, SK_CONSTANTE = 14 };

typedef struct { char nombre[96]; int tipo; int linea; int sangria; } Simbolo;

static int es_ident_ch(int c) { return isalnum(c) || c == '_' || (c & 0x80); }

static int palabra_en(const char *s, const char *w) {
    size_t n = strlen(w);
    return strncmp(s, w, n) == 0 && !es_ident_ch((unsigned char)s[n]);
}

/* Lee las declaraciones de 'texto' en 'out'. Devuelve cuántas. */
static int escanear_simbolos(const char *texto, Simbolo *out, int max) {
    int n = 0, linea = 0;
    const char *p = texto;
    while (*p && n < max) {
        const char *fin = strchr(p, '\n');
        size_t len = fin ? (size_t)(fin - p) : strlen(p);
        const char *s = p; int sangria = 0;
        while (s < p + len && (*s == ' ' || *s == '\t')) { s++; sangria++; }

        static const char *const mods[] = { "abstracta", "abstracto", "estatico", "privado",
            "protegido", "publico", "sobrescribe", NULL };
        for (int again = 1; again;) {
            again = 0;
            for (int i = 0; mods[i]; i++)
                if (palabra_en(s, mods[i])) {
                    s += strlen(mods[i]);
                    while (*s == ' ') s++;
                    again = 1;
                }
        }
        int tipo = 0;
        if      (palabra_en(s, "funcion"))     { tipo = sangria ? SK_METODO : SK_FUNCION; s += 7; }
        else if (palabra_en(s, "generador"))   { tipo = SK_FUNCION; s += 9; }
        else if (palabra_en(s, "clase"))       { tipo = SK_CLASE; s += 5; }
        else if (palabra_en(s, "interfaz"))    { tipo = SK_INTERFAZ; s += 8; }
        else if (palabra_en(s, "enumeracion")) { tipo = SK_ENUM; s += 11; }
        else if (palabra_en(s, "sea"))         { tipo = SK_VARIABLE; s += 3; }
        else if (palabra_en(s, "fijo"))        { tipo = SK_CONSTANTE; s += 4; }
        else if (palabra_en(s, "obtener") || palabra_en(s, "fijar")) {
            tipo = SK_METODO; s += palabra_en(s, "obtener") ? 7 : 5;
        }
        if (tipo) {
            while (*s == ' ') s++;
            if (tipo == SK_VARIABLE || tipo == SK_CONSTANTE) {
                if (palabra_en(s, "lista")) { s += 5; while (*s == ' ') s++; }
                else if (palabra_en(s, "diccionario")) { s += 11; while (*s == ' ') s++; }
            }
            size_t k = 0;
            while (es_ident_ch((unsigned char)s[k]) && k < sizeof(out[n].nombre) - 1) k++;
            if (k > 0) {
                memcpy(out[n].nombre, s, k); out[n].nombre[k] = '\0';
                out[n].tipo = tipo; out[n].linea = linea; out[n].sangria = sangria;
                n++;
            }
        }
        if (!fin) break;
        p = fin + 1;
        linea++;
    }
    return n;
}

/* ─────────────────────────────────────────
   Autocompletado
───────────────────────────────────────── */
enum { CK_FUNCION = 3, CK_CAMPO = 5, CK_VARIABLE = 6, CK_CLASE = 7, CK_MODULO = 9,
       CK_PROPIEDAD = 10, CK_PALABRA = 14, CK_CONSTANTE = 21 };

/* Etiquetas ya emitidas en este completado (se vacía en completar()) */
#define MAX_VISTOS 4096
static char *vistos[MAX_VISTOS];
static int   num_vistos;

static void vistos_vaciar(void) {
    for (int i = 0; i < num_vistos; i++) free(vistos[i]);
    num_vistos = 0;
}

static void item(Buf *b, int *primero, const char *label, int kind) {
    for (int i = 0; i < num_vistos; i++)
        if (strcmp(vistos[i], label) == 0) return;       /* ya está */
    if (num_vistos < MAX_VISTOS) vistos[num_vistos++] = strdup(label);
    if (!*primero) buf_str(b, ",");
    *primero = 0;
    buf_str(b, "{\"label\":");
    buf_json(b, label);
    buf_fmt(b, ",\"kind\":%d}", kind);
}

static int kind_completion(int sk) {
    switch (sk) {
        case SK_FUNCION: return CK_FUNCION;
        case SK_METODO:  return CK_FUNCION;
        case SK_CLASE: case SK_INTERFAZ: case SK_ENUM: return CK_CLASE;
        case SK_CONSTANTE: return CK_CONSTANTE;
        default: return CK_VARIABLE;
    }
}

/* Los miembros de un módulo estándar, leídos del propio intérprete. */
static Interprete *interprete_base(void) {
    static Interprete *in = NULL;
    if (!in) in = interprete_crear();
    return in;
}

static void miembros_modulo(Buf *b, int *primero, const char *modulo) {
    /* Cada módulo se carga una sola vez, en su propio intérprete */
    static struct { char nombre[32]; Interprete *in; } cache[16];
    Interprete *in = NULL;
    for (int i = 0; i < 16 && !in; i++) {
        if (!cache[i].in) {
            snprintf(cache[i].nombre, sizeof(cache[i].nombre), "%s", modulo);
            cache[i].in = interprete_crear();
            modulo_cargar(modulo, cache[i].in->global);
            in = cache[i].in;
        } else if (strcmp(cache[i].nombre, modulo) == 0) in = cache[i].in;
    }
    if (!in) return;
    for (int i = 0; i < in->global->cantidad; i++) {
        EntradaVar *ev = &in->global->vars[i];
        if (strcmp(ev->nombre, modulo) != 0 || !ev->valor ||
            ev->valor->tipo != VAL_DICCIONARIO) continue;
        for (int j = 0; j < ev->valor->diccionario.cantidad; j++) {
            Valor *v = ev->valor->diccionario.valores[j];
            item(b, primero, ev->valor->diccionario.claves[j],
                 v && v->tipo == VAL_FUNCION ? CK_FUNCION : CK_CONSTANTE);
        }
    }
}

static int es_modulo_estandar(const char *n) {
    for (int i = 0; modulo_estandar(i); i++)
        if (strcmp(modulo_estandar(i), n) == 0) return 1;
    return 0;
}

/* Byte donde está (linea, caracter) — el carácter cuenta unidades UTF-16. */
static size_t offset_posicion(const char *t, int linea, int caracter) {
    const char *p = t;
    for (int l = 0; l < linea; l++) {
        const char *nl = strchr(p, '\n');
        if (!nl) return strlen(t);
        p = nl + 1;
    }
    int u = 0;
    while (*p && *p != '\n' && u < caracter) {
        unsigned char c = (unsigned char)*p;
        int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        u += len == 4 ? 2 : 1;
        p += len;
    }
    return (size_t)(p - t);
}

static char *ruta_de_import(const char *ruta_doc, const char *rel) {
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", ruta_doc);
    char *b = strrchr(dir, '/');
    if (b) *b = '\0'; else strcpy(dir, ".");
    char *r = malloc(strlen(dir) + strlen(rel) + 16);
    sprintf(r, "%s/%s%s", dir, rel, strstr(rel, ".lince") ? "" : ".lince");
    return r;
}

static char *leer_fichero(const char *ruta) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n < 0 || n > (64L << 20)) { fclose(f); return NULL; }
    char *t = malloc((size_t)n + 1);
    if (!t) { fclose(f); return NULL; }
    size_t k = fread(t, 1, (size_t)n, f);
    t[k] = '\0';
    fclose(f);
    return t;
}

/* 'importar "./ruta" como alias' en el texto: devuelve la ruta relativa. */
static char *import_con_alias(const char *texto, const char *alias) {
    const char *p = texto;
    while ((p = strstr(p, "importar")) != NULL) {
        const char *q = p + 8;
        while (*q == ' ') q++;
        if (*q == '"') {
            const char *fin = strchr(q + 1, '"');
            if (fin) {
                const char *r = fin + 1;
                while (*r == ' ') r++;
                if (palabra_en(r, "como")) {
                    r += 4;
                    while (*r == ' ') r++;
                    size_t al = strlen(alias);
                    if (strncmp(r, alias, al) == 0 && !es_ident_ch((unsigned char)r[al])) {
                        size_t k = (size_t)(fin - (q + 1));
                        char *rel = malloc(k + 1);
                        memcpy(rel, q + 1, k); rel[k] = '\0';
                        return rel;
                    }
                }
            }
        }
        p += 8;
    }
    return NULL;
}

static void completar(Buf *out, Doc *d, int linea, int caracter) {
    int primero = 1;
    vistos_vaciar();
    buf_str(out, "[");

    size_t off = offset_posicion(d->texto, linea, caracter);
    /* Texto de la línea hasta el cursor, y el identificador antes del punto */
    size_t ini = off;
    while (ini > 0 && d->texto[ini - 1] != '\n') ini--;
    char pre[512];
    size_t pl = off - ini < sizeof(pre) - 1 ? off - ini : sizeof(pre) - 1;
    memcpy(pre, d->texto + off - pl, pl); pre[pl] = '\0';

    size_t e = pl;
    while (e > 0 && es_ident_ch((unsigned char)pre[e - 1])) e--;     /* lo ya escrito */
    char obj[96] = "";
    if (e > 0 && pre[e - 1] == '.') {
        size_t fin = e - 1, ini2 = fin;
        while (ini2 > 0 && es_ident_ch((unsigned char)pre[ini2 - 1])) ini2--;
        if (fin - ini2 > 0 && fin - ini2 < sizeof(obj)) {
            memcpy(obj, pre + ini2, fin - ini2); obj[fin - ini2] = '\0';
        }
    }

    Simbolo *sim = malloc(sizeof(Simbolo) * 2048);
    int ns = escanear_simbolos(d->texto, sim, 2048);

    if (obj[0]) {
        if (es_modulo_estandar(obj)) {
            miembros_modulo(out, &primero, obj);
        } else if (strcmp(obj, "mi") == 0) {
            for (int i = 0; i < ns; i++)
                if (sim[i].tipo == SK_METODO) item(out, &primero, sim[i].nombre, CK_FUNCION);
            /* campos: cada 'mi.nombre' del documento */
            const char *p = d->texto;
            while ((p = strstr(p, "mi.")) != NULL) {
                const char *s = p + 3; size_t k = 0;
                while (es_ident_ch((unsigned char)s[k])) k++;
                if (k > 0 && k < 96 && (p == d->texto || !es_ident_ch((unsigned char)p[-1]))) {
                    char nm[96]; memcpy(nm, s, k); nm[k] = '\0';
                    item(out, &primero, nm, CK_CAMPO);
                }
                p += 3;
            }
        } else {
            char *rel = import_con_alias(d->texto, obj);
            char *ruta_doc = uri_a_ruta(d->uri);
            if (rel && ruta_doc) {
                char *rf = ruta_de_import(ruta_doc, rel);
                char *src = leer_fichero(rf);
                if (src) {
                    Simbolo *sm = malloc(sizeof(Simbolo) * 1024);
                    int nm = escanear_simbolos(src, sm, 1024);
                    for (int i = 0; i < nm; i++)
                        if (sm[i].sangria == 0 && sm[i].tipo != SK_METODO)
                            item(out, &primero, sm[i].nombre, kind_completion(sm[i].tipo));
                    free(sm); free(src);
                }
                free(rf);
            } else {
                /* Un objeto cualquiera: los métodos que definen las clases del archivo */
                for (int i = 0; i < ns; i++)
                    if (sim[i].tipo == SK_METODO) item(out, &primero, sim[i].nombre, CK_FUNCION);
            }
            free(rel); free(ruta_doc);
        }
    } else {
        for (int i = 0; lexer_palabra_clave(i); i++)
            item(out, &primero, lexer_palabra_clave(i), CK_PALABRA);
        for (int i = 0; lexer_palabra_contextual(i); i++)
            item(out, &primero, lexer_palabra_contextual(i), CK_PALABRA);
        for (int i = 0; modulo_estandar(i); i++)
            item(out, &primero, modulo_estandar(i), CK_MODULO);
        Interprete *in = interprete_base();
        for (int i = 0; i < in->global->cantidad; i++) {
            Valor *v = in->global->vars[i].valor;
            item(out, &primero, in->global->vars[i].nombre,
                 v && v->tipo == VAL_CLASE ? CK_CLASE : CK_FUNCION);
        }
        for (int i = 0; i < ns; i++)
            if (sim[i].tipo != SK_METODO)
                item(out, &primero, sim[i].nombre, kind_completion(sim[i].tipo));
    }
    free(sim);
    buf_str(out, "]");
}

/* ─────────────────────────────────────────
   Protocolo
───────────────────────────────────────── */
static void enviar(const char *cuerpo) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", strlen(cuerpo), cuerpo);
    fflush(stdout);
}

static void responder(const char *id, const char *resultado) {
    if (!id[0]) return;                 /* era una notificación: no se contesta */
    Buf b = {0};
    buf_fmt(&b, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":", id);
    buf_str(&b, resultado);
    buf_str(&b, "}");
    enviar(b.d);
    free(b.d);
}

static void diagnostico(Buf *b, int linea, int gravedad, const char *msg, int primero) {
    buf_fmt(b, "%s{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
                "\"end\":{\"line\":%d,\"character\":1000}},\"severity\":%d,"
                "\"source\":\"lince\",\"message\":", primero ? "" : ",", linea - 1, linea - 1, gravedad);
    buf_json(b, msg);
    buf_str(b, "}");
}

static void publicar_diagnosticos(const char *uri, const char *texto) {
    int linea = 1; char msg[1024], buf[4096];
    Buf b = {0};
    buf_str(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    buf_json(&b, uri);
    buf_str(&b, ",\"diagnostics\":[");
    buf[0] = '\0';
    char *ruta = uri_a_ruta(uri);
    lsp_fijar_ruta(ruta);
    int error = analizar_aparte(texto, buf, sizeof buf);
    lsp_fijar_ruta(NULL);
    free(ruta);
    quitar_retornos(buf);
    if (error) {
        leer_error(buf, &linea, msg, sizeof msg);
        diagnostico(&b, linea, 1, msg, 1);
    } else {
        /* "⚠️  Aviso en línea N: mensaje", uno por línea */
        int n = 0;
        for (char *l = buf; *l; ) {
            char *fin = strchr(l, '\n');
            if (fin) *fin = '\0';
            const char *lp = strstr(l, "Aviso en línea ");
            const char *dp = lp ? strchr(lp, ':') : NULL;
            if (dp) diagnostico(&b, atoi(lp + strlen("Aviso en línea ")), 2, dp + 2, n++ == 0);
            if (!fin) break;
            l = fin + 1;
        }
    }
    buf_str(&b, "]}}");
    enviar(b.d);
    free(b.d);
}

/* Palabra bajo (linea, caracter) */
static int palabra_en_posicion(const char *t, int linea, int car, char *w, size_t max) {
    size_t off = offset_posicion(t, linea, car);
    size_t a = off, z = off;
    while (a > 0 && es_ident_ch((unsigned char)t[a - 1])) a--;
    while (t[z] && es_ident_ch((unsigned char)t[z])) z++;
    if (z <= a || z - a >= max) return 0;
    memcpy(w, t + a, z - a); w[z - a] = '\0';
    return 1;
}

static void leer_posicion(const char *msg, int *linea, int *car) {
    const char *pos = json_valor(msg, "position");
    *linea = *car = 0;
    if (!pos) return;
    const char *l = json_valor(pos, "line"), *c = json_valor(pos, "character");
    if (l) *linea = atoi(l);
    if (c) *car = atoi(c);
}

static void atender(const char *msg, int *salir, int *apagado) {
    char *metodo = json_cadena(json_buscar(msg, "method", 1));
    if (!metodo) return;
    const char *idp = json_buscar(msg, "id", 1);
    char id[64] = "";
    if (idp) {
        size_t k = 0;
        if (*idp == '"') { id[k++] = '"'; idp++; while (*idp && *idp != '"' && k < 60) id[k++] = *idp++; id[k++] = '"'; }
        else while (*idp && *idp != ',' && *idp != '}' && k < 60) id[k++] = *idp++;
        id[k] = '\0';
    }

    if (strcmp(metodo, "initialize") == 0) {
        responder(id,
            "{\"capabilities\":{\"textDocumentSync\":1,"
            "\"completionProvider\":{\"triggerCharacters\":[\".\"]},"
            "\"documentSymbolProvider\":true,\"definitionProvider\":true},"
            "\"serverInfo\":{\"name\":\"lince\",\"version\":\"" LINCE_VERSION "\"}}");
    } else if (strcmp(metodo, "shutdown") == 0) {
        *apagado = 1;
        responder(id, "null");
    } else if (strcmp(metodo, "exit") == 0) {
        *salir = 1;
    } else if (strcmp(metodo, "textDocument/didOpen") == 0 ||
               strcmp(metodo, "textDocument/didChange") == 0) {
        char *uri = json_cadena(json_valor(msg, "uri"));
        char *txt = json_cadena(json_valor(msg, "text"));
        if (uri && txt) { doc_fijar(uri, txt); publicar_diagnosticos(uri, txt); }
        free(uri); free(txt);
    } else if (strcmp(metodo, "textDocument/didClose") == 0) {
        char *uri = json_cadena(json_valor(msg, "uri"));
        if (uri) {
            Buf b = {0};
            buf_str(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
            buf_json(&b, uri);
            buf_str(&b, ",\"diagnostics\":[]}}");
            enviar(b.d); free(b.d);
            doc_cerrar(uri);
        }
        free(uri);
    } else if (strcmp(metodo, "textDocument/completion") == 0) {
        char *uri = json_cadena(json_valor(msg, "uri"));
        Doc *d = uri ? doc_buscar(uri) : NULL;
        if (d && id[0]) {
            int l, c; leer_posicion(msg, &l, &c);
            Buf r = {0};
            completar(&r, d, l, c);
            responder(id, r.d);
            free(r.d);
        } else if (id[0]) responder(id, "[]");
        free(uri);
    } else if (strcmp(metodo, "textDocument/documentSymbol") == 0) {
        char *uri = json_cadena(json_valor(msg, "uri"));
        Doc *d = uri ? doc_buscar(uri) : NULL;
        Buf r = {0};
        buf_str(&r, "[");
        if (d) {
            Simbolo *sim = malloc(sizeof(Simbolo) * 2048);
            int ns = escanear_simbolos(d->texto, sim, 2048), primero = 1;
            for (int i = 0; i < ns; i++) {
                if (sim[i].tipo == SK_VARIABLE && sim[i].sangria > 0) continue;
                if (!primero) buf_str(&r, ",");
                primero = 0;
                buf_str(&r, "{\"name\":"); buf_json(&r, sim[i].nombre);
                buf_fmt(&r, ",\"kind\":%d,\"location\":{\"uri\":", sim[i].tipo);
                buf_json(&r, uri);
                buf_fmt(&r, ",\"range\":{\"start\":{\"line\":%d,\"character\":0},"
                            "\"end\":{\"line\":%d,\"character\":1000}}}}", sim[i].linea, sim[i].linea);
            }
            free(sim);
        }
        buf_str(&r, "]");
        if (id[0]) responder(id, r.d);
        free(r.d); free(uri);
    } else if (strcmp(metodo, "textDocument/definition") == 0) {
        char *uri = json_cadena(json_valor(msg, "uri"));
        Doc *d = uri ? doc_buscar(uri) : NULL;
        Buf r = {0};
        buf_str(&r, "null");
        if (d) {
            int l, c; char w[96];
            leer_posicion(msg, &l, &c);
            if (palabra_en_posicion(d->texto, l, c, w, sizeof(w))) {
                Simbolo *sim = malloc(sizeof(Simbolo) * 2048);
                int ns = escanear_simbolos(d->texto, sim, 2048);
                for (int i = 0; i < ns; i++) {
                    if (strcmp(sim[i].nombre, w) != 0) continue;
                    free(r.d); r.d = NULL; r.n = r.cap = 0;
                    buf_str(&r, "{\"uri\":"); buf_json(&r, uri);
                    buf_fmt(&r, ",\"range\":{\"start\":{\"line\":%d,\"character\":0},"
                                "\"end\":{\"line\":%d,\"character\":1000}}}", sim[i].linea, sim[i].linea);
                    break;
                }
                free(sim);
            }
        }
        if (id[0]) responder(id, r.d);
        free(r.d); free(uri);
    } else if (id[0]) {
        /* Petición que no conocemos: el protocolo pide contestar algo */
        Buf b = {0};
        buf_fmt(&b, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":-32601,\"message\":\"no implementado\"}}", id);
        enviar(b.d); free(b.d);
    }
    free(metodo);
}

int lsp_servir(void) {
    int salir = 0, apagado = 0;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);      /* sin traducir \r\n: Content-Length cuenta bytes */
    _setmode(_fileno(stdout), _O_BINARY);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    for (;;) {
        char linea[256];
        long largo = -1;
        /* Cabeceras hasta la línea en blanco */
        for (;;) {
            if (!fgets(linea, sizeof(linea), stdin)) return apagado ? 0 : 1;
            if (linea[0] == '\r' || linea[0] == '\n') break;
            if (strncasecmp(linea, "Content-Length:", 15) == 0) largo = atol(linea + 15);
        }
        if (largo <= 0 || largo > 64L * 1024 * 1024) continue;
        char *cuerpo = malloc((size_t)largo + 1);
        size_t leido = fread(cuerpo, 1, (size_t)largo, stdin);
        cuerpo[leido] = '\0';
        if ((long)leido < largo) { free(cuerpo); return 1; }
        atender(cuerpo, &salir, &apagado);
        free(cuerpo);
        if (salir) return apagado ? 0 : 1;
    }
}

