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
