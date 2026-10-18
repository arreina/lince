/*
 * 🐆 Lince — apk.c
 * 'lince repartir --android juego.lince': el .apk de un juego sin Java ni el
 * SDK de Android. Parte de una plantilla (un .apk de Lince sin juego, hecho
 * con 'android/construir.sh --plantilla'), le pone el nombre del paquete y de
 * la app en el manifiesto (que está en el XML binario de Android), le mete el
 * juego empaquetado en assets/juego.paq y lo firma (firmar.c) con una llave
 * que se crea la primera vez en ~/.lince/android-llave.txt.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif
#include "apk.h"
#include "firmar.h"
#include "empaquetado.h"

/* de stb_image (imagen.c): descomprime deflate sin cabecera */
char *stbi_zlib_decode_noheader_malloc(const char *buffer, int len, int *outlen);

#define PLANTILLA_PAQUETE "org.lince.plantilla"
#define PLANTILLA_NOMBRE  "LINCE_NOMBRE"

static uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void pon16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void pon32(unsigned char *p, uint32_t v) { pon16(p, v & 0xFFFF); pon16(p + 2, v >> 16); }

static uint32_t crc32_de(const unsigned char *p, size_t n) {
    static uint32_t tabla[256];
    if (!tabla[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tabla[i] = c;
        }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = tabla[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static unsigned char *leer_todo(const char *ruta, size_t *n) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long largo = ftell(f);
    rewind(f);
    unsigned char *p = largo > 0 ? malloc((size_t)largo) : NULL;
    if (p && fread(p, 1, (size_t)largo, f) != (size_t)largo) { free(p); p = NULL; }
    fclose(f);
    *n = p ? (size_t)largo : 0;
    return p;
}

/* ── El XML binario de Android ──
   Un manifiesto compilado empieza por el bloque de textos (string pool): se
   rehace con los textos cambiados, y como el resto del archivo los nombra por
   su número, no hay que tocar nada más. */
typedef struct { char **t; int n; } Textos;

/* UTF-16 → UTF-8 (solo lo necesario: el plano básico) */
static char *de_utf16(const unsigned char *p, size_t n) {
    char *s = malloc(n * 3 + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned c = le16(p + 2 * i);
        if (c < 0x80) s[k++] = (char)c;
        else if (c < 0x800) { s[k++] = (char)(0xC0 | c >> 6); s[k++] = (char)(0x80 | (c & 0x3F)); }
        else { s[k++] = (char)(0xE0 | c >> 12); s[k++] = (char)(0x80 | ((c >> 6) & 0x3F)); s[k++] = (char)(0x80 | (c & 0x3F)); }
    }
    s[k] = '\0';
    return s;
}

/* UTF-8 → UTF-16; devuelve cuántas unidades */
static size_t a_utf16(const char *s, uint16_t *salida) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned c;
        if (*p < 0x80) c = *p++;
        else if ((*p & 0xE0) == 0xC0 && p[1]) { c = (unsigned)(*p & 0x1F) << 6 | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { c = (unsigned)(*p & 0x0F) << 12 | (unsigned)(p[1] & 0x3F) << 6 | (p[2] & 0x3F); p += 3; }
        else { c = '?'; p++; while ((*p & 0xC0) == 0x80) p++; }
        if (salida) salida[n] = (uint16_t)c;
        n++;
    }
    return n;
}

static size_t utf8_unidades16(const char *s) { return a_utf16(s, NULL); }

/* Cambia los textos 'de' por 'a' en el XML binario; NULL si no lo entiende */
static unsigned char *axml_cambiar(const unsigned char *x, size_t n, const char *const *de, const char *const *a, int cambios, size_t *n_salida) {
    if (n < 8 + 28 || le16(x) != 0x0003 || le16(x + 8) != 0x0001) return NULL;
    const unsigned char *sp = x + 8;
    uint32_t sp_tam = le32(sp + 4), num = le32(sp + 8), estilos = le32(sp + 12), flags = le32(sp + 16), inicio = le32(sp + 20);
    if (estilos != 0 || 8 + sp_tam > n) return NULL;
    int utf8 = (flags & 0x100) != 0;
    Textos tx = {calloc(num, sizeof(char *)), (int)num};
    for (uint32_t i = 0; i < num; i++) {
        const unsigned char *s = sp + inicio + le32(sp + 28 + 4 * i);
        if (utf8) {
            size_t k = 0, largo;
            k += (s[k] & 0x80) ? 2 : 1;                       /* largo en UTF-16 */
            largo = (s[k] & 0x80) ? ((size_t)(s[k] & 0x7F) << 8 | s[k + 1]) : s[k];
            k += (s[k] & 0x80) ? 2 : 1;
            tx.t[i] = malloc(largo + 1);
            memcpy(tx.t[i], s + k, largo);
            tx.t[i][largo] = '\0';
        } else {
            size_t largo = le16(s), k = 2;
            if (largo & 0x8000) { largo = (largo & 0x7FFF) << 16 | le16(s + 2); k = 4; }
            tx.t[i] = de_utf16(s + k, largo);
        }
        for (int c = 0; c < cambios; c++)
            if (strcmp(tx.t[i], de[c]) == 0) { free(tx.t[i]); tx.t[i] = strdup(a[c]); }
    }
    /* el bloque nuevo: cabecera, posiciones y textos */
    size_t datos = 0;
    for (int i = 0; i < tx.n; i++) {
        if (utf8) { size_t l = strlen(tx.t[i]), u = utf8_unidades16(tx.t[i]); datos += (u > 127 ? 2 : 1) + (l > 127 ? 2 : 1) + l + 1; }
        else { size_t u = utf8_unidades16(tx.t[i]); datos += (u > 32767 ? 4 : 2) + 2 * u + 2; }
    }
    size_t datos_alineados = (datos + 3) & ~(size_t)3;
    size_t nuevo_sp = 28 + 4 * (size_t)tx.n + datos_alineados;
    size_t total = n - sp_tam + nuevo_sp;
    unsigned char *r = calloc(1, total);
    memcpy(r, x, 8);
    pon32(r + 4, (uint32_t)total);
    unsigned char *q = r + 8;
    memcpy(q, sp, 28);
    pon32(q + 4, (uint32_t)nuevo_sp);
    pon32(q + 20, (uint32_t)(28 + 4 * tx.n));
    size_t pos = 0;
    unsigned char *base = q + 28 + 4 * tx.n;
    for (int i = 0; i < tx.n; i++) {
        pon32(q + 28 + 4 * i, (uint32_t)pos);
        unsigned char *s = base + pos;
        if (utf8) {
            size_t l = strlen(tx.t[i]), u = utf8_unidades16(tx.t[i]), k = 0;
            if (u > 127) { s[k++] = (unsigned char)(0x80 | (u >> 8)); s[k++] = (unsigned char)u; } else s[k++] = (unsigned char)u;
            if (l > 127) { s[k++] = (unsigned char)(0x80 | (l >> 8)); s[k++] = (unsigned char)l; } else s[k++] = (unsigned char)l;
            memcpy(s + k, tx.t[i], l);
            s[k + l] = 0;
            pos += k + l + 1;
        } else {
            size_t u = utf8_unidades16(tx.t[i]), k = 0;
            uint16_t *u16 = malloc(sizeof(uint16_t) * (u + 1));
            a_utf16(tx.t[i], u16);
            if (u > 32767) { pon16(s, 0x8000 | (unsigned)(u >> 16)); pon16(s + 2, (unsigned)(u & 0xFFFF)); k = 4; } else { pon16(s, (unsigned)u); k = 2; }
            for (size_t j = 0; j < u; j++) pon16(s + k + 2 * j, u16[j]);
            pon16(s + k + 2 * u, 0);
            pos += k + 2 * u + 2;
            free(u16);
        }
        free(tx.t[i]);
    }
    free(tx.t);
    memcpy(r + 8 + nuevo_sp, x + 8 + sp_tam, n - 8 - sp_tam);
    *n_salida = total;
    return r;
}

/* resources.arsc: el nombre del paquete (128 caracteres UTF-16 fijos) */
static void arsc_cambiar_paquete(unsigned char *p, size_t n, const char *paquete) {
    if (n < 12 || le16(p) != 0x0002) return;
    size_t pos = le16(p + 2);                              /* tras la cabecera de la tabla */
    while (pos + 8 <= n) {
        unsigned tipo = le16(p + pos);
        uint32_t tam = le32(p + pos + 4);
        if (tipo == 0x0200 && pos + 12 + 256 <= n) {       /* RES_TABLE_PACKAGE_TYPE */
            uint16_t u16[128];
            memset(u16, 0, sizeof u16);
            size_t u = a_utf16(paquete, NULL);
            if (u < 128) a_utf16(paquete, u16);
            for (int i = 0; i < 128; i++) pon16(p + pos + 12 + 2 * i, u16[i]);
            return;
        }
        if (tam == 0) return;
        pos += tam;
    }
}

/* ── Zip ── */
typedef struct {
    char nombre[512];
    int metodo;                     /* 0 guardado, 8 comprimido */
    uint32_t crc, comprimido, original;
    const unsigned char *datos;     /* los bytes tal como van en el zip */
    unsigned char *propio;          /* si son nuestros (para liberarlos) */
} Entrada;

static int leer_zip(const unsigned char *z, size_t n, Entrada **salida, int *num) {
    size_t eocd = (size_t)-1;
    for (size_t i = n - 22; ; i--) {
        if (le32(z + i) == 0x06054b50) { eocd = i; break; }
        if (i == 0 || n - i > 22 + 65535) break;
    }
    if (eocd == (size_t)-1) return 0;
    int total = le16(z + eocd + 10);
    size_t cd = le32(z + eocd + 16);
    Entrada *e = calloc((size_t)total, sizeof(Entrada));
    for (int i = 0; i < total; i++) {
        if (cd + 46 > n || le32(z + cd) != 0x02014b50) { free(e); return 0; }
        size_t nl = le16(z + cd + 28), xl = le16(z + cd + 30), cl = le16(z + cd + 32);
        e[i].metodo = le16(z + cd + 10);
        e[i].crc = le32(z + cd + 16);
        e[i].comprimido = le32(z + cd + 20);
        e[i].original = le32(z + cd + 24);
        size_t local = le32(z + cd + 42);
        snprintf(e[i].nombre, sizeof e[i].nombre, "%.*s", (int)nl, (const char *)z + cd + 46);
        if (local + 30 > n) { free(e); return 0; }
        e[i].datos = z + local + 30 + le16(z + local + 26) + le16(z + local + 28);
        cd += 46 + nl + xl + cl;
    }
    *salida = e;
    *num = total;
    return 1;
}

/* Lo de una entrada sin comprimir (malloc), o NULL */
static unsigned char *descomprimir(const Entrada *e, size_t *n) {
    if (e->metodo == 0) {
        unsigned char *p = malloc(e->original + 1);
        memcpy(p, e->datos, e->original);
        *n = e->original;
        return p;
    }
    int largo = 0;
    char *p = stbi_zlib_decode_noheader_malloc((const char *)e->datos, (int)e->comprimido, &largo);
    *n = (size_t)largo;
    return (unsigned char *)p;
}

static void guardar_sin_comprimir(Entrada *e, unsigned char *p, size_t n) {
    free(e->propio);
    e->propio = p;
    e->datos = p;
    e->metodo = 0;
    e->comprimido = e->original = (uint32_t)n;
    e->crc = crc32_de(p, n);
}

typedef struct { unsigned char *p; size_t n, cap; } Buf;
static void mas(Buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, p, n);
    b->n += n;
}

