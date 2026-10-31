/*
 * 🐆 Lince — repartir.c
 * Prepara un juego para compartirlo (ver repartir.h).
 */

#ifndef _WIN32
#define _GNU_SOURCE          /* dlinfo */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include "repartir.h"
#include "empaquetado.h"
#include "interprete.h"
#include "escritorio.h"
#include "imagen.h"
#include "version.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define CREAR_DIR(r) _mkdir(r)
#define SEP '\\'
#else
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#ifdef __linux__
#include <link.h>
#endif
#define CREAR_DIR(r) mkdir(r, 0755)
#define SEP '/'
#endif

#if defined(_WIN32)
#define PLATAFORMA "windows"
#elif defined(__APPLE__)
#define PLATAFORMA "mac"
#else
#define PLATAFORMA "linux"
#endif

/* ── Utilidades de archivos ── */

static int copiar(const char *de, const char *a, int ejecutable) {
    size_t largo = 0;
    char *c = interprete_leer_bytes(de, &largo);
    if (!c) return 0;
    FILE *f = fopen(a, "wb");
    if (!f) { free(c); return 0; }
    int ok = fwrite(c, 1, largo, f) == largo;
    ok = fclose(f) == 0 && ok;
    free(c);
#ifndef _WIN32
    if (ok) chmod(a, ejecutable ? 0755 : 0644);
#else
    (void)ejecutable;
#endif
    return ok;
}

static const char *base(const char *ruta) {
    const char *b = ruta;
    for (const char *p = ruta; *p; p++) if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

#ifdef _WIN32
static void directorio(const char *ruta, char *dir, size_t max) {
    snprintf(dir, max, "%s", ruta);
    char *b = (char *)base(dir);
    if (b > dir) b[-1] = '\0';
    else snprintf(dir, max, ".");
}
#endif

/* ── Windows: qué DLL importa un ejecutable o una DLL (formato PE) ──
   Sirve para llevarse también las DLL de las que depende SDL2.dll cuando no
   viene sola (por ejemplo la de MSYS2). Las del sistema no están en la
   carpeta de SDL y se saltan solas. */
#ifdef _WIN32
static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }

static int pe_importa(const char *ruta, char nombres[][96], int max) {
    size_t n = 0;
    unsigned char *d = (unsigned char *)interprete_leer_bytes(ruta, &n);
    if (!d) return 0;
    int cuantos = 0;
    if (n < 0x40 || d[0] != 'M' || d[1] != 'Z') goto fin;
    uint32_t pe = le32(d + 0x3c);
    if ((size_t)pe + 24 > n || memcmp(d + pe, "PE\0\0", 4) != 0) goto fin;
    uint16_t nsec = le16(d + pe + 6), tam_opc = le16(d + pe + 20);
    size_t opc = pe + 24;
    if (opc + tam_opc > n) goto fin;
    uint16_t magia = le16(d + opc);
    size_t dd = opc + (magia == 0x20b ? 112 : 96);
    if (dd + 16 > n) goto fin;
    uint32_t rva_imp = le32(d + dd + 8);
    size_t secs = opc + tam_opc;
    #define RVA_A_POS(rva, out) do { out = 0; \
        for (int s_ = 0; s_ < nsec; s_++) { const unsigned char *h_ = d + secs + 40 * s_; \
            if (secs + 40 * (size_t)(s_ + 1) > n) break; \
            uint32_t va_ = le32(h_ + 12), tam_ = le32(h_ + 16), raw_ = le32(h_ + 20); \
            if ((rva) >= va_ && (rva) < va_ + tam_) { out = raw_ + ((rva) - va_); break; } } } while (0)
    size_t pos;
    RVA_A_POS(rva_imp, pos);
    if (!pos) goto fin;
    for (; pos + 20 <= n && cuantos < max; pos += 20) {
        uint32_t rva_nombre = le32(d + pos + 12);
        if (!rva_nombre) break;
        size_t pn;
        RVA_A_POS(rva_nombre, pn);
        if (!pn || pn >= n) continue;
        snprintf(nombres[cuantos++], 96, "%.*s", (int)strnlen((const char *)d + pn, n - pn), (const char *)d + pn);
    }
    #undef RVA_A_POS
