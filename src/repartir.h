/*
 * 🐆 Lince — repartir.h
 * 'lince repartir juego.lince': prepara un juego para pasárselo a otra persona.
 * Crea la carpeta <juego>-<sistema> con el ejecutable (programa, imágenes y
 * sonidos dentro), SDL2 y un LEEME.txt, y un .zip con esa carpeta; y el
 * instalador de Windows o el archivo único de Linux (ver escritorio.c).
 */
#ifndef LINCE_REPARTIR_H
#define LINCE_REPARTIR_H

typedef struct {
    const char *carpeta;     /* NULL: <juego>-<sistema> */
    const char *nombre;      /* el que ve el jugador (NULL: el del archivo) */
    const char *icono;       /* una imagen (NULL: icono.png junto al juego, si lo hay) */
    const char *para;        /* "windows" o "linux" (NULL: este sistema) */
} RepartirOpciones;

/* La carpeta y el .zip (listo para itch.io) y, en Windows, el instalador
   <juego>-instalar.exe; en Linux, el archivo único <juego>.x86_64. 0 si va bien. */
int repartir_crear(const char *archivo, const RepartirOpciones *op, const char *const *extras, int num_extras);

/* Escribe un .zip (sin comprimir) con los archivos de 'carpeta', dentro de una
   carpeta con su mismo nombre (y, si 'itch' no es NULL, ese .itch.toml en la
   raíz). 0 si va bien. */
int repartir_zip(const char *carpeta, const char *zip);
int repartir_zip_itch(const char *carpeta, const char *zip, const char *itch);

#endif
