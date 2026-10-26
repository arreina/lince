/*
 * 🐆 Lince — zstd.c
 * Descomprimir Zstandard (RFC 8878), lo justo para las capas de Tiled
 * guardadas con «zstd». Lee todos los tipos de bloque (sin comprimir,
 * repetidos y comprimidos, con Huffman y FSE); no usa diccionarios ni mira
 * la suma de comprobación. Lento pero corto: va bit a bit, y una capa de
 * Tiled son unos pocos kilobytes.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "zstd.h"

/* ── Salida que crece ── */
typedef struct { unsigned char *d; size_t n, cap; int mal; } Sal;

static void sal_byte(Sal *s, unsigned char b) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 4096;
        s->d = realloc(s->d, s->cap);
    }
    s->d[s->n++] = b;
}

/* ── Lectura de bits hacia atrás (los flujos de Huffman y FSE) ── */
typedef struct { const unsigned char *b; long pos; } Atras;    /* pos: bits que quedan */

static int atras_empezar(Atras *r, const unsigned char *b, size_t n) {
    if (n == 0 || b[n - 1] == 0) return 0;
    int alto = 7;
    while (!(b[n - 1] >> alto & 1)) alto--;
    r->b = b;
    r->pos = (long)(n - 1) * 8 + alto;      /* sin el 1 que marca el final */
    return 1;
}

static uint32_t atras_leer(Atras *r, int n) {
    uint32_t v = 0;
    r->pos -= n;
    for (int i = 0; i < n; i++) {
        long q = r->pos + i;
        if (q >= 0) v |= (uint32_t)(r->b[q >> 3] >> (q & 7) & 1) << i;
    }
    return v;
}

/* ── FSE ── */
typedef struct { uint8_t sim, bits; uint16_t base; } FseEntrada;
typedef struct { FseEntrada t[1 << 9]; int log; } Fse;

static int alto_bit(uint32_t v) { int b = -1; while (v) { b++; v >>= 1; } return b; }

static int fse_construir(Fse *f, const int16_t *prob, int nsim, int log) {
    int tam = 1 << log, alto = tam - 1;
    uint16_t sig[256];
    f->log = log;
    for (int s = 0; s < nsim; s++) {
        if (prob[s] == -1) { f->t[alto--].sim = (uint8_t)s; sig[s] = 1; }
        else sig[s] = (uint16_t)(prob[s] > 0 ? prob[s] : 0);
    }
    int paso = (tam >> 1) + (tam >> 3) + 3, pos = 0;
    for (int s = 0; s < nsim; s++)
        for (int i = 0; i < prob[s]; i++) {
            f->t[pos].sim = (uint8_t)s;
            do pos = (pos + paso) & (tam - 1); while (pos > alto);
        }
    if (pos != 0) return 0;
    for (int u = 0; u < tam; u++) {
        int s = f->t[u].sim;
        uint32_t estado = sig[s]++;
        int bits = log - alto_bit(estado);
        f->t[u].bits = (uint8_t)bits;
        f->t[u].base = (uint16_t)((estado << bits) - (uint32_t)tam);
    }
    return 1;
}

/* k bits de b desde el bit 'bit', hacia delante (lo que pasa del final, a 0) */
static uint32_t adelante(const unsigned char *b, size_t n, size_t bit, int k) {
    uint32_t v = 0;
    for (int i = 0; i < k; i++) {
        size_t q = bit + (size_t)i;
        if (q / 8 < n) v |= (uint32_t)(b[q / 8] >> (q % 8) & 1) << i;
    }
    return v;
}

