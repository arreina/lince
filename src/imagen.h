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

#endif
