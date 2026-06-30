/*
 * 🐆 Lince — Lexer
 * Análisis léxico: convierte código fuente en tokens.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

/* ─────────────────────────────────────────
   PALABRAS CLAVE
───────────────────────────────────────── */
typedef struct {
    const char *palabra;
    TipoToken   tipo;
} PalabraClave;

static PalabraClave PALABRAS_CLAVE[] = {
    { "sea",         TOK_SEA              },
    { "fijo",        TOK_FIJO             },
    { "lista",       TOK_LISTA            },
    { "diccionario", TOK_DICCIONARIO      },
    { "funcion",     TOK_FUNCION          },
    { "generador",   TOK_GENERADOR        },
    { "producir",    TOK_PRODUCIR         },
    { "devolver",    TOK_DEVOLVER         },
    { "si",          TOK_SI               },
    { "sino",        TOK_SINO             },
    { "mientras",    TOK_MIENTRAS         },
    { "hacer",       TOK_HACER            },
    { "para",        TOK_PARA             },
    { "cada",        TOK_CADA             },
    { "clase",       TOK_CLASE            },
    { "mi",          TOK_MI               },
    { "extiende",    TOK_EXTIENDE         },
    { "padre",       TOK_PADRE            },
    { "enumeracion", TOK_ENUMERACION      },
    { "interfaz",    TOK_INTERFAZ         },
    { "implementa",  TOK_IMPLEMENTA       },
    { "verdadero",   TOK_VERDADERO        },
    { "falso",       TOK_FALSO            },
    { "nulo",        TOK_NULO             },
    { "escribir",    TOK_ESCRIBIR         },
    { "intentar",    TOK_INTENTAR         },
    { "capturar",    TOK_CAPTURAR         },
    { "finalmente",  TOK_FINALMENTE       },
    { "lanzar",      TOK_LANZAR           },
    { "elegir",      TOK_ELEGIR           },
    { "caso",        TOK_CASO             },
    { "otro",        TOK_OTRO             },
    { "importar",    TOK_IMPORTAR         },
    { "val",         TOK_VAL              },
    { "ref",         TOK_REF              },
    { "numero",      TOK_TIPO_NUMERO      },
    { "texto",       TOK_TIPO_TEXTO       },
    { "logico",      TOK_TIPO_LOGICO      },
    { "y",           TOK_Y                },
    { "o",           TOK_O                },
    { "no",          TOK_NO               },
    { NULL,          TOK_EOF              },
};

static TipoToken buscar_palabra_clave(const char *palabra) {
    for (int i = 0; PALABRAS_CLAVE[i].palabra != NULL; i++) {
        if (strcmp(PALABRAS_CLAVE[i].palabra, palabra) == 0)
            return PALABRAS_CLAVE[i].tipo;
    }
    return TOK_IDENTIFICADOR;
}