/* Lee una tabla FSE del principio de b (hacia delante); devuelve los bytes leídos, o 0 */
static size_t fse_leer_tabla(Fse *f, const unsigned char *b, size_t n, int max_log, int max_sim) {
    int16_t prob[256];
    memset(prob, 0, sizeof prob);
    size_t bit = 0;
#define PEEK(k) adelante(b, n, bit, (k))
    int log = (int)PEEK(4) + 5;
    bit += 4;
    if (log > max_log) return 0;
    int restante = (1 << log) + 1, umbral = 1 << log, nbits = log + 1, s = 0, cero_antes = 0;
    while (restante > 1 && s <= max_sim) {
        if (cero_antes) {
            int rep;
            do { rep = (int)PEEK(2); bit += 2; s += rep; } while (rep == 3);
            cero_antes = 0;
            if (s > max_sim) break;
            continue;
        }
        int max = 2 * umbral - 1 - restante, cuenta;
        uint32_t bajo = PEEK(nbits - 1);
        if ((int)bajo < max) { cuenta = (int)bajo; bit += (size_t)(nbits - 1); }
        else {
            cuenta = (int)PEEK(nbits);
            if (cuenta >= umbral) cuenta -= max;
            bit += (size_t)nbits;
        }
        cuenta--;
        restante -= cuenta < 0 ? -cuenta : cuenta;
        prob[s++] = (int16_t)cuenta;
        if (cuenta == 0) cero_antes = 1;
        while (restante < umbral) { nbits--; umbral >>= 1; }
    }
#undef PEEK
    if (restante != 1 || s > max_sim + 1) return 0;
    size_t usados = (bit + 7) / 8;
    if (usados > n || !fse_construir(f, prob, s, log)) return 0;
    return usados;
}

static void fse_rle(Fse *f, int sim) {
    f->log = 0;
    f->t[0].sim = (uint8_t)sim;
    f->t[0].bits = 0;
    f->t[0].base = 0;
}

/* ── Huffman ── */
typedef struct { uint8_t sim[1 << 11], bits[1 << 11]; int max; int hay; } Huff;

static int huff_de_pesos(Huff *h, const uint8_t *peso, int n) {
    uint32_t total = 0;
    for (int i = 0; i < n; i++) {
        if (peso[i] > 11) return 0;
        if (peso[i]) total += 1u << (peso[i] - 1);
    }
    if (total == 0) return 0;
    int max = alto_bit(total) + 1;
    uint32_t falta = (1u << max) - total;
    if (falta & (falta - 1)) return 0;               /* tiene que ser una potencia de 2 */
    uint8_t w[256];
    memcpy(w, peso, (size_t)n);
    w[n] = (uint8_t)(alto_bit(falta) + 1);           /* el peso del último símbolo, implícito */
    n++;
    if (max > 11) return 0;
    h->max = max;
    int pos = 0;
    for (int p = 1; p <= max; p++)
        for (int s = 0; s < n; s++) {
            if (w[s] != p) continue;
            int cuantos = 1 << (p - 1);
            for (int k = 0; k < cuantos; k++) {
                if (pos >= (1 << max)) return 0;
                h->sim[pos] = (uint8_t)s;
                h->bits[pos] = (uint8_t)(max + 1 - p);
                pos++;
            }
        }
    if (pos != 1 << max) return 0;
    h->hay = 1;
    return 1;
}

/* La descripción del árbol; devuelve los bytes leídos, o 0 */
static size_t huff_leer(Huff *h, const unsigned char *b, size_t n) {
    if (n < 1) return 0;
    uint8_t peso[256];
    int np = 0;
    int cab = b[0];
    if (cab >= 128) {                                 /* pesos de 4 bits, tal cual */
        np = cab - 127;
        size_t bytes = (size_t)(np + 1) / 2;
        if (1 + bytes > n) return 0;
        for (int i = 0; i < np; i++) peso[i] = (uint8_t)(i % 2 == 0 ? b[1 + i / 2] >> 4 : b[1 + i / 2] & 15);
        return huff_de_pesos(h, peso, np) ? 1 + bytes : 0;
    }
    /* pesos comprimidos con FSE: dos estados que se turnan */
    if ((size_t)cab + 1 > n || cab == 0) return 0;
    Fse f;
    size_t t = fse_leer_tabla(&f, b + 1, (size_t)cab, 6, 255);
    if (!t) return 0;
    Atras r;
    if (!atras_empezar(&r, b + 1 + t, (size_t)cab - t)) return 0;
    uint32_t e1 = atras_leer(&r, f.log), e2 = atras_leer(&r, f.log);
    for (;;) {
        if (np >= 255) return 0;
        peso[np++] = f.t[e1].sim;
        e1 = f.t[e1].base + atras_leer(&r, f.t[e1].bits);
        if (r.pos < 0) { peso[np++] = f.t[e2].sim; break; }
        if (np >= 255) return 0;
        peso[np++] = f.t[e2].sim;
        e2 = f.t[e2].base + atras_leer(&r, f.t[e2].bits);
        if (r.pos < 0) { if (np >= 255) return 0; peso[np++] = f.t[e1].sim; break; }
    }
    return huff_de_pesos(h, peso, np) ? 1 + (size_t)cab : 0;
}