fin:
    free(d);
    return cuantos;
}
#endif

/* Copia SDL2 a 'carpeta'. Devuelve cuántos archivos copió (0: no se encontró) */
static int copiar_sdl(const char *carpeta) {
    int copiados = 0;
    char destino[2048];
#ifdef _WIN32
    HMODULE h = LoadLibraryA("SDL2.dll");
    if (!h) return 0;
    char ruta_sdl[MAX_PATH], dir_sdl[MAX_PATH];
    if (!GetModuleFileNameA(h, ruta_sdl, sizeof ruta_sdl)) return 0;
    directorio(ruta_sdl, dir_sdl, sizeof dir_sdl);
    /* Recorrido de dependencias: SDL2.dll y lo que importe que esté en su carpeta */
    char cola[64][96];
    int ncola = 1, i = 0;
    snprintf(cola[0], 96, "%s", base(ruta_sdl));
    for (; i < ncola; i++) {
        char origen[MAX_PATH + 100];
        snprintf(origen, sizeof origen, "%s\\%s", dir_sdl, cola[i]);
        if (GetFileAttributesA(origen) == INVALID_FILE_ATTRIBUTES) continue;
        snprintf(destino, sizeof destino, "%s\\%s", carpeta, cola[i]);
        if (!copiar(origen, destino, 0)) continue;
        copiados++;
        printf("   + %s\n", cola[i]);
        char deps[64][96];
        int nd = pe_importa(origen, deps, 64);
        for (int k = 0; k < nd && ncola < 64; k++) {
            int esta = 0;
            for (int j = 0; j < ncola; j++) if (_stricmp(cola[j], deps[k]) == 0) esta = 1;
            if (!esta) snprintf(cola[ncola++], 96, "%s", deps[k]);
        }
    }
#elif defined(__linux__) && !defined(__ANDROID__)
    void *h = dlopen("libSDL2-2.0.so.0", RTLD_NOW);
    if (!h) return 0;
    struct link_map *lm = NULL;
    if (dlinfo(h, RTLD_DI_LINKMAP, &lm) == 0 && lm && lm->l_name && lm->l_name[0]) {
        snprintf(destino, sizeof destino, "%s/libSDL2-2.0.so.0", carpeta);
        if (copiar(lm->l_name, destino, 0)) { copiados++; printf("   + libSDL2-2.0.so.0\n"); }
    }
#else
    (void)carpeta; (void)destino;
#endif
    return copiados;
}

static void escribir_leeme(const char *carpeta, const char *exe, int hay_sdl, int windows, const char *unico) {
    char ruta[2048];
    snprintf(ruta, sizeof ruta, "%s%cLEEME.txt", carpeta, SEP);
    FILE *f = fopen(ruta, "wb");
    if (!f) return;
    const char *nl = windows ? "\r\n" : "\n";
    fprintf(f, "Juego hecho con Lince (https://arreina.github.io/lince)%s%s", nl, nl);
    if (windows) {
        fprintf(f, "Para jugar: haz doble clic en %s.%s", exe, nl);
        fprintf(f, "Deja las DLL en la misma carpeta que el juego.%s", nl);
        if (unico) fprintf(f, "%sPara instalarlo, con su acceso en el menú Inicio: %s.%s", nl, unico, nl);
    } else {
        fprintf(f, "Para jugar: haz doble clic en %s o, en una terminal, ./%s%s", exe, exe, nl);
        if (hay_sdl)
            fprintf(f, "%sSi no se abre la ventana, instala SDL2 (en Debian/Ubuntu:%s"
                       "sudo apt install libsdl2-2.0-0). El juego usa la del sistema si la%s"
                       "hay, y si no la que va en esta carpeta.%s", nl, nl, nl, nl);
        else
            fprintf(f, "%sHace falta SDL2 (en Debian/Ubuntu: sudo apt install libsdl2-2.0-0).%s", nl, nl);
        if (unico) fprintf(f, "%sTodo en un solo archivo, que se pone en el menú de aplicaciones: %s.%s", nl, unico, nl);
    }
    fclose(f);
}

