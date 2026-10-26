/*
 * 🐆 Lince — migrar.h
 * 'lince migrar': cambia lo que la 2.0 va a cambiar a su forma nueva (ver
 * PROPUESTA_2.0.md). Lo mismo, sin cambiar nada, son los avisos de
 * «obsoleto» de 'lince comprobar' y del editor.
 */
#ifndef LINCE_MIGRAR_H
#define LINCE_MIGRAR_H

typedef struct { int linea; char *que; } MigrarNota;

typedef struct {
    MigrarNota *cambios;  int ncambios;     /* lo que se ha cambiado (o se cambiaría) */
    MigrarNota *mano;     int nmano;        /* lo que hay que mirar a mano */
} MigrarInforme;

/* El código en la forma nueva (malloc; igual que 'codigo' si no hay nada que
   cambiar). Si 'inf' no es NULL, apunta qué se ha cambiado y qué no se puede
   cambiar solo, con la línea del código original. */
char *migrar_codigo(const char *codigo, MigrarInforme *inf);
void  migrar_informe_liberar(MigrarInforme *inf);

/* Los avisos de «obsoleto» de ese código, para 'lince comprobar' y el editor */
void  migrar_avisos(const char *codigo, void (*avisar)(int linea, const char *mensaje));

/* Los archivos .lince de 'ruta' (ella misma si es un archivo; si es una
   carpeta, los de dentro y los de sus subcarpetas, salvo las ocultas).
   Devuelve cuántos; *archivos se libera con free (cada uno y la lista). */
int   migrar_buscar(const char *ruta, char ***archivos);

#endif
