/*
 * 🐆 Lince — escritorio.c
 * Los juegos repartidos para el escritorio (ver escritorio.h).
 *
 * Un juego repartido es, como el de 'lince compilar', el ejecutable de Lince
 * con el juego pegado al final (empaquetado.c), y lleva además:
 *   "lince:paquete"   modo, nombre e id (texto "clave=valor" por líneas)
 *   "lince:icono.png" su icono, de 256×256 (el de la ventana y el del menú)
 *   "lince:sdl/..."   SDL2 para Linux (el archivo único)
 *   "lince:dll/..."   SDL2.dll y las DLL que necesite (el instalador de Windows)
 *
 * En Windows, el instalador es el propio juego en modo "instalador": se
 * instala solo para quien lo abre (sin pedir permiso de administrador) en
 * %LOCALAPPDATA%\Programs\<juego>, con su acceso en el menú Inicio (y en el
 * escritorio, si se quiere), y se desinstala desde la configuración de
 * Windows como cualquier programa. Lo que instala es una copia de sí mismo en
 * modo "instalado" y sin las DLL, que van sueltas al lado.
 *
 * En Linux, el archivo único lleva SDL2 dentro y la saca a la caché si el
 * sistema no la tiene. La primera vez que se abre pregunta si ponerse en el
 * menú de aplicaciones: entonces se copia a ~/.local/share/lince-juegos/<id>
 * con su icono y su .desktop (y '--desinstalar' lo quita).
 */

#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include "escritorio.h"
#include "empaquetado.h"
#include "imagen.h"
#include "icono_lince.h"

#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

/* ── Lo que lleva el paquete ── */

static void quitar_raros(char *s) {          /* sin saltos de línea ni '=' (es "clave=valor") */
    for (; *s; s++) if (*s == '\n' || *s == '\r') *s = ' ';
}

char *escritorio_paquete_texto(const char *modo, const char *nombre, const char *id) {
    size_t n = strlen(modo) + strlen(nombre) + strlen(id) + 32;
    char *t = malloc(n);
    if (!t) return NULL;
    snprintf(t, n, "modo=%s\nnombre=%s\nid=%s\n", modo, nombre, id);
    return t;
}

int escritorio_paquete(EscritorioPaquete *p) {
    memset(p, 0, sizeof *p);
    size_t largo = 0;
    const char *t = empaquetado_buscar("lince:paquete", &largo);
    if (!t) return 0;
    char *copia = malloc(largo + 1);
    if (!copia) return 0;
    memcpy(copia, t, largo);
    copia[largo] = '\0';
    for (char *l = strtok(copia, "\n"); l; l = strtok(NULL, "\n")) {
        char *igual = strchr(l, '=');
        if (!igual) continue;
        *igual = '\0';
        const char *v = igual + 1;
        if (!strcmp(l, "modo")) snprintf(p->modo, sizeof p->modo, "%s", v);
        else if (!strcmp(l, "nombre")) snprintf(p->nombre, sizeof p->nombre, "%s", v);
        else if (!strcmp(l, "id")) snprintf(p->id, sizeof p->id, "%s", v);
    }
    free(copia);
    quitar_raros(p->nombre);
    return p->modo[0] != '\0' && p->id[0] != '\0';
}

const unsigned char *escritorio_icono(size_t *largo) {
    const char *propio = empaquetado_buscar("lince:icono.png", largo);
    if (propio) return (const unsigned char *)propio;
    *largo = sizeof ICONO_LINCE;
    return ICONO_LINCE;
}

#ifdef _WIN32
/* El nombre del juego, como nombre de archivo: sin los caracteres que no
   puede llevar en Windows */
static void nombre_archivo(const char *nombre, char *out, size_t max) {
    size_t k = 0;
    for (const char *c = nombre; *c && k + 1 < max; c++)
        if (!strchr("<>:\"/\\|?*", *c) && (unsigned char)*c >= 32) out[k++] = *c;
    while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '.')) k--;
    out[k] = '\0';
    if (!k) snprintf(out, max, "juego");
}
#endif

