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
#elif defined(__linux__)
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

static void escribir_leeme(const char *carpeta, const char *exe, int hay_sdl) {
    char ruta[2048];
    snprintf(ruta, sizeof ruta, "%s%cLEEME.txt", carpeta, SEP);
    FILE *f = fopen(ruta, "wb");
    if (!f) return;
#ifdef _WIN32
    const char *nl = "\r\n";
#else
    const char *nl = "\n";
#endif
    fprintf(f, "Juego hecho con Lince (https://arreina.github.io/lince)%s%s", nl, nl);
#ifdef _WIN32
    fprintf(f, "Para jugar: haz doble clic en %s.%s", exe, nl);
    fprintf(f, "Deja las DLL en la misma carpeta que el juego.%s", nl);
#else
    fprintf(f, "Para jugar: haz doble clic en %s o, en una terminal, ./%s%s", exe, exe, nl);
    if (hay_sdl)
        fprintf(f, "%sSi no se abre la ventana, instala SDL2 (en Debian/Ubuntu:%s"
                   "sudo apt install libsdl2-2.0-0). El juego usa la del sistema si la%s"
                   "hay, y si no la que va en esta carpeta.%s", nl, nl, nl, nl);
    else
        fprintf(f, "%sHace falta SDL2 (en Debian/Ubuntu: sudo apt install libsdl2-2.0-0).%s", nl, nl);
#endif
    (void)hay_sdl;
    fclose(f);
}

int repartir_crear(const char *archivo, const char *carpeta_pedida, const char *const *extras, int num_extras) {
    char nombre[512];
    snprintf(nombre, sizeof nombre, "%s", base(archivo));
    char *punto = strrchr(nombre, '.');
    if (punto) *punto = '\0';
    char carpeta[1024];
    if (carpeta_pedida) snprintf(carpeta, sizeof carpeta, "%s", carpeta_pedida);
    else snprintf(carpeta, sizeof carpeta, "%s-%s", nombre, PLATAFORMA);
    size_t lc = strlen(carpeta);
    while (lc > 1 && (carpeta[lc - 1] == '/' || carpeta[lc - 1] == '\\')) carpeta[--lc] = '\0';

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
    printf("   Juego   : %s\n", archivo);
    printf("   Carpeta : %s\n\n", carpeta);

    char exe[1100];
#ifdef _WIN32
    snprintf(exe, sizeof exe, "%s.exe", nombre);
#else
    snprintf(exe, sizeof exe, "%s", nombre);
#endif
    char ruta_exe[2200];
    snprintf(ruta_exe, sizeof ruta_exe, "%s%c%s", carpeta, SEP, exe);
    if (empaquetado_crear(archivo, ruta_exe, extras, num_extras) != 0) return 1;
    printf("   + %s\n", exe);

    int hay_sdl = copiar_sdl(carpeta) > 0;
    escribir_leeme(carpeta, exe, hay_sdl);
    printf("   + LEEME.txt\n");
    if (!hay_sdl) {
#ifdef __APPLE__
        printf("\n⚠  En macOS SDL2 no se incluye: quien juegue necesitará instalarla (brew install sdl2).\n");
#else
        printf("\n⚠  No se encontró SDL2 en este equipo, así que no va en la carpeta:\n"
               "   quien juegue necesitará instalarla.\n");
#endif
    }

    char zip[1100];
    snprintf(zip, sizeof zip, "%s.zip", carpeta);
    if (repartir_zip(carpeta, zip) != 0) return 1;
    printf("\n✅ Listo: %s/ y %s\n", carpeta, zip);
    printf("   Pasa el .zip: se descomprime y se juega, sin instalar Lince.\n\n");
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

int repartir_zip(const char *carpeta, const char *zip) {
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

    EntradaZip *ent = calloc((size_t)n + 1, sizeof(EntradaZip));
    int ne = 0, ok = 1;
    for (int i = -1; i < n && ok; i++) {
        /* i = -1: la propia carpeta */
        char nombre[1200];
        char *datos = NULL;
        size_t largo = 0;
        uint32_t modo = 040755;
        if (i < 0) snprintf(nombre, sizeof nombre, "%s/", raiz);
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