/* ─────────────────────────────────────────
   NOMBRES DE TOKENS (para depuración)
───────────────────────────────────────── */
const char *nombre_token(TipoToken tipo) {
    switch (tipo) {
        case TOK_NUMERO:       return "NUMERO";
        case TOK_TEXTO:        return "TEXTO";
        case TOK_IDENTIFICADOR:return "IDENTIFICADOR";
        case TOK_SEA:          return "SEA";
        case TOK_FIJO:         return "FIJO";
        case TOK_LISTA:        return "LISTA";
        case TOK_DICCIONARIO:  return "DICCIONARIO";
        case TOK_FUNCION:      return "FUNCION";
        case TOK_GENERADOR:    return "GENERADOR";
        case TOK_PRODUCIR:     return "PRODUCIR";
        case TOK_DEVOLVER:     return "DEVOLVER";
        case TOK_SI:           return "SI";
        case TOK_SINO:         return "SINO";
        case TOK_MIENTRAS:     return "MIENTRAS";
        case TOK_HACER:        return "HACER";
        case TOK_PARA:         return "PARA";
        case TOK_CADA:         return "CADA";
        case TOK_CLASE:        return "CLASE";
        case TOK_MI:           return "MI";
        case TOK_EXTIENDE:     return "EXTIENDE";
        case TOK_PADRE:        return "PADRE";
        case TOK_ENUMERACION:  return "ENUMERACION";
        case TOK_INTERFAZ:     return "INTERFAZ";
        case TOK_IMPLEMENTA:   return "IMPLEMENTA";
        case TOK_VERDADERO:    return "VERDADERO";
        case TOK_FALSO:        return "FALSO";
        case TOK_NULO:         return "NULO";
        case TOK_ESCRIBIR:     return "ESCRIBIR";
        case TOK_INTENTAR:     return "INTENTAR";
        case TOK_CAPTURAR:     return "CAPTURAR";
        case TOK_FINALMENTE:   return "FINALMENTE";
        case TOK_LANZAR:       return "LANZAR";
        case TOK_ELEGIR:       return "ELEGIR";
        case TOK_CASO:         return "CASO";
        case TOK_OTRO:         return "OTRO";
        case TOK_IMPORTAR:     return "IMPORTAR";
        case TOK_VAL:          return "VAL";
        case TOK_REF:          return "REF";
        case TOK_TIPO_NUMERO:  return "TIPO_NUMERO";
        case TOK_TIPO_TEXTO:   return "TIPO_TEXTO";
        case TOK_TIPO_LOGICO:  return "TIPO_LOGICO";
        case TOK_TIPO_LISTA:   return "TIPO_LISTA";
        case TOK_TIPO_DICCIONARIO: return "TIPO_DICCIONARIO";
        case TOK_TIPO_NULO:    return "TIPO_NULO";
        case TOK_Y:            return "Y";
        case TOK_O:            return "O";
        case TOK_NO:           return "NO";
        case TOK_MAS:          return "MAS";
        case TOK_MENOS:        return "MENOS";
        case TOK_ASTERISCO:    return "ASTERISCO";
        case TOK_BARRA:        return "BARRA";
        case TOK_MODULO:       return "MODULO";
        case TOK_IGUAL:        return "IGUAL";
        case TOK_IGUAL_IGUAL:  return "IGUAL_IGUAL";
        case TOK_DIFERENTE:    return "DIFERENTE";
        case TOK_MENOR:        return "MENOR";
        case TOK_MAYOR:        return "MAYOR";
        case TOK_MENOR_IGUAL:  return "MENOR_IGUAL";
        case TOK_MAYOR_IGUAL:  return "MAYOR_IGUAL";
        case TOK_MAS_MAS:      return "MAS_MAS";
        case TOK_MENOS_MENOS:  return "MENOS_MENOS";
        case TOK_PAREN_IZQ:    return "PAREN_IZQ";
        case TOK_PAREN_DER:    return "PAREN_DER";
        case TOK_LLAVE_IZQ:    return "LLAVE_IZQ";
        case TOK_LLAVE_DER:    return "LLAVE_DER";
        case TOK_CORCHETE_IZQ: return "CORCHETE_IZQ";
        case TOK_CORCHETE_DER: return "CORCHETE_DER";
        case TOK_PUNTO_COMA:   return "PUNTO_COMA";
        case TOK_COMA:         return "COMA";
        case TOK_PUNTO:        return "PUNTO";
        case TOK_DOS_PUNTOS:   return "DOS_PUNTOS";
        case TOK_EOF:          return "EOF";
        default:               return "DESCONOCIDO";
    }
}

/* ─────────────────────────────────────────
   LEXER — INICIALIZACIÓN
───────────────────────────────────────── */
Lexer *lexer_crear(const char *codigo) {
    Lexer *l = malloc(sizeof(Lexer));
    l->codigo    = codigo;
    l->longitud  = strlen(codigo);
    l->pos       = 0;
    l->linea     = 1;
    l->capacidad = 256;
    l->cantidad  = 0;
    l->tokens    = malloc(sizeof(Token) * l->capacidad);
    return l;
}

