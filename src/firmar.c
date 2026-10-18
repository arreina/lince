/*
 * 🐆 Lince — firmar.c
 * Ver firmar.h. Todo en C, sin bibliotecas: SHA-256 (FIPS 180-4), números de
 * 2048 bits con multiplicación de Montgomery, RSA (PKCS #1 v1.5), DER para el
 * certificado X.509 y el esquema de firma v2 de los .apk.
 */
#ifdef _WIN32
#  define _CRT_RAND_S                  /* rand_s: azar del sistema */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "firmar.h"

/* ───────────────────────── SHA-256 ───────────────────────── */
static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_bloque(Sha256 *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(w[i-15], 7) ^ ROTR(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROTR(w[i-2], 17) ^ ROTR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        uint32_t t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

void sha256_empezar(Sha256 *s) {
    static const uint32_t H0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, H0, sizeof H0);
    s->nbuf = 0; s->total = 0;
}

void sha256_mas(Sha256 *s, const void *datos, size_t n) {
    const unsigned char *p = datos;
    s->total += n;
    while (n > 0) {
        size_t k = 64 - s->nbuf < n ? 64 - s->nbuf : n;
        memcpy(s->buf + s->nbuf, p, k);
        s->nbuf += k; p += k; n -= k;
        if (s->nbuf == 64) { sha256_bloque(s, s->buf); s->nbuf = 0; }
    }
}

void sha256_acabar(Sha256 *s, unsigned char r[32]) {
    uint64_t bits = s->total * 8;
    unsigned char uno = 0x80, cero = 0;
    sha256_mas(s, &uno, 1);
    while (s->nbuf != 56) sha256_mas(s, &cero, 1);
    unsigned char largo[8];
    for (int i = 0; i < 8; i++) largo[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha256_mas(s, largo, 8);
    for (int i = 0; i < 8; i++) {
        r[4*i] = (unsigned char)(s->h[i] >> 24); r[4*i+1] = (unsigned char)(s->h[i] >> 16);
        r[4*i+2] = (unsigned char)(s->h[i] >> 8); r[4*i+3] = (unsigned char)s->h[i];
    }
}

void sha256(const void *datos, size_t n, unsigned char r[32]) {
    Sha256 s;
    sha256_empezar(&s);
    sha256_mas(&s, datos, n);
    sha256_acabar(&s, r);
}

/* ───────────────────── Números grandes ─────────────────────
   Cifras de 32 bits, la menos significativa primero; 'n' cifras usadas. */
#define BN_MAX 132
typedef struct { int n; uint32_t d[BN_MAX]; } Bn;

static void bn_ajustar(Bn *a) { while (a->n > 0 && a->d[a->n - 1] == 0) a->n--; }
static void bn_pequeno(Bn *a, uint32_t v) { memset(a, 0, sizeof *a); a->d[0] = v; a->n = v ? 1 : 0; }

static void bn_de_bytes(Bn *a, const unsigned char *p, size_t len) {   /* big-endian */
    memset(a, 0, sizeof *a);
    for (size_t i = 0; i < len; i++) {
        size_t b = len - 1 - i;                     /* byte b desde el final */
        a->d[b / 4] |= (uint32_t)p[i] << (8 * (b % 4));
    }
    a->n = (int)((len + 3) / 4);
    bn_ajustar(a);
}

static void bn_a_bytes(const Bn *a, unsigned char *p, size_t len) {     /* big-endian, 'len' bytes */
    for (size_t i = 0; i < len; i++) {
        size_t b = len - 1 - i;
        p[i] = b / 4 < (size_t)a->n ? (unsigned char)(a->d[b / 4] >> (8 * (b % 4))) : 0;
    }
}

static int bn_cmp(const Bn *a, const Bn *b) {
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--) if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

static void bn_restar(Bn *r, const Bn *a, const Bn *b) {   /* a >= b */
    uint64_t presta = 0;
    for (int i = 0; i < a->n; i++) {
        uint64_t x = (uint64_t)a->d[i] - (i < b->n ? b->d[i] : 0) - presta;
        r->d[i] = (uint32_t)x;
        presta = (x >> 63) & 1;
    }
    r->n = a->n;
    for (int i = a->n; i < BN_MAX; i++) r->d[i] = 0;
    bn_ajustar(r);
}

static void bn_mul(Bn *r, const Bn *a, const Bn *b) {
    Bn t;
    memset(&t, 0, sizeof t);
    for (int i = 0; i < a->n; i++) {
        uint64_t lleva = 0;
        for (int j = 0; j < b->n; j++) {
            uint64_t x = (uint64_t)a->d[i] * b->d[j] + t.d[i + j] + lleva;
            t.d[i + j] = (uint32_t)x;
            lleva = x >> 32;
        }
        t.d[i + b->n] = (uint32_t)lleva;
    }
    t.n = a->n + b->n;
    bn_ajustar(&t);
    *r = t;
}

static void bn_mul_pequeno(Bn *r, const Bn *a, uint32_t m, uint32_t suma) {
    uint64_t lleva = suma;
    Bn t = *a;
    for (int i = 0; i < t.n; i++) {
        uint64_t x = (uint64_t)t.d[i] * m + lleva;
        t.d[i] = (uint32_t)x;
        lleva = x >> 32;
    }
    if (lleva) t.d[t.n++] = (uint32_t)lleva;
    if (t.n == 0 && suma) { t.d[0] = suma; t.n = 1; }
    *r = t;
}

static uint32_t bn_div_pequeno(Bn *r, const Bn *a, uint32_t m) {   /* r = a / m; devuelve el resto */
    uint64_t resto = 0;
    Bn t = *a;
    for (int i = t.n - 1; i >= 0; i--) {
        uint64_t x = resto << 32 | t.d[i];
        t.d[i] = (uint32_t)(x / m);
        resto = x % m;
    }
    bn_ajustar(&t);
    if (r) *r = t;
    return (uint32_t)resto;
}

/* Montgomery: aR mod m con R = 2^(32·n) */
typedef struct { Bn m; int n; uint32_t m0; Bn r2; } Mont;

static void mont_preparar(Mont *c, const Bn *m) {
    c->m = *m;
    c->n = m->n;
    uint32_t x = 1;                               /* inverso de m mod 2^32 (Newton) */
    for (int i = 0; i < 5; i++) x *= 2 - m->d[0] * x;
    c->m0 = (uint32_t)(0 - x);
    Bn r;                                         /* R² mod m, doblando y restando */
    bn_pequeno(&r, 1);
    for (int i = 0; i < 64 * c->n; i++) {
        bn_mul_pequeno(&r, &r, 2, 0);
        if (bn_cmp(&r, m) >= 0) bn_restar(&r, &r, m);
    }
    c->r2 = r;
}

static void mont_mul(Bn *r, const Bn *a, const Bn *b, const Mont *c) {
    int n = c->n;
    uint32_t t[BN_MAX + 2];
    memset(t, 0, sizeof(uint32_t) * (size_t)(n + 2));
    for (int i = 0; i < n; i++) {
        uint64_t lleva = 0;
        uint32_t ai = i < a->n ? a->d[i] : 0;
        for (int j = 0; j < n; j++) {
            uint64_t x = (uint64_t)ai * (j < b->n ? b->d[j] : 0) + t[j] + lleva;
            t[j] = (uint32_t)x;
            lleva = x >> 32;
        }
        uint64_t x = (uint64_t)t[n] + lleva;
        t[n] = (uint32_t)x;
        t[n + 1] = (uint32_t)(x >> 32);
        uint32_t u = t[0] * c->m0;
        x = (uint64_t)u * c->m.d[0] + t[0];
        lleva = x >> 32;
        for (int j = 1; j < n; j++) {
            x = (uint64_t)u * c->m.d[j] + t[j] + lleva;
            t[j - 1] = (uint32_t)x;
            lleva = x >> 32;
        }
        x = (uint64_t)t[n] + lleva;
        t[n - 1] = (uint32_t)x;
        t[n] = t[n + 1] + (uint32_t)(x >> 32);
    }
    Bn res;
    memset(&res, 0, sizeof res);
    for (int i = 0; i <= n; i++) res.d[i] = t[i];
    res.n = n + 1;
    bn_ajustar(&res);
    if (bn_cmp(&res, &c->m) >= 0) bn_restar(&res, &res, &c->m);
    *r = res;
}

/* r = b^e mod m */
static void bn_potencia(Bn *r, const Bn *b, const Bn *e, const Mont *c) {
    Bn x, uno, acc;
    mont_mul(&x, b, &c->r2, c);                   /* b en forma de Montgomery */
    bn_pequeno(&uno, 1);
    mont_mul(&acc, &uno, &c->r2, c);              /* 1 en forma de Montgomery */
    for (int i = e->n * 32 - 1; i >= 0; i--) {
        mont_mul(&acc, &acc, &acc, c);
        if ((e->d[i / 32] >> (i % 32)) & 1) mont_mul(&acc, &acc, &x, c);
    }
    mont_mul(r, &acc, &uno, c);                   /* fuera de la forma de Montgomery */
}

/* ─────────────────────────── Azar ─────────────────────────── */
static int azar(unsigned char *p, size_t n) {
#ifdef _WIN32
    for (size_t i = 0; i < n; i += 4) {
        unsigned int v;
        if (rand_s(&v) != 0) return 0;
        for (size_t k = 0; k < 4 && i + k < n; k++) p[i + k] = (unsigned char)(v >> (8 * k));
    }
    return 1;
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return 0;
    size_t leidos = fread(p, 1, n, f);
    fclose(f);
    return leidos == n;
#endif
}

/* ──────────────────────── Primos y RSA ──────────────────────── */
static const uint32_t PRIMOS_PEQUENOS[] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
    73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191,
    193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313};

/* Miller-Rabin con 'rondas' bases al azar */
static int es_primo(const Bn *p, int rondas) {
    for (size_t i = 0; i < sizeof PRIMOS_PEQUENOS / sizeof PRIMOS_PEQUENOS[0]; i++)
        if (bn_div_pequeno(NULL, p, PRIMOS_PEQUENOS[i]) == 0) return 0;
    Mont c;
    mont_preparar(&c, p);
    Bn p1, d, uno;
    bn_pequeno(&uno, 1);
    bn_restar(&p1, p, &uno);
    d = p1;
    int s = 0;
    while ((d.d[0] & 1) == 0) { bn_div_pequeno(&d, &d, 2); s++; }
    for (int r = 0; r < rondas; r++) {
        unsigned char al[256];
        Bn a, x;
        size_t largo = (size_t)p->n * 4 - 1;
        if (!azar(al, largo)) return 0;
        bn_de_bytes(&a, al, largo);
        if (a.n == 0 || bn_cmp(&a, &uno) == 0) continue;
        bn_potencia(&x, &a, &d, &c);
        if (bn_cmp(&x, &uno) == 0 || bn_cmp(&x, &p1) == 0) continue;
        int compuesto = 1;
        Bn xm, p1m;                               /* los cuadrados, en forma de Montgomery */
        mont_mul(&xm, &x, &c.r2, &c);
        mont_mul(&p1m, &p1, &c.r2, &c);
        for (int k = 1; k < s && compuesto; k++) {
            mont_mul(&xm, &xm, &xm, &c);
            if (bn_cmp(&xm, &p1m) == 0) compuesto = 0;
        }
        if (compuesto) return 0;
    }
    return 1;
}

static int primo_nuevo(Bn *p, int bits, uint32_t e) {
    unsigned char b[128];
    size_t bytes = (size_t)bits / 8;
    for (int intento = 0; intento < 100000; intento++) {
        if (!azar(b, bytes)) return 0;
        b[0] |= 0xC0;                             /* los dos bits de arriba: n tendrá 2·bits */
        b[bytes - 1] |= 1;                        /* impar */
        bn_de_bytes(p, b, bytes);
        if (bn_div_pequeno(NULL, p, e) == 1) continue;      /* que e no divida a p-1 */
        if (es_primo(p, 24)) return 1;
    }
    return 0;
}

struct LlaveRsa {
    Bn n, d;
    uint32_t e;
    Mont mont;
    unsigned char *cert, *pub;
    size_t ncert, npub;
};

/* ─────────────────────────── DER ─────────────────────────── */
typedef struct { unsigned char *p; size_t n, cap; } Buf;

static void buf_mas(Buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 64;
        b->p = realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, p, n);
    b->n += n;
}
static void buf_byte(Buf *b, unsigned char c) { buf_mas(b, &c, 1); }