/* ── Escalar un icono: la media de los píxeles de cada trozo (con el alfa en
   cuenta), centrado en un cuadrado de t × t si no es cuadrado ── */
void escritorio_escalar(const unsigned char *src, int w0, int h0, int t, unsigned char *dst) {
    memset(dst, 0, (size_t)t * t * 4);
    double s = (double)(w0 > h0 ? w0 : h0) / t;
    int ancho = (int)(w0 / s + 0.5), alto = (int)(h0 / s + 0.5);
    int ox = (t - ancho) / 2, oy = (t - alto) / 2;
    for (int y = 0; y < alto; y++)
        for (int x = 0; x < ancho; x++) {
            double x0 = x * s, x1 = (x + 1) * s, y0 = y * s, y1 = (y + 1) * s;
            double r = 0, g = 0, b = 0, a = 0, area = 0;
            for (int sy = (int)y0; sy < (int)(y1 + 0.999) && sy < h0; sy++)
                for (int sx = (int)x0; sx < (int)(x1 + 0.999) && sx < w0; sx++) {
                    double cx = (sx + 1 < x1 ? sx + 1 : x1) - (sx > x0 ? sx : x0);
                    double cy = (sy + 1 < y1 ? sy + 1 : y1) - (sy > y0 ? sy : y0);
                    if (cx <= 0 || cy <= 0) continue;
                    const unsigned char *p = src + ((size_t)sy * w0 + sx) * 4;
                    double pa = p[3] / 255.0 * cx * cy;
                    r += p[0] * pa; g += p[1] * pa; b += p[2] * pa; a += pa; area += cx * cy;
                }
            unsigned char *d = dst + ((size_t)(y + oy) * t + (x + ox)) * 4;
            if (a > 0) { d[0] = (unsigned char)(r / a + 0.5); d[1] = (unsigned char)(g / a + 0.5); d[2] = (unsigned char)(b / a + 0.5); }
            d[3] = area > 0 ? (unsigned char)(a / area * 255 + 0.5) : 0;
        }
}

/* ── Un .exe de Windows (formato PE), en memoria ── */
static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* Dónde empieza la cabecera opcional y cuántas secciones hay; 0 si no es un PE */
static size_t pe_cabecera(const unsigned char *d, size_t n, int *nsec, size_t *secs) {
    if (n < 0x40 || d[0] != 'M' || d[1] != 'Z') return 0;
    uint32_t pe = le32(d + 0x3c);
    if ((size_t)pe + 24 > n || memcmp(d + pe, "PE\0\0", 4) != 0) return 0;
    *nsec = le16(d + pe + 6);
    size_t opc = pe + 24;
    *secs = opc + le16(d + pe + 20);
    if (*secs + 40 * (size_t)*nsec > n || opc + 70 > n) return 0;
    return opc;
}

static size_t pe_rva(const unsigned char *d, int nsec, size_t secs, uint32_t rva) {
    for (int s = 0; s < nsec; s++) {
        const unsigned char *h = d + secs + 40 * (size_t)s;
        uint32_t va = le32(h + 12), tam = le32(h + 16), raw = le32(h + 20);
        if (rva >= va && rva < va + tam) return raw + (rva - va);
    }
    return 0;
}

int escritorio_pe_sin_consola(unsigned char *exe, size_t largo) {
    int nsec;
    size_t secs, opc = pe_cabecera(exe, largo, &nsec, &secs);
    if (!opc) return 0;
    exe[opc + 68] = 2;                       /* IMAGE_SUBSYSTEM_WINDOWS_GUI */
    exe[opc + 69] = 0;
    return 1;
}

