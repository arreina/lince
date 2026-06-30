/*
 * 🐆 Lince — plataforma.h
 * Abstracción de diferencias entre Linux, Mac y Windows
 */

#ifndef LINCE_PLATAFORMA_H
#define LINCE_PLATAFORMA_H

/* ─────────────────────────────────────────
   DETECCIÓN DE PLATAFORMA
───────────────────────────────────────── */
#if defined(_WIN32) || defined(_WIN64)
    #define LINCE_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
    #define LINCE_MAC
#else
    #define LINCE_LINUX
#endif

/* ─────────────────────────────────────────
   ESPERAR (sleep)
───────────────────────────────────────── */
#ifdef LINCE_WINDOWS
    #include <windows.h>
    static inline void lince_esperar_ms(int ms) {
        Sleep((DWORD)ms);
    }
#else
    #include <time.h>
    static inline void lince_esperar_ms(int ms) {
        struct timespec ts;
        ts.tv_sec  = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
#endif

/* ─────────────────────────────────────────
   LIMPIAR PANTALLA
───────────────────────────────────────── */
#ifdef LINCE_WINDOWS
    static inline void lince_limpiar_pantalla(void) {
        system("cls");
    }
#else
    static inline void lince_limpiar_pantalla(void) {
        /* Secuencia ANSI: más rápido y limpio que system("clear") */
        printf("\033[2J\033[H");
    }
#endif

/* ─────────────────────────────────────────
   COLORES ANSI
   En Windows necesitamos activarlos primero
───────────────────────────────────────── */
#ifdef LINCE_WINDOWS
    #include <io.h>
    static inline int lince_colores_disponibles(void) {
        /* Activar colores ANSI en Windows 10+ */
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE) return 0;
        DWORD modo;
        if (!GetConsoleMode(h, &modo)) return 0;
        modo |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        return SetConsoleMode(h, modo) ? 1 : 0;
    }
#else
    #include <unistd.h>
    static inline int lince_colores_disponibles(void) {
        /* Solo usar colores si stdout es una terminal */
        return isatty(STDOUT_FILENO);
    }
#endif

/* ─────────────────────────────────────────
   SEPARADOR DE RUTA
───────────────────────────────────────── */
#ifdef LINCE_WINDOWS
    #define LINCE_SEP_RUTA '\\'
    #define LINCE_SEP_RUTA_STR "\\"
#else
    #define LINCE_SEP_RUTA '/'
    #define LINCE_SEP_RUTA_STR "/"
#endif

/* ─────────────────────────────────────────
   EXTENSIÓN DE EJECUTABLE
───────────────────────────────────────── */
#ifdef LINCE_WINDOWS
    #define LINCE_EXT_EXE ".exe"
#else
    #define LINCE_EXT_EXE ""
#endif

#endif /* LINCE_PLATAFORMA_H */
