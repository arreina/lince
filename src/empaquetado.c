/*
 * 🐆 Lince — empaquetado.c
 *
 * Formato: [ejecutable de lince][carga][pie]
 *   carga = u32 n, y n veces: u32 largo_clave, clave, u64 largo, contenido
 *           (la entrada 0 es el programa principal)
 *   pie   = "LINCEPAQ" + u64 largo de la carga (little endian)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include "empaquetado.h"
#include "tiled.h"
#include "lexer.h"
#include "parser.h"
#include "interprete.h"
#include "paquetes.h"

#ifdef _WIN32
#include <windows.h>
#define PATH_MAXIMO MAX_PATH
#else
#include <unistd.h>
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#define PATH_MAXIMO PATH_MAX
#endif

#define MAGIA "LINCEPAQ"
#define PIE_TAM 16

typedef struct { char *clave; char *contenido; size_t largo; } Entrada;

static Entrada *_entradas = NULL;
static int      _num_entradas = 0;

/* ── Ruta del propio ejecutable ── */
static int ruta_ejecutable(const char *argv0, char *out, size_t max) {
#ifdef _WIN32
    (void)argv0;
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)max);
    return n > 0 && n < max;
#elif defined(__APPLE__)
    uint32_t n = (uint32_t)max;
    if (_NSGetExecutablePath(out, &n) == 0) return 1;
    return argv0 && realpath(argv0, out) != NULL;
#else
    ssize_t n = readlink("/proc/self/exe", out, max - 1);
    if (n > 0) { out[n] = '\0'; return 1; }
    return argv0 && realpath(argv0, out) != NULL;
#endif
}

int empaquetado_ruta_ejecutable(char *out, size_t max) { return ruta_ejecutable(NULL, out, max); }

static void poner_u32(unsigned char *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i)); }
static void poner_u64(unsigned char *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (unsigned char)(v >> (8 * i)); }
static uint32_t leer_u32(const unsigned char *p) { uint32_t v = 0; for (int i = 3; i >= 0; i--) v = v << 8 | p[i]; return v; }
static uint64_t leer_u64(const unsigned char *p) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | p[i]; return v; }

/* La carga (sin el pie) ya leída: la reparte en entradas */
static int repartir_carga(const unsigned char *carga, uint64_t largo) {
    size_t pos = 0;
    if (largo < 4) return 0;
    int n = (int)leer_u32(carga); pos = 4;
    for (int i = 0; i < _num_entradas; i++) { free(_entradas[i].clave); free(_entradas[i].contenido); }
    free(_entradas);
    _num_entradas = 0;
    _entradas = calloc((size_t)n + 1, sizeof(Entrada));
    for (int i = 0; i < n; i++) {
        if (pos + 4 > largo) break;
        uint32_t lk = leer_u32(carga + pos); pos += 4;
        if (pos + lk + 8 > largo) break;
        char *clave = malloc(lk + 1);
        memcpy(clave, carga + pos, lk); clave[lk] = '\0'; pos += lk;
        uint64_t lc = leer_u64(carga + pos); pos += 8;
        if (pos + lc > largo) { free(clave); break; }
        char *cont = malloc((size_t)lc + 1);
        memcpy(cont, carga + pos, (size_t)lc); cont[lc] = '\0'; pos += (size_t)lc;
        _entradas[_num_entradas++] = (Entrada){ clave, cont, (size_t)lc };
    }
    return _num_entradas > 0;
}

int empaquetado_cargar_datos(const unsigned char *datos, size_t largo) {
    if (largo < PIE_TAM || memcmp(datos + largo - PIE_TAM, MAGIA, 8) != 0) return 0;
    uint64_t carga = leer_u64(datos + largo - PIE_TAM + 8);
    if (carga > largo - PIE_TAM) return 0;
    return repartir_carga(datos + (largo - PIE_TAM - carga), carga);
}