int escritorio_pe_icono(unsigned char *exe, size_t largo, const unsigned char *rgba, int ancho, int alto) {
    int nsec;
    size_t secs, opc = pe_cabecera(exe, largo, &nsec, &secs);
    if (!opc) return 0;
    size_t dd = opc + (le16(exe + opc) == 0x20b ? 112 : 96);
    if (dd + 24 > largo) return 0;
    size_t rsrc = pe_rva(exe, nsec, secs, le32(exe + dd + 16));      /* la tabla de recursos */
    if (!rsrc || rsrc + 16 > largo) return 0;
    int cambiados = 0;
    /* raíz → tipo 3 (RT_ICON) → cada icono → cada idioma → sus datos */
    #define ENTRADAS(dir) (le16(exe + (dir) + 12) + le16(exe + (dir) + 14))
    for (int i = 0; i < ENTRADAS(rsrc); i++) {
        size_t e = rsrc + 16 + 8 * (size_t)i;
        if (e + 8 > largo || le32(exe + e) != 3 || !(le32(exe + e + 4) & 0x80000000u)) continue;
        size_t iconos = rsrc + (le32(exe + e + 4) & 0x7fffffffu);
        if (iconos + 16 > largo) break;
        for (int j = 0; j < ENTRADAS(iconos); j++) {
            size_t ej = iconos + 16 + 8 * (size_t)j;
            if (ej + 8 > largo || !(le32(exe + ej + 4) & 0x80000000u)) continue;
            size_t idiomas = rsrc + (le32(exe + ej + 4) & 0x7fffffffu);
            if (idiomas + 16 > largo) continue;
            for (int k = 0; k < ENTRADAS(idiomas); k++) {
                size_t ek = idiomas + 16 + 8 * (size_t)k;
                if (ek + 8 > largo || (le32(exe + ek + 4) & 0x80000000u)) continue;
                size_t hoja = rsrc + le32(exe + ek + 4);
                if (hoja + 16 > largo) continue;
                size_t pos = pe_rva(exe, nsec, secs, le32(exe + hoja)), tam = le32(exe + hoja + 4);
                if (!pos || pos + tam > largo || tam < 40) continue;
                unsigned char *bmp = exe + pos;
                int w = (int)le32(bmp + 4), h = (int)le32(bmp + 8) / 2;
                size_t mascara = (size_t)((w + 31) / 32) * 4 * (size_t)h;
                if (le32(bmp) != 40 || w < 1 || w > 256 || h != w || le16(bmp + 14) != 32 || le32(bmp + 16) != 0 ||
                    tam != 40 + (size_t)w * h * 4 + mascara) continue;          /* solo los BMP de 32 bits */
                unsigned char *px = malloc((size_t)w * w * 4);
                if (!px) continue;
                escritorio_escalar(rgba, ancho, alto, w, px);
                for (int y = 0; y < h; y++)                                     /* de abajo arriba, en BGRA */
                    for (int x = 0; x < w; x++) {
                        const unsigned char *s = px + ((size_t)y * w + x) * 4;
                        unsigned char *o = bmp + 40 + ((size_t)(h - 1 - y) * w + x) * 4;
                        o[0] = s[2]; o[1] = s[1]; o[2] = s[0]; o[3] = s[3];
                    }
                memset(bmp + 40 + (size_t)w * h * 4, 0, mascara);
                free(px);
                cambiados++;
            }
        }
    }
    #undef ENTRADAS
    return cambiados;
}

/* ── Archivos ── */
static int escribir_archivo(const char *ruta, const void *datos, size_t largo, int ejecutable) {
    FILE *f = fopen(ruta, "wb");
    if (!f) return 0;
    int ok = fwrite(datos, 1, largo, f) == largo;
    ok = fclose(f) == 0 && ok;
#ifndef _WIN32
    if (ok) chmod(ruta, ejecutable ? 0755 : 0644);
#else
    (void)ejecutable;
#endif
    return ok;
}

static void crear_carpetas(const char *ruta) {
    char r[2048];
    snprintf(r, sizeof r, "%s", ruta);
    for (char *p = r + 1; *p; p++)
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = '\0';
#ifdef _WIN32
            if (!(p == r + 2 && r[1] == ':')) _mkdir(r);
#else
            mkdir(r, 0755);
#endif
            *p = c;
        }