/* El zip, con los datos sin comprimir alineados a 4 bytes (lo que hace
   zipalign: Android lee así resources.arsc sin copiarlo) */
static unsigned char *escribir_zip(Entrada *e, int num, size_t *n) {
    Buf z = {0}, cd = {0};
    for (int i = 0; i < num; i++) {
        size_t nl = strlen(e[i].nombre), local = z.n;
        size_t relleno = 0;
        if (e[i].metodo == 0) relleno = (4 - (local + 30 + nl) % 4) % 4;
        unsigned char h[30] = {0};
        pon32(h, 0x04034b50); pon16(h + 4, 20); pon16(h + 8, (unsigned)e[i].metodo);
        pon16(h + 10, 0); pon16(h + 12, 0x21);              /* 1 de enero de 1980 */
        pon32(h + 14, e[i].crc); pon32(h + 18, e[i].comprimido); pon32(h + 22, e[i].original);
        pon16(h + 26, (unsigned)nl); pon16(h + 28, (unsigned)relleno);
        mas(&z, h, 30);
        mas(&z, e[i].nombre, nl);
        static const unsigned char ceros[4] = {0};
        mas(&z, ceros, relleno);
        mas(&z, e[i].datos, e[i].comprimido);
        unsigned char c[46] = {0};
        pon32(c, 0x02014b50); pon16(c + 4, 20); pon16(c + 6, 20); pon16(c + 10, (unsigned)e[i].metodo);
        pon16(c + 14, 0x21);
        pon32(c + 16, e[i].crc); pon32(c + 20, e[i].comprimido); pon32(c + 24, e[i].original);
        pon16(c + 28, (unsigned)nl); pon32(c + 42, (uint32_t)local);
        mas(&cd, c, 46);
        mas(&cd, e[i].nombre, nl);
    }
    size_t inicio_cd = z.n;
    mas(&z, cd.p, cd.n);
    unsigned char fin[22] = {0};
    pon32(fin, 0x06054b50); pon16(fin + 8, (unsigned)num); pon16(fin + 10, (unsigned)num);
    pon32(fin + 12, (uint32_t)cd.n); pon32(fin + 16, (uint32_t)inicio_cd);
    mas(&z, fin, 22);
    free(cd.p);
    *n = z.n;
    return z.p;
}

