/*
 * 🐆 Lince — paquetes.c
 * Gestor de paquetes: instalar, desinstalar, listar
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "paquetes.h"
#include "plataforma.h"

/* ─────────────────────────────────────────
   RUTAS
───────────────────────────────────────── */
static void dir_paquetes(char *buf, size_t max) {
#ifdef LINCE_WINDOWS
    const char *home = getenv("USERPROFILE");
    if (!home) home = "C:\\";
    snprintf(buf, max, "%s\\.lince\\paquetes", home);
#else
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, max, "%s/.lince/paquetes", home);
#endif
}

static void crear_dir_si_no_existe(const char *ruta) {
#ifdef LINCE_WINDOWS
    _mkdir(ruta);
#else
    /* Crear recursivamente */
    char tmp[1024];
    strncpy(tmp, ruta, sizeof(tmp) - 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
#endif
}

/* ─────────────────────────────────────────
   EXTRAER NOMBRE DE PAQUETE DESDE URL
───────────────────────────────────────── */
static void nombre_desde_url(const char *url, char *nombre, size_t max) {
    /* Tomar el último segmento de la URL sin extensión */
    const char *ultimo = strrchr(url, '/');
    if (ultimo) ultimo++; else ultimo = url;

    strncpy(nombre, ultimo, max - 1);
    nombre[max - 1] = '\0';

    /* Quitar .lince si tiene */
    char *ext = strstr(nombre, ".lince");
    if (ext) *ext = '\0';

    /* Quitar .git si tiene */
    char *git = strstr(nombre, ".git");
    if (git) *git = '\0';
}

/* ─────────────────────────────────────────
   CONFIRMAR INSTALACIÓN
───────────────────────────────────────── */
static int confirmar(const char *nombre, const char *url) {
    printf("\n🐆 Lince — Gestor de paquetes\n\n");
    printf("   Paquete : %s\n", nombre);
    printf("   Origen  : %s\n\n", url);
    printf("¿Instalar? [s/N] ");
    fflush(stdout);

    char resp[8];
    if (!fgets(resp, sizeof(resp), stdin)) return 0;
    return (resp[0] == 's' || resp[0] == 'S');
}

/* ─────────────────────────────────────────
   RESOLVER RUTA DE PAQUETE
───────────────────────────────────────── */
int paquetes_resolver(const char *nombre, char *ruta_out, size_t max) {
    char dir[1024];
    dir_paquetes(dir, sizeof(dir));
    snprintf(ruta_out, max, "%s/%s.lince", dir, nombre);
    FILE *f = fopen(ruta_out, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}
int paquetes_instalar(const char *url) {
    char nombre[256];
    nombre_desde_url(url, nombre, sizeof(nombre));

    if (!confirmar(nombre, url)) {
        printf("Instalación cancelada.\n\n");
        return 0;
    }

    char dir[1024];
    dir_paquetes(dir, sizeof(dir));
    crear_dir_si_no_existe(dir);

    char destino[1024];
    snprintf(destino, sizeof(destino), "%s/%s.lince", dir, nombre);

    printf("\n⬇  Descargando '%s'...\n", nombre);

    /* Detectar si es GitHub repo o archivo directo */
    char cmd[4096];
    int  es_github = (strstr(url, "github.com") != NULL &&
                      strstr(url, ".lince") == NULL);

    if (es_github) {
        /* GitHub repo — intentar descargar main.lince o nombre.lince */
        char url_raw[1024];
        /* Convertir github.com/user/repo a raw.githubusercontent.com */
        char user_repo[512] = "";
        const char *p = strstr(url, "github.com/");
        if (p) strncpy(user_repo, p + 11, sizeof(user_repo) - 1);

        /* Quitar .git si tiene */
        char *git = strstr(user_repo, ".git");
        if (git) *git = '\0';

        snprintf(url_raw, sizeof(url_raw),
            "https://raw.githubusercontent.com/%s/main/%s.lince",
            user_repo, nombre);

        snprintf(cmd, sizeof(cmd),
            "curl -s -f -L \"%s\" -o \"%s\" 2>/dev/null",
            url_raw, destino);

        if (system(cmd) != 0) {
            /* Intentar con master */
            snprintf(url_raw, sizeof(url_raw),
                "https://raw.githubusercontent.com/%s/master/%s.lince",
                user_repo, nombre);
            snprintf(cmd, sizeof(cmd),
                "curl -s -f -L \"%s\" -o \"%s\" 2>/dev/null",
                url_raw, destino);
            if (system(cmd) != 0) {
                fprintf(stderr,
                    "\n❌ No se pudo descargar el paquete.\n"
                    "   Comprueba que la URL sea correcta y que el archivo\n"
                    "   '%s.lince' exista en el repositorio.\n\n", nombre);
                return 1;
            }
        }
    } else {
        /* URL directa a un .lince */
        snprintf(cmd, sizeof(cmd),
            "curl -s -f -L \"%s\" -o \"%s\" 2>/dev/null",
            url, destino);
        if (system(cmd) != 0) {
            fprintf(stderr,
                "\n❌ No se pudo descargar el paquete desde:\n"
                "   %s\n"
                "   Comprueba que la URL sea correcta.\n\n", url);
            return 1;
        }
    }

    /* Verificar que el archivo descargado parece Lince válido */
    FILE *f = fopen(destino, "r");
    if (!f) {
        fprintf(stderr, "\n❌ Error al guardar el paquete.\n\n");
        return 1;
    }
    char primera[256] = "";
    if (fgets(primera, sizeof(primera), f)) {
        /* Si empieza con <!DOCTYPE o <html es una página de error */
        if (strncmp(primera, "<!DOCTYPE", 9) == 0 ||
            strncmp(primera, "<html", 5) == 0) {
            fclose(f);
            remove(destino);
            fprintf(stderr,
                "\n❌ La URL no devolvió código Lince válido.\n"
                "   Comprueba que el archivo existe en esa ubicación.\n\n");
            return 1;
        }
    }
    fclose(f);

    printf("✅ Paquete '%s' instalado correctamente.\n", nombre);
    printf("   Ubicación: %s\n\n", destino);
    printf("   Úsalo con:\n");
    printf("   importar \"paquete:%s\"\n\n", nombre);
    return 0;
}

/* ─────────────────────────────────────────
   DESINSTALAR
───────────────────────────────────────── */
int paquetes_desinstalar(const char *nombre) {
    char dir[1024], ruta[1024];
    dir_paquetes(dir, sizeof(dir));
    snprintf(ruta, sizeof(ruta), "%s/%s.lince", dir, nombre);

    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr,
            "\n❌ El paquete '%s' no está instalado.\n\n", nombre);
        return 1;
    }
    fclose(f);

    printf("\n🗑️  Desinstalando '%s'...\n", nombre);
    if (remove(ruta) != 0) {
        fprintf(stderr, "\n❌ No se pudo desinstalar '%s'.\n\n", nombre);
        return 1;
    }
    printf("✅ Paquete '%s' desinstalado.\n\n", nombre);
    return 0;
}