static int huff_flujo(const Huff *h, const unsigned char *b, size_t n, unsigned char *sal, size_t cuantos) {
    Atras r;
    if (!atras_empezar(&r, b, n)) return 0;
    for (size_t i = 0; i < cuantos; i++) {
        uint32_t v = atras_leer(&r, h->max);
        int k = (int)v;
        sal[i] = h->sim[k];
        r.pos += h->max - h->bits[k];                 /* los que no usaba, de vuelta */
    }
    return r.pos == 0;
}

/* ── Las tablas de las secuencias ── */
static const int16_t LL_DEF[36] = {4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
                                   2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1};
static const int16_t ML_DEF[53] = {1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1};
static const int16_t OF_DEF[29] = {1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                   -1, -1, -1, -1, -1};
static const uint32_t LL_BASE[36] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28, 32,
                                     40, 48, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
static const uint8_t LL_BITS[36] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 6, 7, 8,
                                    9, 10, 11, 12, 13, 14, 15, 16};
static const uint32_t ML_BASE[53] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                     26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 39, 41, 43, 47, 51, 59, 67, 83, 99,
                                     131, 259, 515, 1027, 2051, 4099, 8195, 16387, 32771, 65539};
static const uint8_t ML_BITS[53] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

typedef struct {
    Huff huff;
    Fse ll, of, ml;
    int hay_ll, hay_of, hay_ml;
    uint32_t rep[3];
} Estado;

/* El modo de una tabla (predefinida, RLE, FSE o la de antes); devuelve bytes leídos, o -1 */
static long tabla_seq(Fse *f, int *hay, int modo, const unsigned char *b, size_t n,
                      const int16_t *def, int ndef, int log_def, int max_log, int max_sim) {
    switch (modo) {
        case 0: fse_construir(f, def, ndef, log_def); *hay = 1; return 0;
        case 1:
            if (n < 1 || b[0] > max_sim) return -1;
            fse_rle(f, b[0]); *hay = 1; return 1;
        case 2: {
            size_t t = fse_leer_tabla(f, b, n, max_log, max_sim);
            if (!t) return -1;
            *hay = 1;
            return (long)t;
        }
        default: return *hay ? 0 : -1;
    }
}

