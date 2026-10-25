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
#include "firmar.h"

const char *interprete_archivo_principal(void);   /* interprete.h (sin mezclarlo con windows.h) */

/* ─────────────────────────────────────────
   RUTAS
───────────────────────────────────────── */
#define ARCHIVO_PROYECTO "lince.paquetes"

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
   PAQUETES DEL JUEGO, CON SU VERSIÓN
   'lince instalar <url>@v1.2' lo baja a paquetes/<nombre>.lince y lo apunta
   en lince.paquetes con la versión y su huella (SHA-256). 'lince instalar'
   sin nada baja otra vez exactamente eso: si lo que llega no tiene la misma
   huella, no lo cambia y lo dice. Así el juego funciona igual en cualquier
   ordenador y aunque el autor del paquete lo cambie.
───────────────────────────────────────── */
typedef struct { char nombre[128], version[128], origen[1024], huella[65]; } Entrada;

static int leer_proyecto(Entrada **out) {
    FILE *f = fopen(ARCHIVO_PROYECTO, "r");
    *out = NULL;
    if (!f) return 0;
    int n = 0, cap = 0;
    char linea[2048];
    while (fgets(linea, sizeof linea, f)) {
        char *p = linea;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || !*p) continue;
        Entrada e;
        memset(&e, 0, sizeof e);
        if (sscanf(p, "%127s %127s %1023s %64s", e.nombre, e.version, e.origen, e.huella) < 3) continue;
        if (n == cap) { cap = cap ? cap * 2 : 8; *out = realloc(*out, sizeof(Entrada) * (size_t)cap); }
        (*out)[n++] = e;
    }
    fclose(f);
    return n;
}

static int guardar_proyecto(const Entrada *e, int n) {
    FILE *f = fopen(ARCHIVO_PROYECTO, "w");
    if (!f) return 0;
    fprintf(f, "# Los paquetes de este juego. 'lince instalar' (sin nada más) los baja\n"
               "# tal cual: esta versión y con esta huella. Para cambiar de versión:\n"
               "#   lince instalar <origen>@<versión>\n"
               "# nombre  versión  origen  huella (SHA-256)\n");
    for (int i = 0; i < n; i++)
        fprintf(f, "%s  %s  %s  %s\n", e[i].nombre, e[i].version, e[i].origen, e[i].huella);
    fclose(f);
    return 1;
}

static void huella_de(const char *ruta, char *hex) {
    hex[0] = '\0';
    FILE *f = fopen(ruta, "rb");
    if (!f) return;
    Sha256 s;
    sha256_empezar(&s);
    unsigned char buf[8192];
    size_t r;
    while ((r = fread(buf, 1, sizeof buf, f)) > 0) sha256_mas(&s, buf, r);
    fclose(f);
    unsigned char d[32];
    sha256_acabar(&s, d);
    for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", d[i]);
}

/* La dirección de donde se baja: en GitHub, el archivo <nombre>.lince del
   repositorio en esa versión (una etiqueta, una rama o un commit) */
static void direccion(const char *origen, const char *nombre, const char *version, char *out, size_t max) {
    const char *gh = strstr(origen, "github.com/");
    if (gh && !strstr(origen, ".lince")) {
        char repo[512];
        snprintf(repo, sizeof repo, "%s", gh + 11);
        char *git = strstr(repo, ".git");
        if (git) *git = '\0';
        size_t l = strlen(repo);
        if (l && repo[l - 1] == '/') repo[l - 1] = '\0';
        /* LINCE_PAQUETES_RAW: otra raíz (para las pruebas, sin internet) */
        const char *raiz = getenv("LINCE_PAQUETES_RAW");
        snprintf(out, max, "%s/%s/%s/%s.lince", raiz ? raiz : "https://raw.githubusercontent.com",
                 repo, version, nombre);
    } else snprintf(out, max, "%s", origen);
}

static int bajar(const char *url, const char *destino) {
    char cmd[4096];
#ifdef LINCE_WINDOWS
    snprintf(cmd, sizeof cmd, "curl -s -f -L \"%s\" -o \"%s\" 2>nul", url, destino);
#else
    snprintf(cmd, sizeof cmd, "curl -s -f -L '%s' -o '%s' 2>/dev/null", url, destino);
#endif
    if (system(cmd) != 0) return 0;
    FILE *f = fopen(destino, "r");
    if (!f) return 0;
    char primera[64] = "";
    int html = fgets(primera, sizeof primera, f) &&
               (!strncmp(primera, "<!DOCTYPE", 9) || !strncmp(primera, "<html", 5));
    fclose(f);
    if (html) { remove(destino); return 0; }
    return 1;
}

