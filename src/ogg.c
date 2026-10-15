/*
 * 🐆 Lince — ogg.c
 * Envoltorio de stb_vorbis (src/terceros) para el módulo juego.
 */
#include "ogg.h"
#include <limits.h>

/* Sólo hace falta decodificar desde memoria: los archivos llegan ya leídos,
   del disco o del ejecutable empaquetado. */
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include "terceros/stb_vorbis.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

short *ogg_decodificar(const unsigned char *bytes, size_t largo,
                       int *canales, int *frecuencia, int *muestras) {
    if (largo < 4 || largo > INT_MAX || memcmp(bytes, "OggS", 4) != 0) return NULL;
    short *salida = NULL;
    int n = stb_vorbis_decode_memory(bytes, (int)largo, canales, frecuencia, &salida);
    if (n <= 0 || !salida || *canales < 1 || *frecuencia <= 0) { free(salida); return NULL; }
    *muestras = n;
    return salida;
}