static int bloque_comprimido(Estado *e, const unsigned char *b, size_t n, Sal *s) {
    if (n < 1) return 0;
    /* ── Los literales ── */
    int tipo = b[0] & 3, formato = b[0] >> 2 & 3;
    size_t regen = 0, comp = 0, cab = 0;
    int flujos = 1;
    if (tipo < 2) {
        if (formato == 0 || formato == 2) { regen = b[0] >> 3; cab = 1; }
        else if (formato == 1) { if (n < 2) return 0; regen = (b[0] >> 4) | (size_t)b[1] << 4; cab = 2; }
        else { if (n < 3) return 0; regen = (b[0] >> 4) | (size_t)b[1] << 4 | (size_t)b[2] << 12; cab = 3; }
    } else {
        if (formato < 2) {
            if (n < 3) return 0;
            regen = (b[0] >> 4) | (size_t)(b[1] & 0x3f) << 4;
            comp = (b[1] >> 6) | (size_t)b[2] << 2;
            cab = 3;
            flujos = formato == 0 ? 1 : 4;
        } else if (formato == 2) {
            if (n < 4) return 0;
            regen = (b[0] >> 4) | (size_t)b[1] << 4 | (size_t)(b[2] & 3) << 12;
            comp = (b[2] >> 2) | (size_t)b[3] << 6;
            cab = 4; flujos = 4;
        } else {
            if (n < 5) return 0;
            regen = (b[0] >> 4) | (size_t)b[1] << 4 | (size_t)(b[2] & 0x3f) << 12;
            comp = (b[2] >> 6) | (size_t)b[3] << 2 | (size_t)b[4] << 10;
            cab = 5; flujos = 4;
        }
    }
    if (regen > (1 << 20)) return 0;
    unsigned char *lit = malloc(regen + 1);
    size_t usado;
    if (tipo == 0) {
        if (cab + regen > n) { free(lit); return 0; }
        memcpy(lit, b + cab, regen);
        usado = cab + regen;
    } else if (tipo == 1) {
        if (cab + 1 > n) { free(lit); return 0; }
        memset(lit, b[cab], regen);
        usado = cab + 1;
    } else {
        if (cab + comp > n) { free(lit); return 0; }
        const unsigned char *p = b + cab;
        size_t q = comp;
        if (tipo == 2) {
            size_t t = huff_leer(&e->huff, p, q);
            if (!t) { free(lit); return 0; }
            p += t; q -= t;
        } else if (!e->huff.hay) { free(lit); return 0; }
        int bien = 1;
        if (flujos == 1) bien = huff_flujo(&e->huff, p, q, lit, regen);
        else {
            if (q < 6) { free(lit); return 0; }
            size_t t1 = p[0] | (size_t)p[1] << 8, t2 = p[2] | (size_t)p[3] << 8, t3 = p[4] | (size_t)p[5] << 8;
            if (6 + t1 + t2 + t3 > q) { free(lit); return 0; }
            size_t t4 = q - 6 - t1 - t2 - t3, parte = (regen + 3) / 4;
            if (parte * 3 > regen) { free(lit); return 0; }
            const unsigned char *f = p + 6;
            bien = huff_flujo(&e->huff, f, t1, lit, parte) &&
                   huff_flujo(&e->huff, f + t1, t2, lit + parte, parte) &&
                   huff_flujo(&e->huff, f + t1 + t2, t3, lit + 2 * parte, parte) &&
                   huff_flujo(&e->huff, f + t1 + t2 + t3, t4, lit + 3 * parte, regen - 3 * parte);
        }
        if (!bien) { free(lit); return 0; }
        usado = cab + comp;
    }
    /* ── Las secuencias ── */
    const unsigned char *p = b + usado;
    size_t q = n - usado;
    size_t nseq = 0;
    if (q < 1) { free(lit); return 0; }
    if (p[0] == 0) { nseq = 0; p++; q--; }
    else if (p[0] < 128) { nseq = p[0]; p++; q--; }
    else if (p[0] < 255) { if (q < 2) { free(lit); return 0; } nseq = ((size_t)(p[0] - 128) << 8) + p[1]; p += 2; q -= 2; }
    else { if (q < 3) { free(lit); return 0; } nseq = p[1] + ((size_t)p[2] << 8) + 0x7F00; p += 3; q -= 3; }
    size_t li = 0;                                    /* por dónde van los literales */
    if (nseq > 0) {
        if (q < 1) { free(lit); return 0; }
        int modos = p[0];
        p++; q--;
        long t;
        if ((t = tabla_seq(&e->ll, &e->hay_ll, modos >> 6 & 3, p, q, LL_DEF, 36, 6, 9, 35)) < 0) { free(lit); return 0; }
        p += t; q -= (size_t)t;
        if ((t = tabla_seq(&e->of, &e->hay_of, modos >> 4 & 3, p, q, OF_DEF, 29, 5, 8, 31)) < 0) { free(lit); return 0; }
        p += t; q -= (size_t)t;
        if ((t = tabla_seq(&e->ml, &e->hay_ml, modos >> 2 & 3, p, q, ML_DEF, 53, 6, 9, 52)) < 0) { free(lit); return 0; }
        p += t; q -= (size_t)t;
        Atras r;
        if (!atras_empezar(&r, p, q)) { free(lit); return 0; }
        uint32_t ell = atras_leer(&r, e->ll.log), eof = atras_leer(&r, e->of.log), eml = atras_leer(&r, e->ml.log);
        for (size_t k = 0; k < nseq; k++) {
            int cof = e->of.t[eof].sim, cml = e->ml.t[eml].sim, cll = e->ll.t[ell].sim;
            if (cof > 31 || cml > 52 || cll > 35) { free(lit); return 0; }
            uint32_t ov = (1u << cof) + atras_leer(&r, cof);
            uint32_t ml = ML_BASE[cml] + atras_leer(&r, ML_BITS[cml]);
            uint32_t ll = LL_BASE[cll] + atras_leer(&r, LL_BITS[cll]);
            uint32_t off;
            if (ov > 3) {
                off = ov - 3;
                e->rep[2] = e->rep[1]; e->rep[1] = e->rep[0]; e->rep[0] = off;
            } else {
                int idx = (int)ov - 1 + (ll == 0);     /* sin literales, se corre uno */
                if (idx == 0) off = e->rep[0];
                else if (idx == 1) { off = e->rep[1]; e->rep[1] = e->rep[0]; e->rep[0] = off; }
                else if (idx == 2) { off = e->rep[2]; e->rep[2] = e->rep[1]; e->rep[1] = e->rep[0]; e->rep[0] = off; }
                else { off = e->rep[0] - 1; e->rep[2] = e->rep[1]; e->rep[1] = e->rep[0]; e->rep[0] = off; }
            }
            if (li + ll > regen) { free(lit); return 0; }
            for (uint32_t i = 0; i < ll; i++) sal_byte(s, lit[li++]);
            if (off == 0 || off > s->n) { free(lit); return 0; }
            for (uint32_t i = 0; i < ml; i++) sal_byte(s, s->d[s->n - off]);
            if (k + 1 < nseq) {
                ell = e->ll.t[ell].base + atras_leer(&r, e->ll.t[ell].bits);
                eml = e->ml.t[eml].base + atras_leer(&r, e->ml.t[eml].bits);
                eof = e->of.t[eof].base + atras_leer(&r, e->of.t[eof].bits);
            }
        }
        if (r.pos != 0) { free(lit); return 0; }
    }
    while (li < regen) sal_byte(s, lit[li++]);
    free(lit);
    return 1;
}

