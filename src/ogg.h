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

/* Lectura poco a poco (música larga sin decodificarla entera). 'bytes' tiene
   que seguir vivo mientras el flujo esté abierto. */
typedef struct OggFlujo OggFlujo;
OggFlujo *ogg_abrir(const unsigned char *bytes, size_t largo, int *canales, int *frecuencia);
/* Hasta 'max' muestras por canal, intercaladas; 0 al llegar al final */
int  ogg_leer(OggFlujo *f, short *destino, int max);
void ogg_rebobinar(OggFlujo *f);
void ogg_cerrar(OggFlujo *f);

#endif
