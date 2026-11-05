/*
 * 🐆 Lince — servicios.h
 * Lo que el juego pregunta al sistema y no es la pantalla ni el sonido: si hay
 * conexión (sistema.conexion) y, en Android, la puerta a la parte en Java
 * (org.lince.Servicios), por donde van también los anuncios y las compras.
 */
#ifndef LINCE_SERVICIOS_H
#define LINCE_SERVICIOS_H

/* "con_conexion", "sin_conexion" o "desconocida". LINCE_CONEXION (sin, con,
   medida, desconocida) la fija para las pruebas. No abre conexiones y
   responde enseguida: lo mira como mucho una vez por segundo. */
const char *servicio_conexion(void);

/* 1 si la red es de pago por uso (datos móviles), 0 si no, -1 si no se sabe */
int servicio_conexion_medida(void);

#ifdef __ANDROID__
/* Llama a org.lince.Servicios.llamar(actividad, orden, datos) y devuelve su
   respuesta (hay que liberarla), o NULL si no se pudo. */
char *android_servicio(const char *orden, const char *datos);
#endif

#endif
