/*
 * 🐆 Lince — repartir.h
 * 'lince repartir juego.lince': prepara un juego para pasárselo a otra persona.
 * Crea la carpeta <juego>-<sistema> con el ejecutable (programa, imágenes y
 * sonidos dentro), SDL2 y un LEEME.txt, y un .zip con esa carpeta.
 */
#ifndef LINCE_REPARTIR_H
#define LINCE_REPARTIR_H

/* 'carpeta' puede ser NULL (entonces <juego>-<sistema>). 0 si va bien. */
int repartir_crear(const char *archivo, const char *carpeta, const char *const *extras, int num_extras);

/* Escribe un .zip (sin comprimir) con los archivos de 'carpeta', dentro de una
   carpeta con su mismo nombre. 0 si va bien. */
int repartir_zip(const char *carpeta, const char *zip);

#endif
