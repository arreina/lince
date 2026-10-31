/*
 * 🐆 Lince — empaquetado.h
 * 'lince compilar': un programa se convierte en un ejecutable que es una copia
 * del propio intérprete con el programa y los archivos que importa pegados al
 * final. Al arrancar, el ejecutable detecta esa carga y la ejecuta.
 */

#ifndef LINCE_EMPAQUETADO_H
#define LINCE_EMPAQUETADO_H

#include <stddef.h>

/* Crea 'salida' a partir de 'archivo', con los archivos o carpetas 'extras'
   (niveles, imágenes…) dentro. 0 si va bien. */
int empaquetado_crear(const char *archivo, const char *salida, const char *const *extras, int num_extras);
/* Lo mismo pero sin el intérprete: solo el programa y sus archivos (lo que va
   dentro de un .apk de Android) */
int empaquetado_crear_datos(const char *archivo, const char *salida, const char *const *extras, int num_extras);
/* Carga un programa empaquetado que ya está en memoria (el de un .apk) */
int empaquetado_cargar_datos(const unsigned char *datos, size_t largo);

/* ¿Lleva este ejecutable un programa dentro? Si es así lo carga y devuelve 1.
   'argv0' sirve para localizarse en sistemas sin /proc. */
int empaquetado_cargar(const char *argv0);

/* Ruta original y código del programa principal empaquetado */
const char *empaquetado_ruta_principal(void);
const char *empaquetado_codigo_principal(void);

/* Ruta del ejecutable que está corriendo; 1 si se pudo saber */
int empaquetado_ruta_ejecutable(char *out, size_t max);

/* Lector para interprete_fijar_lector: contenido empaquetado de una ruta */
const char *empaquetado_buscar(const char *ruta, size_t *largo);

/* Lo que lleva dentro: cuántos archivos y la ruta del i-ésimo (sin "./") */
int empaquetado_cantidad(void);
const char *empaquetado_clave(int i);

/* Para 'lince repartir': el ejecutable de Lince del que parte el próximo
   empaquetado_crear (NULL: el propio; otro, para hacer el de otro sistema) y
   datos de más que lleva dentro (claves "lince:...": el icono, SDL2, cómo se
   llama el juego). Se olvidan al crear. */
void empaquetado_poner_base(const char *exe);
void empaquetado_poner_dato(const char *clave, const char *datos, size_t largo);
/* Copia el propio ejecutable (un juego ya empaquetado) en 'salida' sin las
   entradas que empiezan por 'quitar' y con 'clave' cambiada a 'datos' (lo que
   hace el instalador de Windows con el juego que instala). 0 si va bien. */
int empaquetado_copiar_propio(const char *salida, const char *quitar, const char *clave, const char *datos, size_t largo);

#endif /* LINCE_EMPAQUETADO_H */