/* tag, largo y contenido */
static void der(Buf *b, unsigned char tag, const void *p, size_t n) {
    buf_byte(b, tag);
    if (n < 128) buf_byte(b, (unsigned char)n);
    else if (n < 256) { buf_byte(b, 0x81); buf_byte(b, (unsigned char)n); }
    else { buf_byte(b, 0x82); buf_byte(b, (unsigned char)(n >> 8)); buf_byte(b, (unsigned char)n); }
    buf_mas(b, p, n);
}
static void der_buf(Buf *b, unsigned char tag, Buf *dentro) { der(b, tag, dentro->p, dentro->n); free(dentro->p); dentro->p = NULL; dentro->n = dentro->cap = 0; }

static void der_entero(Buf *b, const unsigned char *be, size_t n) {     /* positivo, sin ceros de más */
    while (n > 1 && be[0] == 0) { be++; n--; }
    Buf t = {0};
    if (be[0] & 0x80) buf_byte(&t, 0);
    buf_mas(&t, be, n);
    der_buf(b, 0x02, &t);
}

static const unsigned char OID_SHA256_RSA[] = {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x05, 0x00};
static const unsigned char OID_RSA[]        = {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00};

/* SubjectPublicKeyInfo de la llave */
static void der_publica(Buf *b, const LlaveRsa *ll) {
    unsigned char nb[256], eb[4] = {(unsigned char)(ll->e >> 24), (unsigned char)(ll->e >> 16), (unsigned char)(ll->e >> 8), (unsigned char)ll->e};
    bn_a_bytes(&ll->n, nb, 256);
    Buf rsa = {0}, rsa_seq = {0}, bits = {0}, alg = {0}, spki = {0};
    der_entero(&rsa, nb, 256);
    der_entero(&rsa, eb, 4);
    der_buf(&rsa_seq, 0x30, &rsa);
    buf_byte(&bits, 0);
    buf_mas(&bits, rsa_seq.p, rsa_seq.n);
    free(rsa_seq.p);
    der(&alg, 0x30, OID_RSA, sizeof OID_RSA);
    buf_mas(&spki, alg.p, alg.n);
    free(alg.p);
    der_buf(&spki, 0x03, &bits);
    der_buf(b, 0x30, &spki);
}

