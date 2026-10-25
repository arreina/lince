/*
 * 🐆 Lince — probar.h
 * 'lince probar': ejecuta las pruebas (prueba_*.lince, *_prueba.lince) y
 * cuenta qué parte del código han recorrido.
 */
#ifndef LINCE_PROBAR_H
#define LINCE_PROBAR_H

/* argv: carpetas o archivos de prueba (ninguno: la carpeta actual) y
   --sin-cobertura. Devuelve 0 si todas pasan. */
int probar_ejecutar(int argc, char *argv[]);

#endif
