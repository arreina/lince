/*
 * 🐆 Lince — lexer.h
 */

#ifndef LINCE_LEXER_H
#define LINCE_LEXER_H

/* ─────────────────────────────────────────
   TIPOS DE TOKEN
───────────────────────────────────────── */
typedef enum {
    /* Literales */
    TOK_NUMERO,
    TOK_TEXTO,
    TOK_IDENTIFICADOR,

    /* Palabras clave */
    TOK_SEA,
    TOK_FIJO,
    TOK_LISTA,
    TOK_DICCIONARIO,
    TOK_FUNCION,
    TOK_GENERADOR,
    TOK_PRODUCIR,
    TOK_DEVOLVER,
    TOK_SI,
    TOK_SINO,
    TOK_MIENTRAS,
    TOK_HACER,
    TOK_PARA,
    TOK_CADA,
    TOK_CLASE,
    TOK_MI,
    TOK_EXTIENDE,
    TOK_PADRE,
    TOK_ENUMERACION,
    TOK_INTERFAZ,
    TOK_IMPLEMENTA,
    TOK_VERDADERO,
    TOK_FALSO,
    TOK_NULO,
    TOK_ESCRIBIR,

    /* Manejo de errores */
    TOK_INTENTAR,
    TOK_CAPTURAR,
    TOK_FINALMENTE,
    TOK_LANZAR,

    /* Control de flujo adicional */
    TOK_ELEGIR,
    TOK_CASO,
    TOK_OTRO,
    TOK_ROMPER,
    TOK_CONTINUAR,

    /* Módulos */
    TOK_IMPORTAR,

    /* Modificadores de parámetros */
    TOK_VAL,
    TOK_REF,

    /* Tipos */
    TOK_TIPO_NUMERO,
    TOK_TIPO_TEXTO,
    TOK_TIPO_LOGICO,
    TOK_TIPO_LISTA,
    TOK_TIPO_DICCIONARIO,
    TOK_TIPO_NULO,

    /* Operadores lógicos */
    TOK_Y,
    TOK_O,
    TOK_NO,

    /* Operadores aritméticos */
    TOK_MAS,
    TOK_MENOS,
    TOK_ASTERISCO,
    TOK_BARRA,
    TOK_MODULO,

    /* Operadores de comparación */
    TOK_IGUAL,
    TOK_IGUAL_IGUAL,
    TOK_DIFERENTE,
    TOK_MENOR,
    TOK_MAYOR,
    TOK_MENOR_IGUAL,
    TOK_MAYOR_IGUAL,

    /* Incremento/decremento */
    TOK_MAS_MAS,
    TOK_MENOS_MENOS,

    /* Asignación compuesta: += -= *= /= %= */
    TOK_MAS_IGUAL,
    TOK_MENOS_IGUAL,
    TOK_POR_IGUAL,
    TOK_ENTRE_IGUAL,
    TOK_MODULO_IGUAL,

    /* Texto con valores dentro, "Vidas: {vidas}": PLANTILLA_INICIO, TEXTO,
       expresión, TEXTO, expresión, ..., TEXTO, PLANTILLA_FIN. Siempre
       alternan, con un TEXTO (quizá vacío) en cada extremo. */
    TOK_PLANTILLA_INICIO,
    TOK_PLANTILLA_FIN,

    /* Delimitadores */
    TOK_PAREN_IZQ,
    TOK_PAREN_DER,
    TOK_LLAVE_IZQ,
    TOK_LLAVE_DER,
    TOK_CORCHETE_IZQ,   /* [ */
    TOK_CORCHETE_DER,   /* ] */
    TOK_PUNTO_COMA,
    TOK_COMA,
    TOK_PUNTO,
    TOK_DOS_PUNTOS,     /* : */
    TOK_PUNTO_OPCIONAL, /* ?. */
    TOK_NULO_O,         /* ?? */

    /* Control */
    TOK_EOF,
} TipoToken;

/* ─────────────────────────────────────────
   TOKEN
───────────────────────────────────────── */
typedef struct {
    TipoToken  tipo;
    char      *valor;
    int        linea;
    int        columna;     /* dónde empieza en su línea, contada en caracteres desde 0 */
    int        ancho;       /* cuántos caracteres ocupa (en su línea) */
    int        desde, hasta;  /* dónde empieza y acaba en el código, en bytes (lo usa formatear) */
} Token;

/* ─────────────────────────────────────────
   LEXER
───────────────────────────────────────── */
typedef struct {
    const char *codigo;
    int         longitud;
    int         pos;
    int         inicio;     /* dónde empezó el token que se lee */
    int         linea;
    Token      *tokens;
    int         cantidad;
    int         capacidad;
} Lexer;

/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
Lexer       *lexer_crear(const char *codigo);
void         lexer_destruir(Lexer *l);
Token       *lexer_tokenizar(Lexer *l, int *cantidad);
const char  *nombre_token(TipoToken tipo);

/* i-ésima palabra reservada / contextual, o NULL al acabar. */
const char  *lexer_palabra_clave(int i);
const char  *lexer_palabra_contextual(int i);

/* Para los errores de sintaxis de un fichero importado */
extern const char *lince_archivo_sintaxis;
const char *lince_de_archivo(void);
/* Si el entorno trae LINCE_COLUMNAS (lo pone el servidor de lenguaje), un error
   de sintaxis añade una última línea "@@columna C ANCHO" para que el editor
   subraye solo lo que falla. */
void lince_marcar_columna(int columna, int ancho);

#endif /* LINCE_LEXER_H */