/* Nombre "CN=Lince" */
static void der_nombre(Buf *b) {
    static const unsigned char CN[] = {0x06, 0x03, 0x55, 0x04, 0x03};
    Buf atv = {0}, seq = {0}, set = {0};
    buf_mas(&atv, CN, sizeof CN);
    der(&atv, 0x0C, "Lince", 5);
    der_buf(&seq, 0x30, &atv);
    der_buf(&set, 0x31, &seq);
    der_buf(b, 0x30, &set);
}

/* RSASSA-PKCS1-v1_5 con SHA-256 */
void llave_firmar(const LlaveRsa *ll, const void *datos, size_t n, unsigned char firma[256]) {
    static const unsigned char INFO[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
                                         0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
    unsigned char em[256];
    em[0] = 0; em[1] = 1;
    size_t relleno = 256 - 3 - sizeof INFO - 32;
    memset(em + 2, 0xFF, relleno);
    em[2 + relleno] = 0;
    memcpy(em + 3 + relleno, INFO, sizeof INFO);
    sha256(datos, n, em + 3 + relleno + sizeof INFO);
    Bn m, s;
    bn_de_bytes(&m, em, 256);
    bn_potencia(&s, &m, &ll->d, &ll->mont);
    bn_a_bytes(&s, firma, 256);
}

/* El certificado: autofirmado, siempre el mismo para la misma llave (número
   de serie sacado de la llave y fechas fijas), así las actualizaciones del
   juego se pueden instalar encima */
static void preparar_certificado(LlaveRsa *ll) {
    unsigned char nb[256], resumen[32];
    bn_a_bytes(&ll->n, nb, 256);
    sha256(nb, 256, resumen);
    resumen[0] &= 0x7F;
    Buf tbs = {0}, alg = {0}, fechas = {0}, cuerpo = {0}, tbs_der = {0}, pub = {0};
    der_entero(&tbs, resumen, 8);
    der(&alg, 0x30, OID_SHA256_RSA, sizeof OID_SHA256_RSA);
    buf_mas(&tbs, alg.p, alg.n);
    der_nombre(&tbs);
    der(&fechas, 0x17, "250101000000Z", 13);
    der(&fechas, 0x17, "491231235959Z", 13);
    der_buf(&tbs, 0x30, &fechas);
    der_nombre(&tbs);
    der_publica(&pub, ll);
    buf_mas(&tbs, pub.p, pub.n);
    der_buf(&tbs_der, 0x30, &tbs);
    unsigned char firma[256];
    llave_firmar(ll, tbs_der.p, tbs_der.n, firma);
    Buf bits = {0};
    buf_byte(&bits, 0);
    buf_mas(&bits, firma, 256);
    buf_mas(&cuerpo, tbs_der.p, tbs_der.n);
    buf_mas(&cuerpo, alg.p, alg.n);
    der_buf(&cuerpo, 0x03, &bits);
    Buf cert = {0};
    der_buf(&cert, 0x30, &cuerpo);
    ll->cert = cert.p; ll->ncert = cert.n;
    ll->pub = pub.p; ll->npub = pub.n;
    free(alg.p); free(tbs_der.p);
}

/* ─────────────────── Crear, guardar y cargar ─────────────────── */
static void hex(FILE *f, const char *nombre, const Bn *a) {
    unsigned char b[256];
    bn_a_bytes(a, b, 256);
    fprintf(f, "%s ", nombre);
    for (int i = 0; i < 256; i++) fprintf(f, "%02x", b[i]);
    fprintf(f, "\n");
}

static int leer_hex(const char *linea, Bn *a) {
    unsigned char b[256];
    for (int i = 0; i < 256; i++) {
        unsigned v;
        if (sscanf(linea + 2 * i, "%2x", &v) != 1) return 0;
        b[i] = (unsigned char)v;
    }
    bn_de_bytes(a, b, 256);
    return 1;
}

static LlaveRsa *llave_nueva(void) {
    LlaveRsa *ll = calloc(1, sizeof *ll);
    ll->e = 65537;
    Bn p, q, uno, p1, q1, fi, t;
    bn_pequeno(&uno, 1);
    do {
        if (!primo_nuevo(&p, 1024, ll->e) || !primo_nuevo(&q, 1024, ll->e)) { free(ll); return NULL; }
    } while (bn_cmp(&p, &q) == 0);
    bn_mul(&ll->n, &p, &q);
    bn_restar(&p1, &p, &uno);
    bn_restar(&q1, &q, &uno);
    bn_mul(&fi, &p1, &q1);
    /* d = (1 + k·fi) / e con k tal que k·fi ≡ -1 (mod e) */
    uint32_t f = bn_div_pequeno(NULL, &fi, ll->e);
    int64_t r0 = ll->e, r1 = f, s0 = 0, s1 = 1;          /* inverso de f mod e */
    while (r1) { int64_t qq = r0 / r1, tmp = r0 - qq * r1; r0 = r1; r1 = tmp; tmp = s0 - qq * s1; s0 = s1; s1 = tmp; }
    int64_t inv = ((s0 % ll->e) + ll->e) % ll->e;
    uint32_t k = (uint32_t)((ll->e - inv) % ll->e);
    bn_mul_pequeno(&t, &fi, k, 1);
    if (bn_div_pequeno(&ll->d, &t, ll->e) != 0) { free(ll); return NULL; }
    return ll;
}

LlaveRsa *llave_cargar_o_crear(const char *ruta, char *error, size_t max) {
    LlaveRsa *ll = NULL;
    FILE *f = fopen(ruta, "r");
    if (f) {
        char linea[1200];
        int bien = 0;
        ll = calloc(1, sizeof *ll);
        ll->e = 65537;
        if (fgets(linea, sizeof linea, f) && strncmp(linea, "lince-llave-rsa 1", 17) == 0) bien++;
        if (bien && fgets(linea, sizeof linea, f) && strncmp(linea, "n ", 2) == 0 && leer_hex(linea + 2, &ll->n)) bien++;
        if (bien == 2 && fgets(linea, sizeof linea, f) && strncmp(linea, "d ", 2) == 0 && leer_hex(linea + 2, &ll->d)) bien++;
        fclose(f);
        if (bien != 3) {
            free(ll);
            snprintf(error, max, "la llave '%s' no se entiende (bórrala y se creará otra, pero los juegos firmados con ella no se podrán actualizar)", ruta);
            return NULL;
        }
    } else {
        ll = llave_nueva();
        if (!ll) { snprintf(error, max, "no se pudo crear una llave nueva (falta azar del sistema)"); return NULL; }
        f = fopen(ruta, "w");
        if (!f) { free(ll); snprintf(error, max, "no se pudo guardar la llave en '%s'", ruta); return NULL; }
        fprintf(f, "lince-llave-rsa 1\n");
        hex(f, "n", &ll->n);
        hex(f, "d", &ll->d);
        fprintf(f, "# La llave con la que se firman tus juegos para Android. Guárdala: sin\n"
                   "# ella no podrás publicar actualizaciones de un juego ya instalado.\n");
        fclose(f);
    }
    mont_preparar(&ll->mont, &ll->n);
    preparar_certificado(ll);
    return ll;
}

void llave_liberar(LlaveRsa *ll) {
    if (!ll) return;
    free(ll->cert); free(ll->pub);
    memset(ll, 0, sizeof *ll);
    free(ll);
}

const unsigned char *llave_certificado(const LlaveRsa *ll, size_t *n) { *n = ll->ncert; return ll->cert; }
const unsigned char *llave_publica(const LlaveRsa *ll, size_t *n) { *n = ll->npub; return ll->pub; }

/* ─────────────────── Esquema de firma v2 del APK ───────────────────
   https://source.android.com/docs/security/features/apksigning/v2 */
static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void pon32(Buf *b, uint32_t v) { unsigned char c[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24)}; buf_mas(b, c, 4); }
static void pon64(Buf *b, uint64_t v) { pon32(b, (uint32_t)v); pon32(b, (uint32_t)(v >> 32)); }
static void con_largo(Buf *b, const Buf *dentro) { pon32(b, (uint32_t)dentro->n); buf_mas(b, dentro->p, dentro->n); }

