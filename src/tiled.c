/*
 * 🐆 Lince — tiled.c
 * Lee los mapas de Tiled (https://www.mapeditor.org): .tmj/.json (JSON) y
 * .tmx (XML, lo que Tiled guarda por defecto), con sus tilesets aparte (.tsj o
 * .tsx). Los deja todos con la forma del JSON de Tiled, ya resuelta: los
 * tilesets de fuera metidos en el mapa, las rutas de las imágenes respecto a
 * la carpeta desde la que se ejecuta el juego y los datos de las capas como
 * listas de números (sin base64 ni compresión). juego.h hace el resto.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "interprete.h"
#include "modulos.h"
#include "tiled.h"

/* de stb_image (imagen.c) */
char *stbi_zlib_decode_malloc(const char *buffer, int len, int *outlen);
char *stbi_zlib_decode_noheader_malloc(const char *buffer, int len, int *outlen);

/* ── Rutas ── */

/* 'rel' visto desde la carpeta del archivo 'base', sin "./" ni "x/.." */
void tiled_ruta_junto(const char *base, const char *rel, char *salida, size_t max) {
    char t[2048];
    if (rel[0] == '/' || (rel[0] && rel[1] == ':')) snprintf(t, sizeof t, "%s", rel);
    else {
        const char *barra = NULL;
        for (const char *c = base; *c; c++) if (*c == '/' || *c == '\\') barra = c;
        if (barra) snprintf(t, sizeof t, "%.*s/%s", (int)(barra - base), base, rel);
        else snprintf(t, sizeof t, "%s", rel);
    }
    for (char *c = t; *c; c++) if (*c == '\\') *c = '/';
    /* las partes, quitando "." y deshaciendo ".." */
    char *partes[256];
    int n = 0, absoluta = t[0] == '/';
    for (char *p = strtok(t, "/"); p && n < 256; p = strtok(NULL, "/")) {
        if (strcmp(p, ".") == 0) continue;
        if (strcmp(p, "..") == 0 && n > 0 && strcmp(partes[n - 1], "..") != 0) { n--; continue; }
        partes[n++] = p;
    }
    size_t k = 0;
    salida[0] = '\0';
    if (absoluta && max > 1) salida[k++] = '/';
    for (int i = 0; i < n; i++) {
        int escrito = snprintf(salida + k, max - k, "%s%s", i ? "/" : "", partes[i]);
        if (escrito < 0 || (size_t)escrito >= max - k) break;
        k += (size_t)escrito;
    }
    salida[k < max ? k : max - 1] = '\0';
}

/* ── XML, lo justo para Tiled ──
   Elementos con atributos, texto y comentarios; sin DTD ni espacios de nombres. */
typedef struct Xml {
    char *nombre, *texto;
    char **claves, **valores;
    int natr;
    struct Xml **hijos;
    int nhijos;
} Xml;

static void xml_liberar(Xml *x) {
    if (!x) return;
    free(x->nombre); free(x->texto);
    for (int i = 0; i < x->natr; i++) { free(x->claves[i]); free(x->valores[i]); }
    free(x->claves); free(x->valores);
    for (int i = 0; i < x->nhijos; i++) xml_liberar(x->hijos[i]);
    free(x->hijos);
    free(x);
}

/* Copia [a, b) cambiando &amp; &lt; &gt; &quot; &apos; y &#...; */
static char *xml_texto(const char *a, const char *b) {
    char *r = malloc((size_t)(b - a) + 1), *o = r;
    while (a < b) {
        if (*a == '&') {
            const char *fin = memchr(a, ';', (size_t)(b - a));
            if (fin) {
                size_t l = (size_t)(fin - a - 1);
                unsigned c = 0;
                if (l == 3 && !strncmp(a + 1, "amp", 3)) c = '&';
                else if (l == 2 && !strncmp(a + 1, "lt", 2)) c = '<';
                else if (l == 2 && !strncmp(a + 1, "gt", 2)) c = '>';
                else if (l == 4 && !strncmp(a + 1, "quot", 4)) c = '"';
                else if (l == 4 && !strncmp(a + 1, "apos", 4)) c = '\'';
                else if (a[1] == '#') c = (unsigned)(a[2] == 'x' ? strtoul(a + 3, NULL, 16) : strtoul(a + 2, NULL, 10));
                if (c) {
                    if (c < 0x80) *o++ = (char)c;
                    else if (c < 0x800) { *o++ = (char)(0xC0 | c >> 6); *o++ = (char)(0x80 | (c & 0x3F)); }
                    else { *o++ = (char)(0xE0 | c >> 12); *o++ = (char)(0x80 | ((c >> 6) & 0x3F)); *o++ = (char)(0x80 | (c & 0x3F)); }
                    a = fin + 1;
                    continue;
                }
            }
        }
        *o++ = *a++;
    }
    *o = '\0';
    return r;
}