/* El nombre que ve el jugador sin --nombre: el del archivo sin los números
   del principio (01_pong → pong) */
static void nombre_de_archivo(const char *archivo, char *out, size_t max) {
    char b[512];
    snprintf(b, sizeof b, "%s", base(archivo));
    char *punto = strrchr(b, '.');
    if (punto) *punto = '\0';
    const char *p = b;
    while ((*p >= '0' && *p <= '9') || *p == '_') p++;
    snprintf(out, max, "%s", *p ? p : b);
}

/* El id (carpetas, el registro de Windows, el .desktop): el nombre en
   minúsculas, sin tildes y con guiones (La Piedra del Lince → la-piedra-del-lince) */
static void id_de_nombre(const char *nombre, char *out, size_t max) {
    static const char *const TILDES[][2] = {{"á", "a"}, {"é", "e"}, {"í", "i"}, {"ó", "o"}, {"ú", "u"}, {"ü", "u"},
        {"ñ", "n"}, {"Á", "a"}, {"É", "e"}, {"Í", "i"}, {"Ó", "o"}, {"Ú", "u"}, {"Ü", "u"}, {"Ñ", "n"}, {"ç", "c"},
        {"Ç", "c"}, {"à", "a"}, {"è", "e"}, {"ò", "o"}};
    size_t k = 0;
    int guion = 0;
    for (const char *c = nombre; *c && k + 2 < max; ) {
        char sale = 0;
        size_t salto = 1;
        if ((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9')) sale = *c;
        else if (*c >= 'A' && *c <= 'Z') sale = (char)(*c + 32);
        else
            for (size_t t = 0; t < sizeof TILDES / sizeof TILDES[0]; t++)
                if (!strncmp(c, TILDES[t][0], strlen(TILDES[t][0]))) { sale = TILDES[t][1][0]; salto = strlen(TILDES[t][0]); break; }
        c += salto;
        if (sale) {
            if (guion && k > 0) out[k++] = '-';
            out[k++] = sale;
            guion = 0;
        } else guion = 1;
    }
    out[k] = '\0';
    if (!k) snprintf(out, max, "juego");
}

/* El nombre como nombre de archivo: sin lo que no puede llevar en Windows */
static void nombre_archivo(const char *nombre, char *out, size_t max) {
    size_t k = 0;
    for (const char *c = nombre; *c && k + 1 < max; c++)
        if (!strchr("<>:\"/\\|?*", *c) && (unsigned char)*c >= 32) out[k++] = *c;
    while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '.')) k--;
    out[k] = '\0';
    if (!k) snprintf(out, max, "juego");
}

/* El icono, como PNG de 256×256 (para meterlo dentro y ponerlo en el .exe) */
static unsigned char *icono_256(const char *ruta, unsigned char **rgba, size_t *largo) {
    size_t n = 0;
    char *bytes = interprete_leer_bytes(ruta, &n);
    if (!bytes) { fprintf(stderr, "\n❌ No se puede leer el icono '%s'.\n\n", ruta); return NULL; }
    int w, h;
    const char *por = NULL;
    unsigned char *px = imagen_decodificar((const unsigned char *)bytes, n, &w, &h, &por);
    free(bytes);
    if (!px) { fprintf(stderr, "\n❌ El icono '%s' no es una imagen que entienda (%s).\n\n", ruta, por ? por : "?"); return NULL; }
    if (w < 16 || h < 16) { free(px); fprintf(stderr, "\n❌ El icono '%s' es demasiado pequeño (mide %dx%d).\n\n", ruta, w, h); return NULL; }
    unsigned char *t = malloc(256 * 256 * 4);
    if (!t) { free(px); return NULL; }
    escritorio_escalar(px, w, h, 256, t);
    free(px);
    unsigned char *png = imagen_png(t, 256, 256, largo);
    if (!png) { free(t); return NULL; }
    *rgba = t;
    return png;
}