/* ── Lectura (arranque de un programa empaquetado) ── */
int empaquetado_cargar(const char *argv0) {
    char ruta[PATH_MAXIMO + 1];
    if (!ruta_ejecutable(argv0, ruta, sizeof(ruta))) return 0;
    FILE *f = fopen(ruta, "rb");
    if (!f) return 0;
    unsigned char pie[PIE_TAM];
    if (fseek(f, -PIE_TAM, SEEK_END) != 0 || fread(pie, 1, PIE_TAM, f) != PIE_TAM ||
        memcmp(pie, MAGIA, 8) != 0) { fclose(f); return 0; }
    uint64_t largo = leer_u64(pie + 8);
    long fin = ftell(f);
    if (fin < 0 || largo > (uint64_t)fin - PIE_TAM) { fclose(f); return 0; }
    unsigned char *carga = malloc((size_t)largo + 1);
    if (!carga || fseek(f, fin - PIE_TAM - (long)largo, SEEK_SET) != 0 ||
        fread(carga, 1, (size_t)largo, f) != (size_t)largo) { free(carga); fclose(f); return 0; }
    fclose(f);
    int ok = repartir_carga(carga, largo);
    free(carga);
    return ok;
}

const char *empaquetado_ruta_principal(void)   { return _num_entradas ? _entradas[0].clave : NULL; }
const char *empaquetado_codigo_principal(void) { return _num_entradas ? _entradas[0].contenido : NULL; }

/* «./a/b» y «a/b» son la misma ruta */
static const char *sin_punto(const char *r) {
    while (r[0] == '.' && r[1] == '/') r += 2;
    return r;
}

int empaquetado_cantidad(void) { return _num_entradas; }
const char *empaquetado_clave(int i) { return i >= 0 && i < _num_entradas ? sin_punto(_entradas[i].clave) : NULL; }

const char *empaquetado_buscar(const char *ruta, size_t *largo) {
    const char *rn = sin_punto(ruta);
    for (int i = 0; i < _num_entradas; i++)
        if (strcmp(_entradas[i].clave, ruta) == 0 || strcmp(sin_punto(_entradas[i].clave), rn) == 0) {
            if (largo) *largo = _entradas[i].largo;
            return _entradas[i].contenido;
        }
    return NULL;
}

/* ── Escritura (lince compilar) ── */
static int ya_esta(const char *clave) {
    for (int i = 0; i < _num_entradas; i++)
        if (strcmp(_entradas[i].clave, clave) == 0) return 1;
    return 0;
}

static void anadir_bin(const char *clave, const char *contenido, size_t largo) {
    _entradas = realloc(_entradas, sizeof(Entrada) * (size_t)(_num_entradas + 1));
    char *copia = malloc(largo + 1);
    memcpy(copia, contenido, largo); copia[largo] = '\0';
    _entradas[_num_entradas++] = (Entrada){ strdup(clave), copia, largo };
}

static void anadir(const char *clave, const char *contenido) {
    anadir_bin(clave, contenido, strlen(contenido));
}

/* Añade un archivo o, si es una carpeta, todo lo que contiene (sin los
   ocultos). La clave es la ruta tal como se pasó: así la encuentra el programa
   cuando la pide con esa misma ruta relativa. */
static int anadir_extra(const char *ruta) {
    struct stat st;
    if (stat(ruta, &st) != 0) {
        fprintf(stderr, "\n❌ No existe '%s' (indicado con --incluir).\n\n", ruta);
        return 0;
    }
#ifdef _WIN32
    if (st.st_mode & _S_IFDIR) {
        char patron[2048];
        snprintf(patron, sizeof patron, "%s\\*", ruta);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(patron, &fd);
        if (h == INVALID_HANDLE_VALUE) return 1;
        int ok = 1;
        do {
            if (fd.cFileName[0] == '.') continue;
            char sub[2048];
            size_t l = strlen(ruta);
            snprintf(sub, sizeof sub, "%s%s%s", ruta, (l && (ruta[l - 1] == '/' || ruta[l - 1] == '\\')) ? "" : "/", fd.cFileName);
            ok = anadir_extra(sub);
        } while (ok && FindNextFileA(h, &fd));
        FindClose(h);
        return ok;
    }
#else
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(ruta);
        if (!d) return 0;
        struct dirent *e;
        int ok = 1;
        while (ok && (e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char sub[2048];
            size_t l = strlen(ruta);
            snprintf(sub, sizeof sub, "%s%s%s", ruta, (l && ruta[l - 1] == '/') ? "" : "/", e->d_name);
            ok = anadir_extra(sub);
        }
        closedir(d);
        return ok;
    }
#endif
    size_t largo = 0;
    char *c = interprete_leer_bytes(ruta, &largo);
    if (!c) { fprintf(stderr, "\n❌ No se pudo leer '%s'.\n\n", ruta); return 0; }
    const char *clave = sin_punto(ruta);
    if (!ya_esta(clave)) anadir_bin(clave, c, largo);
    free(c);
    return 1;
}

