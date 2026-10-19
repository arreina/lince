/*
 * 🐆 Lince — tests/fuzz/fuzz.c
 * Pruebas con datos estropeados al azar: parte de entradas buenas (semillas),
 * las estropea (cambia, mete, quita y mezcla bytes, y mete trozos típicos del
 * formato) y se las da a un lector de Lince. Cada prueba va en un proceso
 * aparte: si el lector se cuelga o AddressSanitizer encuentra un error de
 * memoria, la entrada se guarda para repetirlo. Se compila con 'make fuzz'.
 *
 *   tests/fuzz/fuzz <objetivo> <cuántas> <carpeta de fallos> <semilla>...
 *   tests/fuzz/fuzz <objetivo> repetir <carpeta> <archivo>      (repite un fallo)
 *
 * Objetivos: json, tiled (un mapa .tmx/.tmj o un tileset), imagen, ogg y
 * lince (el lexer y el analizador; los programas no se ejecutan).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "../../src/interprete.h"
#include "../../src/modulos.h"
#include "../../src/imagen.h"
#include "../../src/ogg.h"
#include "../../src/tiled.h"
#include "../../src/lexer.h"
#include "../../src/parser.h"

#define FALLO 99            /* el código con el que sale el proceso si ASan encuentra algo */

typedef struct { unsigned char *p; size_t n; } Datos;

static uint64_t azar_estado = 88172645463325252ull;
static uint64_t azar(void) {
    azar_estado ^= azar_estado << 13; azar_estado ^= azar_estado >> 7; azar_estado ^= azar_estado << 17;
    return azar_estado;
}
static size_t azar_hasta(size_t n) { return n ? (size_t)(azar() % n) : 0; }

static Datos leer(const char *ruta) {
    Datos d = {0};
    FILE *f = fopen(ruta, "rb");
    if (!f) return d;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    rewind(f);
    d.p = malloc(l > 0 ? (size_t)l : 1);
    d.n = l > 0 ? fread(d.p, 1, (size_t)l, f) : 0;
    fclose(f);
    return d;
}

/* Trozos típicos de cada formato, para meterlos en sitios al azar */
static const char *const TROZOS_JSON[] = {"{", "}", "[", "]", ",", ":", "\"", "\\u", "\\ud83d", "\\", "null", "true",
    "false", "-", "1e999", "0.0000001", "-0", "\"a\":", "[[[[[[[[", "]]]]]]]]", "{{{{", "\x00", "\xff", NULL};
static const char *const TROZOS_XML[] = {"<", ">", "</", "/>", "=\"", "\"", "&", "&amp;", "&#", "&#x", "&#999999999;",
    "<![CDATA[", "]]>", "<!--", "-->", "<?", "?>", "<layer>", "<data encoding=\"base64\" compression=\"zlib\">",
    "<data encoding=\"csv\">", "<tileset firstgid=\"1\" source=\"", "<tile id=\"", "<animation>", "<frame tileid=\"",
    "width=\"", "height=\"", "-1", "4294967295", "99999999", "<object ", "<polygon points=\"", ",", NULL};
static const char *const TROZOS_LINCE[] = {"funcion", "tarea", "si", "sino", "mientras", "para", "cada", "en", "sea",
    "fijo", "devolver", "clase", "enumeracion", "elegir", "caso", "otro", "intentar", "capturar", "lanzar", "esperar",
    "producir", "importar", "{", "}", "(", ")", "[", "]", "\"", "\"{", "\\{", ",", ":", ".", "=", "==", "++", "--",
    "+=", "\n", "#", "val", "texto", "numero", "->", "y", "o", "no", "mi", "romper", "continuar", NULL};
static const char *const TROZOS_BIN[] = {"\x00", "\xff", "\x7f", "\x80", "\xff\xff\xff\xff", "\x00\x00\x00\x00",
    "\x7f\xff\xff\xff", "\x80\x00\x00\x00", "IHDR", "IDAT", "IEND", "OggS", "vorbis", "\x01vorbis", "\x05vorbis", NULL};