/* ── El Lince de otro sistema (para hacer el juego de Windows desde Linux, o al revés) ──
   Se busca en LINCE_WINDOWS / LINCE_LINUX (el ejecutable o su carpeta), en
   la carpeta lince-windows / lince-linux junto a este lince y en
   ~/.lince/windows / ~/.lince/linux. Es lo que hay dentro de las descargas
   de cada versión. */
static int buscar_lince_para(int windows, char *exe, size_t max) {
    const char *nombre = windows ? "lince.exe" : "lince";
    char candidatos[4][1200];
    int n = 0;
    const char *e = getenv(windows ? "LINCE_WINDOWS" : "LINCE_LINUX");
    if (e && e[0]) snprintf(candidatos[n++], sizeof candidatos[0], "%s", e);
    char yo[1024];
    if (empaquetado_ruta_ejecutable(yo, sizeof yo)) {
        char *b = (char *)base(yo);
        *b = '\0';
        snprintf(candidatos[n++], sizeof candidatos[0], "%slince-%s", yo, windows ? "windows" : "linux");
    }
    const char *casa = getenv("HOME");
    if (!casa || !casa[0]) casa = getenv("USERPROFILE");
    if (casa) snprintf(candidatos[n++], sizeof candidatos[0], "%s/.lince/%s", casa, windows ? "windows" : "linux");
    for (int i = 0; i < n; i++) {
        struct stat st;
        if (stat(candidatos[i], &st) != 0) continue;
        if (st.st_mode & S_IFDIR) snprintf(exe, max, "%s%c%s", candidatos[i], SEP, nombre);
        else snprintf(exe, max, "%s", candidatos[i]);
        size_t largo = 0;
        unsigned char *d = (unsigned char *)interprete_leer_bytes(exe, &largo);
        if (!d) continue;
        int bien = largo > 4 && (windows ? d[0] == 'M' && d[1] == 'Z' : !memcmp(d, "\x7f" "ELF", 4));
        int misma = 0;                                   /* ¿de esta misma versión? */
        size_t lv = strlen(LINCE_VERSION);
        for (size_t k = 0; bien && !misma && k + lv + 1 < largo; k++)
            misma = d[k] == LINCE_VERSION[0] && !memcmp(d + k, LINCE_VERSION, lv) && d[k + lv] == '\0';
        free(d);
        if (!bien) continue;
        if (!misma) printf("⚠  %s no parece de Lince %s: mejor el de esta misma versión.\n", exe, LINCE_VERSION);
        return 1;
    }
    fprintf(stderr, "\n❌ Para hacer el juego de %s desde aquí hace falta el Lince de %s de esta versión:\n"
                    "   descarga lince-%s-%s.%s de https://github.com/arreina/lince/releases,\n"
                    "   descomprímelo y pon su carpeta en ~/.lince/%s (o di dónde está con %s=carpeta).\n\n",
            windows ? "Windows" : "Linux", windows ? "Windows" : "Linux", LINCE_VERSION, windows ? "windows" : "linux",
            windows ? "zip" : "tar.gz", windows ? "windows" : "linux", windows ? "LINCE_WINDOWS" : "LINCE_LINUX");
    return 0;
}

