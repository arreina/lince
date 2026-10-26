/*
 * 🐆 Lince — zstd.h
 * Descomprimir Zstandard (para las capas de Tiled guardadas con zstd).
 */
#ifndef LINCE_ZSTD_H
#define LINCE_ZSTD_H

#include <stddef.h>

/* Los datos descomprimidos (con malloc) y su largo, o NULL si están mal */
unsigned char *zstd_descomprimir(const unsigned char *datos, size_t n, size_t *largo);

#endif
