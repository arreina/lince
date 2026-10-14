/*
 * 🐆 Lince — lsp.h
 * Servidor de lenguaje (Language Server Protocol) por stdin/stdout:
 * 'lince lsp'. Lo usa cualquier editor con soporte LSP.
 */
#ifndef LINCE_LSP_H
#define LINCE_LSP_H

/* Atiende peticiones hasta 'exit' o fin de la entrada. Devuelve el código de salida. */
int lsp_servir(void);

/* Parsea 'texto' sin ejecutarlo. Devuelve 0 si es correcto; si no, 1 y deja
   en *linea y en 'mensaje' lo que dijo el parser. Se hace en un proceso hijo:
   el parser termina el proceso al primer error. */
int lsp_comprobar_texto(const char *texto, int *linea, char *mensaje, size_t max);

#endif