/* Baja 'e' a paquetes/; con huella, tiene que ser esa. 1 si va bien. */
static int bajar_entrada(Entrada *e, int comprobar) {
    char url[2048], tmp[1200], destino[1200];
    fflush(stdout);                      /* que lo de stderr salga en su sitio */
    crear_dir_si_no_existe("paquetes");
    snprintf(destino, sizeof destino, "paquetes/%s.lince", e->nombre);
    snprintf(tmp, sizeof tmp, "paquetes/.%s.bajando", e->nombre);
    direccion(e->origen, e->nombre, e->version, url, sizeof url);
    if (!bajar(url, tmp)) {
        fprintf(stderr, "❌ No se pudo bajar '%s' (versión %s) de:\n   %s\n", e->nombre, e->version, url);
        return 0;
    }
    char h[65];
    huella_de(tmp, h);
    if (comprobar && e->huella[0] && strcmp(h, e->huella) != 0) {
        remove(tmp);
        fprintf(stderr, "❌ '%s' %s no es el que se apuntó en %s: ha cambiado desde entonces.\n"
                        "   No lo cambio. Si quieres el nuevo, instálalo otra vez:\n"
                        "   lince instalar %s@%s\n", e->nombre, e->version, ARCHIVO_PROYECTO, e->origen, e->version);
        return 0;
    }
    remove(destino);
    if (rename(tmp, destino) != 0) { remove(tmp); fprintf(stderr, "❌ No pude guardar %s\n", destino); return 0; }
    snprintf(e->huella, sizeof e->huella, "%s", h);
    return 1;
}

static int instalar_en_proyecto(const char *url, const char *version) {
    Entrada *es;
    int n = leer_proyecto(&es);
    Entrada e;
    memset(&e, 0, sizeof e);
    nombre_desde_url(url, e.nombre, sizeof e.nombre);
    snprintf(e.origen, sizeof e.origen, "%s", url);
    /* sin versión, la de siempre si ya estaba; si no, la rama principal */
    const char *v = version;
    for (int i = 0; i < n && !v; i++) if (strcmp(es[i].nombre, e.nombre) == 0) v = es[i].version;
    snprintf(e.version, sizeof e.version, "%s", v && v[0] ? v : "main");

    printf("\n🐆 Lince — Gestor de paquetes\n\n");
    printf("   Paquete : %s\n", e.nombre);
    printf("   Versión : %s\n", e.version);
    printf("   Origen  : %s\n", e.origen);
    printf("   Para    : este juego (paquetes/%s.lince, apuntado en %s)\n\n", e.nombre, ARCHIVO_PROYECTO);
    printf("¿Instalar? [s/N] ");
    fflush(stdout);
    char resp[8];
    if (!fgets(resp, sizeof resp, stdin) || (resp[0] != 's' && resp[0] != 'S')) {
        printf("Instalación cancelada.\n\n");
        free(es);
        return 0;
    }
    if (!bajar_entrada(&e, 0)) { free(es); return 1; }
    int k = 0;
    while (k < n && strcmp(es[k].nombre, e.nombre) != 0) k++;
    if (k == n) { es = realloc(es, sizeof(Entrada) * (size_t)(n + 1)); n++; }
    es[k] = e;
    if (!guardar_proyecto(es, n)) { fprintf(stderr, "❌ No pude escribir %s\n", ARCHIVO_PROYECTO); free(es); return 1; }
    printf("✅ '%s' %s instalado para este juego (huella %.12s…).\n\n", e.nombre, e.version, e.huella);
    printf("   Úsalo con:\n   importar \"paquete:%s\"\n\n", e.nombre);
    free(es);
    return 0;
}

