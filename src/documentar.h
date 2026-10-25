/*
 * 🐆 Lince — documentar.h
 * La ayuda de un programa sacada de sus comentarios: los '#' que van justo
 * encima de una función, clase, método, enumeración o constante (sin línea
 * en blanco entre medias) son su ayuda. El primer bloque de comentarios del
 * archivo, si va separado del código, es la ayuda del archivo entero.
 *
 * La usan 'lince documentar' (un .md con todo) y el editor (al pasar el
 * ratón por encima de un nombre). Lee el texto sin el parser, así que también
 * vale con el programa a medio escribir.
 */
#ifndef LINCE_DOCUMENTAR_H
#define LINCE_DOCUMENTAR_H

typedef struct {
    char  que[16];        /* funcion, clase, interfaz, enumeracion, metodo, sea, fijo… */
    char  nombre[96];
    char  dentro[96];     /* la clase, interfaz o enumeración donde está, o "" */
    char  firma[512];     /* la línea que la define, sin el cuerpo */
    char *ayuda;          /* los comentarios de encima, sin el '#'; puede ser "" */
    int   linea;          /* desde 0 */
    int   privado;        /* empieza por _ o lleva 'privado' */
} Definicion;

/* Las definiciones del código, en orden. Se liberan con documentar_liberar. */
int  documentar_escanear(const char *codigo, Definicion **defs);
void documentar_liberar(Definicion *defs, int n);

/* La ayuda del archivo (el primer bloque de comentarios), o NULL. Hay que liberarla. */
char *documentar_cabecera(const char *codigo);

/* Todo en Markdown: título, la ayuda del archivo y cada cosa pública con su
   firma y su ayuda. Hay que liberarlo. */
char *documentar_markdown(const char *codigo, const char *titulo);

/* Lo que enseña el editor al pasar por 'nombre' (la definición en 'linea' si
   es >= 0, o la primera con ese nombre): firma y ayuda en Markdown, o NULL. */
char *documentar_ayuda(const char *codigo, const char *nombre, int linea);

#endif