/* Un archivo que necesita un mapa de Tiled (un tileset, una imagen) */
static void anadir_de_mapa(const char *ruta) {
    struct stat st;
    if (ya_esta(sin_punto(ruta))) return;
    if (stat(ruta, &st) != 0) {
        fprintf(stderr, "⚠  El mapa necesita '%s', que no existe aquí: no va dentro.\n", ruta);
        return;
    }
    if (anadir_extra(ruta)) printf("   + %s\n", sin_punto(ruta));
}

/* Valida la sintaxis (el parser termina el proceso si hay un error) y recorre
   los 'importar' de 'codigo', añadiendo cada archivo que encuentra. 'actual' es
   el fichero desde el que se resuelven los './x'. */
static void recoger(const char *codigo, const char *actual) {
    Lexer  *lx  = lexer_crear(codigo);
    int     cnt = 0;
    Token  *tok = lexer_tokenizar(lx, &cnt);
    Parser *pa  = parser_crear(tok, cnt);
    parser_parsear(pa);
    /* Imágenes, sonidos y fuentes nombrados en el código: van dentro sin --incluir.
       Sólo estos tipos: un .txt o un .json puede ser un archivo que el juego
       escribe (récords), y empaquetado taparía la versión del disco. */
    static const char *const MEDIOS[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".wav", ".ogg", ".ttf", ".otf",
                                          ".tmx", ".tmj", NULL };
    for (int i = 0; i < cnt; i++) {
        /* los idiomas (la carpeta idiomas) van dentro si el juego traduce algo */
        if (tok[i].tipo == TOK_IDENTIFICADOR && tok[i].valor && i > 0 && tok[i - 1].tipo == TOK_PUNTO &&
            (!strcmp(tok[i].valor, "traducir") || !strcmp(tok[i].valor, "idioma") ||
             !strcmp(tok[i].valor, "idiomas") || !strcmp(tok[i].valor, "menu_idiomas"))) {
            struct stat sti;
            if (!ya_esta("idiomas/") && stat("idiomas", &sti) == 0 && (sti.st_mode & S_IFDIR)) {
                anadir_bin("idiomas/", "", 0);          /* marca: ya se han metido */
                if (anadir_extra("idiomas")) printf("   + idiomas/\n");
            }
            continue;
        }
        if (tok[i].tipo != TOK_TEXTO || !tok[i].valor) continue;
        const char *v = tok[i].valor, *ext = strrchr(v, '.');
        if (!ext || strchr(ext, '/')) continue;
        int medio = 0;
        for (int k = 0; MEDIOS[k]; k++) {
            const char *a = ext, *b = MEDIOS[k];
            while (*a && *b && (*a | 32) == *b) { a++; b++; }
            if (!*a && !*b) medio = 1;
        }
        if (!medio || ya_esta(sin_punto(v))) continue;
        struct stat st;
        if (stat(v, &st) != 0 || (st.st_mode & S_IFDIR)) {
            fprintf(stderr, "⚠  '%s' aparece en el código pero no existe aquí: no va dentro.\n", v);
            continue;
        }
        if (anadir_extra(v)) printf("   + %s\n", sin_punto(v));
        /* un mapa de Tiled: también sus tilesets y sus imágenes */
        if ((ext[1] | 32) == 't' && (ext[2] | 32) == 'm') tiled_archivos(v, anadir_de_mapa);
    }
    for (int i = 0; i + 1 < cnt; i++) {
        if (tok[i].tipo != TOK_IMPORTAR || tok[i + 1].tipo != TOK_TEXTO) continue;
        const char *nombre = tok[i + 1].valor;
        char ruta[1024];
        const char *desde = actual;
        if (strncmp(nombre, "paquete:", 8) == 0) {
            if (!paquetes_resolver(nombre + 8, ruta, sizeof(ruta))) {
                fprintf(stderr, "⚠  El paquete '%s' no está instalado: el ejecutable no lo llevará.\n", nombre + 8);
                continue;
            }
            desde = actual;        /* como el intérprete: un paquete no cambia el fichero actual */
        } else if (strncmp(nombre, "./", 2) == 0 || strncmp(nombre, "../", 3) == 0) {
            if (!interprete_resolver_import(actual, nombre, ruta, sizeof(ruta))) {
                fprintf(stderr, "⚠  No se encontró '%s.lince' (importado desde %s).\n", nombre, actual);
                continue;
            }
            desde = ruta;
        } else {
            continue;              /* módulo estándar: ya va dentro del intérprete */
        }
        if (ya_esta(ruta)) continue;
        char *sub = interprete_leer_fuente(ruta);
        if (!sub) continue;
        anadir(ruta, sub);
        recoger(sub, desde);
        free(sub);
    }
    parser_destruir(pa);
    lexer_destruir(lx);
}

