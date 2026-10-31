/*
 * 🐆 Lince — imagen.h
 * Lectura de PNG, JPG, BMP, GIF y TGA para el módulo juego (con stb_image),
 * sin depender de SDL2_image.
 */
#ifndef LINCE_IMAGEN_H
#define LINCE_IMAGEN_H

#include <stddef.h>

/* Decodifica la imagen que hay en 'bytes'. Devuelve los píxeles en RGBA, 4
   bytes por píxel, fila a fila (liberar con free), o NULL si no se entiende;
   entonces 'error' dice por qué. */
unsigned char *imagen_decodificar(const unsigned char *bytes, size_t largo,
                                  int *ancho, int *alto, const char **error);

/* Guarda los píxeles (RGBA, como los de imagen_decodificar) en un PNG.
   Devuelve 1 si se pudo escribir. */
int imagen_guardar_png(const char *ruta, const unsigned char *px, int ancho, int alto);
/* Lo mismo, en memoria: el PNG entero (liberar con free) y su largo */
unsigned char *imagen_png(const unsigned char *px, int ancho, int alto, size_t *largo);

#endif
