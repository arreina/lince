/*
 * 🐆 Lince — recarga.h
 * Recarga en caliente: vigila el programa (y lo que importa) mientras corre un
 * juego y, al guardar, lo vuelve a ejecutar sin cerrar la ventana. Las
 * variables 'sea' del nivel superior conservan su valor; las constantes
 * 'fijo', las funciones y las clases salen del código nuevo.
 */

#ifndef LINCE_RECARGA_H
#define LINCE_RECARGA_H

/* Empieza a vigilar 'archivo' (el programa principal). */
void recarga_iniciar(const char *archivo);
int  recarga_activa(void);

/* Añade un archivo a la vigilancia (los 'importar "./x"'). No hace nada si la
   recarga no está activa. */
void recarga_vigilar(const char *ruta);

/* ¿Ha cambiado algún archivo vigilado? Devuelve 1 si hay código nuevo con la
   sintaxis correcta, listo para ejecutarse; 0 si no (sin cambios, o el cambio
   tiene un error de sintaxis: ver recarga_error). Con forzar = 1 comprueba el
   código aunque no haya cambiado (F5). */
int  recarga_comprobar(int forzar);

/* Error de sintaxis del último cambio («línea N: mensaje»), o NULL. */
const char *recarga_error(void);

/* Tras un 1 de recarga_comprobar: 1 si hay que empezar de cero (F5) en vez
   de conservar las variables. */
int  recarga_completa(void);
void recarga_pedir_completa(int si);

/* Fecha de modificación y tamaño de un archivo (-1 si no existe). */
void recarga_huella(const char *ruta, long long *mtime, long long *tam);

#endif /* LINCE_RECARGA_H */
