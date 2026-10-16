/*
 * 🐆 Lince — imagen.c
 * Envoltorio de stb_image (src/terceros) para el módulo juego.
 */
#include "imagen.h"
#include <limits.h>
#include <stdlib.h>

/* Las imágenes llegan ya leídas, del disco o del ejecutable empaquetado */
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS     /* si no, con MinGW lince.exe pediría libwinpthread-1.dll */
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_ONLY_TGA
#define STB_IMAGE_IMPLEMENTATION

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "terceros/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

unsigned char *imagen_decodificar(const unsigned char *bytes, size_t largo,
                                  int *ancho, int *alto, const char **error) {
    if (largo == 0 || largo > INT_MAX) { if (error) *error = "archivo vacío o demasiado grande"; return NULL; }
    int canales = 0;
    unsigned char *px = stbi_load_from_memory(bytes, (int)largo, ancho, alto, &canales, 4);
    if (!px && error) *error = stbi_failure_reason();
    return px;
}

/* ── Escritura de PNG ──
   Sin compresión: los datos van en bloques «stored» de deflate. Para sprites
   y hojas de animación (pocos KB) es más que suficiente, y cualquier editor
   los abre y los guarda luego comprimidos. */
#include <stdio.h>
#include <string.h>

static unsigned long png_crc_tabla[256];

static unsigned long png_crc(unsigned long c, const unsigned char *b, size_t n) {
    if (!png_crc_tabla[1])
        for (unsigned long i = 0; i < 256; i++) {
            unsigned long v = i;
            for (int k = 0; k < 8; k++) v = (v & 1) ? 0xEDB88320UL ^ (v >> 1) : v >> 1;
            png_crc_tabla[i] = v;
        }
    for (size_t i = 0; i < n; i++) c = png_crc_tabla[(c ^ b[i]) & 255] ^ (c >> 8);
    return c;
}

static void png_u32(unsigned char *b, unsigned long v) {
    b[0] = (unsigned char)(v >> 24); b[1] = (unsigned char)(v >> 16); b[2] = (unsigned char)(v >> 8); b[3] = (unsigned char)v;
}

static int png_trozo(FILE *f, const char *tipo, const unsigned char *datos, size_t n) {
    unsigned char cab[8];
    png_u32(cab, (unsigned long)n);
    memcpy(cab + 4, tipo, 4);
    unsigned long c = png_crc(0xFFFFFFFFUL, cab + 4, 4);
    c = png_crc(c, datos, n) ^ 0xFFFFFFFFUL;
    unsigned char fin[4];
    png_u32(fin, c);
    return fwrite(cab, 1, 8, f) == 8 && (n == 0 || fwrite(datos, 1, n, f) == n) && fwrite(fin, 1, 4, f) == 4;
}

int imagen_guardar_png(const char *ruta, const unsigned char *px, int ancho, int alto) {
    if (ancho < 1 || alto < 1 || ancho > 16384 || alto > 16384) return 0;
    size_t fila = (size_t)ancho * 4 + 1, crudo = fila * alto;
    size_t bloques = crudo / 65535 + 1;
    unsigned char *z = malloc(2 + crudo + bloques * 5 + 4);
    if (!z) return 0;
    size_t n = 0;
    z[n++] = 0x78; z[n++] = 0x01;                       /* zlib sin compresión */
    unsigned long a1 = 1, a2 = 0;                       /* adler-32 */
    size_t hecho = 0;
    while (hecho < crudo || hecho == 0) {
        size_t trozo = crudo - hecho > 65535 ? 65535 : crudo - hecho;
        z[n++] = (unsigned char)(hecho + trozo == crudo);
        z[n++] = (unsigned char)trozo; z[n++] = (unsigned char)(trozo >> 8);
        z[n++] = (unsigned char)~trozo; z[n++] = (unsigned char)(~trozo >> 8);
        for (size_t i = 0; i < trozo; i++, hecho++) {
            size_t y = hecho / fila, x = hecho % fila;
            unsigned char b = x == 0 ? 0 : px[y * (size_t)ancho * 4 + x - 1];   /* filtro 0 al principio de cada fila */
            z[n++] = b;
            a1 = (a1 + b) % 65521; a2 = (a2 + a1) % 65521;
        }
        if (crudo == 0) break;
    }
    png_u32(z + n, a2 << 16 | a1); n += 4;
    FILE *f = fopen(ruta, "wb");
    if (!f) { free(z); return 0; }
    unsigned char ihdr[13];
    png_u32(ihdr, (unsigned long)ancho); png_u32(ihdr + 4, (unsigned long)alto);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = ihdr[11] = ihdr[12] = 0;      /* 8 bits, RGBA */
    int ok = fwrite("\x89PNG\r\n\x1a\n", 1, 8, f) == 8 && png_trozo(f, "IHDR", ihdr, 13) &&
             png_trozo(f, "IDAT", z, n) && png_trozo(f, "IEND", NULL, 0);
    free(z);
    return fclose(f) == 0 && ok;
}