/* Resumen de una parte en trozos de 1 MB: los resúmenes de los trozos van a 'todos' */
static void resumir_trozos(const unsigned char *p, size_t n, Buf *todos, uint32_t *cuantos) {
    for (size_t i = 0; i < n; i += 1048576) {
        size_t largo = n - i < 1048576 ? n - i : 1048576;
        unsigned char cab[5] = {0xa5, (unsigned char)largo, (unsigned char)(largo >> 8), (unsigned char)(largo >> 16), (unsigned char)(largo >> 24)};
        Sha256 s;
        unsigned char r[32];
        sha256_empezar(&s);
        sha256_mas(&s, cab, 5);
        sha256_mas(&s, p + i, largo);
        sha256_acabar(&s, r);
        buf_mas(todos, r, 32);
        (*cuantos)++;
    }
}

unsigned char *apk_firmar_v2(const unsigned char *zip, size_t n, const LlaveRsa *ll, size_t *n_salida) {
    /* el final del directorio central (EOCD) */
    size_t eocd = (size_t)-1;
    for (size_t i = n >= 22 ? n - 22 : 0; i + 22 <= n; i--) {
        if (le32(zip + i) == 0x06054b50) { eocd = i; break; }
        if (i == 0 || n - i > 22 + 65535) break;
    }
    if (eocd == (size_t)-1) return NULL;
    size_t cd = le32(zip + eocd + 16), cd_largo = le32(zip + eocd + 12);
    if (cd + cd_largo != eocd) return NULL;

    /* el resumen de las tres partes: entradas, directorio central y EOCD */
    Buf trozos = {0};
    uint32_t cuantos = 0;
    resumir_trozos(zip, cd, &trozos, &cuantos);
    resumir_trozos(zip + cd, cd_largo, &trozos, &cuantos);
    resumir_trozos(zip + eocd, n - eocd, &trozos, &cuantos);
    unsigned char cab[5] = {0x5a, (unsigned char)cuantos, (unsigned char)(cuantos >> 8), (unsigned char)(cuantos >> 16), (unsigned char)(cuantos >> 24)};
    unsigned char resumen[32];
    Sha256 s;
    sha256_empezar(&s);
    sha256_mas(&s, cab, 5);
    sha256_mas(&s, trozos.p, trozos.n);
    sha256_acabar(&s, resumen);
    free(trozos.p);

    /* signed data: resúmenes, certificados, atributos */
    Buf res = {0}, res_uno = {0}, res_seq = {0}, certs = {0}, cert_uno = {0}, firmado = {0}, vacio = {0};
    pon32(&res_uno, 0x0103);                       /* RSASSA-PKCS1-v1_5 con SHA2-256 */
    pon32(&res_uno, 32);
    buf_mas(&res_uno, resumen, 32);
    con_largo(&res, &res_uno);
    con_largo(&res_seq, &res);
    size_t nc;
    const unsigned char *c = llave_certificado(ll, &nc);
    buf_mas(&cert_uno, c, nc);
    con_largo(&certs, &cert_uno);
    buf_mas(&firmado, res_seq.p, res_seq.n);         /* la secuencia ya lleva su largo */
    Buf certs_seq = {0};
    con_largo(&certs_seq, &certs);
    buf_mas(&firmado, certs_seq.p, certs_seq.n);
    con_largo(&firmado, &vacio);

    /* la firma, la clave pública y el firmante */
    unsigned char firma[256];
    llave_firmar(ll, firmado.p, firmado.n, firma);
    Buf fir_uno = {0}, firmas = {0}, firmas_seq = {0}, publica = {0}, firmante = {0}, firmantes = {0}, valor = {0};
    pon32(&fir_uno, 0x0103);
    pon32(&fir_uno, 256);
    buf_mas(&fir_uno, firma, 256);
    con_largo(&firmas, &fir_uno);
    con_largo(&firmas_seq, &firmas);
    size_t np;
    const unsigned char *pk = llave_publica(ll, &np);
    buf_mas(&publica, pk, np);
    con_largo(&firmante, &firmado);
    buf_mas(&firmante, firmas_seq.p, firmas_seq.n);
    con_largo(&firmante, &publica);
    con_largo(&firmantes, &firmante);
    con_largo(&valor, &firmantes);

    /* el bloque de firma */
    Buf bloque = {0};
    uint64_t par = 4 + valor.n;
    uint64_t tam = 8 + par + 8 + 16;
    pon64(&bloque, tam);
    pon64(&bloque, par);
    pon32(&bloque, 0x7109871a);
    buf_mas(&bloque, valor.p, valor.n);
    pon64(&bloque, tam);
    buf_mas(&bloque, "APK Sig Block 42", 16);

    /* el .apk: entradas, bloque, directorio central y EOCD apuntando a él */
    size_t total = n + bloque.n;
    unsigned char *salida = malloc(total);
    memcpy(salida, zip, cd);
    memcpy(salida + cd, bloque.p, bloque.n);
    memcpy(salida + cd + bloque.n, zip + cd, n - cd);
    uint32_t nuevo_cd = (uint32_t)(cd + bloque.n);
    unsigned char *e = salida + eocd + bloque.n + 16;
    e[0] = (unsigned char)nuevo_cd; e[1] = (unsigned char)(nuevo_cd >> 8); e[2] = (unsigned char)(nuevo_cd >> 16); e[3] = (unsigned char)(nuevo_cd >> 24);
    *n_salida = total;

    free(res.p); free(res_uno.p); free(res_seq.p); free(certs.p); free(cert_uno.p); free(firmado.p);
    free(certs_seq.p); free(fir_uno.p); free(firmas.p); free(firmas_seq.p); free(publica.p);
    free(firmante.p); free(firmantes.p); free(valor.p); free(bloque.p);
    return salida;
}