/* SDL2 de otro sistema: lo que haya junto a ese Lince (en Windows, todas sus DLL) */
static int copiar_sdl_de(const char *exe, const char *carpeta, int windows) {
    char dir[1200];
    snprintf(dir, sizeof dir, "%s", exe);
    char *b = (char *)base(dir);
    if (b > dir) b[-1] = '\0'; else snprintf(dir, sizeof dir, ".");
    int copiados = 0;
#ifdef _WIN32
    char patron[1300];
    snprintf(patron, sizeof patron, "%s\\%s", dir, windows ? "*.dll" : "libSDL2-2.0.so.0");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(patron, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char de[2600], a[2600];
            snprintf(de, sizeof de, "%s\\%s", dir, fd.cFileName);
            snprintf(a, sizeof a, "%s\\%s", carpeta, fd.cFileName);
            if (copiar(de, a, 0)) { copiados++; printf("   + %s\n", fd.cFileName); }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        int vale = windows ? l > 4 && !strcasecmp(e->d_name + l - 4, ".dll") : !strcmp(e->d_name, "libSDL2-2.0.so.0");
        if (!vale) continue;
        char de[2600], a[2600];
        snprintf(de, sizeof de, "%s/%s", dir, e->d_name);
        snprintf(a, sizeof a, "%s/%s", carpeta, e->d_name);
        if (copiar(de, a, 0)) { copiados++; printf("   + %s\n", e->d_name); }
    }
    if (d) closedir(d);
#endif
    return copiados;
}

/* Un .exe recién hecho: su icono y sin consola (es un juego, no un programa de terminal) */
static void retocar_exe(const char *ruta, const unsigned char *rgba) {
    size_t largo = 0;
    unsigned char *d = (unsigned char *)interprete_leer_bytes(ruta, &largo);
    if (!d) return;
    escritorio_pe_sin_consola(d, largo);
    if (rgba && !escritorio_pe_icono(d, largo, rgba, 256, 256))
        printf("⚠  Este Lince para Windows no tiene sitio para el icono (es anterior a la 2.7): el juego lleva el de siempre.\n");
    FILE *f = fopen(ruta, "wb");
    if (f) { fwrite(d, 1, largo, f); fclose(f); }
    free(d);
}

/* Las DLL (Windows) o SDL2 (Linux) de la carpeta, dentro del archivo único */
static void meter_bibliotecas(const char *carpeta, int windows) {
#ifdef _WIN32
    char patron[1300];
    snprintf(patron, sizeof patron, "%s\\%s", carpeta, windows ? "*.dll" : "libSDL2-2.0.so.0");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(patron, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const char *n = fd.cFileName;
#else
    DIR *d = opendir(carpeta);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) {
        const char *n = e->d_name;
        size_t l = strlen(n);
        if (windows ? !(l > 4 && !strcasecmp(n + l - 4, ".dll")) : strcmp(n, "libSDL2-2.0.so.0") != 0) continue;
#endif
        char r[2600], clave[400];
        snprintf(r, sizeof r, "%s%c%s", carpeta, SEP, n);
        snprintf(clave, sizeof clave, "%s%s", windows ? "lince:dll/" : "lince:sdl/", n);
        size_t largo = 0;
        char *datos = interprete_leer_bytes(r, &largo);
        if (datos) { empaquetado_poner_dato(clave, datos, largo); free(datos); }
#ifdef _WIN32
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    }
    if (d) closedir(d);
#endif
}