static void meter(Datos *d, size_t pos, const void *p, size_t n) {
    d->p = realloc(d->p, d->n + n + 1);
    memmove(d->p + pos + n, d->p + pos, d->n - pos);
    memcpy(d->p + pos, p, n);
    d->n += n;
}

/* Unos cuantos cambios al azar */
static void estropear(Datos *d, const Datos *semillas, int nsem, const char *const *trozos) {
    int cambios = 1 + (int)azar_hasta(8);
    for (int k = 0; k < cambios; k++) {
        size_t pos = azar_hasta(d->n + 1);
        switch (azar_hasta(7)) {
            case 0: if (d->n) d->p[azar_hasta(d->n)] ^= (unsigned char)(1u << azar_hasta(8)); break;
            case 1: if (d->n) d->p[azar_hasta(d->n)] = (unsigned char)azar(); break;
            case 2: { unsigned char b = (unsigned char)azar(); meter(d, pos, &b, 1); break; }
            case 3: if (d->n > 1) {                                 /* quitar un trozo */
                size_t l = 1 + azar_hasta(d->n - pos < 64 ? d->n - pos + 1 : 64);
                if (pos + l > d->n) l = d->n - pos;
                memmove(d->p + pos, d->p + pos + l, d->n - pos - l);
                d->n -= l;
            } break;
            case 4: if (d->n > 1) {                                 /* repetir un trozo */
                size_t a = azar_hasta(d->n), l = 1 + azar_hasta(d->n - a < 128 ? d->n - a : 128);
                unsigned char *t = malloc(l);
                memcpy(t, d->p + a, l);
                meter(d, pos, t, l);
                free(t);
            } break;
            case 5: if (nsem > 0) {                                 /* un trozo de otra semilla */
                const Datos *o = &semillas[azar_hasta((size_t)nsem)];
                if (o->n) {
                    size_t a = azar_hasta(o->n), l = 1 + azar_hasta(o->n - a < 256 ? o->n - a : 256);
                    meter(d, pos, o->p + a, l);
                }
            } break;
            default: {                                              /* un trozo típico del formato */
                int nt = 0;
                while (trozos[nt]) nt++;
                const char *t = trozos[azar_hasta((size_t)nt)];
                meter(d, pos, t, t[0] ? strlen(t) : 1);
            }
        }
    }
}

static const char *_ruta_tmp;
static void avisar_nada(int linea, const char *mensaje) { (void)linea; (void)mensaje; }