/* ── Dónde están la plantilla y la llave ── */
static const char *carpeta_usuario(void) {
#ifdef _WIN32
    const char *h = getenv("USERPROFILE");
    if (!h || !h[0]) h = getenv("HOME");
#else
    const char *h = getenv("HOME");
#endif
    return h && h[0] ? h : ".";
}

static int buscar_plantilla(char *ruta, size_t max) {
    const char *e = getenv("LINCE_ANDROID_PLANTILLA");
    struct stat st;
    if (e && stat(e, &st) == 0) { snprintf(ruta, max, "%s", e); return 1; }
    char yo[1024];
    if (empaquetado_ruta_ejecutable(yo, sizeof yo)) {
        char *barra = strrchr(yo, '/');
#ifdef _WIN32
        char *contra = strrchr(yo, '\\');
        if (!barra || (contra && contra > barra)) barra = contra;
#endif
        if (barra) {
            barra[1] = '\0';
            snprintf(ruta, max, "%slince-android.apk", yo);
            if (stat(ruta, &st) == 0) return 1;
        }
    }
    snprintf(ruta, max, "%s/.lince/lince-android.apk", carpeta_usuario());
    return stat(ruta, &st) == 0;
}

int apk_crear(const char *juego, const char *salida, const char *nombre, const char *id,
              const char *const *extras, int num_extras) {
    char plantilla[1200], ruta_llave[1200], error[400], paq[1200];
    if (!buscar_plantilla(plantilla, sizeof plantilla)) {
        fprintf(stderr, "\n❌ No encuentro la plantilla de Android (lince-android.apk).\n"
                        "   Ponla junto al ejecutable de Lince o en %s/.lince/, o di dónde está con\n"
                        "   LINCE_ANDROID_PLANTILLA. Viene con cada versión de Lince, o se hace con\n"
                        "   android/construir.sh --plantilla (ver android/LEEME.md).\n\n", carpeta_usuario());
        return 1;
    }
    /* el juego, empaquetado (como 'lince compilar --datos') */
    snprintf(paq, sizeof paq, "%s.paq.tmp", salida);
    if (empaquetado_crear_datos(juego, paq, extras, num_extras) != 0) return 1;
    size_t n_paq;
    unsigned char *datos_paq = leer_todo(paq, &n_paq);
    remove(paq);
    if (!datos_paq) { fprintf(stderr, "\n❌ No se pudo empaquetar '%s'.\n\n", juego); return 1; }

    size_t n_pl;
    unsigned char *pl = leer_todo(plantilla, &n_pl);
    Entrada *e = NULL;
    int num = 0;
    if (!pl || !leer_zip(pl, n_pl, &e, &num)) {
        fprintf(stderr, "\n❌ '%s' no es una plantilla de Android de Lince.\n\n", plantilla);
        free(pl); free(datos_paq);
        return 1;
    }
    /* lo que va en el .apk: todo menos la firma vieja, con el manifiesto y
       los recursos cambiados, y el juego */
    Entrada *nuevas = calloc((size_t)num + 1, sizeof(Entrada));
    int k = 0, hay_manifiesto = 0;
    const char *de[2] = {PLANTILLA_PAQUETE, PLANTILLA_NOMBRE}, *a[2] = {id, nombre};
    for (int i = 0; i < num; i++) {
        if (strncmp(e[i].nombre, "META-INF/", 9) == 0 || strcmp(e[i].nombre, "assets/juego.paq") == 0) continue;
        nuevas[k] = e[i];
        if (strcmp(e[i].nombre, "AndroidManifest.xml") == 0) {
            size_t n, n2;
            unsigned char *xml = descomprimir(&e[i], &n);
            unsigned char *cambiado = xml ? axml_cambiar(xml, n, de, a, 2, &n2) : NULL;
            free(xml);
            if (!cambiado) break;
            guardar_sin_comprimir(&nuevas[k], cambiado, n2);
            hay_manifiesto = 1;
        } else if (strcmp(e[i].nombre, "resources.arsc") == 0) {
            size_t n;
            unsigned char *r = descomprimir(&e[i], &n);
            if (r) { arsc_cambiar_paquete(r, n, id); guardar_sin_comprimir(&nuevas[k], r, n); }
        }
        k++;
    }
    if (!hay_manifiesto) {
        fprintf(stderr, "\n❌ No se pudo cambiar el manifiesto de la plantilla '%s'.\n\n", plantilla);
        for (int i = 0; i < k; i++) free(nuevas[i].propio);
        free(nuevas); free(e); free(pl); free(datos_paq);
        return 1;
    }
    snprintf(nuevas[k].nombre, sizeof nuevas[k].nombre, "assets/juego.paq");
    guardar_sin_comprimir(&nuevas[k], datos_paq, n_paq);
    k++;

    size_t n_zip, n_apk;
    unsigned char *zip = escribir_zip(nuevas, k, &n_zip);
    for (int i = 0; i < k; i++) free(nuevas[i].propio);
    free(nuevas); free(e); free(pl);

    /* la firma */
    snprintf(ruta_llave, sizeof ruta_llave, "%s/.lince", carpeta_usuario());
    mkdir(ruta_llave, 0700);
    snprintf(ruta_llave, sizeof ruta_llave, "%s/.lince/android-llave.txt", carpeta_usuario());
    LlaveRsa *ll = llave_cargar_o_crear(ruta_llave, error, sizeof error);
    if (!ll) { fprintf(stderr, "\n❌ %s\n\n", error); free(zip); return 1; }
    unsigned char *apk = apk_firmar_v2(zip, n_zip, ll, &n_apk);
    llave_liberar(ll);
    free(zip);
    if (!apk) { fprintf(stderr, "\n❌ No se pudo firmar el .apk.\n\n"); return 1; }
    FILE *f = fopen(salida, "wb");
    if (!f || fwrite(apk, 1, n_apk, f) != n_apk) {
        if (f) fclose(f);
        free(apk);
        fprintf(stderr, "\n❌ No se pudo escribir '%s'.\n\n", salida);
        return 1;
    }
    fclose(f);
    free(apk);
    printf("✅ %s (%.1f MB) — %s, %s\n", salida, n_apk / 1048576.0, nombre, id);
    printf("   Firmado con tu llave: %s (guárdala para poder actualizar el juego)\n", ruta_llave);
    return 0;
}