unsigned char *zstd_descomprimir(const unsigned char *b, size_t n, size_t *largo) {
    Sal s = {0};
    Estado *e = calloc(1, sizeof *e);
    size_t i = 0;
    while (i + 4 <= n) {
        uint32_t magia = b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16 | (uint32_t)b[i + 3] << 24;
        i += 4;
        if ((magia & 0xFFFFFFF0u) == 0x184D2A50u) {          /* un trozo que se salta */
            if (i + 4 > n) goto mal;
            uint32_t t = b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16 | (uint32_t)b[i + 3] << 24;
            i += 4 + t;
            continue;
        }
        if (magia != 0xFD2FB528u || i >= n) goto mal;
        int fhd = b[i++];
        int fcs_flag = fhd >> 6, unico = fhd >> 5 & 1, suma = fhd >> 2 & 1, did = fhd & 3;
        if (!unico) i++;                                  /* el tamaño de la ventana: no hace falta */
        static const int DID[4] = {0, 1, 2, 4};
        int fcs = fcs_flag == 0 ? unico : fcs_flag == 1 ? 2 : fcs_flag == 2 ? 4 : 8;
        i += (size_t)DID[did] + (size_t)fcs;
        e->rep[0] = 1; e->rep[1] = 4; e->rep[2] = 8;
        e->hay_ll = e->hay_of = e->hay_ml = 0;
        e->huff.hay = 0;
        for (;;) {
            if (i + 3 > n) goto mal;
            uint32_t c = b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16;
            i += 3;
            int ultimo = c & 1, tipo = c >> 1 & 3;
            size_t tam = c >> 3;
            if (tipo == 0) {
                if (i + tam > n) goto mal;
                for (size_t k = 0; k < tam; k++) sal_byte(&s, b[i + k]);
                i += tam;
            } else if (tipo == 1) {
                if (i + 1 > n) goto mal;
                for (size_t k = 0; k < tam; k++) sal_byte(&s, b[i]);
                i += 1;
            } else if (tipo == 2) {
                if (i + tam > n || !bloque_comprimido(e, b + i, tam, &s)) goto mal;
                i += tam;
            } else goto mal;
            if (s.n > ((size_t)1 << 28)) goto mal;        /* más de 256 MB: no es una capa de verdad */
            if (ultimo) break;
        }
        if (suma) i += 4;
    }
    free(e);
    *largo = s.n;
    if (!s.d) s.d = malloc(1);
    return s.d;
mal:
    free(e);
    free(s.d);
    return NULL;
}