int repartir_crear(const char *archivo, const RepartirOpciones *op, const char *const *extras, int num_extras) {
    int windows = op->para ? !strcmp(op->para, "windows") : !strcmp(PLATAFORMA, "windows");
    int de_otro = op->para && strcmp(op->para, PLATAFORMA) != 0;
    const char *plataforma = op->para ? op->para : PLATAFORMA;
    int escritorio = !strcmp(plataforma, "windows") || !strcmp(plataforma, "linux");   /* en macOS, lo de siempre */
    char lince_otro[1300] = "";
    if (de_otro && !buscar_lince_para(windows, lince_otro, sizeof lince_otro)) return 1;

    char archivo_base[512];
    snprintf(archivo_base, sizeof archivo_base, "%s", base(archivo));
    char *punto = strrchr(archivo_base, '.');
    if (punto) *punto = '\0';
    char visible[256], id[128], fichero[300];
    if (op->nombre) snprintf(visible, sizeof visible, "%s", op->nombre);
    else nombre_de_archivo(archivo, visible, sizeof visible);
    id_de_nombre(visible, id, sizeof id);
    if (op->nombre) nombre_archivo(visible, fichero, sizeof fichero);   /* el ejecutable se llama como el juego */
    else snprintf(fichero, sizeof fichero, "%s", archivo_base);

    char carpeta[1024];
    if (op->carpeta) snprintf(carpeta, sizeof carpeta, "%s", op->carpeta);
    else snprintf(carpeta, sizeof carpeta, "%s-%s", archivo_base, plataforma);
    size_t lc = strlen(carpeta);
    while (lc > 1 && (carpeta[lc - 1] == '/' || carpeta[lc - 1] == '\\')) carpeta[--lc] = '\0';

    /* el icono: --icono, o icono.png junto al juego */
    char ruta_icono[1200] = "";
    if (op->icono) snprintf(ruta_icono, sizeof ruta_icono, "%s", op->icono);
    else {
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", archivo);
        char *b = (char *)base(dir);
        *b = '\0';
        snprintf(ruta_icono, sizeof ruta_icono, "%sicono.png", dir);
        struct stat sti;
        if (stat(ruta_icono, &sti) != 0) ruta_icono[0] = '\0';
    }
    unsigned char *icono = NULL, *icono_rgba = NULL;
    size_t largo_icono = 0;
    if (ruta_icono[0] && !(icono = icono_256(ruta_icono, &icono_rgba, &largo_icono))) return 1;

    struct stat st;
    if (stat(carpeta, &st) == 0 && !(st.st_mode & S_IFDIR)) {
        fprintf(stderr, "\n❌ '%s' ya existe y no es una carpeta.\n\n", carpeta);
        return 1;
    }
    CREAR_DIR(carpeta);
    if (stat(carpeta, &st) != 0) {
        fprintf(stderr, "\n❌ No se pudo crear la carpeta '%s'.\n\n", carpeta);
        return 1;
    }

    printf("\n🐆 Lince — Repartir\n\n");
    printf("   Juego   : %s («%s»)\n", archivo, visible);
    printf("   Para    : %s\n", windows ? "Windows" : plataforma);
    if (ruta_icono[0]) printf("   Icono   : %s\n", ruta_icono);
    printf("   Carpeta : %s\n\n", carpeta);

    /* 1. La carpeta: el juego, SDL2 y el LEEME (y el .zip, listo para itch.io) */
    char exe[1100];
    snprintf(exe, sizeof exe, windows ? "%s.exe" : "%s", fichero);
    char ruta_exe[2200];
    snprintf(ruta_exe, sizeof ruta_exe, "%s%c%s", carpeta, SEP, exe);
    char *meta = escritorio_paquete_texto("carpeta", visible, id);
    if (de_otro) empaquetado_poner_base(lince_otro);
    if (escritorio && meta) empaquetado_poner_dato("lince:paquete", meta, strlen(meta));
    if (icono) empaquetado_poner_dato("lince:icono.png", (const char *)icono, largo_icono);
    free(meta);
    if (empaquetado_crear(archivo, ruta_exe, extras, num_extras) != 0) return 1;
    if (windows) retocar_exe(ruta_exe, icono_rgba);
    printf("   + %s\n", exe);
    int hay_sdl = (de_otro ? copiar_sdl_de(lince_otro, carpeta, windows) : copiar_sdl(carpeta)) > 0;

    /* 2. Todo en un archivo: el instalador de Windows o el archivo único de Linux */
    char unico[1200] = "", ruta_unico[2400] = "";
    if (escritorio) {
        snprintf(unico, sizeof unico, windows ? "%s-instalar.exe" : "%s.x86_64", fichero);
        char dir[1100];
        snprintf(dir, sizeof dir, "%s", carpeta);
        char *b = (char *)base(dir);
        *b = '\0';
        snprintf(ruta_unico, sizeof ruta_unico, "%s%s", dir, unico);
        meta = escritorio_paquete_texto(windows ? "instalador" : "archivo", visible, id);
        if (de_otro) empaquetado_poner_base(lince_otro);
        if (meta) empaquetado_poner_dato("lince:paquete", meta, strlen(meta));
        free(meta);
        if (icono) empaquetado_poner_dato("lince:icono.png", (const char *)icono, largo_icono);
        meter_bibliotecas(carpeta, windows);
        printf("\n");
        if (empaquetado_crear(archivo, ruta_unico, extras, num_extras) != 0) return 1;
        if (windows) retocar_exe(ruta_unico, icono_rgba);
    }
    escribir_leeme(carpeta, exe, hay_sdl, windows, unico[0] ? unico : NULL);
    printf("   + LEEME.txt\n");
    if (!hay_sdl) {
        if (!strcmp(plataforma, "mac"))
            printf("\n⚠  En macOS SDL2 no se incluye: quien juegue necesitará instalarla (brew install sdl2).\n");
        else
            printf("\n⚠  No se encontró SDL2 %s, así que no va dentro:\n"
                   "   quien juegue necesitará instalarla.\n", de_otro ? "junto a ese Lince" : "en este equipo");
    }

    char zip[1100], itch[1600];
    snprintf(zip, sizeof zip, "%s.zip", carpeta);
    snprintf(itch, sizeof itch, "[[actions]]\nname = \"play\"\npath = \"%s/%s\"\n", base(carpeta), exe);
    if (repartir_zip_itch(carpeta, zip, escritorio ? itch : NULL) != 0) return 1;
    free(icono);
    free(icono_rgba);
    printf("\n✅ Listo:\n");
    printf("   %s/ y %s: se descomprime y se juega, sin instalar nada (y el .zip se sube tal cual a itch.io)\n", carpeta, zip);
    if (ruta_unico[0] && windows) printf("   %s: el instalador, todo en un archivo (menú Inicio y desinstalar como cualquier programa)\n", ruta_unico);
    else if (ruta_unico[0]) printf("   %s: todo en un archivo, también SDL2; la primera vez se ofrece a ponerse en el menú\n", ruta_unico);
    printf("\n");
    return 0;
}

