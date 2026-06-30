/*
 * 🐆 Lince — paquetes.h
 */

#ifndef LINCE_PAQUETES_H
#define LINCE_PAQUETES_H

int paquetes_instalar(const char *url);
int paquetes_desinstalar(const char *nombre);
int paquetes_listar(void);

/* Resuelve "paquete:nombre" a la ruta real del archivo */
int paquetes_resolver(const char *nombre, char *ruta_out, size_t max);

#endif /* LINCE_PAQUETES_H */