/* ─────────────────────────────────────────
   LISTAR
───────────────────────────────────────── */
int paquetes_listar(void) {
    char dir[1024];
    dir_paquetes(dir, sizeof(dir));

    printf("\n🐆 Lince — Paquetes instalados\n");
    printf("   Directorio: %s\n\n", dir);

    char cmd[2048];
#ifdef LINCE_WINDOWS
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\\*.lince\" 2>nul", dir);
#else
    snprintf(cmd, sizeof(cmd), "ls \"%s\"/*.lince 2>/dev/null", dir);
#endif

    FILE *proc = popen(cmd, "r");
    if (!proc) {
        printf("   (ninguno)\n\n");
        return 0;
    }

    char linea[512];
    int  count = 0;
    while (fgets(linea, sizeof(linea), proc)) {
        /* Extraer solo el nombre sin ruta ni extensión */
        char *nombre = strrchr(linea, '/');
        if (!nombre) nombre = strrchr(linea, '\\');
        if (nombre) nombre++; else nombre = linea;

        char *ext = strstr(nombre, ".lince");
        if (ext) *ext = '\0';

        /* Quitar salto de línea */
        nombre[strcspn(nombre, "\n\r")] = '\0';

        printf("   📦 %s\n", nombre);
        count++;
    }
    pclose(proc);

    if (count == 0)
        printf("   (ninguno)\n");

    printf("\n");
    return 0;
}
