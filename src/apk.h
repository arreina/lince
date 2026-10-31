/*
 * 🐆 Lince — apk.h
 * 'lince repartir --android': el .apk de un juego a partir de la plantilla de
 * Android de Lince, sin Java ni el SDK de Android (ver apk.c).
 */
#ifndef LINCE_APK_H
#define LINCE_APK_H

typedef struct {
    const char *nombre;          /* el de la app en el móvil */
    const char *id;              /* el del paquete (org.lince.mijuego) */
    int version;                 /* el número de versión (versionCode): sube en cada una */
    const char *version_nombre;  /* la que ve el jugador ("1.2"; NULL: el número) */
    int aab;                     /* el .aab de Google Play (y su ficha) en vez del .apk */
    const char *icono;           /* una imagen (NULL: icono.png junto al juego, o el de Lince) */
    const char *capturas;        /* los fotogramas de las capturas de la ficha ("60,180,300") */
    const char *guion;           /* lo que se pulsa mientras (como LINCE_JUEGO_GUION: "30:espacio") */
} ApkOpciones;

/* El .apk (o el .aab y la carpeta con lo que pide la ficha de Google Play). 0 si va bien. */
int apk_crear(const char *juego, const char *salida, const ApkOpciones *op, const char *const *extras, int num_extras);

#endif /* LINCE_APK_H */
