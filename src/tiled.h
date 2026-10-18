/*
 * 🐆 Lince — tiled.h
 * Los mapas de Tiled (.tmj, .tmx y sus tilesets .tsj, .tsx): ver tiled.c.
 */
#ifndef LINCE_TILED_H
#define LINCE_TILED_H

#include <stddef.h>
#include "interprete.h"

/* El mapa con la forma del JSON de Tiled, ya resuelto (tilesets dentro,
   rutas desde la carpeta del juego, capas como listas de números), o NULL
   con el motivo en 'error' */
Valor *tiled_leer(const char *ruta, char *error, size_t max);

/* 'rel' visto desde la carpeta del archivo 'base' */
void tiled_ruta_junto(const char *base, const char *rel, char *salida, size_t max);

/* Llama a 'anadir' con cada archivo que necesita el mapa (tilesets e imágenes) */
void tiled_archivos(const char *ruta, void (*anadir)(const char *ruta));

#endif /* LINCE_TILED_H */
