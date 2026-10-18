/*
 * 🐆 Lince — android.c
 * Arranque de un juego en Android. El .apk lleva libSDL2.so, este Lince como
 * libmain.so y el juego empaquetado (lince compilar --datos) en assets/juego.paq.
 * SDLActivity carga las dos bibliotecas y llama a SDL_main (en main.c), que
 * llama aquí antes de ejecutar el juego. Ver android/LEEME.md.
 */
#ifdef __ANDROID__
#include <android/log.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "empaquetado.h"

/* Lo que el juego escribe (escribir(), los errores) va al registro del
   sistema: se ve con 'adb logcat -s lince' */
static int _tubo[2];
static void *a_logcat(void *nada) {
    (void)nada;
    FILE *f = fdopen(_tubo[0], "r");
    char linea[1024];
    while (f && fgets(linea, sizeof linea, f)) __android_log_write(ANDROID_LOG_INFO, "lince", linea);
    return NULL;
}

static void avisar(const char *mensaje) { __android_log_write(ANDROID_LOG_ERROR, "lince", mensaje); }

int android_preparar(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (pipe(_tubo) == 0) {
        dup2(_tubo[1], 1);
        dup2(_tubo[1], 2);
        pthread_t hilo;
        if (pthread_create(&hilo, NULL, a_logcat, NULL) == 0) pthread_detach(hilo);
    }
    void *sdl = dlopen("libSDL2.so", RTLD_NOW);
    if (!sdl) { avisar("no se pudo cargar libSDL2.so"); return 0; }

    /* Los archivos que escriba el juego (récords, partidas) van a su carpeta privada */
    const char *(*carpeta)(void) = (const char *(*)(void))dlsym(sdl, "SDL_AndroidGetInternalStoragePath");
    const char *dir = carpeta ? carpeta() : NULL;
    if (dir && chdir(dir) != 0) avisar("no se pudo entrar en la carpeta del juego");

    /* El juego, desde dentro del .apk */
    void *(*abrir)(const char *, const char *) = (void *(*)(const char *, const char *))dlsym(sdl, "SDL_RWFromFile");
    long long (*tam)(void *) = (long long (*)(void *))dlsym(sdl, "SDL_RWsize");
    size_t (*leer)(void *, void *, size_t, size_t) = (size_t (*)(void *, void *, size_t, size_t))dlsym(sdl, "SDL_RWread");
    int (*cerrar)(void *) = (int (*)(void *))dlsym(sdl, "SDL_RWclose");
    if (!abrir || !tam || !leer || !cerrar) { avisar("a libSDL2.so le faltan funciones"); return 0; }
    void *rw = abrir("juego.paq", "rb");
    if (!rw) { avisar("el .apk no lleva assets/juego.paq"); return 0; }
    long long largo = tam(rw);
    unsigned char *datos = largo > 0 ? malloc((size_t)largo) : NULL;
    size_t leido = datos ? leer(rw, datos, 1, (size_t)largo) : 0;
    cerrar(rw);
    int ok = datos && leido == (size_t)largo && empaquetado_cargar_datos(datos, (size_t)largo);
    free(datos);
    if (!ok) avisar("assets/juego.paq no es un juego de Lince");
    return ok;
}
#else
typedef int lince_android_vacio;        /* en otros sistemas no hay nada aquí */
#endif