/* ─────────────────── Firma JAR (la del .aab) ───────────────────
   META-INF/MANIFEST.MF con el resumen de cada archivo, LINCE.SF con el de cada
   apartado del manifiesto, y LINCE.RSA: la firma del .SF en PKCS #7.
   https://docs.oracle.com/javase/8/docs/technotes/guides/jar/jar.html#Signed_JAR_File */
static void base64(Buf *b, const unsigned char *p, size_t n) {
    static const char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < n ? (uint32_t)p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        char c[4] = {A[v >> 18 & 63], A[v >> 12 & 63], i + 1 < n ? A[v >> 6 & 63] : '=', i + 2 < n ? A[v & 63] : '='};
        buf_mas(b, c, 4);
    }
}

/* Una línea "clave: valor" del manifiesto, partida a 72 bytes como pide el formato */
static void linea_mf(Buf *b, const char *clave, const char *valor, size_t nvalor) {
    Buf l = {0};
    buf_mas(&l, clave, strlen(clave));
    buf_mas(&l, ": ", 2);
    buf_mas(&l, valor, nvalor);
    for (size_t i = 0; i < l.n; ) {
        size_t cabe = i == 0 ? 72 : 71;
        size_t trozo = l.n - i < cabe ? l.n - i : cabe;
        if (i > 0) buf_byte(b, ' ');
        buf_mas(b, l.p + i, trozo);
        buf_mas(b, "\r\n", 2);
        i += trozo;
    }
    free(l.p);
}

