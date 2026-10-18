/*
 * 🐆 Lince — firmar.h
 * Lo necesario para firmar un .apk de Android sin Java ni OpenSSL: SHA-256,
 * llaves RSA de 2048 bits (crearlas, guardarlas y firmar con ellas), un
 * certificado X.509 autofirmado y el bloque de firma v2 de Android.
 */
#ifndef LINCE_FIRMAR_H
#define LINCE_FIRMAR_H

#include <stddef.h>
#include <stdint.h>

/* ── SHA-256 ── */
typedef struct { uint32_t h[8]; unsigned char buf[64]; size_t nbuf; uint64_t total; } Sha256;
void sha256_empezar(Sha256 *s);
void sha256_mas(Sha256 *s, const void *datos, size_t n);
void sha256_acabar(Sha256 *s, unsigned char resumen[32]);
void sha256(const void *datos, size_t n, unsigned char resumen[32]);

/* ── Llave RSA de 2048 bits, con su certificado ── */
typedef struct LlaveRsa LlaveRsa;

/* La llave guardada en 'ruta'; si no existe, una nueva que se guarda ahí.
   NULL si no se pudo (con el motivo en 'error'). */
LlaveRsa *llave_cargar_o_crear(const char *ruta, char *error, size_t max);
void      llave_liberar(LlaveRsa *ll);

/* Firma (RSASSA-PKCS1-v1_5 con SHA-256) de 'datos'; 'firma' tiene 256 bytes */
void      llave_firmar(const LlaveRsa *ll, const void *datos, size_t n, unsigned char firma[256]);
/* El certificado X.509 (DER) y la clave pública (SubjectPublicKeyInfo, DER) */
const unsigned char *llave_certificado(const LlaveRsa *ll, size_t *n);
const unsigned char *llave_publica(const LlaveRsa *ll, size_t *n);

/* Firma un .apk (un zip ya alineado, sin firmar) con el esquema v2 de
   Android: devuelve el .apk entero con el bloque de firma antes del
   directorio central (malloc), o NULL si el zip no es válido. */
unsigned char *apk_firmar_v2(const unsigned char *zip, size_t n, const LlaveRsa *ll, size_t *n_salida);

#endif /* LINCE_FIRMAR_H */