static void xml_espacios(const char **p) { while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++; }

/* Salta comentarios, <?...?> y <!...>; 1 si saltó algo */
static int xml_saltar(const char **p) {
    if (!strncmp(*p, "<!--", 4)) { const char *f = strstr(*p, "-->"); *p = f ? f + 3 : *p + strlen(*p); return 1; }
    if (!strncmp(*p, "<?", 2) || !strncmp(*p, "<!", 2)) { const char *f = strchr(*p, '>'); *p = f ? f + 1 : *p + strlen(*p); return 1; }
    return 0;
}

static Xml *xml_elemento(const char **p, int hondo) {
    if (**p != '<' || hondo > 64) return NULL;
    (*p)++;
    const char *ini = *p;
    while (**p && !strchr(" \t\r\n/>", **p)) (*p)++;
    Xml *x = calloc(1, sizeof *x);
    x->nombre = xml_texto(ini, *p);
    for (;;) {
        xml_espacios(p);
        if (**p == '/' && (*p)[1] == '>') { *p += 2; return x; }
        if (**p == '>') { (*p)++; break; }
        if (!**p) { xml_liberar(x); return NULL; }
        const char *k0 = *p;
        while (**p && !strchr(" \t\r\n=/>", **p)) (*p)++;
        const char *k1 = *p;
        xml_espacios(p);
        if (**p != '=') { xml_liberar(x); return NULL; }
        (*p)++;
        xml_espacios(p);
        char comilla = **p;
        if (comilla != '"' && comilla != '\'') { xml_liberar(x); return NULL; }
        const char *v0 = ++(*p);
        while (**p && **p != comilla) (*p)++;
        if (!**p) { xml_liberar(x); return NULL; }
        x->claves = realloc(x->claves, sizeof(char *) * (size_t)(x->natr + 1));
        x->valores = realloc(x->valores, sizeof(char *) * (size_t)(x->natr + 1));
        x->claves[x->natr] = xml_texto(k0, k1);
        x->valores[x->natr] = xml_texto(v0, *p);
        x->natr++;
        (*p)++;
    }
    /* contenido: texto e hijos, hasta </nombre> */
    size_t ntexto = 0;
    for (;;) {
        const char *t0 = *p;
        while (**p && **p != '<') (*p)++;
        if (*p > t0) {
            char *t = xml_texto(t0, *p);
            size_t l = strlen(t);
            x->texto = realloc(x->texto, ntexto + l + 1);
            memcpy(x->texto + ntexto, t, l + 1);
            ntexto += l;
            free(t);
        }
        if (!**p) { xml_liberar(x); return NULL; }
        if (!strncmp(*p, "<![CDATA[", 9)) {
            const char *f = strstr(*p, "]]>");
            if (!f) { xml_liberar(x); return NULL; }
            size_t l = (size_t)(f - (*p + 9));
            x->texto = realloc(x->texto, ntexto + l + 1);
            memcpy(x->texto + ntexto, *p + 9, l);
            ntexto += l;
            x->texto[ntexto] = '\0';
            *p = f + 3;
            continue;
        }
        if (xml_saltar(p)) continue;
        if ((*p)[1] == '/') {
            const char *f = strchr(*p, '>');
            *p = f ? f + 1 : *p + strlen(*p);
            return x;
        }
        Xml *h = xml_elemento(p, hondo + 1);
        if (!h) { xml_liberar(x); return NULL; }
        x->hijos = realloc(x->hijos, sizeof(Xml *) * (size_t)(x->nhijos + 1));
        x->hijos[x->nhijos++] = h;
    }
}

