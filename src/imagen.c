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
   Cada fila con el filtro que la deja más pequeña (de los de PNG: nada,
   izquierda, arriba, media y Paeth) y comprimida con deflate de códigos fijos
   y repeticiones (LZ77). No llega a lo que sacan los editores, pero una
   captura de un juego, con sus colores lisos, se queda en unos pocos KB. */
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

/* Bits de deflate: se escriben del menos al más significativo */
typedef struct { unsigned char *b; size_t n; unsigned long acum; int nbits; } PngBits;

static void png_bits(PngBits *o, unsigned long v, int n) {
    o->acum |= v << o->nbits;
    o->nbits += n;
    while (o->nbits >= 8) { o->b[o->n++] = (unsigned char)o->acum; o->acum >>= 8; o->nbits -= 8; }
}

/* Un código de Huffman va del bit más significativo al menos: al revés */
static void png_codigo(PngBits *o, unsigned v, int n) {
    unsigned r = 0;
    for (int i = 0; i < n; i++) { r = (r << 1) | (v & 1); v >>= 1; }
    png_bits(o, r, n);
}

/* Un símbolo de los códigos fijos (0-255 letras, 256 fin, 257-285 largos) */
static void png_simbolo(PngBits *o, int s) {
    if (s < 144) png_codigo(o, 0x30 + (unsigned)s, 8);
    else if (s < 256) png_codigo(o, 0x190 + (unsigned)(s - 144), 9);
    else if (s < 280) png_codigo(o, (unsigned)(s - 256), 7);
    else png_codigo(o, 0xC0 + (unsigned)(s - 280), 8);
}

static const int PNG_LARGO[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
                                  67, 83, 99, 115, 131, 163, 195, 227, 258};
static const int PNG_LARGO_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                        4, 4, 4, 4, 5, 5, 5, 5, 0};
static const int PNG_DIST[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const int PNG_DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
                                       9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/* 'd' comprimido en un solo bloque de códigos fijos; deja en 'o' los bytes */
static void png_deflate(PngBits *o, const unsigned char *d, size_t n) {
    enum { VENTANA = 32768, HASH = 1 << 15, INTENTOS = 48 };
    int *cabeza = malloc(sizeof(int) * HASH), *previo = malloc(sizeof(int) * VENTANA);
    png_bits(o, 1, 1);                                  /* el último bloque */
    png_bits(o, 1, 2);                                  /* de códigos fijos */
    if (cabeza && previo) for (int i = 0; i < HASH; i++) cabeza[i] = -1;
    size_t i = 0;
    while (i < n) {
        int mejor = 0, distancia = 0;
        if (cabeza && previo && i + 2 < n) {
            unsigned h = ((unsigned)d[i] << 10 ^ (unsigned)d[i + 1] << 5 ^ d[i + 2]) & (HASH - 1);
            int c = cabeza[h];
            for (int k = 0; k < INTENTOS && c >= 0 && i - (size_t)c <= VENTANA; k++) {
                size_t maximo = n - i < 258 ? n - i : 258, l = 0;
                while (l < maximo && d[(size_t)c + l] == d[i + l]) l++;
                if ((int)l > mejor) { mejor = (int)l; distancia = (int)(i - (size_t)c); if (l == maximo) break; }
                int sig = previo[c % VENTANA];
                if (sig >= c) break;
                c = sig;
            }
            previo[i % VENTANA] = cabeza[h];
            cabeza[h] = (int)i;
        }
        if (mejor >= 3) {
            int k = 28;
            while (PNG_LARGO[k] > mejor) k--;
            png_simbolo(o, 257 + k);
            png_bits(o, (unsigned long)(mejor - PNG_LARGO[k]), PNG_LARGO_EXTRA[k]);
            int j = 29;
            while (PNG_DIST[j] > distancia) j--;
            png_codigo(o, (unsigned)j, 5);
            png_bits(o, (unsigned long)(distancia - PNG_DIST[j]), PNG_DIST_EXTRA[j]);
            for (size_t f = i + 1; f < i + (size_t)mejor && f + 2 < n; f++) {   /* lo repetido también se busca luego */
                unsigned h = ((unsigned)d[f] << 10 ^ (unsigned)d[f + 1] << 5 ^ d[f + 2]) & (HASH - 1);
                previo[f % VENTANA] = cabeza[h];
                cabeza[h] = (int)f;
            }
            i += (size_t)mejor;
        } else {
            png_simbolo(o, d[i]);
            i++;
        }
    }
    png_simbolo(o, 256);
    if (o->nbits > 0) png_bits(o, 0, 8 - o->nbits);
    free(cabeza);
    free(previo);
}

static int png_paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

int imagen_guardar_png(const char *ruta, const unsigned char *px, int ancho, int alto) {
    if (ancho < 1 || alto < 1 || ancho > 16384 || alto > 16384) return 0;
    size_t ancho4 = (size_t)ancho * 4, fila = ancho4 + 1, crudo = fila * alto;
    unsigned char *f = malloc(crudo), *prueba = malloc(ancho4);
    PngBits o = {malloc(crudo + crudo / 7 + 64), 0, 0, 0};
    if (!f || !prueba || !o.b) { free(f); free(prueba); free(o.b); return 0; }
    for (int y = 0; y < alto; y++) {                    /* el filtro que deja la fila con números más pequeños */
        const unsigned char *a = px + (size_t)y * ancho4, *arriba = y > 0 ? a - ancho4 : NULL;
        long mejor = -1;
        for (int t = 0; t < 5; t++) {
            long suma = 0;
            for (size_t x = 0; x < ancho4; x++) {
                int iz = x >= 4 ? a[x - 4] : 0, ar = arriba ? arriba[x] : 0, di = x >= 4 && arriba ? arriba[x - 4] : 0;
                int p = t == 0 ? 0 : t == 1 ? iz : t == 2 ? ar : t == 3 ? (iz + ar) / 2 : png_paeth(iz, ar, di);
                prueba[x] = (unsigned char)(a[x] - p);
                suma += prueba[x] < 128 ? prueba[x] : 256 - prueba[x];
            }
            if (mejor < 0 || suma < mejor) {
                mejor = suma;
                f[(size_t)y * fila] = (unsigned char)t;
                memcpy(f + (size_t)y * fila + 1, prueba, ancho4);
            }
        }
    }
    free(prueba);
    o.b[o.n++] = 0x78; o.b[o.n++] = 0x01;               /* zlib */
    png_deflate(&o, f, crudo);
    unsigned long a1 = 1, a2 = 0;                       /* adler-32 de lo que había antes de comprimir */
    for (size_t i = 0; i < crudo; i++) { a1 = (a1 + f[i]) % 65521; a2 = (a2 + a1) % 65521; }
    free(f);
    png_u32(o.b + o.n, a2 << 16 | a1); o.n += 4;
    FILE *fa = fopen(ruta, "wb");
    if (!fa) { free(o.b); return 0; }
    unsigned char ihdr[13];
    png_u32(ihdr, (unsigned long)ancho); png_u32(ihdr + 4, (unsigned long)alto);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = ihdr[11] = ihdr[12] = 0;      /* 8 bits, RGBA */
    int ok = fwrite("\x89PNG\r\n\x1a\n", 1, 8, fa) == 8 && png_trozo(fa, "IHDR", ihdr, 13) &&
             png_trozo(fa, "IDAT", o.b, o.n) && png_trozo(fa, "IEND", NULL, 0);
    free(o.b);
    return fclose(fa) == 0 && ok;
}