static void linea_resumen(Buf *b, const char *clave, const void *p, size_t n) {
    unsigned char r[32];
    Buf b64 = {0};
    sha256(p, n, r);
    base64(&b64, r, 32);
    linea_mf(b, clave, (const char *)b64.p, b64.n);
    free(b64.p);
}

void jar_firmar(const LlaveRsa *ll, const char *const *nombres, const unsigned char *const *datos,
                const size_t *largos, int num, JarFirma *salida) {
    static const char PRINCIPAL[] = "Manifest-Version: 1.0\r\nCreated-By: Lince\r\n\r\n";
    static const char FIRMA[]     = "Signature-Version: 1.0\r\nCreated-By: Lince\r\n";
    Buf mf = {0}, sf = {0}, apartados = {0};
    buf_mas(&mf, PRINCIPAL, strlen(PRINCIPAL));
    for (int i = 0; i < num; i++) {
        size_t inicio = mf.n;
        linea_mf(&mf, "Name", nombres[i], strlen(nombres[i]));
        linea_resumen(&mf, "SHA-256-Digest", datos[i], largos[i]);
        buf_mas(&mf, "\r\n", 2);
        /* en el .SF, el resumen de este apartado del manifiesto */
        linea_mf(&apartados, "Name", nombres[i], strlen(nombres[i]));
        linea_resumen(&apartados, "SHA-256-Digest", mf.p + inicio, mf.n - inicio);
        buf_mas(&apartados, "\r\n", 2);
    }
    buf_mas(&sf, FIRMA, strlen(FIRMA));
    linea_resumen(&sf, "SHA-256-Digest-Manifest", mf.p, mf.n);
    linea_resumen(&sf, "SHA-256-Digest-Manifest-Main-Attributes", mf.p, strlen(PRINCIPAL));
    buf_mas(&sf, "\r\n", 2);
    buf_mas(&sf, apartados.p, apartados.n);
    free(apartados.p);

    /* PKCS #7 SignedData, sin el contenido (el .SF va aparte) */
    static const unsigned char OID_SIGNED_DATA[] = {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x02};
    static const unsigned char OID_DATA[]        = {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x01};
    static const unsigned char OID_SHA256[]      = {0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00};
    static const unsigned char UNO[] = {1};
    unsigned char firma[256], nb[256], serie[32];
    llave_firmar(ll, sf.p, sf.n, firma);
    bn_a_bytes(&ll->n, nb, 256);                  /* el número de serie del certificado */
    sha256(nb, 256, serie);
    serie[0] &= 0x7F;

    Buf alg = {0}, algs = {0}, contenido = {0}, certs = {0}, emisor = {0}, firmante = {0}, firmantes = {0};
    Buf alg_rsa = {0}, sd = {0}, sd_seq = {0}, explicito = {0}, ci = {0}, rsa = {0};
    der(&alg, 0x30, OID_SHA256, sizeof OID_SHA256);
    der(&algs, 0x31, alg.p, alg.n);
    der(&contenido, 0x30, OID_DATA, sizeof OID_DATA);
    der(&certs, 0xA0, ll->cert, ll->ncert);
    der_nombre(&emisor);
    der_entero(&emisor, serie, 8);
    der_entero(&firmante, UNO, 1);
    der_buf(&firmante, 0x30, &emisor);
    buf_mas(&firmante, alg.p, alg.n);
    der(&alg_rsa, 0x30, OID_RSA, sizeof OID_RSA);
    buf_mas(&firmante, alg_rsa.p, alg_rsa.n);
    der(&firmante, 0x04, firma, 256);
    Buf firmante_seq = {0};
    der_buf(&firmante_seq, 0x30, &firmante);
    der_buf(&firmantes, 0x31, &firmante_seq);
    der_entero(&sd, UNO, 1);
    buf_mas(&sd, algs.p, algs.n);
    buf_mas(&sd, contenido.p, contenido.n);
    buf_mas(&sd, certs.p, certs.n);
    buf_mas(&sd, firmantes.p, firmantes.n);
    der_buf(&sd_seq, 0x30, &sd);
    der_buf(&explicito, 0xA0, &sd_seq);
    buf_mas(&ci, OID_SIGNED_DATA, sizeof OID_SIGNED_DATA);
    buf_mas(&ci, explicito.p, explicito.n);
    der_buf(&rsa, 0x30, &ci);
    free(alg.p); free(algs.p); free(contenido.p); free(certs.p); free(firmantes.p); free(alg_rsa.p); free(explicito.p);

    salida->mf = mf.p;   salida->nmf = mf.n;
    salida->sf = sf.p;   salida->nsf = sf.n;
    salida->rsa = rsa.p; salida->nrsa = rsa.n;
}