static Xml *xml_leer(const char *s) {
    const char *p = s;
    for (;;) {
        xml_espacios(&p);
        if (!xml_saltar(&p)) break;
    }
    return xml_elemento(&p, 0);
}

static const char *xml_atr(const Xml *x, const char *k) {
    for (int i = 0; i < x->natr; i++) if (!strcmp(x->claves[i], k)) return x->valores[i];
    return NULL;
}

/* ── De TMX/TSX a la forma del JSON de Tiled ── */
static int es_numero(const char *v) {
    if (!v || !*v) return 0;
    char *fin;
    strtod(v, &fin);
    return *fin == '\0';
}

/* Los atributos como claves: los que son números, como números */
static void atributos(const Xml *x, Valor *d) {
    for (int i = 0; i < x->natr; i++) {
        const char *k = x->claves[i], *v = x->valores[i];
        if (!strcmp(k, "class")) k = "type";          /* Tiled 1.9+ lo llama class; el JSON, type o class */
        Valor *val = es_numero(v) ? valor_numero(strtod(v, NULL)) : valor_texto(v);
        int ya = valor_diccionario_buscar(d, k);
        if (ya >= 0) { valor_destruir(d->diccionario.valores[ya]); d->diccionario.valores[ya] = val; }
        else valor_diccionario_agregar(d, k, val);
    }
}

static void poner(Valor *d, const char *k, Valor *v) {
    int ya = valor_diccionario_buscar(d, k);
    if (ya >= 0) { valor_destruir(d->diccionario.valores[ya]); d->diccionario.valores[ya] = v; }
    else valor_diccionario_agregar(d, k, v);
}

/* <properties><property name type value/></properties> → [{name, type, value}] */
static Valor *propiedades_xml(const Xml *x) {
    Valor *l = valor_lista_crear();
    for (int i = 0; i < x->nhijos; i++) {
        if (strcmp(x->hijos[i]->nombre, "properties")) continue;
        for (int j = 0; j < x->hijos[i]->nhijos; j++) {
            Xml *p = x->hijos[i]->hijos[j];
            if (strcmp(p->nombre, "property")) continue;
            const char *nombre = xml_atr(p, "name"), *tipo = xml_atr(p, "type"), *valor = xml_atr(p, "value");
            if (!nombre) continue;
            if (!tipo) tipo = "string";
            if (!valor) valor = p->texto ? p->texto : "";
            Valor *d = valor_diccionario_crear();
            valor_diccionario_agregar(d, "name", valor_texto(nombre));
            valor_diccionario_agregar(d, "type", valor_texto(tipo));
            Valor *v;
            if (!strcmp(tipo, "int") || !strcmp(tipo, "float") || !strcmp(tipo, "object")) v = valor_numero(strtod(valor, NULL));
            else if (!strcmp(tipo, "bool")) v = valor_booleano(!strcmp(valor, "true"));
            else v = valor_texto(valor);
            valor_diccionario_agregar(d, "value", v);
            lista_agregar(l, d);
        }
    }
    return l;
}

static Valor *tileset_xml(const Xml *x) {
    Valor *ts = valor_diccionario_crear();
    atributos(x, ts);
    Valor *tiles = valor_lista_crear();
    for (int i = 0; i < x->nhijos; i++) {
        Xml *h = x->hijos[i];
        if (!strcmp(h->nombre, "image")) {
            const char *s = xml_atr(h, "source"), *trans = xml_atr(h, "trans");
            if (s) poner(ts, "image", valor_texto(s));
            if (trans) {                                    /* el color que es transparente */
                char c[16];
                snprintf(c, sizeof c, "#%s", trans[0] == '#' ? trans + 1 : trans);
                poner(ts, "transparentcolor", valor_texto(c));
            }
            if (xml_atr(h, "width")) poner(ts, "imagewidth", valor_numero(atof(xml_atr(h, "width"))));
            if (xml_atr(h, "height")) poner(ts, "imageheight", valor_numero(atof(xml_atr(h, "height"))));
        } else if (!strcmp(h->nombre, "tileoffset")) {
            Valor *o = valor_diccionario_crear();
            atributos(h, o);
            poner(ts, "tileoffset", o);
        } else if (!strcmp(h->nombre, "tile")) {
            Valor *t = valor_diccionario_crear();
            atributos(h, t);
            for (int j = 0; j < h->nhijos; j++) {
                Xml *a = h->hijos[j];
                if (!strcmp(a->nombre, "animation")) {
                    Valor *frames = valor_lista_crear();
                    for (int k = 0; k < a->nhijos; k++) {
                        Valor *f = valor_diccionario_crear();
                        atributos(a->hijos[k], f);
                        lista_agregar(frames, f);
                    }
                    poner(t, "animation", frames);
                } else if (!strcmp(a->nombre, "image")) {
                    const char *s = xml_atr(a, "source");
                    if (s) poner(t, "image", valor_texto(s));
                }
            }
            poner(t, "properties", propiedades_xml(h));
            lista_agregar(tiles, t);
        }
    }
    poner(ts, "tiles", tiles);
    poner(ts, "properties", propiedades_xml(x));
    return ts;
}