static int _solo_datos = 0;
static char *_base = NULL;                 /* el ejecutable de Lince del que se parte (NULL: el propio) */
static Entrada *_datos = NULL;             /* lo que se añade al final (empaquetado_poner_dato) */
static int _num_datos = 0;

void empaquetado_poner_base(const char *exe) {
    free(_base);
    _base = exe ? strdup(exe) : NULL;
}

void empaquetado_poner_dato(const char *clave, const char *datos, size_t largo) {
    _datos = realloc(_datos, sizeof(Entrada) * (size_t)(_num_datos + 1));
    char *copia = malloc(largo + 1);
    memcpy(copia, datos, largo); copia[largo] = '\0';
    _datos[_num_datos++] = (Entrada){ strdup(clave), copia, largo };
}

static void olvidar_datos(void) {
    for (int i = 0; i < _num_datos; i++) { free(_datos[i].clave); free(_datos[i].contenido); }
    free(_datos);
    _datos = NULL; _num_datos = 0;
    empaquetado_poner_base(NULL);
}

/* Abre el ejecutable de Lince 'exe' y dice dónde acaba el intérprete (si ya
   lleva un programa, la copia tiene que empezar antes) */
static FILE *abrir_interprete(const char *exe, long *fin_ejecutable) {
    FILE *in = fopen(exe, "rb");
    if (!in) return NULL;
    unsigned char pie[PIE_TAM];
    fseek(in, 0, SEEK_END);
    *fin_ejecutable = ftell(in);
    if (*fin_ejecutable >= PIE_TAM && fseek(in, -PIE_TAM, SEEK_END) == 0 &&
        fread(pie, 1, PIE_TAM, in) == PIE_TAM && memcmp(pie, MAGIA, 8) == 0)
        *fin_ejecutable -= PIE_TAM + (long)leer_u64(pie + 8);
    rewind(in);
    return in;
}

static void copiar_interprete(FILE *in, FILE *out, long resta) {
    char buf[65536];
    while (resta > 0) {
        size_t n = fread(buf, 1, (size_t)(resta > (long)sizeof(buf) ? (long)sizeof(buf) : resta), in);
        if (n == 0) break;
        fwrite(buf, 1, n, out);
        resta -= (long)n;
    }
}

/* La carga y el pie: las entradas que no empiezan por 'quitar', con 'clave' cambiada */
static void escribir_carga(FILE *out, const char *quitar, const char *clave, const char *datos, size_t largo_datos) {
    int cuantas = 0;
    for (int i = 0; i < _num_entradas; i++)
        if (!quitar || strncmp(_entradas[i].clave, quitar, strlen(quitar)) != 0) cuantas++;
    uint64_t total = 4;
    unsigned char tmp[8];
    poner_u32(tmp, (uint32_t)cuantas);
    fwrite(tmp, 1, 4, out);
    for (int i = 0; i < _num_entradas; i++) {
        if (quitar && strncmp(_entradas[i].clave, quitar, strlen(quitar)) == 0) continue;
        int cambia = clave && strcmp(_entradas[i].clave, clave) == 0;
        const char *c = cambia ? datos : _entradas[i].contenido;
        size_t lc = cambia ? largo_datos : _entradas[i].largo;
        size_t lk = strlen(_entradas[i].clave);
        poner_u32(tmp, (uint32_t)lk); fwrite(tmp, 1, 4, out);
        fwrite(_entradas[i].clave, 1, lk, out);
        poner_u64(tmp, lc); fwrite(tmp, 1, 8, out);
        fwrite(c, 1, lc, out);
        total += 4 + lk + 8 + lc;
    }
    unsigned char pie[PIE_TAM];
    memcpy(pie, MAGIA, 8);
    poner_u64(pie + 8, total);
    fwrite(pie, 1, PIE_TAM, out);
}