#ifdef _WIN32
    _mkdir(r);
#else
    mkdir(r, 0755);
#endif
}

/* ═══ Linux: el archivo único ═══ */
#if !defined(_WIN32) && !defined(__ANDROID__) && !defined(__APPLE__)

static uint32_t crc32_de(const unsigned char *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    return ~c;
}

/* $XDG_DATA_HOME (o ~/.local/share), $XDG_CACHE_HOME (o ~/.cache) */
static int carpeta_xdg(const char *variable, const char *defecto, char *out, size_t max) {
    const char *x = getenv(variable), *home = getenv("HOME");
    if (x && x[0] == '/') { snprintf(out, max, "%s", x); return 1; }
    if (!home || !home[0]) return 0;
    snprintf(out, max, "%s/%s", home, defecto);
    return 1;
}

const char *escritorio_sdl_del_paquete(void) {
    static char ruta[2048];
    size_t largo = 0;
    const char *sdl = empaquetado_buscar("lince:sdl/libSDL2-2.0.so.0", &largo);
    char cache[1500];
    if (!sdl || !largo || !carpeta_xdg("XDG_CACHE_HOME", ".cache", cache, sizeof cache)) return NULL;
    char carpeta[1800];
    snprintf(carpeta, sizeof carpeta, "%s/lince/sdl-%zu-%08x", cache, largo, (unsigned)crc32_de((const unsigned char *)sdl, largo));
    snprintf(ruta, sizeof ruta, "%s/libSDL2-2.0.so.0", carpeta);
    struct stat st;
    if (stat(ruta, &st) == 0 && (size_t)st.st_size == largo) return ruta;
    crear_carpetas(carpeta);
    char tmp[2100];
    snprintf(tmp, sizeof tmp, "%s.%d", ruta, (int)getpid());           /* entera o nada, aunque se abran dos a la vez */
    if (!escribir_archivo(tmp, sdl, largo, 0) || rename(tmp, ruta) != 0) { remove(tmp); return NULL; }
    return ruta;
}

/* Dónde va todo lo de este juego al ponerlo en el menú */
typedef struct { char carpeta[1600], exe[1800], icono[1800], desktop[1800], respuesta[1800]; } RutasMenu;

static int rutas_menu(const EscritorioPaquete *p, RutasMenu *r) {
    char datos[1400];
    if (!carpeta_xdg("XDG_DATA_HOME", ".local/share", datos, sizeof datos)) return 0;
    snprintf(r->carpeta, sizeof r->carpeta, "%s/lince-juegos/%s", datos, p->id);
    snprintf(r->exe, sizeof r->exe, "%s/%s", r->carpeta, p->id);
    snprintf(r->icono, sizeof r->icono, "%s/icons/hicolor/256x256/apps/lince-%s.png", datos, p->id);
    snprintf(r->desktop, sizeof r->desktop, "%s/applications/lince-%s.desktop", datos, p->id);
    snprintf(r->respuesta, sizeof r->respuesta, "%s/lince-juegos/%s.menu", datos, p->id);
    return 1;
}

static void carpeta_de(const char *ruta, char *out, size_t max) {
    snprintf(out, max, "%s", ruta);
    char *b = strrchr(out, '/');
    if (b && b > out) *b = '\0';
}