int paquetes_instalar_proyecto(void) {
    Entrada *es;
    int n = leer_proyecto(&es);
    if (n == 0) {
        free(es);
        fprintf(stderr, "\n❌ Aquí no hay %s con paquetes que instalar.\n"
                        "   Instala uno con: lince instalar <url>@<versión>\n\n", ARCHIVO_PROYECTO);
        return 1;
    }
    printf("\n🐆 Lince — Los paquetes de %s\n\n", ARCHIVO_PROYECTO);
    int mal = 0, cambios = 0;
    for (int i = 0; i < n; i++) {
        char destino[1200], h[65];
        snprintf(destino, sizeof destino, "paquetes/%s.lince", es[i].nombre);
        huella_de(destino, h);
        if (es[i].huella[0] && strcmp(h, es[i].huella) == 0) {
            printf("   ✅ %s %s (ya estaba)\n", es[i].nombre, es[i].version);
            continue;
        }
        int sin_huella = !es[i].huella[0];
        if (bajar_entrada(&es[i], 1)) {
            printf("   ⬇  %s %s\n", es[i].nombre, es[i].version);
            if (sin_huella) cambios = 1;          /* se apunta la huella que faltaba */
        } else mal++;
    }
    if (cambios) guardar_proyecto(es, n);
    free(es);
    printf("\n");
    return mal ? 1 : 0;
}

/* ¿Es un paquete de este juego? Lo quita de lince.paquetes y de paquetes/ */
static int desinstalar_de_proyecto(const char *nombre) {
    Entrada *es;
    int n = leer_proyecto(&es), k = 0;
    while (k < n && strcmp(es[k].nombre, nombre) != 0) k++;
    if (k == n) { free(es); return -1; }
    memmove(&es[k], &es[k + 1], sizeof(Entrada) * (size_t)(n - k - 1));
    guardar_proyecto(es, n - 1);
    free(es);
    char ruta[1200];
    snprintf(ruta, sizeof ruta, "paquetes/%s.lince", nombre);
    remove(ruta);
    printf("\n✅ Paquete '%s' quitado de este juego.\n\n", nombre);
    return 0;
}

/* ─────────────────────────────────────────
   RESOLVER RUTA DE PAQUETE
───────────────────────────────────────── */
/* El directorio de 'archivo' ("" si no tiene) */
static void carpeta_de(const char *archivo, char *out, size_t max) {
    snprintf(out, max, "%s", archivo);
    char *b = strrchr(out, '/');
#ifdef LINCE_WINDOWS
    char *b2 = strrchr(out, '\\');
    if (!b || (b2 && b2 > b)) b = b2;
#endif
    if (b) b[1] = '\0'; else out[0] = '\0';
}

int paquetes_resolver(const char *nombre, char *ruta_out, size_t max) {
    /* Primero los del juego (paquetes/ junto al programa, con la versión de
       lince.paquetes), luego los de la carpeta de trabajo y luego los de
       todos (~/.lince/paquetes) */
    char base[1024];
    const char *principal = interprete_archivo_principal();
    carpeta_de(principal ? principal : "", base, sizeof base);
    for (int k = 0; k < 2; k++) {
        if (k == 1 && !base[0]) break;
        snprintf(ruta_out, max, "%spaquetes/%s.lince", k == 0 ? base : "", nombre);
        FILE *f = fopen(ruta_out, "r");
        if (f) { fclose(f); return 1; }
    }
    char dir[1024];
    dir_paquetes(dir, sizeof(dir));
    snprintf(ruta_out, max, "%s/%s.lince", dir, nombre);
    FILE *f = fopen(ruta_out, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}
static int instalar_en_proyecto(const char *url, const char *version);

int paquetes_instalar(const char *url) {
    /* url@versión, o una carpeta con lince.paquetes: el paquete es del juego */
    const char *barra = strrchr(url, '/');
    const char *arroba = strrchr(barra ? barra : url, '@');
    FILE *proyecto = fopen(ARCHIVO_PROYECTO, "r");
    if (proyecto) fclose(proyecto);
    if (arroba || proyecto) {
        char base[1024];
        snprintf(base, sizeof base, "%.*s", (int)(arroba ? arroba - url : (long)strlen(url)), url);
        return instalar_en_proyecto(base, arroba ? arroba + 1 : NULL);
    }
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
    if (desinstalar_de_proyecto(nombre) == 0) return 0;
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

    Entrada *es;
    int np = leer_proyecto(&es);
    if (np > 0) {
        printf("\n🐆 Lince — Paquetes de este juego (%s)\n\n", ARCHIVO_PROYECTO);
        for (int i = 0; i < np; i++) printf("   📦 %-20s %-12s %s\n", es[i].nombre, es[i].version, es[i].origen);
    }
    free(es);

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