/* ── ZIP sin comprimir ──
   Las imágenes PNG y los sonidos OGG ya van comprimidos, así que comprimir
   otra vez apenas ahorra. Los permisos de Unix van en los atributos externos
   para que el ejecutable siga siéndolo al descomprimir. */
static uint32_t crc32_de(const unsigned char *p, size_t n) {
    static uint32_t tabla[256];
    if (!tabla[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tabla[i] = c;
        }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = tabla[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void p16(FILE *f, unsigned v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); }
static void p32(FILE *f, uint32_t v) { p16(f, v & 0xFFFF); p16(f, v >> 16); }

typedef struct { char *nombre; uint32_t crc, tam, pos, modo; } EntradaZip;

static int comparar(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int repartir_zip(const char *carpeta, const char *zip) { return repartir_zip_itch(carpeta, zip, NULL); }

int repartir_zip_itch(const char *carpeta, const char *zip, const char *itch) {
    /* archivos de la carpeta (sin subcarpetas: repartir la deja plana) */
    char *nombres[512];
    int n = 0;
#ifdef _WIN32
    char patron[1100];
    snprintf(patron, sizeof patron, "%s\\*", carpeta);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(patron, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < 512) nombres[n++] = strdup(fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(carpeta);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL && n < 512) {
            if (e->d_name[0] == '.') continue;
            char r[2048];
            struct stat st;
            snprintf(r, sizeof r, "%s/%s", carpeta, e->d_name);
            if (stat(r, &st) == 0 && S_ISREG(st.st_mode)) nombres[n++] = strdup(e->d_name);
        }
        closedir(d);
    }
#endif
    qsort(nombres, (size_t)n, sizeof(char *), comparar);

    FILE *f = fopen(zip, "wb");
    if (!f) { fprintf(stderr, "\n❌ No se pudo crear '%s'.\n\n", zip); return 1; }
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    unsigned hora = (unsigned)(t->tm_hour << 11 | t->tm_min << 5 | t->tm_sec / 2);
    unsigned fecha = (unsigned)((t->tm_year - 80) << 9 | (t->tm_mon + 1) << 5 | t->tm_mday);
    const char *raiz = base(carpeta);

    EntradaZip *ent = calloc((size_t)n + 2, sizeof(EntradaZip));
    int ne = 0, ok = 1;
    for (int i = itch ? -2 : -1; i < n && ok; i++) {
        /* i = -2: el .itch.toml, en la raíz (donde lo busca itch.io); -1: la propia carpeta */
        char nombre[1200];
        char *datos = NULL;
        size_t largo = 0;
        uint32_t modo = 040755;
        if (i == -2) {
            snprintf(nombre, sizeof nombre, ".itch.toml");
            largo = strlen(itch);
            datos = malloc(largo + 1);
            if (!datos) { ok = 0; break; }
            memcpy(datos, itch, largo + 1);
            modo = 0100644;
        } else if (i < 0) snprintf(nombre, sizeof nombre, "%s/", raiz);
        else {
            snprintf(nombre, sizeof nombre, "%s/%s", raiz, nombres[i]);
            char r[2048];
            snprintf(r, sizeof r, "%s%c%s", carpeta, SEP, nombres[i]);
            datos = interprete_leer_bytes(r, &largo);
            if (!datos || largo > 0xFFFFFFF0u) { free(datos); ok = 0; break; }
            modo = 0100644;
#ifndef _WIN32
            struct stat st;
            if (stat(r, &st) == 0 && (st.st_mode & 0111)) modo = 0100755;
#else
            size_t l = strlen(nombres[i]);
            if (l > 4 && _stricmp(nombres[i] + l - 4, ".exe") == 0) modo = 0100755;
#endif
        }
        EntradaZip *z = &ent[ne++];
        z->nombre = strdup(nombre);
        z->crc = datos ? crc32_de((const unsigned char *)datos, largo) : 0;
        z->tam = (uint32_t)largo;
        z->pos = (uint32_t)ftell(f);
        z->modo = modo;
        p32(f, 0x04034b50); p16(f, 20); p16(f, 0x0800); p16(f, 0);      /* UTF-8, sin comprimir */
        p16(f, hora); p16(f, fecha);
        p32(f, z->crc); p32(f, z->tam); p32(f, z->tam);
        p16(f, (unsigned)strlen(nombre)); p16(f, 0);
        fwrite(nombre, 1, strlen(nombre), f);
        if (datos) fwrite(datos, 1, largo, f);
        free(datos);
    }
    uint32_t inicio = (uint32_t)ftell(f);
    for (int i = 0; i < ne; i++) {
        EntradaZip *z = &ent[i];
        p32(f, 0x02014b50); p16(f, 3 << 8 | 20); p16(f, 20); p16(f, 0x0800); p16(f, 0);
        p16(f, hora); p16(f, fecha);
        p32(f, z->crc); p32(f, z->tam); p32(f, z->tam);
        p16(f, (unsigned)strlen(z->nombre)); p16(f, 0); p16(f, 0); p16(f, 0); p16(f, 0);
        p32(f, z->modo << 16 | ((z->modo & 040000) ? 0x10 : 0));        /* + atributo de carpeta de MS-DOS */
        p32(f, z->pos);
        fwrite(z->nombre, 1, strlen(z->nombre), f);
    }
    uint32_t fin = (uint32_t)ftell(f);
    p32(f, 0x06054b50); p16(f, 0); p16(f, 0); p16(f, (unsigned)ne); p16(f, (unsigned)ne);
    p32(f, fin - inicio); p32(f, inicio); p16(f, 0);
    ok = ok && !ferror(f);
    ok = fclose(f) == 0 && ok;
    for (int i = 0; i < ne; i++) free(ent[i].nombre);
    free(ent);
    for (int i = 0; i < n; i++) free(nombres[i]);
    if (!ok) { fprintf(stderr, "\n❌ Error al escribir '%s'.\n\n", zip); return 1; }
    return 0;
}