static Valor *capas_xml(const Xml *x);

static Valor *objeto_xml(const Xml *o) {
    Valor *d = valor_diccionario_crear();
    atributos(o, d);
    for (int i = 0; i < o->nhijos; i++) {
        Xml *h = o->hijos[i];
        if (!strcmp(h->nombre, "point")) poner(d, "point", valor_booleano(1));
        else if (!strcmp(h->nombre, "ellipse")) poner(d, "ellipse", valor_booleano(1));
        else if (!strcmp(h->nombre, "polygon") || !strcmp(h->nombre, "polyline")) {
            Valor *pts = valor_lista_crear();
            const char *s = xml_atr(h, "points");
            while (s && *s) {
                char *fin;
                double px = strtod(s, &fin);
                if (*fin != ',') break;
                double py = strtod(fin + 1, &fin);
                Valor *p = valor_diccionario_crear();
                valor_diccionario_agregar(p, "x", valor_numero(px));
                valor_diccionario_agregar(p, "y", valor_numero(py));
                lista_agregar(pts, p);
                s = fin;
                while (*s == ' ') s++;
            }
            poner(d, h->nombre, pts);
        } else if (!strcmp(h->nombre, "text")) {
            poner(d, "text", valor_texto(h->texto ? h->texto : ""));
        }
    }
    poner(d, "properties", propiedades_xml(o));
    return d;
}

static Valor *capa_xml(const Xml *h) {
    Valor *c = valor_diccionario_crear();
    atributos(h, c);
    if (!strcmp(h->nombre, "layer")) {
        poner(c, "type", valor_texto("tilelayer"));
        for (int i = 0; i < h->nhijos; i++) {
            Xml *d = h->hijos[i];
            if (strcmp(d->nombre, "data")) continue;
            const char *cod = xml_atr(d, "encoding"), *comp = xml_atr(d, "compression");
            if (d->nhijos > 0 && !cod) {                       /* <tile gid=.../> uno a uno */
                Valor *l = valor_lista_crear();
                for (int k = 0; k < d->nhijos; k++) {
                    const char *g = xml_atr(d->hijos[k], "gid");
                    lista_agregar(l, valor_numero(g ? strtod(g, NULL) : 0));
                }
                poner(c, "data", l);
            } else if (cod && !strcmp(cod, "csv")) {
                Valor *l = valor_lista_crear();
                const char *s = d->texto ? d->texto : "";
                while (*s) {
                    while (*s && (*s < '0' || *s > '9')) s++;
                    if (!*s) break;
                    char *fin;
                    lista_agregar(l, valor_numero((double)strtoul(s, &fin, 10)));
                    s = fin;
                }
                poner(c, "data", l);
            } else {
                poner(c, "data", valor_texto(d->texto ? d->texto : ""));
                if (cod) poner(c, "encoding", valor_texto(cod));
                if (comp) poner(c, "compression", valor_texto(comp));
            }
            for (int k = 0; k < d->nhijos; k++)
                if (!strcmp(d->hijos[k]->nombre, "chunk")) poner(c, "infinite", valor_booleano(1));
        }
    } else if (!strcmp(h->nombre, "objectgroup")) {
        poner(c, "type", valor_texto("objectgroup"));
        Valor *objs = valor_lista_crear();
        for (int i = 0; i < h->nhijos; i++)
            if (!strcmp(h->hijos[i]->nombre, "object")) lista_agregar(objs, objeto_xml(h->hijos[i]));
        poner(c, "objects", objs);
    } else if (!strcmp(h->nombre, "group")) {
        poner(c, "type", valor_texto("group"));
        poner(c, "layers", capas_xml(h));
    } else if (!strcmp(h->nombre, "imagelayer")) {
        poner(c, "type", valor_texto("imagelayer"));
        for (int i = 0; i < h->nhijos; i++)
            if (!strcmp(h->hijos[i]->nombre, "image") && xml_atr(h->hijos[i], "source"))
                poner(c, "image", valor_texto(xml_atr(h->hijos[i], "source")));
    }
    poner(c, "properties", propiedades_xml(h));
    return c;
}

