/*
 * 🐆 Lince — ogg.h
 * Decodifica un archivo Ogg Vorbis entero a muestras de 16 bits.
 */
#ifndef LINCE_OGG_H
#define LINCE_OGG_H

#include <stddef.h>

/* Devuelve las muestras intercaladas (liberar con free) o NULL si los bytes
   no son un Ogg Vorbis válido. */
short *ogg_decodificar(const unsigned char *bytes, size_t largo,
                       int *canales, int *frecuencia, int *muestras);

#endif
