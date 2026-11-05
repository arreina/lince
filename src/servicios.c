/*
 * 🐆 Lince — servicios.c
 * sistema.conexion(): si el aparato tiene conexión a Internet ahora mismo, sin
 * abrir ninguna conexión. En Linux mira si hay una ruta por defecto por una
 * interfaz encendida; en Windows se lo pregunta al gestor de redes
 * (INetworkListManager); en Android, a ConnectivityManager (por la parte en
 * Java: org.lince.Servicios), que además dice si la red es de pago por uso.
 */
#ifdef _WIN32
#define COBJMACROS
#include <winsock2.h>
#include <windows.h>
#include <ole2.h>
#include <netlistmgr.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "servicios.h"

static double ahora(void) {
#ifdef _WIN32
    return GetTickCount64() / 1000.0;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec / 1e9;
#endif
}

/* LINCE_CONEXION, para las pruebas: 1 si está puesta (y lo que dice) */
static int de_prueba(const char **estado, int *medida) {
    const char *e = getenv("LINCE_CONEXION");
    if (!e || !*e) return 0;
    *medida = -1;
    if (!strcmp(e, "con") || !strcmp(e, "con_conexion")) { *estado = "con_conexion"; *medida = 0; }
    else if (!strcmp(e, "medida")) { *estado = "con_conexion"; *medida = 1; }
    else if (!strcmp(e, "sin") || !strcmp(e, "sin_conexion")) *estado = "sin_conexion";
    else *estado = "desconocida";
    return 1;
}

#if defined(__linux__) && !defined(__ANDROID__)
/* ¿Está encendida? (operstate "up", o "unknown" en las que no lo dicen, como las VPN) */
static int interfaz_encendida(const char *nombre) {
    char ruta[160], estado[32] = "";
    snprintf(ruta, sizeof ruta, "/sys/class/net/%s/operstate", nombre);
    FILE *f = fopen(ruta, "r");
    if (!f) return 1;
    if (!fgets(estado, sizeof estado, f)) estado[0] = '\0';
    fclose(f);
    return !strncmp(estado, "up", 2) || !strncmp(estado, "unknown", 7);
}

/* 1 con una ruta por defecto (IPv4 o IPv6) por una interfaz encendida, 0 sin, -1 si no se puede leer */
static int mirar_rutas(void) {
    int leidas = 0, hay = 0;
    char linea[512];
    FILE *f = fopen("/proc/net/route", "r");
    if (f) {
        leidas = 1;
        while (!hay && fgets(linea, sizeof linea, f)) {
            char nombre[64];
            unsigned destino, pasarela, flags;
            if (sscanf(linea, "%63s %x %x %x", nombre, &destino, &pasarela, &flags) == 4 &&
                destino == 0 && (flags & 1) && strcmp(nombre, "lo") && interfaz_encendida(nombre))
                hay = 1;
        }
        fclose(f);
    }
    f = fopen("/proc/net/ipv6_route", "r");
    if (f) {
        leidas = 1;
        while (!hay && fgets(linea, sizeof linea, f)) {
            char destino[40], prefijo[8], nombre[64];
            unsigned flags;
            /* destino largo origen largo pasarela métrica usos refs flags interfaz */
            if (sscanf(linea, "%39s %7s %*s %*s %*s %*s %*s %*s %x %63s", destino, prefijo, &flags, nombre) == 4 &&
                !strcmp(prefijo, "00") && strspn(destino, "0") == 32 && (flags & 1) && !(flags & 0x200) &&
                strcmp(nombre, "lo") && interfaz_encendida(nombre))
                hay = 1;
        }
        fclose(f);
    }
    return leidas ? hay : -1;
}
#endif

#ifdef _WIN32
/* {DCB00C01-570F-4A9B-8D69-199FDBA5723B} y {DCB00000-570F-4A9B-8D69-199FDBA5723B} */
static const GUID CLSID_GESTOR_REDES = {0xDCB00C01, 0x570F, 0x4A9B, {0x8D, 0x69, 0x19, 0x9F, 0xDB, 0xA5, 0x72, 0x3B}};
static const GUID IID_GESTOR_REDES   = {0xDCB00000, 0x570F, 0x4A9B, {0x8D, 0x69, 0x19, 0x9F, 0xDB, 0xA5, 0x72, 0x3B}};

static int mirar_windows(void) {
    static INetworkListManager *g;
    static int probado;
    if (!probado) {
        probado = 1;
        HRESULT h = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        (void)h;                     /* ya iniciado (por SDL) también vale */
        if (FAILED(CoCreateInstance(&CLSID_GESTOR_REDES, NULL, CLSCTX_ALL, &IID_GESTOR_REDES, (void **)&g))) g = NULL;
    }
    if (!g) return -1;
    NLM_CONNECTIVITY c = NLM_CONNECTIVITY_DISCONNECTED;
    if (FAILED(INetworkListManager_GetConnectivity(g, &c))) return -1;
    return (c & (NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET)) ? 1 : 0;
}
#endif

static const char *_estado = "desconocida";
static int _medida = -1;
static double _mirado = -10;

static void mirar(void) {
    double t = ahora();
    if (t - _mirado < 1.0) return;              /* como mucho una vez por segundo */
    _mirado = t;
#ifdef __ANDROID__
    char *r = android_servicio("conexion", "");
    _estado = !r ? "desconocida" : !strcmp(r, "con_conexion") ? "con_conexion" :
              !strcmp(r, "sin_conexion") ? "sin_conexion" : "desconocida";
    free(r);
    r = android_servicio("conexion_medida", "");
    _medida = !r ? -1 : !strcmp(r, "si") ? 1 : !strcmp(r, "no") ? 0 : -1;
    free(r);
#else
    int hay = -1;
#if defined(__linux__)
    hay = mirar_rutas();
#elif defined(_WIN32)
    hay = mirar_windows();
#endif
    _estado = hay < 0 ? "desconocida" : hay ? "con_conexion" : "sin_conexion";
    _medida = -1;
#endif
}

const char *servicio_conexion(void) {
    const char *e;
    int m;
    if (de_prueba(&e, &m)) return e;
    mirar();
    return _estado;
}

int servicio_conexion_medida(void) {
    const char *e;
    int m;
    if (de_prueba(&e, &m)) return m;
    mirar();
    return strcmp(_estado, "con_conexion") ? -1 : _medida;
}