void lexer_destruir(Lexer *l) {
    for (int i = 0; i < l->cantidad; i++) {
        if (l->tokens[i].valor)
            free(l->tokens[i].valor);
    }
    free(l->tokens);
    free(l);
}

/* ─────────────────────────────────────────
   LEXER — UTILIDADES INTERNAS
───────────────────────────────────────── */
static char actual(Lexer *l) {
    if (l->pos < l->longitud) return l->codigo[l->pos];
    return '\0';
}

static char siguiente(Lexer *l) {
    if (l->pos + 1 < l->longitud) return l->codigo[l->pos + 1];
    return '\0';
}

static char avanzar(Lexer *l) {
    char c = l->codigo[l->pos++];
    if (c == '\n') l->linea++;
    return c;
}

static void agregar_token(Lexer *l, TipoToken tipo, const char *valor, int linea) {
    if (l->cantidad >= l->capacidad) {
        l->capacidad *= 2;
        l->tokens = realloc(l->tokens, sizeof(Token) * l->capacidad);
    }
    Token *t = &l->tokens[l->cantidad++];
    t->tipo  = tipo;
    t->valor = valor ? strdup(valor) : NULL;
    t->linea = linea;
}

static void error_lexico(Lexer *l, char c) {
    fprintf(stderr,
        "\n❌ Error en línea %d:\n"
        "   Encontré el carácter '%c' y no sé qué hacer con él.\n"
        "   Revisa que no hayas escrito algo por error.\n\n",
        l->linea, c);
    exit(1);
}

/* ─────────────────────────────────────────
   LEXER — LECTURA DE TOKENS
───────────────────────────────────────── */
static void saltar_espacios_y_comentarios(Lexer *l) {
    while (l->pos < l->longitud) {
        char c = actual(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            avanzar(l);
        } else if (c == '#') {
            while (l->pos < l->longitud && actual(l) != '\n')
                avanzar(l);
        } else {
            break;
        }
    }
}

static void leer_numero(Lexer *l, int linea) {
    int inicio = l->pos;
    while (l->pos < l->longitud && isdigit(actual(l)))
        avanzar(l);
    if (actual(l) == '.' && isdigit(siguiente(l))) {
        avanzar(l); /* consumir punto */
        while (l->pos < l->longitud && isdigit(actual(l)))
            avanzar(l);
    }
    int longitud = l->pos - inicio;
    char *buf = malloc(longitud + 1);
    strncpy(buf, l->codigo + inicio, longitud);
    buf[longitud] = '\0';
    agregar_token(l, TOK_NUMERO, buf, linea);
    free(buf);
}

static void leer_texto(Lexer *l, int linea) {
    avanzar(l); /* consumir comilla inicial */
    char buf[4096];
    int  bi = 0;

    while (l->pos < l->longitud && actual(l) != '"') {
        if (actual(l) == '\\') {
            avanzar(l);
            char esc = avanzar(l);
            switch (esc) {
                case 'n':  buf[bi++] = '\n';   break;
                case 't':  buf[bi++] = '\t';   break;
                case '"':  buf[bi++] = '"';    break;
                case '\\': buf[bi++] = '\\';   break;
                case 'e':  buf[bi++] = '\x1b'; break;  /* ESC para colores ANSI */
                case 'r':  buf[bi++] = '\r';   break;
                default:   buf[bi++] = esc;    break;
            }
        } else {
            buf[bi++] = avanzar(l);
        }
    }

    if (actual(l) == '\0') {
        fprintf(stderr,
            "\n❌ Error en línea %d:\n"
            "   Abriste una cadena de texto con '\"' pero nunca la cerraste.\n"
            "   Añade '\"' al final del texto.\n\n", linea);
        exit(1);
    }

    avanzar(l); /* consumir comilla final */
    buf[bi] = '\0';
    agregar_token(l, TOK_TEXTO, buf, linea);
}

