/*
 * 🐆 Lince — letra.h
 * Fuentes TrueType/OpenType (.ttf, .otf): dibuja las letras una vez, a un
 * tamaño, en una imagen (un atlas), y dice dónde está cada una.
 */
#ifndef LINCE_LETRA_H
#define LINCE_LETRA_H

#include <stddef.h>

/* Una letra en el atlas: el trozo (sx, sy, sw, sh), dónde va respecto al
   punto de escritura (dx, dy; dy desde la línea base) y cuánto avanza */
typedef struct { int cp, sx, sy, sw, sh; float dx, dy, avance; } LetraGlifo;

typedef struct {
    LetraGlifo *glifos;        /* uno por letra que la fuente tiene */
    int nglifos;
    unsigned char *rgba;       /* el atlas: blanco con la letra en el alfa */
    int ancho, alto;
    float ascenso, alto_linea; /* de lo alto de la línea a la base; de una línea a la siguiente */
    float *kerning;            /* (c1 - 32) * 95 + (c2 - 32) para ASCII visible, o NULL */
} LetraAtlas;

/* Prepara la fuente a 'tam' píxeles de alto con los caracteres de 'cps' que
   tenga. 1 si va bien; 0 si los bytes no son una fuente. */
int  letra_preparar(const unsigned char *bytes, size_t largo, float tam,
                    const int *cps, int ncps, LetraAtlas *a);
void letra_liberar(LetraAtlas *a);

#endif
