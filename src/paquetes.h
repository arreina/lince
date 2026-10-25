/*
 * 🐆 Lince — paquetes.h
 */

#ifndef LINCE_PAQUETES_H
#define LINCE_PAQUETES_H

/* url@versión (o con lince.paquetes en la carpeta): para este juego, en
   paquetes/ y apuntado en lince.paquetes; si no, para todos (~/.lince) */
int paquetes_instalar(const char *url);
/* 'lince instalar' sin nada: los de lince.paquetes, tal cual se apuntaron */
int paquetes_instalar_proyecto(void);
int paquetes_desinstalar(const char *nombre);
int paquetes_listar(void);

/* Resuelve "paquete:nombre" a la ruta real del archivo */
int paquetes_resolver(const char *nombre, char *ruta_out, size_t max);

#endif /* LINCE_PAQUETES_H */