static int poner_en_menu(const EscritorioPaquete *p, int callado) {
    RutasMenu r;
    char yo[2048], dir[1800];
    if (!rutas_menu(p, &r) || !empaquetado_ruta_ejecutable(yo, sizeof yo)) return 0;
    crear_carpetas(r.carpeta);
    if (strcmp(yo, r.exe) != 0) {                       /* una copia, por si se mueve o se borra el archivo */
        FILE *in = fopen(yo, "rb");
        if (!in) return 0;
        fseek(in, 0, SEEK_END);
        long n = ftell(in);
        rewind(in);
        char *todo = n > 0 ? malloc((size_t)n) : NULL;
        int ok = todo && fread(todo, 1, (size_t)n, in) == (size_t)n;
        fclose(in);
        char tmp[1900];
        snprintf(tmp, sizeof tmp, "%s.nuevo", r.exe);
        ok = ok && escribir_archivo(tmp, todo, (size_t)n, 1) && rename(tmp, r.exe) == 0;
        free(todo);
        if (!ok) return 0;
    }
    size_t largo;
    const unsigned char *png = escritorio_icono(&largo);
    int w, h;
    const char *por = NULL;
    unsigned char *px = imagen_decodificar(png, largo, &w, &h, &por);
    if (px) {
        unsigned char *t = malloc(256 * 256 * 4);
        if (t) {
            escritorio_escalar(px, w, h, 256, t);
            carpeta_de(r.icono, dir, sizeof dir);
            crear_carpetas(dir);
            imagen_guardar_png(r.icono, t, 256, 256);
            free(t);
        }
        free(px);
    }
    carpeta_de(r.desktop, dir, sizeof dir);
    crear_carpetas(dir);
    char escritorio[6000];
    snprintf(escritorio, sizeof escritorio,
             "[Desktop Entry]\nType=Application\nName=%s\nComment=Juego hecho con Lince\nExec=\"%s\"\n"
             "Icon=lince-%s\nTerminal=false\nCategories=Game;\nStartupWMClass=lince-%s\n",
             p->nombre, r.exe, p->id, p->id);
    if (!escribir_archivo(r.desktop, escritorio, strlen(escritorio), 1)) return 0;
    escribir_archivo(r.respuesta, "si\n", 3, 0);
    if (!callado) printf("✅ «%s» está en el menú de aplicaciones (para quitarlo: \"%s\" --desinstalar)\n", p->nombre, r.exe);
    return 1;
}

static int quitar_del_menu(const EscritorioPaquete *p) {
    RutasMenu r;
    if (!rutas_menu(p, &r)) return 0;
    remove(r.desktop);
    remove(r.icono);
    remove(r.exe);
    rmdir(r.carpeta);
    escribir_archivo(r.respuesta, "no\n", 3, 0);          /* y no vuelve a preguntar */
    printf("✅ «%s» ya no está en el menú de aplicaciones (sus partidas guardadas siguen en su sitio)\n", p->nombre);
    return 1;
}

int escritorio_toca_preguntar(void) {
    EscritorioPaquete p;
    RutasMenu r;
    char yo[2048];
    if (!escritorio_paquete(&p) || strcmp(p.modo, "archivo") != 0 || !rutas_menu(&p, &r)) return 0;
    const char *v = getenv("SDL_VIDEODRIVER");
    if (getenv("LINCE_JUEGO_FOTOGRAMAS") || (v && (!strcmp(v, "dummy") || !strcmp(v, "offscreen")))) return 0;
    if (empaquetado_ruta_ejecutable(yo, sizeof yo) && !strcmp(yo, r.exe)) return 0;   /* ya es la del menú */
    struct stat st;
    return stat(r.respuesta, &st) != 0;
}

void escritorio_responder(int si) {
    EscritorioPaquete p;
    RutasMenu r;
    if (!escritorio_paquete(&p) || !rutas_menu(&p, &r)) return;
    if (si) {
        if (!poner_en_menu(&p, 0)) fprintf(stderr, "⚠️  No se pudo poner «%s» en el menú de aplicaciones\n", p.nombre);
    } else {
        char dir[1800];
        carpeta_de(r.respuesta, dir, sizeof dir);
        crear_carpetas(dir);
        escribir_archivo(r.respuesta, "no\n", 3, 0);
    }
}