static Valor *capas_xml(const Xml *x) {
    Valor *l = valor_lista_crear();
    for (int i = 0; i < x->nhijos; i++) {
        const char *n = x->hijos[i]->nombre;
        if (!strcmp(n, "layer") || !strcmp(n, "objectgroup") || !strcmp(n, "group") || !strcmp(n, "imagelayer"))
            lista_agregar(l, capa_xml(x->hijos[i]));
    }
    return l;
}

static Valor *mapa_xml(const Xml *x) {
    Valor *m = valor_diccionario_crear();
    atributos(x, m);
    Valor *ts = valor_lista_crear();
    for (int i = 0; i < x->nhijos; i++)
        if (!strcmp(x->hijos[i]->nombre, "tileset")) lista_agregar(ts, tileset_xml(x->hijos[i]));
    poner(m, "tilesets", ts);
    poner(m, "layers", capas_xml(x));
    poner(m, "properties", propiedades_xml(x));
    return m;
}

/* ── Leer un archivo de Tiled (JSON o XML) ── */
static Valor *leer_documento(const char *ruta, int tileset, char *error, size_t max) {
    size_t largo = 0;
    char *bytes = interprete_leer_bytes(ruta, &largo);
    if (!bytes) { snprintf(error, max, "no se encuentra '%s'", ruta); return NULL; }
    char *texto = malloc(largo + 1);
    memcpy(texto, bytes, largo);
    texto[largo] = '\0';
    free(bytes);
    const char *p = texto;
    if (!strncmp(p, "\xEF\xBB\xBF", 3)) p += 3;              /* marca UTF-8 */
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    Valor *v = NULL;
    if (*p == '<') {
        Xml *x = xml_leer(p);
        if (!x) snprintf(error, max, "'%s' no es un XML que se entienda", ruta);
        else if (!tileset && strcmp(x->nombre, "map")) snprintf(error, max, "'%s' no es un mapa de Tiled", ruta);
        else if (tileset && strcmp(x->nombre, "tileset")) snprintf(error, max, "'%s' no es un tileset de Tiled", ruta);
        else v = tileset ? tileset_xml(x) : mapa_xml(x);
        xml_liberar(x);
    } else {
        v = json_a_valor(p);
        if (!v || v->tipo != VAL_DICCIONARIO) {
            if (v) valor_destruir(v);
            v = NULL;
            snprintf(error, max, "'%s' no es un JSON de Tiled que se entienda", ruta);
        }
    }
    free(texto);
    return v;
}

static Valor *campo(Valor *d, const char *k) {
    int i = d && d->tipo == VAL_DICCIONARIO ? valor_diccionario_buscar(d, k) : -1;
    return i >= 0 ? d->diccionario.valores[i] : NULL;
}

static const char *campo_texto(Valor *d, const char *k) {
    Valor *v = campo(d, k);
    return v && v->tipo == VAL_TEXTO ? v->texto : NULL;
}

/* base64 → bytes (malloc) */
static unsigned char *de_base64(const char *s, size_t *n) {
    size_t l = strlen(s);
    unsigned char *r = malloc(l / 4 * 3 + 4);
    uint32_t acum = 0;
    int bits = 0;
    size_t k = 0;
    for (; *s; s++) {
        int v;
        char c = *s;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62;
        else if (c == '/') v = 63;
        else continue;                          /* espacios, saltos y '=' */
        acum = acum << 6 | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; r[k++] = (unsigned char)(acum >> bits); }
    }
    *n = k;
    return r;
}

