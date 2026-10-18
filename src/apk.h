/*
 * 🐆 Lince — apk.h
 * 'lince repartir --android': el .apk de un juego a partir de la plantilla de
 * Android de Lince, sin Java ni el SDK de Android (ver apk.c).
 */
#ifndef LINCE_APK_H
#define LINCE_APK_H

/* 'nombre' es el de la app en el móvil e 'id' el del paquete (org.lince.mijuego).
   0 si va bien. */
int apk_crear(const char *juego, const char *salida, const char *nombre, const char *id,
              const char *const *extras, int num_extras);

#endif /* LINCE_APK_H */