int escritorio_arrancar(int argc, char **argv, int *codigo) {
    EscritorioPaquete p;
    if (!escritorio_paquete(&p) || strcmp(p.modo, "archivo") != 0) return 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--instalar-en-menu")) { *codigo = poner_en_menu(&p, 0) ? 0 : 1; return 1; }
        if (!strcmp(argv[i], "--desinstalar")) { *codigo = quitar_del_menu(&p) ? 0 : 1; return 1; }
    }
    return 0;
}

/* ═══ Windows: el instalador ═══ */
#elif defined(_WIN32)

const char *escritorio_sdl_del_paquete(void) { return NULL; }
int escritorio_toca_preguntar(void) { return 0; }
void escritorio_responder(int si) { (void)si; }

static wchar_t *ancho(const char *utf8) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t *w = malloc(sizeof(wchar_t) * (size_t)(n > 0 ? n : 1));
    if (!w) return NULL;
    if (n <= 0) w[0] = 0;
    else MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    return w;
}

static void a_utf8(const wchar_t *w, char *out, size_t max) {
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)max, NULL, NULL)) out[0] = '\0';
}

static int preguntar(int callado, const char *titulo, const char *texto) {
    if (callado) return 1;
    wchar_t *t = ancho(titulo), *m = ancho(texto);
    int r = MessageBoxW(NULL, m, t, MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDYES;
    free(t); free(m);
    return r;
}

static void avisar(int callado, const char *titulo, const char *texto, int error) {
    if (callado) { fprintf(error ? stderr : stdout, "%s %s\n", error ? "❌" : "✅", texto); return; }
    wchar_t *t = ancho(titulo), *m = ancho(texto);
    MessageBoxW(NULL, m, t, MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION) | MB_SETFOREGROUND);
    free(t); free(m);
}

/* Una carpeta conocida de Windows (el menú Inicio, el escritorio...), en UTF-8 */
static int carpeta_conocida(REFKNOWNFOLDERID id, char *out, size_t max) {
    PWSTR w = NULL;
    int ok = SHGetKnownFolderPath(id, KF_FLAG_CREATE, NULL, &w) == S_OK;
    if (ok) a_utf8(w, out, max);
    CoTaskMemFree(w);
    return ok && out[0];
}

/* Un acceso directo (.lnk) a 'destino' */
static int acceso_directo(const char *lnk, const char *destino, const char *carpeta, const char *descripcion) {
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl))) return 0;
    wchar_t *d = ancho(destino), *c = ancho(carpeta), *t = ancho(descripcion), *l = ancho(lnk);
    IShellLinkW_SetPath(sl, d);
    IShellLinkW_SetWorkingDirectory(sl, c);
    IShellLinkW_SetDescription(sl, t);
    IShellLinkW_SetIconLocation(sl, d, 0);
    if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
        ok = SUCCEEDED(IPersistFile_Save(pf, l, TRUE));
        IPersistFile_Release(pf);
    }
    IShellLinkW_Release(sl);
    free(d); free(c); free(t); free(l);
    return ok;
}

static void valor_registro(HKEY k, const char *nombre, const char *valor) {
    wchar_t *n = ancho(nombre), *v = ancho(valor);
    RegSetValueExW(k, n, 0, REG_SZ, (const BYTE *)v, (DWORD)((wcslen(v) + 1) * sizeof(wchar_t)));
    free(n); free(v);
}

static void numero_registro(HKEY k, const char *nombre, DWORD valor) {
    wchar_t *n = ancho(nombre);
    RegSetValueExW(k, n, 0, REG_DWORD, (const BYTE *)&valor, sizeof valor);
    free(n);
}