/* Los datos de una capa como lista de números */
static int datos_capa(Valor *capa, char *error, size_t max) {
    Valor *d = campo(capa, "data");
    if (!d || d->tipo == VAL_LISTA) return 1;
    if (d->tipo != VAL_TEXTO) return 1;
    const char *cod = campo_texto(capa, "encoding"), *comp = campo_texto(capa, "compression");
    if (cod && strcmp(cod, "base64")) { snprintf(error, max, "capa con codificación '%s' (Lince entiende csv y base64)", cod); return 0; }
    size_t n;
    unsigned char *b = de_base64(d->texto, &n);
    if (comp && *comp) {
        int largo = 0;
        char *sin = NULL;
        if (!strcmp(comp, "zlib")) sin = stbi_zlib_decode_malloc((const char *)b, (int)n, &largo);
        else if (!strcmp(comp, "gzip") && n > 18 && b[0] == 0x1f && b[1] == 0x8b) {
            size_t i = 10;                            /* la cabecera de gzip */
            int fl = b[3];
            if (fl & 4 && i + 2 <= n) i += 2 + (size_t)(b[i] | b[i + 1] << 8);
            if (fl & 8) { while (i < n && b[i]) i++; i++; }
            if (fl & 16) { while (i < n && b[i]) i++; i++; }
            if (fl & 2) i += 2;
            if (i < n) sin = stbi_zlib_decode_noheader_malloc((const char *)b + i, (int)(n - i), &largo);
        } else {
            free(b);
            snprintf(error, max, "capa comprimida con '%s': en Tiled, guarda el mapa con capas en CSV, o en base64 sin comprimir, con zlib o con gzip", comp);
            return 0;
        }
        free(b);
        if (!sin) { snprintf(error, max, "no se pudieron descomprimir los datos de una capa"); return 0; }
        b = (unsigned char *)sin;
        n = (size_t)largo;
    }
    Valor *l = valor_lista_crear();
    for (size_t i = 0; i + 4 <= n; i += 4)
        lista_agregar(l, valor_numero((double)((uint32_t)b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16 | (uint32_t)b[i + 3] << 24)));
    free(b);
    poner(capa, "data", l);
    return 1;
}

static int resolver_capas(Valor *capas, const char *ruta, char *error, size_t max) {
    if (!capas || capas->tipo != VAL_LISTA) return 1;
    for (int i = 0; i < capas->lista.cantidad; i++) {
        Valor *c = capas->lista.elementos[i];
        const char *tipo = campo_texto(c, "type");
        if (!tipo) continue;
        if (!strcmp(tipo, "tilelayer")) {
            Valor *inf = campo(c, "infinite");
            if (campo(c, "chunks") || (inf && inf->tipo == VAL_BOOLEANO && inf->booleano)) {
                snprintf(error, max, "'%s' es un mapa infinito: en Tiled, quita «Infinito» en las propiedades del mapa", ruta);
                return 0;
            }
            if (!datos_capa(c, error, max)) return 0;
        } else if (!strcmp(tipo, "group")) {
            if (!resolver_capas(campo(c, "layers"), ruta, error, max)) return 0;
        } else if (!strcmp(tipo, "imagelayer")) {
            const char *im = campo_texto(c, "image");
            if (im && *im) {
                char r[2048];
                tiled_ruta_junto(ruta, im, r, sizeof r);
                poner(c, "image", valor_texto(r));
            }
        }
    }
    return 1;
}