int empaquetado_copiar_propio(const char *salida, const char *quitar, const char *clave, const char *datos, size_t largo) {
    char yo[PATH_MAXIMO + 1];
    long fin;
    if (!ruta_ejecutable(NULL, yo, sizeof(yo))) return 1;
    FILE *in = abrir_interprete(yo, &fin);
    if (!in) return 1;
    FILE *out = fopen(salida, "wb");
    if (!out) { fclose(in); return 1; }
    copiar_interprete(in, out, fin);
    fclose(in);
    escribir_carga(out, quitar, clave, datos, largo);
    int mal = ferror(out);
    mal = fclose(out) != 0 || mal;
#ifndef _WIN32
    if (!mal) chmod(salida, 0755);
#endif
    return mal;
}

int empaquetado_crear_datos(const char *archivo, const char *salida, const char *const *extras, int num_extras) {
    _solo_datos = 1;
    int r = empaquetado_crear(archivo, salida, extras, num_extras);
    _solo_datos = 0;
    return r;
}

int empaquetado_crear(const char *archivo, const char *salida, const char *const *extras, int num_extras) {
    char principal[PATH_MAXIMO + 1];
#ifdef _WIN32
    if (!_fullpath(principal, archivo, sizeof(principal))) snprintf(principal, sizeof(principal), "%s", archivo);
#else
    if (!realpath(archivo, principal)) snprintf(principal, sizeof(principal), "%s", archivo);
#endif
    char *codigo = interprete_leer_fuente(principal);
    if (!codigo) {
        fprintf(stderr, "\n❌ No se pudo leer '%s'.\n\n", archivo);
        return 1;
    }
    _entradas = NULL; _num_entradas = 0;
    anadir(principal, codigo);
    recoger(codigo, principal);
    free(codigo);
    for (int i = 0; i < num_extras; i++)
        if (!anadir_extra(extras[i])) { olvidar_datos(); return 1; }
    for (int i = 0; i < _num_datos; i++)
        if (!ya_esta(_datos[i].clave)) anadir_bin(_datos[i].clave, _datos[i].contenido, _datos[i].largo);

    FILE *out;
    if (_solo_datos) {                  /* solo el programa y sus archivos, sin el intérprete */
        olvidar_datos();
        out = fopen(salida, "wb");
        if (!out) { fprintf(stderr, "\n❌ No se pudo crear '%s'.\n\n", salida); return 1; }
        escribir_carga(out, NULL, NULL, NULL, 0);
        goto cerrar;
    }
    char yo[PATH_MAXIMO + 1];
    if (_base) snprintf(yo, sizeof yo, "%s", _base);
    else if (!ruta_ejecutable(NULL, yo, sizeof(yo))) {
        fprintf(stderr, "\n❌ No se pudo localizar el ejecutable de Lince.\n\n");
        olvidar_datos();
        return 1;
    }
    olvidar_datos();
    long fin_ejecutable;
    FILE *in = abrir_interprete(yo, &fin_ejecutable);
    if (!in) { fprintf(stderr, "\n❌ No se pudo leer '%s'.\n\n", yo); return 1; }
    out = fopen(salida, "wb");
    if (!out) { fclose(in); fprintf(stderr, "\n❌ No se pudo crear '%s'.\n\n", salida); return 1; }
    copiar_interprete(in, out, fin_ejecutable);
    fclose(in);
    escribir_carga(out, NULL, NULL, NULL, 0);

cerrar:;
    int mal = ferror(out);
    fclose(out);
    if (mal) { fprintf(stderr, "\n❌ Error al escribir '%s'.\n\n", salida); return 1; }
#ifndef _WIN32
    if (!_solo_datos) chmod(salida, 0755);
#endif
    return 0;
}