/* Lo que hace cada objetivo con la entrada (en el proceso hijo) */
static void probar(const char *objetivo, const Datos *d) {
    char *texto = malloc(d->n + 1);
    memcpy(texto, d->p, d->n);
    texto[d->n] = '\0';
    if (!strcmp(objetivo, "json")) {
        Valor *v = json_a_valor(texto);
        if (v) { char *t = json_de_valor(v, 2); free(t); valor_destruir(v); }
    } else if (!strcmp(objetivo, "tiled")) {
        FILE *f = fopen(_ruta_tmp, "wb");
        if (f) { fwrite(d->p, 1, d->n, f); fclose(f); }
        char error[300];
        Valor *v = tiled_leer(_ruta_tmp, error, sizeof error);
        if (v) valor_destruir(v);
    } else if (!strcmp(objetivo, "imagen")) {
        int w, h;
        const char *e;
        free(imagen_decodificar(d->p, d->n, &w, &h, &e));
    } else if (!strcmp(objetivo, "ogg")) {
        int c, fr, m;
        free(ogg_decodificar(d->p, d->n, &c, &fr, &m));
        OggFlujo *f = ogg_abrir(d->p, d->n, &c, &fr);
        if (f) {
            short buf[4096];
            for (int i = 0; i < 64 && ogg_leer(f, buf, 1024) > 0; i++) {}
            ogg_cerrar(f);
        }
    } else if (!strcmp(objetivo, "lince")) {
        /* un error de sintaxis termina el proceso (exit 1): no cuenta como fallo */
        Lexer *lx = lexer_crear(texto);
        int cnt = 0;
        Token *tok = lexer_tokenizar(lx, &cnt);
        Parser *pa = parser_crear(tok, cnt);
        Nodo *prog = parser_parsear(pa);
        parser_avisos(pa, avisar_nada);
        nodo_destruir(prog);
        parser_destruir(pa);
        lexer_destruir(lx);
    }
    free(texto);
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "uso: %s <json|tiled|imagen|ogg|lince> <cuántas> <carpeta de fallos> <semilla>...\n", argv[0]);
        return 2;
    }
    const char *objetivo = argv[1];
    if (!strcmp(argv[2], "repetir")) {                 /* fuzz <objetivo> repetir <carpeta> <archivo>: sin proceso aparte */
        Datos d = leer(argv[4]);
        char r[512];
        snprintf(r, sizeof r, "%s/repetir.tmp", argv[3]);
        mkdir(argv[3], 0755);
        _ruta_tmp = r;
        probar(objetivo, &d);
        printf("sin fallos\n");
        return 0;
    }
    long cuantas = atol(argv[2]);
    const char *fallos = argv[3];
    const char *const *trozos = !strcmp(objetivo, "json") ? TROZOS_JSON : !strcmp(objetivo, "tiled") ? TROZOS_XML
                              : !strcmp(objetivo, "lince") ? TROZOS_LINCE : TROZOS_BIN;
    int nsem = argc - 4;
    Datos *semillas = calloc((size_t)nsem, sizeof(Datos));
    for (int i = 0; i < nsem; i++) {
        semillas[i] = leer(argv[4 + i]);
        if (!semillas[i].p) { fprintf(stderr, "no se puede leer la semilla %s\n", argv[4 + i]); return 2; }
    }
    const char *s = getenv("FUZZ_SEMILLA");
    if (s) azar_estado ^= strtoull(s, NULL, 10) * 0x9E3779B97F4A7C15ull;
    mkdir(fallos, 0755);
    char ruta_tmp[512];
    snprintf(ruta_tmp, sizeof ruta_tmp, "%s/entrada-%d.tmp", fallos, (int)getpid());
    _ruta_tmp = ruta_tmp;
    int encontrados = 0;
    for (long i = 0; i < cuantas; i++) {
        Datos d = {0};
        const Datos *base = &semillas[azar_hasta((size_t)nsem)];
        d.n = base->n;
        d.p = malloc(d.n + 1);
        memcpy(d.p, base->p, d.n);
        estropear(&d, semillas, nsem, trozos);
        if (d.n > (1u << 20)) d.n = 1u << 20;
        pid_t hijo = fork();
        if (hijo == 0) {
            alarm(10);                                     /* un cuelgue cuenta como fallo */
            probar(objetivo, &d);
            _exit(0);
        }
        int estado = 0;
        waitpid(hijo, &estado, 0);
        int malo = WIFSIGNALED(estado) || (WIFEXITED(estado) && WEXITSTATUS(estado) == FALLO);
        if (malo) {
            char r[512];
            snprintf(r, sizeof r, "%s/%s-%ld.bin", fallos, objetivo, i);
            FILE *f = fopen(r, "wb");
            if (f) { fwrite(d.p, 1, d.n, f); fclose(f); }
            fprintf(stderr, "❌ %s: fallo con la entrada %s (%s %d)\n", objetivo, r,
                    WIFSIGNALED(estado) ? "señal" : "código", WIFSIGNALED(estado) ? WTERMSIG(estado) : WEXITSTATUS(estado));
            encontrados++;
        }
        free(d.p);
    }
    remove(ruta_tmp);
    if (encontrados) { fprintf(stderr, "❌ %s: %d fallos en %ld pruebas\n", objetivo, encontrados, cuantas); return 1; }
    printf("✅ %s: %ld pruebas sin fallos\n", objetivo, cuantas);
    return 0;
}