Valor *tiled_leer(const char *ruta, char *error, size_t max) {
    Valor *m = leer_documento(ruta, 0, error, max);
    if (!m) return NULL;
    Valor *inf = campo(m, "infinite");
    if (inf && ((inf->tipo == VAL_BOOLEANO && inf->booleano) || (inf->tipo == VAL_NUMERO && inf->numero != 0))) {
        snprintf(error, max, "'%s' es un mapa infinito: en Tiled, quita «Infinito» en las propiedades del mapa", ruta);
        valor_destruir(m);
        return NULL;
    }
    const char *ori = campo_texto(m, "orientation");
    if (ori && strcmp(ori, "orthogonal")) {
        snprintf(error, max, "'%s' es un mapa «%s»: Lince entiende los ortogonales (casillas en rejilla)", ruta, ori);
        valor_destruir(m);
        return NULL;
    }
    /* los tilesets: los de fuera, dentro; las imágenes, con su ruta desde aquí */
    Valor *ts = campo(m, "tilesets");
    for (int i = 0; ts && ts->tipo == VAL_LISTA && i < ts->lista.cantidad; i++) {
        Valor *t = ts->lista.elementos[i];
        const char *fuente = campo_texto(t, "source");
        char ruta_ts[2048];
        snprintf(ruta_ts, sizeof ruta_ts, "%s", ruta);
        if (fuente) {
            tiled_ruta_junto(ruta, fuente, ruta_ts, sizeof ruta_ts);
            Valor *ext = leer_documento(ruta_ts, 1, error, max);
            if (!ext) { valor_destruir(m); return NULL; }
            for (int k = 0; k < ext->diccionario.cantidad; k++)
                if (strcmp(ext->diccionario.claves[k], "firstgid"))
                    poner(t, ext->diccionario.claves[k], ext->diccionario.valores[k]);
            /* los valores pasan a 't': se vacía 'ext' sin destruirlos */
            for (int k = 0; k < ext->diccionario.cantidad; k++)
                if (strcmp(ext->diccionario.claves[k], "firstgid")) ext->diccionario.valores[k] = valor_nulo();
            valor_destruir(ext);
        }
        const char *im = campo_texto(t, "image");
        if (im) {
            char r[2048];
            tiled_ruta_junto(ruta_ts, im, r, sizeof r);
            poner(t, "image", valor_texto(r));
        }
        Valor *tiles = campo(t, "tiles");
        for (int k = 0; tiles && tiles->tipo == VAL_LISTA && k < tiles->lista.cantidad; k++) {
            const char *ti = campo_texto(tiles->lista.elementos[k], "image");
            if (ti) {
                char r[2048];
                tiled_ruta_junto(ruta_ts, ti, r, sizeof r);
                poner(tiles->lista.elementos[k], "image", valor_texto(r));
            }
        }
    }
    if (!resolver_capas(campo(m, "layers"), ruta, error, max)) { valor_destruir(m); return NULL; }
    return m;
}

/* Los archivos que necesita un mapa (sus tilesets y sus imágenes), para que
   'lince compilar' los meta dentro: llama a 'anadir' con cada ruta */
void tiled_archivos(const char *ruta, void (*anadir)(const char *ruta)) {
    char error[300];
    Valor *m = tiled_leer(ruta, error, sizeof error);
    if (!m) return;
    Valor *ts = campo(m, "tilesets");
    for (int i = 0; ts && ts->tipo == VAL_LISTA && i < ts->lista.cantidad; i++) {
        Valor *t = ts->lista.elementos[i];
        const char *im = campo_texto(t, "image");
        if (im) anadir(im);
        Valor *tiles = campo(t, "tiles");
        for (int k = 0; tiles && tiles->tipo == VAL_LISTA && k < tiles->lista.cantidad; k++) {
            const char *ti = campo_texto(tiles->lista.elementos[k], "image");
            if (ti) anadir(ti);
        }
    }
    valor_destruir(m);
    /* los tilesets de fuera: se vuelven a mirar en el documento tal cual */
    size_t largo = 0;
    char *b = interprete_leer_bytes(ruta, &largo);
    if (!b) return;
    char *t = malloc(largo + 1);
    memcpy(t, b, largo);
    t[largo] = '\0';
    free(b);
    for (const char *p = t; (p = strstr(p, "source")); p += 6) {
        const char *q = p + 6;
        while (*q == '"' || *q == ' ' || *q == ':' || *q == '=') q++;
        if (*q == '\'') q++;
        const char *fin = q;
        while (*fin && *fin != '"' && *fin != '\'') fin++;
        size_t l = (size_t)(fin - q);
        if (l > 4 && l < 1000 && (!strncmp(fin - 4, ".tsx", 4) || !strncmp(fin - 4, ".tsj", 4) || !strncmp(fin - 5, ".json", 5))) {
            char rel[1024], r[2048];
            snprintf(rel, sizeof rel, "%.*s", (int)l, q);
            tiled_ruta_junto(ruta, rel, r, sizeof r);
            anadir(r);
        }
    }
    free(t);
}