static void leer_identificador(Lexer *l, int linea) {
    int inicio = l->pos;
    while (l->pos < l->longitud && (isalnum(actual(l)) || actual(l) == '_'))
        avanzar(l);
    int longitud = l->pos - inicio;
    char *buf = malloc(longitud + 1);
    strncpy(buf, l->codigo + inicio, longitud);
    buf[longitud] = '\0';

    TipoToken tipo = buscar_palabra_clave(buf);
    agregar_token(l, tipo, buf, linea);
    free(buf);
}

/* ─────────────────────────────────────────
   LEXER — TOKENIZAR
───────────────────────────────────────── */
Token *lexer_tokenizar(Lexer *l, int *cantidad) {
    while (l->pos < l->longitud) {
        saltar_espacios_y_comentarios(l);
        if (l->pos >= l->longitud) break;

        int  linea = l->linea;
        char c     = actual(l);

        if (isdigit(c)) {
            leer_numero(l, linea);
            continue;
        }

        if (c == '"') {
            leer_texto(l, linea);
            continue;
        }

        if (isalpha(c) || c == '_') {
            leer_identificador(l, linea);
            continue;
        }

        /* Operadores y delimitadores */
        avanzar(l);
        switch (c) {
            case '=':
                if (actual(l) == '=') { avanzar(l); agregar_token(l, TOK_IGUAL_IGUAL, "==", linea); }
                else                  {              agregar_token(l, TOK_IGUAL,        "=",  linea); }
                break;
            case '!':
                if (actual(l) == '=') { avanzar(l); agregar_token(l, TOK_DIFERENTE, "!=", linea); }
                else                  { error_lexico(l, '!'); }
                break;
            case '<':
                if (actual(l) == '=') { avanzar(l); agregar_token(l, TOK_MENOR_IGUAL, "<=", linea); }
                else                  {              agregar_token(l, TOK_MENOR,        "<",  linea); }
                break;
            case '>':
                if (actual(l) == '=') { avanzar(l); agregar_token(l, TOK_MAYOR_IGUAL, ">=", linea); }
                else                  {              agregar_token(l, TOK_MAYOR,        ">",  linea); }
                break;
            case '+':
                if (actual(l) == '+') { avanzar(l); agregar_token(l, TOK_MAS_MAS, "++", linea); }
                else                  {              agregar_token(l, TOK_MAS,      "+",  linea); }
                break;
            case '-':
                if (actual(l) == '-') { avanzar(l); agregar_token(l, TOK_MENOS_MENOS, "--", linea); }
                else                  {              agregar_token(l, TOK_MENOS,        "-",  linea); }
                break;
            case '*': agregar_token(l, TOK_ASTERISCO,  "*", linea); break;
            case '/': agregar_token(l, TOK_BARRA,       "/", linea); break;
            case '%': agregar_token(l, TOK_MODULO,      "%", linea); break;
            case '(': agregar_token(l, TOK_PAREN_IZQ,    "(", linea); break;
            case ')': agregar_token(l, TOK_PAREN_DER,    ")", linea); break;
            case '{': agregar_token(l, TOK_LLAVE_IZQ,    "{", linea); break;
            case '}': agregar_token(l, TOK_LLAVE_DER,    "}", linea); break;
            case '[': agregar_token(l, TOK_CORCHETE_IZQ, "[", linea); break;
            case ']': agregar_token(l, TOK_CORCHETE_DER, "]", linea); break;
            case ';': agregar_token(l, TOK_PUNTO_COMA,   ";", linea); break;
            case ',': agregar_token(l, TOK_COMA,         ",", linea); break;
            case '.': agregar_token(l, TOK_PUNTO,        ".", linea); break;
            case ':': agregar_token(l, TOK_DOS_PUNTOS,   ":", linea); break;
            default:  error_lexico(l, c); break;
        }
    }

    agregar_token(l, TOK_EOF, NULL, l->linea);
    *cantidad = l->cantidad;
    return l->tokens;
}
