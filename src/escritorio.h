/*
 * 🐆 Lince — escritorio.h
 * Los juegos repartidos para el escritorio ('lince repartir'): lo que llevan
 * dentro además del juego (el nombre, el icono, SDL2), el instalador de
 * Windows, el archivo único de Linux que se pone en el menú de aplicaciones,
 * y cambiar el icono de un .exe.
 */

#ifndef LINCE_ESCRITORIO_H
#define LINCE_ESCRITORIO_H

#include <stddef.h>

/* Cómo se repartió el juego (la entrada "lince:paquete"): modo "carpeta" (la
   del .zip), "instalador" (el de Windows), "instalado" (lo que instaló) o
   "archivo" (el archivo único de Linux) */
typedef struct { char modo[16]; char nombre[256]; char id[128]; } EscritorioPaquete;
int escritorio_paquete(EscritorioPaquete *p);           /* 1 si el juego se repartió así */
char *escritorio_paquete_texto(const char *modo, const char *nombre, const char *id);   /* liberar con free */

/* Al arrancar un juego empaquetado: si toca instalar o desinstalar (en vez
   de jugar), lo hace, deja en *codigo cómo acaba y devuelve 1 */
int escritorio_arrancar(int argc, char **argv, int *codigo);

/* El icono del juego (PNG): el suyo o el de Lince */
const unsigned char *escritorio_icono(size_t *largo);

/* Linux: SDL2 sacada del propio juego (el archivo único) a una carpeta de
   la caché, para cargarla. NULL si no la lleva. */
const char *escritorio_sdl_del_paquete(void);
/* Linux: al abrir la ventana del archivo único, ¿se pregunta si ponerlo en el
   menú? 1 si toca preguntar; luego escritorio_responder(si) */
int escritorio_toca_preguntar(void);
void escritorio_responder(int si);

/* Un .exe de Windows (en memoria): cambia sus iconos (los BMP de 32 bits de
   lince.exe) por 'rgba' (ancho × alto) escalado a cada tamaño, y lo pasa a
   programa de ventanas (sin consola). Devuelve cuántos iconos cambió. */
int escritorio_pe_icono(unsigned char *exe, size_t largo, const unsigned char *rgba, int ancho, int alto);
int escritorio_pe_sin_consola(unsigned char *exe, size_t largo);
/* Escala un icono RGBA a t × t (centrado si no es cuadrado), en 'dst' */
void escritorio_escalar(const unsigned char *src, int ancho, int alto, int t, unsigned char *dst);

#endif /* LINCE_ESCRITORIO_H */