static void clave_desinstalar(const EscritorioPaquete *p, char *out, size_t max) {
    snprintf(out, max, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Lince-%s", p->id);
}

/* Los accesos del menú Inicio y del escritorio */
static void rutas_accesos(const EscritorioPaquete *p, char *menu, char *escritorio, size_t max) {
    char archivo[300], carpeta[1200];
    nombre_archivo(p->nombre, archivo, sizeof archivo);
    menu[0] = escritorio[0] = '\0';
    if (carpeta_conocida(&FOLDERID_Programs, carpeta, sizeof carpeta)) snprintf(menu, max, "%s\\%s.lnk", carpeta, archivo);
    if (carpeta_conocida(&FOLDERID_Desktop, carpeta, sizeof carpeta)) snprintf(escritorio, max, "%s\\%s.lnk", carpeta, archivo);
}

static int instalar(const EscritorioPaquete *p, int callado) {
    char archivo[300], base[1200], dir[1600], exe[1900], msg[4000];
    nombre_archivo(p->nombre, archivo, sizeof archivo);
    const char *en = getenv("LINCE_INSTALAR_EN");           /* para las pruebas */
    if (en && en[0]) snprintf(dir, sizeof dir, "%s", en);
    else {
        if (!carpeta_conocida(&FOLDERID_UserProgramFiles, base, sizeof base)) {
            if (!carpeta_conocida(&FOLDERID_LocalAppData, base, sizeof base)) {
                avisar(callado, p->nombre, "No se encuentra la carpeta donde instalar.", 1);
                return 1;
            }
            strncat(base, "\\Programs", sizeof base - strlen(base) - 1);
        }
        snprintf(dir, sizeof dir, "%s\\%s", base, archivo);
    }
    snprintf(exe, sizeof exe, "%s\\%s.exe", dir, archivo);
    snprintf(msg, sizeof msg, "¿Instalar «%s»?\n\nSe instala solo para ti, en:\n%s\n\n"
             "Tendrá su acceso en el menú Inicio, y se desinstala desde Configuración → Aplicaciones.",
             p->nombre, dir);
    if (!preguntar(callado, p->nombre, msg)) return 0;
    crear_carpetas(dir);
    /* el juego, sin las DLL (van sueltas) y en modo "instalado" */
    char *meta = escritorio_paquete_texto("instalado", p->nombre, p->id);
    int mal = !meta || empaquetado_copiar_propio(exe, "lince:dll/", "lince:paquete", meta, strlen(meta)) != 0;
    free(meta);
    if (mal) {
        snprintf(msg, sizeof msg, "No se pudo escribir %s.\n\n¿Está abierto el juego? Ciérralo y vuelve a probar.", exe);
        avisar(callado, p->nombre, msg, 1);
        return 1;
    }
    long tam = 0;
    for (int i = 0; i < empaquetado_cantidad(); i++) {
        const char *clave = empaquetado_clave(i);
        if (strncmp(clave, "lince:dll/", 10) != 0) continue;
        size_t largo = 0;
        const char *datos = empaquetado_buscar(clave, &largo);
        char ruta[2000];
        snprintf(ruta, sizeof ruta, "%s\\%s", dir, clave + 10);
        if (!datos || !escribir_archivo(ruta, datos, largo, 0)) {
            snprintf(msg, sizeof msg, "No se pudo escribir %s.", ruta);
            avisar(callado, p->nombre, msg, 1);
            return 1;
        }
        tam += (long)largo;
    }
    struct stat st;
    if (stat(exe, &st) == 0) tam += (long)st.st_size;
    /* los accesos directos */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    char menu[2000], escritorio[2000];
    rutas_accesos(p, menu, escritorio, sizeof menu);
    if (menu[0]) acceso_directo(menu, exe, dir, p->nombre);
    if (escritorio[0] && preguntar(callado, p->nombre, "¿Pongo también un acceso directo en el escritorio?"))
        acceso_directo(escritorio, exe, dir, p->nombre);
    CoUninitialize();
    /* para desinstalarlo desde Configuración → Aplicaciones */
    char clave[400], quitar[2100];
    clave_desinstalar(p, clave, sizeof clave);
    wchar_t *wc = ancho(clave);
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, wc, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS) {
        valor_registro(k, "DisplayName", p->nombre);
        valor_registro(k, "DisplayIcon", exe);
        valor_registro(k, "Publisher", "Hecho con Lince");
        valor_registro(k, "InstallLocation", dir);
        snprintf(quitar, sizeof quitar, "\"%s\" --desinstalar", exe);
        valor_registro(k, "UninstallString", quitar);
        snprintf(quitar, sizeof quitar, "\"%s\" --desinstalar --sin-preguntar", exe);
        valor_registro(k, "QuietUninstallString", quitar);
        numero_registro(k, "NoModify", 1);
        numero_registro(k, "NoRepair", 1);
        numero_registro(k, "EstimatedSize", (DWORD)(tam / 1024));
        RegCloseKey(k);
    }
    free(wc);
    if (callado) { printf("✅ «%s» instalado en %s\n", p->nombre, dir); return 0; }
    snprintf(msg, sizeof msg, "«%s» está instalado.\n\n¿Jugar ahora?", p->nombre);
    if (preguntar(0, p->nombre, msg)) {
        wchar_t *we = ancho(exe), *wd = ancho(dir);
        STARTUPINFOW si;
        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        PROCESS_INFORMATION pi;
        if (CreateProcessW(we, NULL, NULL, NULL, FALSE, 0, NULL, wd, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
        free(we); free(wd);
    }
    return 0;
}

static int desinstalar(const EscritorioPaquete *p, int callado) {
    char yo[2048], dir[2048], msg[3000];
    if (!empaquetado_ruta_ejecutable(yo, sizeof yo)) return 1;
    snprintf(dir, sizeof dir, "%s", yo);
    char *b = strrchr(dir, '\\');
    if (b) *b = '\0';
    snprintf(msg, sizeof msg, "¿Desinstalar «%s»?\n\nSe borra %s.\nTus partidas guardadas no se tocan.", p->nombre, dir);
    if (!preguntar(callado, p->nombre, msg)) return 0;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    char menu[2000], escritorio[2000];
    rutas_accesos(p, menu, escritorio, sizeof menu);
    CoUninitialize();
    if (menu[0]) { wchar_t *w = ancho(menu); DeleteFileW(w); free(w); }
    if (escritorio[0]) { wchar_t *w = ancho(escritorio); DeleteFileW(w); free(w); }
    char clave[400];
    clave_desinstalar(p, clave, sizeof clave);
    wchar_t *wc = ancho(clave);
    RegDeleteTreeW(HKEY_CURRENT_USER, wc);
    RegDeleteKeyW(HKEY_CURRENT_USER, wc);
    free(wc);
    /* la carpeta (con este mismo .exe, que no se puede borrar mientras corre):
       la borra cmd un par de segundos después de que esto acabe */
    char orden[3000];
    snprintf(orden, sizeof orden, "cmd.exe /d /c ping 127.0.0.1 -n 3 > nul & rmdir /s /q \"%s\"", dir);
    wchar_t *wo = ancho(orden);
    STARTUPINFOW si;
        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, wo, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    free(wo);
    snprintf(msg, sizeof msg, "«%s» se ha desinstalado.", p->nombre);
    avisar(callado, p->nombre, msg, 0);
    return 0;
}

int escritorio_arrancar(int argc, char **argv, int *codigo) {
    EscritorioPaquete p;
    if (!escritorio_paquete(&p)) return 0;
    int callado = 0, quitar = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sin-preguntar")) callado = 1;
        if (!strcmp(argv[i], "--desinstalar")) quitar = 1;
    }
    if (!strcmp(p.modo, "instalador")) { *codigo = instalar(&p, callado); return 1; }
    if (!strcmp(p.modo, "instalado") && quitar) { *codigo = desinstalar(&p, callado); return 1; }
    return 0;
}

#else      /* Android, macOS: nada de esto */
const char *escritorio_sdl_del_paquete(void) { return NULL; }
int escritorio_toca_preguntar(void) { return 0; }
void escritorio_responder(int si) { (void)si; }
int escritorio_arrancar(int argc, char **argv, int *codigo) { (void)argc; (void)argv; (void)codigo; return 0; }
#endif
