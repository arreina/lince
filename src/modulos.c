/*
 * 🐆 Lince — modulos.c
 * Módulos estándar: matematica, texto, archivos, tiempo
 */

#ifndef _WIN32
#define _GNU_SOURCE      /* strptime, timegm */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#else
#include <windows.h>
#endif
#include "modulos.h"
#include "plataforma.h"

/* Función del entorno — declaración externa */

/* ─────────────────────────────────────────
   UTILIDADES INTERNAS
───────────────────────────────────────── */

/* Crea un valor número — necesitamos acceso a la API interna */

/* Macro para definir una función nativa en un módulo */

typedef struct {
    const char *nombre;
    FnNativa    fn;
    int         num_args; /* -1 = variadic */
} EntradaModulo;


/* ─────────────────────────────────────────
   MÓDULO: matematica
───────────────────────────────────────── */

static Valor *fn_mat_raiz(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_NUMERO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "raiz() espera un numero", 0);
        return valor_nulo();
    }
    return valor_numero(sqrt(a[0]->numero));
}

static Valor *fn_mat_potencia(Valor **a, int n) {
    (void)n;
    return valor_numero(pow(a[0]->numero, a[1]->numero));
}

static Valor *fn_mat_abs(Valor **a, int n) {
    (void)n;
    return valor_numero(fabs(a[0]->numero));
}

static Valor *fn_mat_redondear(Valor **a, int n) {
    (void)n;
    return valor_numero(round(a[0]->numero));
}

static Valor *fn_mat_piso(Valor **a, int n) {
    (void)n;
    return valor_numero(floor(a[0]->numero));
}

static Valor *fn_mat_techo(Valor **a, int n) {
    (void)n;
    return valor_numero(ceil(a[0]->numero));
}

static Valor *fn_mat_seno(Valor **a, int n) {
    (void)n;
    return valor_numero(sin(a[0]->numero));
}

static Valor *fn_mat_coseno(Valor **a, int n) {
    (void)n;
    return valor_numero(cos(a[0]->numero));
}

static Valor *fn_mat_tangente(Valor **a, int n) {
    (void)n;
    return valor_numero(tan(a[0]->numero));
}

static Valor *fn_mat_logaritmo(Valor **a, int n) {
    (void)n;
    if (a[0]->numero <= 0) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorMatematico",
            "logaritmo() requiere un número positivo", 0);
        return valor_nulo();
    }
    return valor_numero(log(a[0]->numero));
}

static Valor *fn_mat_maximo(Valor **a, int n) {
    (void)n;
    return valor_numero(a[0]->numero > a[1]->numero ? a[0]->numero : a[1]->numero);
}

static Valor *fn_mat_minimo(Valor **a, int n) {
    (void)n;
    return valor_numero(a[0]->numero < a[1]->numero ? a[0]->numero : a[1]->numero);
}

static Valor *fn_mat_aleatorio(Valor **a, int n) {
    if (n == 0) {
        return valor_numero((double)rand() / RAND_MAX);
    }
    int min = (int)a[0]->numero;
    int max = (int)a[1]->numero;
    return valor_numero(min + rand() % (max - min + 1));
}

static Valor *fn_mat_truncar(Valor **a, int n) {
    (void)n;
    return valor_numero(trunc(a[0]->numero));
}

static Valor *fn_mat_es_entero(Valor **a, int n) {
    (void)n;
    return valor_booleano(a[0]->numero == trunc(a[0]->numero));
}

static Valor *fn_mat_factorial(Valor **a, int n) {
    (void)n;
    long long x = (long long)a[0]->numero;
    if (x < 0) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorMatematico",
            "factorial() no acepta números negativos.", 0);
        return valor_nulo();
    }
    long long r = 1;
    for (long long i = 2; i <= x; i++) r *= i;
    return valor_numero((double)r);
}

static Valor *fn_mat_combinaciones(Valor **a, int n) {
    (void)n;
    long long nv = (long long)a[0]->numero;
    long long k  = (long long)a[1]->numero;
    if (k < 0 || k > nv) return valor_numero(0);
    if (k == 0 || k == nv) return valor_numero(1);
    if (k > nv - k) k = nv - k;
    long long r = 1;
    for (long long i = 0; i < k; i++) {
        r = r * (nv - i) / (i + 1);
    }
    return valor_numero((double)r);
}

static Valor *fn_mat_log2(Valor **a, int n) {
    (void)n;
    if (a[0]->numero <= 0) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorMatematico",
            "log2() solo acepta números positivos.", 0);
        return valor_nulo();
    }
    return valor_numero(log2(a[0]->numero));
}

static Valor *fn_mat_log10(Valor **a, int n) {
    (void)n;
    if (a[0]->numero <= 0) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorMatematico",
            "log10() solo acepta números positivos.", 0);
        return valor_nulo();
    }
    return valor_numero(log10(a[0]->numero));
}

/* ─────────────────────────────────────────
   MÓDULO: texto
───────────────────────────────────────── */

static Valor *fn_txt_dividir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "dividir() espera dos textos", 0);
        return valor_nulo();
    }
    const char *src = a[0]->texto;
    const char *sep = a[1]->texto;
    size_t sep_len  = strlen(sep);
    Valor *lista    = valor_lista_crear();

    if (sep_len == 0) {
        for (int i = 0; src[i]; i++) {
            char tmp[2] = { src[i], '\0' };
            lista_agregar(lista, valor_texto(tmp));
        }
        return lista;
    }

    const char *pos = src;
    while (1) {
        const char *found = strstr(pos, sep);
        if (!found) {
            lista_agregar(lista, valor_texto(pos));
            break;
        }
        size_t len = found - pos;
        char *part = malloc(len + 1);
        strncpy(part, pos, len);
        part[len] = '\0';
        lista_agregar(lista, valor_texto(part));
        free(part);
        pos = found + sep_len;
    }
    return lista;
}

static Valor *fn_txt_unir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_LISTA || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "unir() espera una lista y un texto separador", 0);
        return valor_nulo();
    }
    Valor *lista    = a[0];
    const char *sep = a[1]->texto;
    char buf[8192]  = "";

    for (int i = 0; i < lista->lista.cantidad; i++) {
        if (lista->lista.elementos[i]->tipo == VAL_TEXTO)
            strncat(buf, lista->lista.elementos[i]->texto, sizeof(buf)-strlen(buf)-1);
        if (i < lista->lista.cantidad - 1)
            strncat(buf, sep, sizeof(buf)-strlen(buf)-1);
    }
    return valor_texto(buf);
}

static Valor *fn_txt_repetir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_NUMERO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "repetir() espera un texto y un numero", 0);
        return valor_nulo();
    }
    const char *s = a[0]->texto;
    int veces     = (int)a[1]->numero;
    char buf[8192] = "";
    for (int i = 0; i < veces; i++)
        strncat(buf, s, sizeof(buf)-strlen(buf)-1);
    return valor_texto(buf);
}

static Valor *fn_txt_invertir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "invertir() espera un texto", 0);
        return valor_nulo();
    }
    /* Se invierte por caracteres: dar la vuelta a los bytes partiría
       las vocales acentuadas y la ñ, que ocupan dos. */
    const char *src = a[0]->texto;
    size_t bytes = strlen(src);
    char *s = malloc(bytes + 1);
    size_t destino = bytes;
    size_t i = 0;
    while (src[i]) {
        int tam = utf8_tam(src + i);
        destino -= (size_t)tam;
        memcpy(s + destino, src + i, (size_t)tam);
        i += (size_t)tam;
    }
    s[bytes] = '\0';
    Valor *r = valor_texto(s);
    free(s);
    return r;
}

static Valor *fn_txt_formato(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_LISTA) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "formato() espera un texto y una lista de argumentos", 0);
        return valor_nulo();
    }
    const char *fmt = a[0]->texto;
    Valor *args     = a[1];
    char buf[8192]  = "";
    int  arg_idx    = 0;
    int  bi         = 0;

    for (int i = 0; fmt[i] && bi < 8190; i++) {
        if (fmt[i] == '{' && fmt[i+1] == '}' && arg_idx < args->lista.cantidad) {
            Valor *v = args->lista.elementos[arg_idx++];
            if (v->tipo == VAL_NUMERO) {
                if (v->numero == (long long)v->numero)
                    bi += snprintf(buf+bi, sizeof(buf)-bi, "%lld", (long long)v->numero);
                else
                    bi += snprintf(buf+bi, sizeof(buf)-bi, "%g", v->numero);
            } else if (v->tipo == VAL_TEXTO) {
                bi += snprintf(buf+bi, sizeof(buf)-bi, "%s", v->texto);
            } else if (v->tipo == VAL_BOOLEANO) {
                bi += snprintf(buf+bi, sizeof(buf)-bi, "%s", v->booleano ? "verdadero" : "falso");
            }
            i++; /* saltar '}' */
        } else {
            buf[bi++] = fmt[i];
        }
    }
    buf[bi] = '\0';
    return valor_texto(buf);
}

static Valor *fn_txt_a_numero(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "a_numero() espera un texto", 0);
        return valor_nulo();
    }
    char *fin;
    double d = strtod(a[0]->texto, &fin);
    if (fin == a[0]->texto) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "No se puede convertir ese texto a número", 0);
        return valor_nulo();
    }
    return valor_numero(d);
}

static Valor *fn_txt_de_numero(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_NUMERO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "de_numero() espera un numero", 0);
        return valor_nulo();
    }
    char buf[64];
    if (a[0]->numero == (long long)a[0]->numero)
        snprintf(buf, sizeof(buf), "%lld", (long long)a[0]->numero);
    else
        snprintf(buf, sizeof(buf), "%g", a[0]->numero);
    return valor_texto(buf);
}

static Valor *fn_txt_a_lista(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "a_lista() espera un texto", 0);
        return valor_nulo();
    }
    Valor *lista = valor_lista_crear();
    /* Un elemento por carácter, no por byte. */
    const char *s = a[0]->texto;
    for (int i = 0; s[i]; ) {
        int tam = utf8_tam(s + i);
        char tmp[5] = {0};
        for (int k = 0; k < tam && k < 4; k++) tmp[k] = s[i + k];
        i += tam;
        lista_agregar(lista, valor_texto(tmp));
    }
    return lista;
}

static Valor *fn_txt_posicion(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "posicion() espera dos textos", 0);
        return valor_nulo();
    }
    const char *found = strstr(a[0]->texto, a[1]->texto);
    if (!found) return valor_numero(-1);
    /* Se devuelve la posición en caracteres, para que sirva como índice
       con a[0][pos] y encaje con longitud(). */
    int chars = 0;
    for (const char *p = a[0]->texto; p < found; p += utf8_tam(p)) chars++;
    return valor_numero(chars);
}

static Valor *fn_txt_contar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "contar() espera dos textos", 0);
        return valor_nulo();
    }
    const char *src = a[0]->texto;
    const char *pat = a[1]->texto;
    size_t pat_len  = strlen(pat);
    int    count    = 0;
    if (pat_len == 0) return valor_numero(0);
    const char *pos = src;
    while ((pos = strstr(pos, pat)) != NULL) { count++; pos += pat_len; }
    return valor_numero(count);
}

static Valor *fn_txt_extraer(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_NUMERO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "extraer() espera texto, inicio y fin", 0);
        return valor_nulo();
    }
    /* Inicio y fin van en caracteres; se traducen a bytes para cortar
       sin partir una letra por la mitad. */
    int len   = utf8_longitud(a[0]->texto);
    int ini   = (int)a[1]->numero;
    int fin   = (int)a[2]->numero;
    if (ini < 0) ini = 0;
    if (fin > len) fin = len;
    if (ini >= fin) return valor_texto("");
    int bini = utf8_desplazamiento(a[0]->texto, ini);
    int bfin = utf8_desplazamiento(a[0]->texto, fin);
    if (bini < 0) return valor_texto("");
    if (bfin < 0) bfin = (int)strlen(a[0]->texto);
    int sz = bfin - bini;
    char *buf = malloc(sz + 1);
    memcpy(buf, a[0]->texto + bini, sz);
    buf[sz] = '\0';
    Valor *r = valor_texto(buf);
    free(buf);
    return r;
}

static Valor *fn_txt_es_numero(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
    char *fin;
    strtod(a[0]->texto, &fin);
    return valor_booleano(fin != a[0]->texto && *fin == '\0');
}

static Valor *fn_txt_es_letra(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
    /* Las vocales acentuadas y la ñ también son letras. */
    for (int i = 0; a[0]->texto[i]; ) {
        if (!utf8_es_letra(a[0]->texto + i)) return valor_booleano(0);
        i += utf8_tam(a[0]->texto + i);
    }
    return valor_booleano(strlen(a[0]->texto) > 0);
}

static Valor *fn_txt_es_vacio(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
    return valor_booleano(strlen(a[0]->texto) == 0);
}

static Valor *fn_txt_empieza_con(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) return valor_booleano(0);
    return valor_booleano(strncmp(a[0]->texto, a[1]->texto, strlen(a[1]->texto)) == 0);
}

static Valor *fn_txt_termina_con(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) return valor_booleano(0);
    size_t ls = strlen(a[0]->texto), lp = strlen(a[1]->texto);
    if (lp > ls) return valor_booleano(0);
    return valor_booleano(strcmp(a[0]->texto + ls - lp, a[1]->texto) == 0);
}

static Valor *fn_txt_rellenar_izq(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "rellenar_izq() espera texto, ancho y caracter de relleno", 0);
        return valor_nulo();
    }
    int ancho = (int)a[1]->numero;
    const char *s = a[0]->texto, *r = a[2]->texto;
    /* El ancho se mide en caracteres, para que las columnas cuadren
       aunque el texto lleve tildes. */
    int len = utf8_longitud(s);
    if (len >= ancho) return valor_texto(s);
    int pad = ancho - len;
    size_t bytes = strlen(s);
    char *buf = malloc((size_t)pad + bytes + 1);
    for (int i = 0; i < pad; i++) buf[i] = r[0];
    strcpy(buf + pad, s);
    Valor *v = valor_texto(buf); free(buf);
    return v;
}

static Valor *fn_txt_rellenar_der(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "rellenar_der() espera texto, ancho y caracter de relleno", 0);
        return valor_nulo();
    }
    int ancho = (int)a[1]->numero;
    const char *s = a[0]->texto, *r = a[2]->texto;
    int len = utf8_longitud(s);           /* ancho en caracteres */
    if (len >= ancho) return valor_texto(s);
    size_t bytes = strlen(s);
    int pad = ancho - len;
    char *buf = malloc(bytes + (size_t)pad + 1);
    memcpy(buf, s, bytes);
    for (int i = 0; i < pad; i++) buf[bytes + (size_t)i] = r[0];
    buf[bytes + (size_t)pad] = '\0';
    Valor *v = valor_texto(buf); free(buf);
    return v;
}

static Valor *fn_txt_centrar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "centrar() espera texto, ancho y caracter de relleno", 0);
        return valor_nulo();
    }
    int ancho = (int)a[1]->numero;
    const char *s = a[0]->texto, *r = a[2]->texto;
    int len = utf8_longitud(s);           /* ancho en caracteres */
    if (len >= ancho) return valor_texto(s);
    size_t bytes = strlen(s);
    int pad_total = ancho - len;
    int pad_izq   = pad_total / 2;
    int pad_der   = pad_total - pad_izq;
    char *buf = malloc(bytes + (size_t)pad_total + 1);
    for (int i = 0; i < pad_izq; i++) buf[i] = r[0];
    memcpy(buf + pad_izq, s, bytes);
    for (int i = 0; i < pad_der; i++) buf[(size_t)pad_izq + bytes + (size_t)i] = r[0];
    buf[(size_t)pad_izq + bytes + (size_t)pad_der] = '\0';
    Valor *v = valor_texto(buf); free(buf);
    return v;
}

/* ─────────────────────────────────────────
   MÓDULO: archivos
───────────────────────────────────────── */

static Valor *fn_arc_leer(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "leer() espera una ruta de archivo", 0);
        return valor_nulo();
    }
    FILE *f = fopen(a[0]->texto, "r");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "No se pudo abrir el archivo '%s'", a[0]->texto);
        hay_error = 1;
        valor_error = valor_error_nuevo("Error", msg, 0);
        return valor_nulo();
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    rewind(f);
    char *buf = malloc(tam + 1);
    size_t leido = fread(buf, 1, tam, f);
    buf[leido] = '\0';
    fclose(f);
    Valor *v = valor_texto(buf);
    free(buf);
    return v;
}

static Valor *fn_arc_escribir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "escribir() espera ruta y contenido", 0);
        return valor_nulo();
    }
    FILE *f = fopen(a[0]->texto, "w");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "No se pudo escribir en '%s'", a[0]->texto);
        hay_error = 1;
        valor_error = valor_error_nuevo("Error", msg, 0);
        return valor_nulo();
    }
    fputs(a[1]->texto, f);
    fclose(f);
    return valor_nulo();
}

static Valor *fn_arc_agregar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "agregar() espera ruta y contenido", 0);
        return valor_nulo();
    }
    FILE *f = fopen(a[0]->texto, "a");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "No se pudo abrir '%s'", a[0]->texto);
        hay_error = 1;
        valor_error = valor_error_nuevo("Error", msg, 0);
        return valor_nulo();
    }
    fputs(a[1]->texto, f);
    fclose(f);
    return valor_nulo();
}

static Valor *fn_arc_existe(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
    FILE *f = fopen(a[0]->texto, "r");
    if (f) { fclose(f); return valor_booleano(1); }
    return valor_booleano(0);
}

static Valor *fn_arc_eliminar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
    return valor_booleano(remove(a[0]->texto) == 0);
}

static Valor *fn_arc_crear_directorio(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
#ifdef LINCE_WINDOWS
    return valor_booleano(_mkdir(a[0]->texto) == 0);
#else
    #include <sys/stat.h>
    return valor_booleano(mkdir(a[0]->texto, 0755) == 0);
#endif
}

static Valor *fn_arc_es_directorio(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
#ifdef LINCE_WINDOWS
    DWORD attr = GetFileAttributesA(a[0]->texto);
    return valor_booleano(attr != INVALID_FILE_ATTRIBUTES &&
                          (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    if (stat(a[0]->texto, &st) == 0)
        return valor_booleano(S_ISDIR(st.st_mode));
    return valor_booleano(0);
#endif
}

static Valor *fn_arc_copiar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO)
        return valor_booleano(0);
    FILE *src = fopen(a[0]->texto, "rb");
    if (!src) return valor_booleano(0);
    FILE *dst = fopen(a[1]->texto, "wb");
    if (!dst) { fclose(src); return valor_booleano(0); }
    char buf[4096];
    size_t leido;
    while ((leido = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, leido, dst);
    fclose(src);
    fclose(dst);
    return valor_booleano(1);
}

static Valor *fn_arc_mover(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO)
        return valor_booleano(0);
    return valor_booleano(rename(a[0]->texto, a[1]->texto) == 0);
}

static Valor *fn_arc_listar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_lista_crear();
    char cmd[1024];
#ifdef LINCE_WINDOWS
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\" 2>nul", a[0]->texto);
#else
    snprintf(cmd, sizeof(cmd), "ls \"%s\" 2>/dev/null", a[0]->texto);
#endif
    FILE *proc = popen(cmd, "r");
    if (!proc) return valor_lista_crear();
    Valor *lista = valor_lista_crear();
    char linea[1024];
    while (fgets(linea, sizeof(linea), proc)) {
        linea[strcspn(linea, "\n\r")] = '\0';
        if (strlen(linea) > 0)
            lista_agregar(lista, valor_texto(linea));
    }
    pclose(proc);
    return lista;
}

/* ─────────────────────────────────────────
   MÓDULO: tiempo
───────────────────────────────────────── */

static Valor *fn_tpo_ahora(Valor **a, int n) {
    (void)a; (void)n;
    return valor_numero((double)time(NULL));
}

static Valor *fn_tpo_esperar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_NUMERO) return valor_nulo();
    lince_esperar_ms((int)a[0]->numero);
    return valor_nulo();
}

static Valor *fn_tpo_fecha(Valor **a, int n) {
    (void)a; (void)n;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return valor_texto(buf);
}

/* ── Fechas: formatear, descomponer, crear y analizar ──
   Las marcas de tiempo son segundos desde 1970 (como tiempo.ahora()). Cada
   función tiene su versión local y su versión _utc. */
#ifdef _WIN32
#define lince_timegm _mkgmtime
#else
#define lince_timegm timegm
#endif

static int tpo_num(Valor **a, int n, int i, double *out) {
    if (i >= n || a[i]->tipo != VAL_NUMERO) return 0;
    *out = a[i]->numero;
    return 1;
}

static Valor *tpo_formatear(Valor **a, int n, int utc) {
    double ts;
    if (!tpo_num(a, n, 0, &ts) || n < 2 || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "formatear() espera (marca de tiempo, formato)", 0);
        return valor_nulo();
    }
    time_t t = (time_t)ts;
    struct tm tmv;
    if (utc) { struct tm *p = gmtime(&t); if (p) tmv = *p; else memset(&tmv, 0, sizeof tmv); }
    else     { struct tm *p = localtime(&t); if (p) tmv = *p; else memset(&tmv, 0, sizeof tmv); }
    char buf[512];
    size_t k = strftime(buf, sizeof(buf), a[1]->texto, &tmv);
    buf[k] = '\0';
    return valor_texto(buf);
}
static Valor *fn_tpo_formatear(Valor **a, int n)     { return tpo_formatear(a, n, 0); }
static Valor *fn_tpo_formatear_utc(Valor **a, int n) { return tpo_formatear(a, n, 1); }

static Valor *tpo_partes(Valor **a, int n, int utc) {
    double ts;
    if (!tpo_num(a, n, 0, &ts)) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "partes() espera una marca de tiempo", 0);
        return valor_nulo();
    }
    time_t t = (time_t)ts;
    struct tm *p = utc ? gmtime(&t) : localtime(&t);
    Valor *d = valor_diccionario_crear();
    if (!p) return d;
    valor_diccionario_agregar(d, "anio",       valor_numero(p->tm_year + 1900));
    valor_diccionario_agregar(d, "mes",        valor_numero(p->tm_mon + 1));
    valor_diccionario_agregar(d, "dia",        valor_numero(p->tm_mday));
    valor_diccionario_agregar(d, "hora",       valor_numero(p->tm_hour));
    valor_diccionario_agregar(d, "minuto",     valor_numero(p->tm_min));
    valor_diccionario_agregar(d, "segundo",    valor_numero(p->tm_sec));
    valor_diccionario_agregar(d, "dia_semana", valor_numero(p->tm_wday));   /* 0 = domingo */
    valor_diccionario_agregar(d, "dia_anio",   valor_numero(p->tm_yday + 1));
    return d;
}
static Valor *fn_tpo_partes(Valor **a, int n)     { return tpo_partes(a, n, 0); }
static Valor *fn_tpo_partes_utc(Valor **a, int n) { return tpo_partes(a, n, 1); }

static Valor *tpo_crear(Valor **a, int n, int utc) {
    double v[6] = {0, 1, 1, 0, 0, 0};
    if (n < 3 || n > 6) goto mal;
    for (int i = 0; i < n; i++) if (!tpo_num(a, n, i, &v[i])) goto mal;
    struct tm tmv;
    memset(&tmv, 0, sizeof tmv);
    tmv.tm_year = (int)v[0] - 1900; tmv.tm_mon = (int)v[1] - 1; tmv.tm_mday = (int)v[2];
    tmv.tm_hour = (int)v[3]; tmv.tm_min = (int)v[4]; tmv.tm_sec = (int)v[5];
    tmv.tm_isdst = -1;
    return valor_numero((double)(utc ? lince_timegm(&tmv) : mktime(&tmv)));
mal:
    hay_error = 1;
    valor_error = valor_error_nuevo("ErrorTipo", "crear() espera (anio, mes, dia[, hora, minuto, segundo])", 0);
    return valor_nulo();
}
static Valor *fn_tpo_crear(Valor **a, int n)     { return tpo_crear(a, n, 0); }
static Valor *fn_tpo_crear_utc(Valor **a, int n) { return tpo_crear(a, n, 1); }

static Valor *tpo_analizar(Valor **a, int n, int utc) {
    if (n < 2 || a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "analizar() espera (texto, formato)", 0);
        return valor_nulo();
    }
#ifdef _WIN32
    (void)utc;
    hay_error = 1;
    valor_error = valor_error_nuevo("Error", "analizar() no está disponible en Windows", 0);
    return valor_nulo();
#else
    struct tm tmv;
    memset(&tmv, 0, sizeof tmv);
    tmv.tm_isdst = -1;
    char *fin = strptime(a[0]->texto, a[1]->texto, &tmv);
    if (!fin || *fin) return valor_nulo();          /* no encaja con el formato */
    return valor_numero((double)(utc ? lince_timegm(&tmv) : mktime(&tmv)));
#endif
}
static Valor *fn_tpo_analizar(Valor **a, int n)     { return tpo_analizar(a, n, 0); }
static Valor *fn_tpo_analizar_utc(Valor **a, int n) { return tpo_analizar(a, n, 1); }

static Valor *fn_tpo_hora(Valor **a, int n) {
    (void)a; (void)n;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return valor_texto(buf);
}

/* ─────────────────────────────────────────
   MÓDULO: servidor — HTTP con rutas dinámicas, cookies y concurrencia
───────────────────────────────────────── */
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#endif

/* Antes había aquí un envoltorio que además del manejador le pasaba a
   interprete_llamar_funcion el Entorno en el que ejecutarlo, y de ese
   parámetro colgaba todo el tinglado de _srv_ent_tmp / _srv_entorno_global /
   SrvRuta.entorno_fn. Nunca sirvió de nada: una función Lince se ejecuta
   siempre en su propio closure, así que ese Entorno no llegaba a leerse. */
#define SRV_MAX_RUTAS 64
typedef struct {
    char    metodo[8];
    char    ruta[256];
    Valor  *manejador;
} SrvRuta;

static SrvRuta   _srv_rutas[SRV_MAX_RUTAS];
static int       _srv_nrutas = 0;
static char      _srv_dir_estaticos[256] = "";

/* Forward declaration */
static Valor *_srv_llamar_manejador(Valor *fn,
                                     const char *metodo, const char *ruta,
                                     const char *cuerpo, Valor *consulta,
                                     Valor *params, Valor *cookies);

/* ── Utilidades ────────────────────────── */

static void _srv_url_decode(const char *src, char *dst, int dstlen) {
    int i = 0;
    while (*src && i < dstlen - 1) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            dst[i++] = (*src == '+') ? ' ' : *src;
            src++;
        }
    }
    dst[i] = '\0';
}

/* "a=1&b=2" → diccionario Lince */
static Valor *_srv_parsear_query(const char *qs) {
    Valor *dic = valor_diccionario_crear();
    if (!qs || !*qs) return dic;
    char buf[2048];
    strncpy(buf, qs, sizeof(buf) - 1);
    buf[sizeof(buf)-1] = '\0';
    char *p = buf;
    while (p && *p) {
        char *amp = strchr(p, '&');
        if (amp) *amp = '\0';
        char *eq = strchr(p, '=');
        if (eq) {
            *eq = '\0';
            char key[256], val[512];
            _srv_url_decode(p,    key, sizeof(key));
            _srv_url_decode(eq+1, val, sizeof(val));
            valor_diccionario_agregar(dic, key, valor_texto(val));
        }
        p = amp ? amp + 1 : NULL;
    }
    return dic;
}

/* Cabecera "Cookie: n=v; n2=v2" → diccionario Lince */
static Valor *_srv_parsear_cookies(const char *buf) {
    Valor *dic = valor_diccionario_crear();
    const char *hdr = strstr(buf, "\r\nCookie:");
    if (!hdr) hdr = strstr(buf, "\r\ncookie:");
    if (!hdr) return dic;
    hdr += 9;  /* saltar "\r\nCookie:" */
    while (*hdr == ' ') hdr++;
    const char *fin = strstr(hdr, "\r\n");
    char linea[2048] = "";
    if (fin) { size_t len = (size_t)(fin - hdr); if (len >= sizeof(linea)) len = sizeof(linea)-1; strncpy(linea, hdr, len); }
    else strncpy(linea, hdr, sizeof(linea)-1);

    char *p = linea;
    while (p && *p) {
        while (*p == ' ') p++;
        char *sc = strchr(p, ';');
        if (sc) *sc = '\0';
        char *eq = strchr(p, '=');
        if (eq) {
            *eq = '\0';
            char *nombre = p, *valor = eq + 1;
            while (*nombre == ' ') nombre++;
            valor_diccionario_agregar(dic, nombre, valor_texto(valor));
        }
        p = sc ? sc + 1 : NULL;
    }
    return dic;
}

/* Coincide "/api/items/:id/detalle" con "/api/items/42/detalle".
   Devuelve 1 si hay coincidencia y rellena params con los capturas. */
static int _srv_coincidir_ruta(const char *patron, const char *ruta, Valor *params) {
    /* Copia mutables para tokenizar */
    char pat[256], rut[256];
    strncpy(pat, patron, 255); pat[255] = '\0';
    strncpy(rut, ruta,   255); rut[255] = '\0';

    char *pseg[32], *rseg[32];
    int np = 0, nr = 0;

    /* Dividir por '/' */
    char *tok = strtok(pat, "/");
    while (tok && np < 32) { pseg[np++] = tok; tok = strtok(NULL, "/"); }
    tok = strtok(rut, "/");
    while (tok && nr < 32) { rseg[nr++] = tok; tok = strtok(NULL, "/"); }

    if (np != nr) return 0;

    for (int i = 0; i < np; i++) {
        if (pseg[i][0] == ':') {
            /* Segmento paramétrico — capturar */
            valor_diccionario_agregar(params, pseg[i] + 1, valor_texto(rseg[i]));
        } else if (strcmp(pseg[i], rseg[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

/* ── Parsear petición HTTP ──────────────── */

static void _srv_parsear_peticion(const char *buf, char *metodo,
                                   char *ruta, char *query, char *cuerpo) {
    metodo[0] = ruta[0] = query[0] = cuerpo[0] = '\0';
    const char *p = buf;
    int i = 0;
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && i < 7)
        metodo[i++] = *p++;
    metodo[i] = '\0';
    if (*p == ' ') p++;
    i = 0;
    while (*p && *p != ' ' && *p != '?' && *p != '\r' && *p != '\n' && i < 255)
        ruta[i++] = *p++;
    ruta[i] = '\0';
    if (*p == '?') {
        p++; i = 0;
        while (*p && *p != ' ' && *p != '\r' && *p != '\n' && i < 1023)
            query[i++] = *p++;
        query[i] = '\0';
    }
    const char *sep = strstr(buf, "\r\n\r\n");
    if (sep) strncpy(cuerpo, sep + 4, 65535);
}

/* ── MIME ───────────────────────────────── */

static const char *_srv_mime(const char *ext) {
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html")==0 || strcmp(ext, ".htm")==0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css")==0)  return "text/css";
    if (strcmp(ext, ".js")==0)   return "application/javascript";
    if (strcmp(ext, ".json")==0) return "application/json";
    if (strcmp(ext, ".png")==0)  return "image/png";
    if (strcmp(ext, ".jpg")==0)  return "image/jpeg";
    if (strcmp(ext, ".svg")==0)  return "image/svg+xml";
    if (strcmp(ext, ".ico")==0)  return "image/x-icon";
    if (strcmp(ext, ".jpeg")==0) return "image/jpeg";
    if (strcmp(ext, ".gif")==0)  return "image/gif";
    if (strcmp(ext, ".webp")==0) return "image/webp";
    if (strcmp(ext, ".woff2")==0) return "font/woff2";
    if (strcmp(ext, ".txt")==0)  return "text/plain; charset=utf-8";
    return "text/plain";
}

/* ── Enviar respuesta HTTP ──────────────── */

static char _srv_loc[1024] = "";   /* cabecera Location de la respuesta en curso */
static int _srv_cors = 1;   /* cabecera Access-Control-Allow-Origin: * (la quita 'ventana') */

static void _srv_enviar_respuesta_n(int fd, int codigo, const char *tipo,
                                    const char *cuerpo, size_t cuerpo_len, Valor *cookies) {
    const char *estado = "OK";
    if (codigo == 201) estado = "Created";
    else if (codigo == 204) estado = "No Content";
    else if (codigo == 302) estado = "Found";
    else if (codigo == 400) estado = "Bad Request";
    else if (codigo == 401) estado = "Unauthorized";
    else if (codigo == 403) estado = "Forbidden";
    else if (codigo == 404) estado = "Not Found";
    else if (codigo == 405) estado = "Method Not Allowed";
    else if (codigo == 500) estado = "Internal Server Error";

    if (!tipo || strpbrk(tipo, "\r\n")) tipo = "text/plain";
    const char *cors = _srv_cors ? "Access-Control-Allow-Origin: *\r\n" : "";

    /* Construir cabeceras base. Si no cabe entero no se puede enviar
       truncado: cortar a media línea dejaría la respuesta sin el fin de
       cabeceras y el cliente leería el cuerpo como si fuera una más. En
       ese caso se recurre a un Content-Type genérico, que siempre cabe. */
    char cab[2048];
    int clen = snprintf(cab, sizeof(cab),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "%s",
        codigo, estado, tipo, cuerpo_len, cors);
    if (clen < 0 || (size_t)clen >= sizeof(cab)) {
        fprintf(stderr, "⚠  servidor: cabeceras demasiado largas "
                        "(¿'tipo' excesivo?), se usa text/plain\n");
        clen = snprintf(cab, sizeof(cab),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "%s",
            codigo, estado, cuerpo_len, cors);
        if (clen < 0) return;
    }

    send(fd, cab, (size_t)clen, 0);

    if (_srv_loc[0] && !strpbrk(_srv_loc, "\r\n")) {
        char loc[1100];
        int ll = snprintf(loc, sizeof loc, "Location: %s\r\n", _srv_loc);
        if (ll > 0 && (size_t)ll < sizeof loc) send(fd, loc, (size_t)ll, 0);
    }

    /* Set-Cookie por cada entrada del diccionario cookies */
    if (cookies && cookies->tipo == VAL_DICCIONARIO) {
        for (int i = 0; i < cookies->diccionario.cantidad; i++) {
            const char *nombre = cookies->diccionario.claves[i];
            const char *valor  = cookies->diccionario.valores[i]->tipo == VAL_TEXTO
                    ? cookies->diccionario.valores[i]->texto : "";
            /* Evitar inyección/división de cabeceras: descartar CR/LF */
            if (strpbrk(nombre, "\r\n") || strpbrk(valor, "\r\n")) continue;

            char sc[512];
            int sclen = snprintf(sc, sizeof(sc),
                "Set-Cookie: %s=%s; Path=/; HttpOnly\r\n", nombre, valor);
            if (sclen < 0) continue;
            /* Si no cabe se descarta la cookie entera: enviarla truncada
               se comería el \r\n final y dejaría la respuesta malformada. */
            if ((size_t)sclen >= sizeof(sc)) {
                fprintf(stderr, "⚠  servidor: cookie '%s' demasiado larga "
                                "(máx. %zu bytes), se descarta\n",
                        nombre, sizeof(sc) - 1);
                continue;
            }
            send(fd, sc, (size_t)sclen, 0);
        }
    }

    send(fd, "\r\n", 2, 0);
    if (cuerpo && cuerpo_len > 0) send(fd, cuerpo, cuerpo_len, 0);
}

static void _srv_enviar_respuesta(int fd, int codigo, const char *tipo,
                                  const char *cuerpo, Valor *cookies) {
    _srv_enviar_respuesta_n(fd, codigo, tipo, cuerpo, cuerpo ? strlen(cuerpo) : 0, cookies);
}

/* ── Manejar conexión ───────────────────── */

static void _srv_manejar_conexion(int fd) {
    char buf[65536] = "";
    ssize_t n = recv(fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(fd); return; }
    buf[n] = '\0';

    char metodo[8], ruta[256], query_raw[1024], cuerpo[65536];
    _srv_parsear_peticion(buf, metodo, ruta, query_raw, cuerpo);

    Valor *consulta = _srv_parsear_query(query_raw);
    Valor *cookies  = _srv_parsear_cookies(buf);

    /* Buscar ruta registrada (con soporte de parámetros). Las rutas de la
       API tienen prioridad sobre los archivos estáticos, así que se
       intenta el despacho primero; solo si ninguna ruta coincide se
       recurre a servir un archivo estático (para GET) o al 404. */
    for (int i = 0; i < _srv_nrutas; i++) {
        if (_srv_rutas[i].metodo[0] != '\0' &&
            strcmp(_srv_rutas[i].metodo, metodo) != 0) continue;

        Valor *params = valor_diccionario_crear();
        int coincide = _srv_coincidir_ruta(_srv_rutas[i].ruta, ruta, params);
        if (!coincide) { valor_destruir(params); continue; }

        Valor *resp = _srv_llamar_manejador(
            _srv_rutas[i].manejador,
            metodo, ruta, cuerpo, consulta, params, cookies);

        valor_destruir(params);

        Valor *cookies_resp = NULL;
        if (hay_error) {
            /* El manejador falló: se cuenta en la consola del servidor y el
               cliente recibe un 500, no una página vacía como si fuera bien. */
            if (valor_error && valor_error->tipo == VAL_ERROR)
                fprintf(stderr, "❌ %s %s: %s %s\n", metodo, ruta,
                        valor_error->error->tipo, valor_error->error->mensaje);
            hay_error = 0;
            _srv_enviar_respuesta(fd, 500, "text/html; charset=utf-8",
                "<h1>500 — Error interno</h1><p>El servidor no pudo atender la petición.</p>", NULL);
        } else if (resp && resp->tipo == VAL_DICCIONARIO) {
            int cod = 200;
            const char *tipo_r = "text/html; charset=utf-8";
            char *cuerpo_r = "";
            for (int j = 0; j < resp->diccionario.cantidad; j++) {
                if (strcmp(resp->diccionario.claves[j], "codigo") == 0 &&
                    resp->diccionario.valores[j]->tipo == VAL_NUMERO)
                    cod = (int)resp->diccionario.valores[j]->numero;
                if (strcmp(resp->diccionario.claves[j], "tipo") == 0 &&
                    resp->diccionario.valores[j]->tipo == VAL_TEXTO)
                    tipo_r = resp->diccionario.valores[j]->texto;
                if (strcmp(resp->diccionario.claves[j], "cuerpo") == 0 &&
                    resp->diccionario.valores[j]->tipo == VAL_TEXTO)
                    cuerpo_r = resp->diccionario.valores[j]->texto;
                if (strcmp(resp->diccionario.claves[j], "cookies") == 0 &&
                    resp->diccionario.valores[j]->tipo == VAL_DICCIONARIO)
                    cookies_resp = resp->diccionario.valores[j];
                if (strcmp(resp->diccionario.claves[j], "ubicacion") == 0 &&
                    resp->diccionario.valores[j]->tipo == VAL_TEXTO)
                    snprintf(_srv_loc, sizeof _srv_loc, "%s", resp->diccionario.valores[j]->texto);
            }
            _srv_enviar_respuesta(fd, cod, tipo_r, cuerpo_r, cookies_resp);
        } else if (resp && resp->tipo == VAL_TEXTO) {
            _srv_enviar_respuesta(fd, 200, "text/html; charset=utf-8", resp->texto, NULL);
        } else {
            _srv_enviar_respuesta(fd, 200, "text/plain", "", NULL);
        }
        valor_destruir(resp);
        close(fd);
        valor_destruir(consulta); valor_destruir(cookies);
        return;
    }

    /* Ninguna ruta coincide: intentar servir un archivo estático (GET) */
    if (_srv_dir_estaticos[0] && strcmp(metodo, "GET") == 0) {
        char ruta_file[512];
        const char *r = ruta;
        if (strcmp(r, "/") == 0) r = "/index.html";
        /* Nunca fuera de la carpeta de estáticos */
        if (strstr(r, "..") == NULL) {
            snprintf(ruta_file, sizeof(ruta_file), "%s%s", _srv_dir_estaticos, r);
            size_t tam = 0;
            char *cont = interprete_leer_bytes(ruta_file, &tam);
            if (cont) {
                const char *ext = strrchr(ruta_file, '.');
                _srv_enviar_respuesta_n(fd, 200, _srv_mime(ext), cont, tam, NULL);
                free(cont); close(fd);
                valor_destruir(consulta); valor_destruir(cookies);
                return;
            }
        }
    }

    _srv_enviar_respuesta(fd, 404, "text/html",
        "<h1>404 — No encontrado</h1><p>La página no existe.</p>", NULL);
    close(fd);
    valor_destruir(consulta); valor_destruir(cookies);
}

/* ── Llamar manejador Lince ─────────────── */

static Valor *_srv_llamar_manejador(Valor *fn,
                                     const char *metodo, const char *ruta_s,
                                     const char *cuerpo, Valor *consulta,
                                     Valor *params, Valor *cookies) {
    if (!fn || fn->tipo != VAL_FUNCION) return valor_nulo();
    Valor *req = valor_diccionario_crear();
    req->diccionario.claves[0]  = strdup("metodo");
    req->diccionario.valores[0] = valor_texto(metodo);
    req->diccionario.claves[1]  = strdup("ruta");
    req->diccionario.valores[1] = valor_texto(ruta_s);
    req->diccionario.claves[2]  = strdup("cuerpo");
    req->diccionario.valores[2] = valor_texto(cuerpo);
    req->diccionario.claves[3]  = strdup("consulta");
    req->diccionario.valores[3] = consulta ? consulta : valor_diccionario_crear();
    req->diccionario.claves[4]  = strdup("params");
    req->diccionario.valores[4] = params  ? params   : valor_diccionario_crear();
    req->diccionario.claves[5]  = strdup("cookies");
    req->diccionario.valores[5] = cookies ? cookies  : valor_diccionario_crear();
    req->diccionario.cantidad   = 6;
    /* req toma una referencia propia de consulta/params/cookies: el
       llamador conserva su propia referencia y los libera él mismo,
       así que aquí hay que retenerlos para no dejar el refcount en 0
       cuando el entorno de la función se destruya. */
    if (consulta) consulta->refs++;
    if (params)   params->refs++;
    if (cookies)  cookies->refs++;

    Valor *args[1] = { req };
    hay_error = 0;
    return interprete_llamar_funcion(fn, args, 1);
}

/* ── Registro de rutas ──────────────────── */

static Valor *_srv_registrar_ruta(Valor **a, int n, const char *metodo) {
    if (n < 2 || a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_FUNCION)
        return valor_nulo();
    if (_srv_nrutas >= SRV_MAX_RUTAS) return valor_nulo();
    strncpy(_srv_rutas[_srv_nrutas].metodo, metodo, 7);
    strncpy(_srv_rutas[_srv_nrutas].ruta, a[0]->texto, 255);
    a[1]->refs++;
    _srv_rutas[_srv_nrutas].manejador  = a[1];
    _srv_nrutas++;
    return valor_nulo();
}

static Valor *fn_srv_ruta(Valor **a, int n)       { return _srv_registrar_ruta(a, n, ""); }
static Valor *fn_srv_obtener(Valor **a, int n)    { return _srv_registrar_ruta(a, n, "GET"); }
static Valor *fn_srv_enviar(Valor **a, int n)     { return _srv_registrar_ruta(a, n, "POST"); }
static Valor *fn_srv_borrar(Valor **a, int n)     { return _srv_registrar_ruta(a, n, "DELETE"); }
static Valor *fn_srv_actualizar(Valor **a, int n) { return _srv_registrar_ruta(a, n, "PUT"); }

/* ── Constructores de respuesta ─────────── */

static Valor *fn_srv_estaticos(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo == VAL_TEXTO)
        strncpy(_srv_dir_estaticos, a[0]->texto, 255);
    return valor_nulo();
}

static Valor *fn_srv_html(Valor **a, int n) {
    const char *cont = (a[0]->tipo == VAL_TEXTO) ? a[0]->texto : "";
    int cod = (n >= 2 && a[1]->tipo == VAL_NUMERO) ? (int)a[1]->numero : 200;
    Valor *res = valor_diccionario_crear();
    res->diccionario.claves[0]  = strdup("codigo");
    res->diccionario.valores[0] = valor_numero(cod);
    res->diccionario.claves[1]  = strdup("tipo");
    res->diccionario.valores[1] = valor_texto("text/html; charset=utf-8");
    res->diccionario.claves[2]  = strdup("cuerpo");
    res->diccionario.valores[2] = valor_texto(cont);
    res->diccionario.cantidad   = 3;
    return res;
}

static Valor *fn_srv_json(Valor **a, int n) {
    const char *cont = (a[0]->tipo == VAL_TEXTO) ? a[0]->texto : "{}";
    int cod = (n >= 2 && a[1]->tipo == VAL_NUMERO) ? (int)a[1]->numero : 200;
    Valor *res = valor_diccionario_crear();
    res->diccionario.claves[0]  = strdup("codigo");
    res->diccionario.valores[0] = valor_numero(cod);
    res->diccionario.claves[1]  = strdup("tipo");
    res->diccionario.valores[1] = valor_texto("application/json");
    res->diccionario.claves[2]  = strdup("cuerpo");
    res->diccionario.valores[2] = valor_texto(cont);
    res->diccionario.cantidad   = 3;
    return res;
}

static Valor *fn_srv_texto(Valor **a, int n) {
    const char *cont = (a[0]->tipo == VAL_TEXTO) ? a[0]->texto : "";
    int cod = (n >= 2 && a[1]->tipo == VAL_NUMERO) ? (int)a[1]->numero : 200;
    Valor *res = valor_diccionario_crear();
    res->diccionario.claves[0]  = strdup("codigo");
    res->diccionario.valores[0] = valor_numero(cod);
    res->diccionario.claves[1]  = strdup("tipo");
    res->diccionario.valores[1] = valor_texto("text/plain; charset=utf-8");
    res->diccionario.claves[2]  = strdup("cuerpo");
    res->diccionario.valores[2] = valor_texto(cont);
    res->diccionario.cantidad   = 3;
    return res;
}

static Valor *fn_srv_redirigir(Valor **a, int n) {
    (void)n;
    const char *url = (a[0]->tipo == VAL_TEXTO) ? a[0]->texto : "/";
    char redir[1024];
    snprintf(redir, sizeof(redir),
        "<html><head><meta http-equiv='refresh' content='0;url=%s'></head></html>", url);
    Valor *res = valor_diccionario_crear();
    res->diccionario.claves[0]  = strdup("codigo");
    res->diccionario.valores[0] = valor_numero(302);
    res->diccionario.claves[1]  = strdup("tipo");
    res->diccionario.valores[1] = valor_texto("text/html");
    res->diccionario.claves[2]  = strdup("cuerpo");
    res->diccionario.valores[2] = valor_texto(redir);
    res->diccionario.cantidad   = 3;
    valor_diccionario_agregar(res, "ubicacion", valor_texto(url));
    return res;
}

/* ── Escuchar (con fork por conexión) ───── */

static int _srv_solo_local = 0;   /* 'ventana' escucha sólo en 127.0.0.1 */

static Valor *fn_srv_escuchar(Valor **a, int n) {
    int puerto = (n >= 1 && a[0]->tipo == VAL_NUMERO) ? (int)a[0]->numero : 8080;

#ifdef _WIN32
    printf("⚠  El módulo servidor no está disponible en Windows aún.\n");
    return valor_nulo();
#else
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);  /* recolectar hijos automáticamente */

    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        hay_error = 1;
        valor_error = valor_error_nuevo("Error", "No se pudo crear el socket", 0);
        return valor_nulo();
    }
    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = _srv_solo_local ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
    addr.sin_port        = htons(puerto);

    if (bind(srv_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(srv_fd);
        hay_error = 1;
        valor_error = valor_error_nuevo("Error",
            "No se pudo enlazar el puerto (¿está en uso?)", 0);
        return valor_nulo();
    }
    listen(srv_fd, 128);
    printf("🐆 Servidor Lince escuchando en http://localhost:%d\n", puerto);
    fflush(stdout);

    while (1) {
        struct sockaddr_in cli = {0};
        socklen_t cli_len = sizeof(cli);
        int cli_fd = accept(srv_fd, (struct sockaddr*)&cli, &cli_len);
        if (cli_fd < 0) continue;

        pid_t pid = fork();
        if (pid == 0) {
            /* Proceso hijo: manejar esta conexión y salir */
            close(srv_fd);
            /* Restaurar SIGCHLD por defecto: el padre lo ignora para
               autorecolectar hijos, pero heredarlo aquí rompería
               pclose()/waitpid() si el manejador de la ruta usa
               sistema.proceso() u otro popen(). */
            signal(SIGCHLD, SIG_DFL);
            _srv_manejar_conexion(cli_fd);
            exit(0);
        } else if (pid > 0) {
            /* Proceso padre: seguir aceptando */
            close(cli_fd);
        } else {
            /* fork() falló (p. ej. límite de procesos alcanzado):
               no se puede atender esta conexión, cerrarla. */
            close(cli_fd);
        }
    }
    return valor_nulo();
#endif
}

/* servidor.formulario(cuerpo): «a=1&b=hola+mundo%21» → {"a": "1", "b": "hola mundo!"}.
   Es el formato en que un <form> HTML envía sus campos. */
static int srv_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static char *srv_decodificar(const char *s, size_t n) {
    char *o = malloc(n + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '+') o[k++] = ' ';
        else if (s[i] == '%' && i + 2 < n + 0 && srv_hex(s[i + 1]) >= 0 && srv_hex(s[i + 2]) >= 0) {
            o[k++] = (char)(srv_hex(s[i + 1]) << 4 | srv_hex(s[i + 2]));
            i += 2;
        } else o[k++] = s[i];
    }
    o[k] = '\0';
    return o;
}
static Valor *fn_srv_formulario(Valor **a, int n) {
    Valor *d = valor_diccionario_crear();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return d;
    const char *p = a[0]->texto;
    while (*p) {
        const char *fin = strchr(p, '&');
        size_t largo = fin ? (size_t)(fin - p) : strlen(p);
        const char *eq = memchr(p, '=', largo);
        size_t lk = eq ? (size_t)(eq - p) : largo;
        if (lk > 0) {
            char *k = srv_decodificar(p, lk);
            char *v = eq ? srv_decodificar(eq + 1, largo - lk - 1) : strdup("");
            int pos = valor_diccionario_buscar(d, k);
            if (pos >= 0) { valor_destruir(d->diccionario.valores[pos]); d->diccionario.valores[pos] = valor_texto(v); }
            else valor_diccionario_agregar(d, k, valor_texto(v));
            free(k); free(v);
        }
        if (!fin) break;
        p = fin + 1;
    }
    return d;
}

/* servidor.ventana(puerto[, ancho, alto]): como escuchar(), pero además abre
   la aplicación en una ventana propia del navegador (sin barra de direcciones)
   y termina el programa cuando se cierra la ventana. El servidor sólo atiende
   en 127.0.0.1. Con la variable de entorno LINCE_NAVEGADOR se elige el programa
   a lanzar (recibe la URL como único argumento). */
#ifndef _WIN32
static void ventana_esperar_servidor(int puerto) {
    for (int i = 0; i < 100; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sa = {0};
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sa.sin_port = htons(puerto);
        int ok = connect(fd, (struct sockaddr *)&sa, sizeof sa) == 0;
        close(fd);
        if (ok) return;
        usleep(100000);
    }
}

/* Lanza 'argv' y espera a que acabe. Devuelve -1 si no se pudo ejecutar */
static int ventana_lanzar(char *const argv[]) {
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) {
        signal(SIGCHLD, SIG_DFL);
        if (!freopen("/dev/null", "w", stdout)) _exit(127);
        if (!freopen("/dev/null", "w", stderr)) _exit(127);
        execvp(argv[0], argv);
        _exit(127);
    }
    int st = 0;
    waitpid(p, &st, 0);
    if (WIFEXITED(st) && WEXITSTATUS(st) == 127) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

static void ventana_ayudante(pid_t padre, int puerto, int ancho, int alto) {
    signal(SIGCHLD, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    ventana_esperar_servidor(puerto);
    char url[64], perfil[96], tam[48];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/", puerto);

    int cerrada = 0;
    const char *propio = getenv("LINCE_NAVEGADOR");
    if (propio && *propio) {
        char *const av[] = { (char *)propio, url, NULL };
        if (ventana_lanzar(av) >= 0) cerrada = 1;
    } else {
        static const char *const candidatos[] = {
            "google-chrome", "google-chrome-stable", "chromium", "chromium-browser",
            "microsoft-edge", "microsoft-edge-stable", "brave-browser",
            "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
            "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
            "/Applications/Chromium.app/Contents/MacOS/Chromium", NULL };
        snprintf(perfil, sizeof perfil, "--user-data-dir=/tmp/lince-ventana-%d", (int)getpid());
        snprintf(tam, sizeof tam, "--window-size=%d,%d", ancho, alto);
        char app[96];
        snprintf(app, sizeof app, "--app=%s", url);
        for (int i = 0; candidatos[i] && !cerrada; i++) {
            char *const av[] = { (char *)candidatos[i], app, perfil, tam,
                                 "--no-first-run", "--no-default-browser-check", NULL };
            time_t t0 = time(NULL);
            int r = ventana_lanzar(av);
            if (r >= 0 && (r == 0 || time(NULL) - t0 >= 2)) cerrada = 1;
        }
        char rm[128];
        snprintf(rm, sizeof rm, "rm -rf /tmp/lince-ventana-%d >/dev/null 2>&1", (int)getpid());
        /* los procesos del navegador pueden tardar un instante en soltar el perfil */
        if (system(rm) != 0) { sleep(1); if (system(rm) != 0) { /* se queda en /tmp */ } }
    }
    if (cerrada) {
        if (getppid() == padre) kill(padre, SIGTERM);
        return;
    }
    /* Sin navegador con modo aplicación: el predeterminado, y el servidor sigue */
    char *const xdg[] = { "xdg-open", url, NULL };
    char *const mac[] = { "open", url, NULL };
    if (ventana_lanzar(xdg) < 0) ventana_lanzar(mac);
    fprintf(stderr, "Abre %s en tu navegador (Ctrl+C para terminar).\n", url);
}

static void ventana_salir(int sig) { (void)sig; _exit(0); }
#endif

static Valor *fn_srv_ventana(Valor **a, int n) {
#ifdef _WIN32
    (void)a; (void)n;
    printf("⚠  servidor.ventana() no está disponible en Windows aún.\n");
    return valor_nulo();
#else
    int puerto = (n >= 1 && a[0]->tipo == VAL_NUMERO) ? (int)a[0]->numero : 8765;
    int ancho  = (n >= 2 && a[1]->tipo == VAL_NUMERO) ? (int)a[1]->numero : 1000;
    int alto   = (n >= 3 && a[2]->tipo == VAL_NUMERO) ? (int)a[2]->numero : 700;
    fflush(NULL);
    pid_t padre = getpid();
    pid_t h = fork();
    if (h == 0) {
        ventana_ayudante(padre, puerto, ancho, alto);
        _exit(0);
    }
    signal(SIGTERM, ventana_salir);       /* al cerrar la ventana: salida limpia */
    _srv_solo_local = 1;
    _srv_cors = 0;       /* una página ajena no debe poder leer esta aplicación */
    Valor *p = valor_numero(puerto);
    Valor *args[1] = { p };
    Valor *r = fn_srv_escuchar(args, 1);
    valor_destruir(p);
    return r;
#endif
}

/* ─────────────────────────────────────────
   MÓDULO: terminal — colores y TUI
───────────────────────────────────────── */
#ifdef _WIN32
#include <windows.h>
static int _ter_colores_win = -1;
static void _ter_init_win(void) {
    if (_ter_colores_win < 0) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        GetConsoleMode(h, &mode);
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        _ter_colores_win = 1;
    }
}
#define TER_INIT() _ter_init_win()
#else
#define TER_INIT() ((void)0)
#endif

static char *_ter_val_texto(Valor *v) {
    static char buf[256];
    if (!v) return "";
    if (v->tipo == VAL_TEXTO)   return v->texto;
    if (v->tipo == VAL_NUMERO) {
        if (v->numero == (long long)v->numero)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->numero);
        else
            snprintf(buf, sizeof(buf), "%.6g", v->numero);
        return buf;
    }
    if (v->tipo == VAL_BOOLEANO) return v->booleano ? "verdadero" : "falso";
    if (v->tipo == VAL_NULO)     return "nulo";
    return "";
}

static Valor *fn_ter_color(const char *codigo, Valor **a, int n) {
    (void)n;
    TER_INIT();
    char buf[4096];
    snprintf(buf, sizeof(buf), "\033[%sm%s\033[0m", codigo,
             _ter_val_texto(a[0]));
    return valor_texto(buf);
}

static Valor *fn_ter_negro(Valor **a, int n)    { return fn_ter_color("30", a, n); }
static Valor *fn_ter_rojo(Valor **a, int n)     { return fn_ter_color("31", a, n); }
static Valor *fn_ter_verde(Valor **a, int n)    { return fn_ter_color("32", a, n); }
static Valor *fn_ter_amarillo(Valor **a, int n) { return fn_ter_color("33", a, n); }
static Valor *fn_ter_azul(Valor **a, int n)     { return fn_ter_color("34", a, n); }
static Valor *fn_ter_magenta(Valor **a, int n)  { return fn_ter_color("35", a, n); }
static Valor *fn_ter_cian(Valor **a, int n)     { return fn_ter_color("36", a, n); }
static Valor *fn_ter_blanco(Valor **a, int n)   { return fn_ter_color("37", a, n); }
static Valor *fn_ter_negrita(Valor **a, int n)  { return fn_ter_color("1",  a, n); }
static Valor *fn_ter_cursiva(Valor **a, int n)  { return fn_ter_color("3",  a, n); }
static Valor *fn_ter_subrayado(Valor **a, int n){ return fn_ter_color("4",  a, n); }

static Valor *fn_ter_limpiar(Valor **a, int n) {
    (void)a; (void)n;
    TER_INIT();
    printf("\033[2J\033[H");
    fflush(stdout);
    return valor_nulo();
}

static Valor *fn_ter_mover(Valor **a, int n) {
    (void)n;
    TER_INIT();
    int fila = (a[0]->tipo == VAL_NUMERO) ? (int)a[0]->numero : 1;
    int col  = (a[1]->tipo == VAL_NUMERO) ? (int)a[1]->numero : 1;
    printf("\033[%d;%dH", fila, col);
    fflush(stdout);
    return valor_nulo();
}

static Valor *fn_ter_ancho(Valor **a, int n) {
    (void)a; (void)n;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return valor_numero(csbi.srWindow.Right - csbi.srWindow.Left + 1);
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return valor_numero(w.ws_col > 0 ? w.ws_col : 80);
#endif
}

static Valor *fn_ter_alto(Valor **a, int n) {
    (void)a; (void)n;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return valor_numero(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return valor_numero(w.ws_row > 0 ? w.ws_row : 24);
#endif
}

static Valor *fn_ter_barra(Valor **a, int n) {
    /* barra(valor, maximo, ancho) → texto tipo [====    ] */
    (void)n;
    double valor  = (a[0]->tipo == VAL_NUMERO) ? a[0]->numero : 0;
    double maximo = (a[1]->tipo == VAL_NUMERO) ? a[1]->numero : 100;
    int    ancho  = (a[2]->tipo == VAL_NUMERO) ? (int)a[2]->numero : 20;
    if (ancho < 2) ancho = 2;
    if (maximo <= 0) maximo = 1;
    int lleno = (int)((valor / maximo) * ancho);
    if (lleno < 0) lleno = 0;
    if (lleno > ancho) lleno = ancho;
    char buf[256] = "[";
    for (int i = 0; i < ancho; i++)
        strncat(buf, i < lleno ? "=" : " ", sizeof(buf)-strlen(buf)-1);
    strncat(buf, "]", sizeof(buf)-strlen(buf)-1);
    return valor_texto(buf);
}

static Valor *fn_ter_tabla(Valor **a, int n) {
    /* tabla(lista_de_listas) → texto con tabla formateada */
    (void)n;
    if (a[0]->tipo != VAL_LISTA) return valor_texto("");
    int num_filas = a[0]->lista.cantidad;
    if (num_filas == 0) return valor_texto("");

    /* Calcular número de columnas y ancho máximo de cada una */
    int num_cols = 0;
    for (int i = 0; i < num_filas; i++) {
        Valor *fila = a[0]->lista.elementos[i];
        if (fila->tipo == VAL_LISTA && fila->lista.cantidad > num_cols)
            num_cols = fila->lista.cantidad;
    }
    if (num_cols == 0 || num_cols > 64) return valor_texto("");

    int anchos[64] = {0};
    for (int i = 0; i < num_filas; i++) {
        Valor *fila = a[0]->lista.elementos[i];
        if (fila->tipo != VAL_LISTA) continue;
        for (int j = 0; j < fila->lista.cantidad && j < 64; j++) {
            char *s = _ter_val_texto(fila->lista.elementos[j]); char *_s_dup = strdup(s);
            int len = (int)strlen(s);
            if (len > anchos[j]) anchos[j] = len;
        }
    }

    /* Construir separador */
    char sep[2048] = "+";
    for (int j = 0; j < num_cols; j++) {
        for (int k = 0; k < anchos[j] + 2; k++)
            strncat(sep, "-", sizeof(sep)-strlen(sep)-1);
        strncat(sep, "+", sizeof(sep)-strlen(sep)-1);
    }

    char buf[65536] = "";
    strncat(buf, sep, sizeof(buf)-strlen(buf)-1);
    strncat(buf, "\n", sizeof(buf)-strlen(buf)-1);

    for (int i = 0; i < num_filas; i++) {
        Valor *fila = a[0]->lista.elementos[i];
        strncat(buf, "|", sizeof(buf)-strlen(buf)-1);
        if (fila->tipo == VAL_LISTA) {
            for (int j = 0; j < num_cols; j++) {
                char celda[256] = "";
                if (j < fila->lista.cantidad) {
                    char *s = _ter_val_texto(fila->lista.elementos[j]); char *_s_dup = strdup(s);
                    strncpy(celda, s, sizeof(celda)-1);
                }
                char col_buf[512];
                snprintf(col_buf, sizeof(col_buf), " %-*s |", anchos[j], celda);
                strncat(buf, col_buf, sizeof(buf)-strlen(buf)-1);
            }
        }
        strncat(buf, "\n", sizeof(buf)-strlen(buf)-1);
        if (i == 0) {
            strncat(buf, sep, sizeof(buf)-strlen(buf)-1);
            strncat(buf, "\n", sizeof(buf)-strlen(buf)-1);
        }
    }
    strncat(buf, sep, sizeof(buf)-strlen(buf)-1);
    return valor_texto(buf);
}

/* ─────────────────────────────────────────
   MÓDULO: sistema
───────────────────────────────────────── */

static Valor *fn_sis_ejecutar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "ejecutar() espera un texto con el comando", 0);
        return valor_nulo();
    }

    FILE *proc = popen(a[0]->texto, "r");
    if (!proc) {
        hay_error = 1;
        valor_error = valor_error_nuevo("Error",
            "No se pudo ejecutar el comando", 0);
        return valor_nulo();
    }

    char   buf[65536] = "";
    char   linea[4096];
    while (fgets(linea, sizeof(linea), proc))
        strncat(buf, linea, sizeof(buf) - strlen(buf) - 1);

    pclose(proc);
    return valor_texto(buf);
}

static Valor *fn_sis_proceso(Valor **a, int n) {
    /* proceso(cmd) → diccionario {salida, error, codigo} */
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_nulo();

    char cmd_out[4096], cmd_err[4096];
    snprintf(cmd_out, sizeof(cmd_out), "%s 2>/tmp/_lince_stderr.txt", a[0]->texto);
    FILE *proc = popen(cmd_out, "r");
    if (!proc) return valor_nulo();

    char buf[65536] = "";
    char linea[4096];
    while (fgets(linea, sizeof(linea), proc))
        strncat(buf, linea, sizeof(buf) - strlen(buf) - 1);
    int codigo = pclose(proc);

    /* Leer stderr */
    char buf_err[65536] = "";
    FILE *ferr = fopen("/tmp/_lince_stderr.txt", "r");
    if (ferr) {
        while (fgets(linea, sizeof(linea), ferr))
            strncat(buf_err, linea, sizeof(buf_err) - strlen(buf_err) - 1);
        fclose(ferr);
        remove("/tmp/_lince_stderr.txt");
    }

    /* Construir diccionario de resultado */
    Valor *res = valor_diccionario_crear();
    res->diccionario.claves[0]  = strdup("salida");
    res->diccionario.valores[0] = valor_texto(buf);
    res->diccionario.claves[1]  = strdup("error");
    res->diccionario.valores[1] = valor_texto(buf_err);
    res->diccionario.claves[2]  = strdup("codigo");
    res->diccionario.valores[2] = valor_numero((double)(codigo >> 8));
    res->diccionario.claves[3]  = strdup("ok");
    res->diccionario.valores[3] = valor_booleano(codigo == 0);
    res->diccionario.cantidad   = 4;
    return res;
}

static Valor *fn_sis_entorno(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_nulo();
    const char *val = getenv(a[0]->texto);
    if (!val) {
        /* Valor por defecto opcional */
        if (n >= 2 && a[1]->tipo == VAL_TEXTO) return valor_texto(a[1]->texto);
        return valor_nulo();
    }
    return valor_texto(val);
}

static Valor *fn_sis_establecer_entorno(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) return valor_booleano(0);
#ifdef _WIN32
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s=%s", a[0]->texto, a[1]->texto);
    return valor_booleano(_putenv(buf) == 0);
#else
    return valor_booleano(setenv(a[0]->texto, a[1]->texto, 1) == 0);
#endif
}

static Valor *fn_sis_plataforma(Valor **a, int n) {
    (void)a; (void)n;
#ifdef _WIN32
    return valor_texto("windows");
#elif defined(__APPLE__)
    return valor_texto("mac");
#else
    return valor_texto("linux");
#endif
}

static Valor *fn_sis_pid(Valor **a, int n) {
    (void)a; (void)n;
#ifdef _WIN32
    return valor_numero((double)GetCurrentProcessId());
#else
    return valor_numero((double)getpid());
#endif
}

static Valor *fn_sis_existe(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_booleano(0);
    FILE *f = fopen(a[0]->texto, "r");
    if (f) { fclose(f); return valor_booleano(1); }
    return valor_booleano(0);
}

static Valor *fn_sis_salir(Valor **a, int n) {
    (void)n;
    int codigo = (a[0]->tipo == VAL_NUMERO) ? (int)a[0]->numero : 0;
    exit(codigo);
    return valor_nulo();
}

/* ─────────────────────────────────────────
   MÓDULO: json
───────────────────────────────────────── */

/* Parser JSON interno — convierte JSON a valores de Lince */
static Valor *json_parsear_valor(const char *s, int *pos);

static void json_saltar_espacios(const char *s, int *pos) {
    while (s[*pos] && (s[*pos] == ' ' || s[*pos] == '\t' ||
                       s[*pos] == '\n' || s[*pos] == '\r'))
        (*pos)++;
}

static Valor *json_parsear_texto(const char *s, int *pos) {
    (*pos)++; /* saltar " */
    char buf[8192]; int bi = 0;
    while (s[*pos] && s[*pos] != '"') {
        if (s[*pos] == '\\') {
            (*pos)++;
            switch (s[*pos]) {
                case 'n': buf[bi++] = '\n'; break;
                case 't': buf[bi++] = '\t'; break;
                case '"': buf[bi++] = '"';  break;
                case '\\':buf[bi++] = '\\'; break;
                default:  buf[bi++] = s[*pos]; break;
            }
        } else {
            buf[bi++] = s[*pos];
        }
        (*pos)++;
    }
    if (s[*pos] == '"') (*pos)++;
    buf[bi] = '\0';
    return valor_texto(buf);
}

static Valor *json_parsear_numero(const char *s, int *pos) {
    int inicio = *pos;
    if (s[*pos] == '-') (*pos)++;
    while (s[*pos] >= '0' && s[*pos] <= '9') (*pos)++;
    if (s[*pos] == '.') {
        (*pos)++;
        while (s[*pos] >= '0' && s[*pos] <= '9') (*pos)++;
    }
    if (s[*pos] == 'e' || s[*pos] == 'E') {
        (*pos)++;
        if (s[*pos] == '+' || s[*pos] == '-') (*pos)++;
        while (s[*pos] >= '0' && s[*pos] <= '9') (*pos)++;
    }
    char tmp[64];
    int len = *pos - inicio;
    strncpy(tmp, s + inicio, len); tmp[len] = '\0';
    return valor_numero(atof(tmp));
}

static Valor *json_parsear_array(const char *s, int *pos) {
    (*pos)++; /* saltar [ */
    Valor *lista = valor_lista_crear();
    json_saltar_espacios(s, pos);
    if (s[*pos] == ']') { (*pos)++; return lista; }
    while (s[*pos]) {
        json_saltar_espacios(s, pos);
        Valor *elem = json_parsear_valor(s, pos);
        lista_agregar(lista, elem);
        json_saltar_espacios(s, pos);
        if (s[*pos] == ',') { (*pos)++; continue; }
        if (s[*pos] == ']') { (*pos)++; break; }
    }
    return lista;
}

static Valor *json_parsear_objeto(const char *s, int *pos) {
    (*pos)++; /* saltar { */
    Valor *dic = valor_diccionario_crear();
    json_saltar_espacios(s, pos);
    if (s[*pos] == '}') { (*pos)++; return dic; }
    while (s[*pos]) {
        json_saltar_espacios(s, pos);
        if (s[*pos] != '"') break;
        Valor *vclave = json_parsear_texto(s, pos);
        char  *clave  = strdup(vclave->texto);
        valor_destruir(vclave);
        json_saltar_espacios(s, pos);
        if (s[*pos] == ':') (*pos)++;
        json_saltar_espacios(s, pos);
        Valor *valor = json_parsear_valor(s, pos);
        valor_diccionario_asegurar(dic, dic->diccionario.cantidad + 1);
        int i = dic->diccionario.cantidad;
        dic->diccionario.claves[i]  = clave;
        dic->diccionario.valores[i] = valor;
        dic->diccionario.cantidad++;
        json_saltar_espacios(s, pos);
        if (s[*pos] == ',') { (*pos)++; continue; }
        if (s[*pos] == '}') { (*pos)++; break; }
    }
    return dic;
}

static Valor *json_parsear_valor(const char *s, int *pos) {
    json_saltar_espacios(s, pos);
    char c = s[*pos];
    if (c == '"') return json_parsear_texto(s, pos);
    if (c == '[') return json_parsear_array(s, pos);
    if (c == '{') return json_parsear_objeto(s, pos);
    if (c == '-' || (c >= '0' && c <= '9')) return json_parsear_numero(s, pos);
    if (strncmp(s + *pos, "true",  4) == 0) { *pos += 4; return valor_booleano(1); }
    if (strncmp(s + *pos, "false", 5) == 0) { *pos += 5; return valor_booleano(0); }
    if (strncmp(s + *pos, "null",  4) == 0) { *pos += 4; return valor_nulo(); }
    return valor_nulo();
}

static Valor *fn_json_parsear(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo",
            "json.parsear() espera un texto", 0);
        return valor_nulo();
    }
    int pos = 0;
    return json_parsear_valor(a[0]->texto, &pos);
}

/* Serializar valor de Lince a JSON */
static void json_escribir_valor(Valor *v, char *buf, size_t max) {
    char tmp[256];
    if (!v || v->tipo == VAL_NULO) {
        strncat(buf, "null", max - strlen(buf) - 1);
        return;
    }
    switch (v->tipo) {
        case VAL_NUMERO:
            if (v->numero == (long long)v->numero)
                snprintf(tmp, sizeof(tmp), "%lld", (long long)v->numero);
            else
                snprintf(tmp, sizeof(tmp), "%g", v->numero);
            strncat(buf, tmp, max - strlen(buf) - 1);
            break;
        case VAL_TEXTO: {
            strncat(buf, "\"", max - strlen(buf) - 1);
            /* Escapar caracteres especiales */
            for (int i = 0; v->texto[i]; i++) {
                char esc[4] = {0};
                switch (v->texto[i]) {
                    case '"':  strcpy(esc, "\\\""); break;
                    case '\\': strcpy(esc, "\\\\"); break;
                    case '\n': strcpy(esc, "\\n");  break;
                    case '\t': strcpy(esc, "\\t");  break;
                    default: esc[0] = v->texto[i]; esc[1] = '\0'; break;
                }
                strncat(buf, esc, max - strlen(buf) - 1);
            }
            strncat(buf, "\"", max - strlen(buf) - 1);
            break;
        }
        case VAL_BOOLEANO:
            strncat(buf, v->booleano ? "true" : "false", max - strlen(buf) - 1);
            break;
        case VAL_LISTA:
            strncat(buf, "[", max - strlen(buf) - 1);
            for (int i = 0; i < v->lista.cantidad; i++) {
                json_escribir_valor(v->lista.elementos[i], buf, max);
                if (i < v->lista.cantidad - 1)
                    strncat(buf, ", ", max - strlen(buf) - 1);
            }
            strncat(buf, "]", max - strlen(buf) - 1);
            break;
        case VAL_DICCIONARIO:
            strncat(buf, "{", max - strlen(buf) - 1);
            for (int i = 0; i < v->diccionario.cantidad; i++) {
                strncat(buf, "\"", max - strlen(buf) - 1);
                strncat(buf, v->diccionario.claves[i], max - strlen(buf) - 1);
                strncat(buf, "\": ", max - strlen(buf) - 1);
                json_escribir_valor(v->diccionario.valores[i], buf, max);
                if (i < v->diccionario.cantidad - 1)
                    strncat(buf, ", ", max - strlen(buf) - 1);
            }
            strncat(buf, "}", max - strlen(buf) - 1);
            break;
        default:
            strncat(buf, "null", max - strlen(buf) - 1);
            break;
    }
}

static Valor *fn_json_texto(Valor **a, int n) {
    (void)n;
    char buf[65536] = "";
    json_escribir_valor(a[0], buf, sizeof(buf));
    return valor_texto(buf);
}

/* ─────────────────────────────────────────
   MÓDULO: red
   Usa libcurl si está disponible,
   o popen como fallback portable
───────────────────────────────────────── */

/* Construye y ejecuta una petición HTTP usando curl (CLI) */
static Valor *hacer_peticion(const char *metodo, const char *url,
                              const char *cuerpo, Valor *cabeceras) {
    char cmd[8192] = "";
    char tmp[4096];

    /* Comando base */
    snprintf(cmd, sizeof(cmd),
        "curl -s -w '\\n%%{http_code}' -X %s", metodo);

    /* Cabeceras */
    if (cabeceras && cabeceras->tipo == VAL_DICCIONARIO) {
        for (int i = 0; i < cabeceras->diccionario.cantidad; i++) {
            snprintf(tmp, sizeof(tmp), " -H \"%s: %s\"",
                cabeceras->diccionario.claves[i],
                cabeceras->diccionario.valores[i]->tipo == VAL_TEXTO
                    ? cabeceras->diccionario.valores[i]->texto : "");
            strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
        }
    }

    /* Cuerpo */
    if (cuerpo && strlen(cuerpo) > 0) {
        snprintf(tmp, sizeof(tmp), " -d '%s'", cuerpo);
        strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
    }

    /* URL */
    snprintf(tmp, sizeof(tmp), " \"%s\" 2>/dev/null", url);
    strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);

    FILE *proc = popen(cmd, "r");
    if (!proc) {
        hay_error = 1;
        valor_error = valor_error_nuevo("Error",
            "No se pudo conectar. ¿Está curl instalado?", 0);
        return valor_nulo();
    }

    char salida[65536] = ""; char linea[4096];
    while (fgets(linea, sizeof(linea), proc))
        strncat(salida, linea, sizeof(salida) - strlen(salida) - 1);
    pclose(proc);

    /* La última línea es el código HTTP */
    int codigo = 0;
    char cuerpo_resp[65536] = "";
    char *ultima = strrchr(salida, '\n');
    if (ultima) {
        *ultima = '\0';
        codigo = atoi(ultima + 1);
        strncpy(cuerpo_resp, salida, sizeof(cuerpo_resp) - 1);
    } else {
        strncpy(cuerpo_resp, salida, sizeof(cuerpo_resp) - 1);
    }

    /* Construir diccionario de respuesta */
    Valor *resp = valor_diccionario_crear();
    resp->diccionario.claves[0]  = strdup("codigo");
    resp->diccionario.valores[0] = valor_numero(codigo);
    resp->diccionario.claves[1]  = strdup("cuerpo");
    resp->diccionario.valores[1] = valor_texto(cuerpo_resp);
    resp->diccionario.claves[2]  = strdup("ok");
    resp->diccionario.valores[2] = valor_booleano(codigo >= 200 && codigo < 400);
    resp->diccionario.cantidad   = 3;
    return resp;
}

static Valor *fn_red_obtener(Valor **a, int n) {
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "obtener() espera una URL", 0);
        return valor_nulo();
    }
    Valor *cabeceras = (n >= 2) ? a[1] : NULL;
    return hacer_peticion("GET", a[0]->texto, "", cabeceras);
}

static Valor *fn_red_enviar(Valor **a, int n) {
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "enviar() espera una URL", 0);
        return valor_nulo();
    }
    const char *cuerpo   = (n >= 2 && a[1]->tipo == VAL_TEXTO) ? a[1]->texto : "";
    Valor      *cabeceras = (n >= 3) ? a[2] : NULL;
    return hacer_peticion("POST", a[0]->texto, cuerpo, cabeceras);
}

static Valor *fn_red_actualizar(Valor **a, int n) {
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "actualizar() espera una URL", 0);
        return valor_nulo();
    }
    const char *cuerpo   = (n >= 2 && a[1]->tipo == VAL_TEXTO) ? a[1]->texto : "";
    Valor      *cabeceras = (n >= 3) ? a[2] : NULL;
    return hacer_peticion("PUT", a[0]->texto, cuerpo, cabeceras);
}

static Valor *fn_red_eliminar(Valor **a, int n) {
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "eliminar() espera una URL", 0);
        return valor_nulo();
    }
    Valor *cabeceras = (n >= 2) ? a[1] : NULL;
    return hacer_peticion("DELETE", a[0]->texto, "", cabeceras);
}

/* Gancho de anfitrión — ver modulos.h */
static ModuloExterno _modulo_externo = NULL;

void modulo_fijar_externo(ModuloExterno fn) { _modulo_externo = fn; }

/* Registra un módulo como diccionario en el entorno */
static void registrar_modulo_diccionario(Entorno *entorno,
    const char *nombre_mod,
    FuncionNativa *fns, int num_fns,
    const char **cnames, double *cvalues, int num_c)
{
    /* Creamos un objeto especial tipo diccionario que contiene
       punteros a funciones nativas. Lo representamos internamente
       como un VAL_OBJETO con campos especiales. */

    /* Para simplificar: usamos un entorno separado y registramos
       las funciones como valores especiales accesibles via mod.fn() */

    /* Estrategia: crear un Valor de tipo objeto que almacena las
       funciones nativas como campos con puntero de función */

    /* Usamos VAL_DICCIONARIO con valores especiales de tipo VAL_FUNCION */
    Valor *modulo = malloc(sizeof(Valor));
    modulo->tipo      = VAL_DICCIONARIO;
    modulo->refs      = 1;
    modulo->es_modulo = 1;
    modulo->diccionario.cantidad  = 0;
    modulo->diccionario.indice = NULL; modulo->diccionario.indice_cap = 0; modulo->diccionario.indice_n = 0;
    modulo->diccionario.capacidad = num_fns + num_c + 8;
    modulo->diccionario.claves   = malloc(sizeof(char*)  * modulo->diccionario.capacidad);
    modulo->diccionario.valores  = malloc(sizeof(Valor*) * modulo->diccionario.capacidad);

    /* Añadir funciones como entradas especiales */
    for (int i = 0; i < num_fns; i++) {
        /* Creamos un FuncionLince vacío que apunta a la función nativa */
        /* Usamos un truco: guardamos el puntero en el campo cuerpo=NULL
           y lo identificamos por un flag especial en interprete.c */
        FuncionLince *f = calloc(1, sizeof(FuncionLince));
        f->nombre          = strdup(fns[i].nombre);
        f->num_parametros  = fns[i].num_args;
        f->cuerpo          = (Nodo*)(uintptr_t)fns[i].fn; /* truco: guardar puntero */
        f->entorno_closure = NULL; /* señal de función nativa */

        /* calloc, no malloc: 'es_modulo' no se toca aquí y el intérprete lo
           lee — en NODO_LLAMADA vale 99 para marcar un generador. Con la
           basura del montón dentro, un 'sea f = matematica.raiz' seguido de
           'f()' hacía que tratara el puntero a función nativa como cuerpo de
           generador y lo desreferenciara. Lo mismo vale para cualquier campo
           que se le añada a Valor en el futuro. */
        Valor *vf = calloc(1, sizeof(Valor));
        vf->tipo  = VAL_FUNCION;
        vf->refs  = 1;
        vf->funcion = f;

        modulo->diccionario.claves[modulo->diccionario.cantidad]  = strdup(fns[i].nombre);
        modulo->diccionario.valores[modulo->diccionario.cantidad] = vf;
        modulo->diccionario.cantidad++;
    }

    /* Añadir constantes */
    for (int i = 0; i < num_c; i++) {
        modulo->diccionario.claves[modulo->diccionario.cantidad]  = strdup(cnames[i]);
        modulo->diccionario.valores[modulo->diccionario.cantidad] = valor_numero(cvalues[i]);
        modulo->diccionario.cantidad++;
    }

    entorno_definir(entorno, nombre_mod, modulo, 0);
}

/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
/* ─────────────────────────────────────────
   MÓDULO: expresiones
───────────────────────────────────────── */

#ifndef LINCE_WINDOWS
#include <regex.h>

static Valor *fn_exp_coincidir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO)
        return valor_booleano(0);
    regex_t re;
    if (regcomp(&re, a[1]->texto, REG_EXTENDED) != 0)
        return valor_booleano(0);
    int res = regexec(&re, a[0]->texto, 0, NULL, 0);
    regfree(&re);
    return valor_booleano(res == 0);
}

static Valor *fn_exp_buscar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO)
        return valor_nulo();
    regex_t re;
    if (regcomp(&re, a[1]->texto, REG_EXTENDED) != 0)
        return valor_nulo();
    regmatch_t match;
    int res = regexec(&re, a[0]->texto, 1, &match, 0);
    regfree(&re);
    if (res != 0) return valor_nulo();
    int len = match.rm_eo - match.rm_so;
    char *buf = malloc(len + 1);
    strncpy(buf, a[0]->texto + match.rm_so, len);
    buf[len] = '\0';
    Valor *v = valor_texto(buf);
    free(buf);
    return v;
}

static Valor *fn_exp_todos(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO)
        return valor_lista_crear();
    regex_t re;
    if (regcomp(&re, a[1]->texto, REG_EXTENDED) != 0)
        return valor_lista_crear();
    Valor *lista  = valor_lista_crear();
    const char *s = a[0]->texto;
    regmatch_t match;
    while (regexec(&re, s, 1, &match, 0) == 0) {
        int len = match.rm_eo - match.rm_so;
        char *buf = malloc(len + 1);
        strncpy(buf, s + match.rm_so, len);
        buf[len] = '\0';
        lista_agregar(lista, valor_texto(buf));
        free(buf);
        s += (match.rm_eo > 0) ? match.rm_eo : 1;
    }
    regfree(&re);
    return lista;
}

static Valor *fn_exp_reemplazar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO || a[2]->tipo != VAL_TEXTO)
        return valor_nulo();
    regex_t re;
    if (regcomp(&re, a[1]->texto, REG_EXTENDED) != 0)
        return valor_texto(a[0]->texto);
    const char *src = a[0]->texto;
    const char *rep = a[2]->texto;
    char buf[65536] = "";
    regmatch_t match;
    while (regexec(&re, src, 1, &match, 0) == 0) {
        strncat(buf, src, match.rm_so);
        strncat(buf, rep, sizeof(buf) - strlen(buf) - 1);
        src += (match.rm_eo > 0) ? match.rm_eo : 1;
    }
    strncat(buf, src, sizeof(buf) - strlen(buf) - 1);
    regfree(&re);
    return valor_texto(buf);
}

#else
/* Windows — búsqueda de subcadena simple */
static Valor *fn_exp_coincidir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO)
        return valor_booleano(0);
    return valor_booleano(strstr(a[0]->texto, a[1]->texto) != NULL);
}
static Valor *fn_exp_buscar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO)
        return valor_nulo();
    return strstr(a[0]->texto, a[1]->texto)
        ? valor_texto(a[1]->texto) : valor_nulo();
}
static Valor *fn_exp_todos(Valor **a, int n) {
    (void)n;
    Valor *lista = valor_lista_crear();
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) return lista;
    const char *s = a[0]->texto;
    int len = strlen(a[1]->texto);
    while ((s = strstr(s, a[1]->texto)) != NULL) {
        lista_agregar(lista, valor_texto(a[1]->texto));
        s += len;
    }
    return lista;
}
static Valor *fn_exp_reemplazar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO || a[2]->tipo != VAL_TEXTO)
        return valor_nulo();
    char buf[65536] = "";
    const char *s = a[0]->texto, *p = a[1]->texto, *rep = a[2]->texto;
    int plen = strlen(p);
    const char *pos;
    while ((pos = strstr(s, p)) != NULL) {
        strncat(buf, s, pos - s);
        strncat(buf, rep, sizeof(buf) - strlen(buf) - 1);
        s = pos + plen;
    }
    strncat(buf, s, sizeof(buf) - strlen(buf) - 1);
    return valor_texto(buf);
}
#endif /* LINCE_WINDOWS */

void modulo_registrar(Entorno *entorno, const char *nombre,
                      FuncionNativa *fns, int num_fns) {
    registrar_modulo_diccionario(entorno, nombre, fns, num_fns, NULL, NULL, 0);
}

/* ─────────────────────────────────────────
   MÓDULO: pruebas
   Pruebas unitarias para programas Lince. Cuenta aciertos y fallos;
   resumen() imprime el total y sale con código 1 si algo falló.
───────────────────────────────────────── */
static int _prb_total, _prb_fallos;

static int prb_iguales(Valor *a, Valor *b) {
    if (a->tipo != b->tipo) return 0;
    switch (a->tipo) {
        case VAL_NUMERO:    return a->numero == b->numero;
        case VAL_TEXTO:     return strcmp(a->texto, b->texto) == 0;
        case VAL_BOOLEANO:  return a->booleano == b->booleano;
        case VAL_NULO:      return 1;
        case VAL_LISTA:
            if (a->lista.cantidad != b->lista.cantidad) return 0;
            for (int i = 0; i < a->lista.cantidad; i++)
                if (!prb_iguales(a->lista.elementos[i], b->lista.elementos[i])) return 0;
            return 1;
        case VAL_DICCIONARIO:
            if (a->diccionario.cantidad != b->diccionario.cantidad) return 0;
            for (int i = 0; i < a->diccionario.cantidad; i++) {
                int hallada = 0;
                for (int j = 0; j < b->diccionario.cantidad && !hallada; j++)
                    if (strcmp(a->diccionario.claves[i], b->diccionario.claves[j]) == 0) {
                        if (!prb_iguales(a->diccionario.valores[i], b->diccionario.valores[j])) return 0;
                        hallada = 1;
                    }
                if (!hallada) return 0;
            }
            return 1;
        default:            return a == b;
    }
}

static const char *prb_desc(Valor **a, int n) {
    return (n > 0 && a[0]->tipo == VAL_TEXTO) ? a[0]->texto : "(sin descripción)";
}

static void prb_registrar(int ok, const char *desc) {
    _prb_total++;
    if (ok) printf("    ✅ %s\n", desc);
    else  { _prb_fallos++; printf("    ❌ FALLO: %s\n", desc); }
}

static Valor *fn_prb_suite(Valor **a, int n) {
    printf("  ▶ %s\n", prb_desc(a, n));
    return valor_nulo();
}

static Valor *fn_prb_afirmar(Valor **a, int n) {
    int ok = n > 1 && a[1]->tipo == VAL_BOOLEANO && a[1]->booleano;
    prb_registrar(ok, prb_desc(a, n));
    return valor_nulo();
}

static Valor *fn_prb_igual(Valor **a, int n) {
    if (n < 3) return valor_nulo();
    int ok = prb_iguales(a[1], a[2]);
    prb_registrar(ok, prb_desc(a, n));
    if (!ok) {
        char *e = valor_a_texto_repl(a[1]), *o = valor_a_texto_repl(a[2]);
        printf("       esperado: %s\n       obtenido: %s\n", e, o);
        free(e); free(o);
    }
    return valor_nulo();
}

static Valor *fn_prb_lanza(Valor **a, int n) {
    if (n < 2 || a[1]->tipo != VAL_FUNCION) {
        prb_registrar(0, prb_desc(a, n));
        return valor_nulo();
    }
    hay_error = 0;
    Valor *r = interprete_llamar_funcion(a[1], NULL, 0);
    int lanzo = hay_error;
    if (r) valor_destruir(r);
    if (lanzo) {
        hay_error = 0;
        valor_destruir(valor_error);
        valor_error = NULL;
    }
    prb_registrar(lanzo, prb_desc(a, n));
    return valor_nulo();
}

static Valor *fn_prb_resumen(Valor **a, int n) {
    (void)a; (void)n;
    printf("\n  %d pruebas: %d correctas, %d fallidas\n",
           _prb_total, _prb_total - _prb_fallos, _prb_fallos);
    fflush(stdout);
    if (_prb_fallos > 0) exit(1);
    return valor_nulo();
}

static void cargar_pruebas(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"suite",    fn_prb_suite,    1},
        {"afirmar",  fn_prb_afirmar,  2},
        {"igual",    fn_prb_igual,    3},
        {"lanza",    fn_prb_lanza,    2},
        {"resumen",  fn_prb_resumen,  0},
    };
    registrar_modulo_diccionario(entorno, "pruebas",
        fns, 5, NULL, NULL, 0);
}

/* ─────────────────────────────────────────
   MÓDULO: sqlite
   Carga libsqlite3 en tiempo de ejecución (dlopen / LoadLibrary): Lince no la
   necesita para compilarse, y si no está instalada solo falla 'importar "sqlite"'.
   Las conexiones se manejan como números.
───────────────────────────────────────── */
#ifdef _WIN32
#define SQL_ABRIR_LIB()      ((void *)LoadLibraryA("sqlite3.dll"))
#define SQL_SIMBOLO(h, n)    ((void *)GetProcAddress((HMODULE)(h), n))
#else
#include <dlfcn.h>
static void *sql_abrir_lib(void) {
    static const char *const nombres[] = {
        "libsqlite3.so.0", "libsqlite3.so", "libsqlite3.dylib", NULL };
    for (int i = 0; nombres[i]; i++) {
        void *h = dlopen(nombres[i], RTLD_NOW);
        if (h) return h;
    }
    return NULL;
}
#define SQL_ABRIR_LIB()      sql_abrir_lib()
#define SQL_SIMBOLO(h, n)    dlsym(h, n)
#endif

#define SQL_ROW  100
#define SQL_DONE 101
#define SQL_MAX_CONEXIONES 32

static struct {
    int (*open)(const char *, void **);
    int (*close)(void *);
    int (*prepare)(void *, const char *, int, void **, const char **);
    int (*step)(void *);
    int (*finalize)(void *);
    int (*bind_double)(void *, int, double);
    int (*bind_text)(void *, int, const char *, int, void *);
    int (*bind_null)(void *, int);
    int (*column_count)(void *);
    const char *(*column_name)(void *, int);
    int (*column_type)(void *, int);
    double (*column_double)(void *, int);
    const unsigned char *(*column_text)(void *, int);
    const char *(*errmsg)(void *);
    int (*changes)(void *);
    long long (*last_id)(void *);
} _sql;
static int   _sql_cargada = 0;
static void *_sql_bd[SQL_MAX_CONEXIONES];

static int sql_cargar(void) {
    if (_sql_cargada) return 1;
    void *h = SQL_ABRIR_LIB();
    if (!h) return 0;
#define SQL_FN(campo, nombre) \
    *(void **)(&_sql.campo) = SQL_SIMBOLO(h, nombre); if (!_sql.campo) return 0;
    SQL_FN(open, "sqlite3_open")
    SQL_FN(close, "sqlite3_close")
    SQL_FN(prepare, "sqlite3_prepare_v2")
    SQL_FN(step, "sqlite3_step")
    SQL_FN(finalize, "sqlite3_finalize")
    SQL_FN(bind_double, "sqlite3_bind_double")
    SQL_FN(bind_text, "sqlite3_bind_text")
    SQL_FN(bind_null, "sqlite3_bind_null")
    SQL_FN(column_count, "sqlite3_column_count")
    SQL_FN(column_name, "sqlite3_column_name")
    SQL_FN(column_type, "sqlite3_column_type")
    SQL_FN(column_double, "sqlite3_column_double")
    SQL_FN(column_text, "sqlite3_column_text")
    SQL_FN(errmsg, "sqlite3_errmsg")
    SQL_FN(changes, "sqlite3_changes")
    SQL_FN(last_id, "sqlite3_last_insert_rowid")
#undef SQL_FN
    _sql_cargada = 1;
    return 1;
}

static Valor *sql_error(const char *tipo, const char *msg) {
    hay_error = 1;
    valor_error = valor_error_nuevo(tipo, msg, 0);
    return valor_nulo();
}

/* Conexión indicada por el argumento i, o NULL (con el error ya puesto) */
static void *sql_conexion(Valor **a, int n, int i) {
    if (i >= n || a[i]->tipo != VAL_NUMERO) { sql_error("ErrorTipo", "se esperaba una conexión de base de datos"); return NULL; }
    int k = (int)a[i]->numero;
    if (k < 0 || k >= SQL_MAX_CONEXIONES || !_sql_bd[k]) { sql_error("ErrorBD", "la conexión está cerrada o no existe"); return NULL; }
    return _sql_bd[k];
}

static Valor *fn_sql_abrir(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return sql_error("ErrorTipo", "abrir() espera la ruta del archivo");
    if (!sql_cargar())
        return sql_error("ErrorBD", "no se encuentra libsqlite3: instálala (por ejemplo 'sudo apt install libsqlite3-0')");
    int k = 0;
    while (k < SQL_MAX_CONEXIONES && _sql_bd[k]) k++;
    if (k == SQL_MAX_CONEXIONES) return sql_error("ErrorBD", "demasiadas conexiones abiertas");
    const char *ruta = strcmp(a[0]->texto, ":memoria:") == 0 ? ":memory:" : a[0]->texto;
    void *db = NULL;
    if (_sql.open(ruta, &db) != 0) {
        char msg[600];
        snprintf(msg, sizeof(msg), "no se pudo abrir '%s': %s", a[0]->texto, db ? _sql.errmsg(db) : "sin memoria");
        if (db) _sql.close(db);
        return sql_error("ErrorBD", msg);
    }
    _sql_bd[k] = db;
    return valor_numero(k);
}

static Valor *fn_sql_cerrar(Valor **a, int n) {
    void *db = sql_conexion(a, n, 0);
    if (!db) return valor_nulo();
    _sql.close(db);
    _sql_bd[(int)a[0]->numero] = NULL;
    return valor_nulo();
}

/* Prepara 'sql' y le liga los parámetros (lista opcional en a[2]) */
static void *sql_preparar(void *db, const char *sql, const char **resto, Valor **a, int n) {
    void *st = NULL;
    if (_sql.prepare(db, sql, -1, &st, resto) != 0) {
        sql_error("ErrorBD", _sql.errmsg(db));
        return NULL;
    }
    if (!st) return NULL;                              /* texto vacío o solo comentarios */
    if (n > 2 && a[2]->tipo == VAL_LISTA) {
        for (int i = 0; i < a[2]->lista.cantidad; i++) {
            Valor *p = a[2]->lista.elementos[i];
            switch (p->tipo) {
                case VAL_NUMERO:   _sql.bind_double(st, i + 1, p->numero); break;
                case VAL_BOOLEANO: _sql.bind_double(st, i + 1, p->booleano ? 1 : 0); break;
                case VAL_TEXTO:    _sql.bind_text(st, i + 1, p->texto, -1, (void *)-1); break;
                case VAL_NULO:     _sql.bind_null(st, i + 1); break;
                default:
                    _sql.finalize(st);
                    sql_error("ErrorTipo", "los parámetros solo pueden ser número, texto, lógico o nulo");
                    return NULL;
            }
        }
    }
    return st;
}

/* ejecutar(bd, sql[, parametros]): una o varias sentencias; devuelve las filas afectadas por la última */
static Valor *fn_sql_ejecutar(Valor **a, int n) {
    void *db = sql_conexion(a, n, 0);
    if (!db) return valor_nulo();
    if (n < 2 || a[1]->tipo != VAL_TEXTO) return sql_error("ErrorTipo", "ejecutar() espera (conexión, sql[, parámetros])");
    const char *sql = a[1]->texto;
    int afectadas = 0;
    while (sql && *sql) {
        const char *resto = NULL;
        void *st = sql_preparar(db, sql, &resto, a, n);
        if (hay_error) return valor_nulo();
        if (st) {
            int r;
            while ((r = _sql.step(st)) == SQL_ROW) { }
            if (r != SQL_DONE) {
                char msg[600];
                snprintf(msg, sizeof(msg), "%s", _sql.errmsg(db));
                _sql.finalize(st);
                return sql_error("ErrorBD", msg);
            }
            afectadas = _sql.changes(db);
            _sql.finalize(st);
        }
        sql = resto;
    }
    return valor_numero(afectadas);
}

/* consultar(bd, sql[, parametros]): lista de diccionarios, uno por fila */
static Valor *fn_sql_consultar(Valor **a, int n) {
    void *db = sql_conexion(a, n, 0);
    if (!db) return valor_nulo();
    if (n < 2 || a[1]->tipo != VAL_TEXTO) return sql_error("ErrorTipo", "consultar() espera (conexión, sql[, parámetros])");
    void *st = sql_preparar(db, a[1]->texto, NULL, a, n);
    if (hay_error) return valor_nulo();
    Valor *filas = valor_lista_crear();
    if (!st) return filas;
    int r, cols = _sql.column_count(st);
    while ((r = _sql.step(st)) == SQL_ROW) {
        Valor *fila = valor_diccionario_crear();
        for (int c = 0; c < cols; c++) {
            Valor *v;
            switch (_sql.column_type(st, c)) {
                case 1: case 2: v = valor_numero(_sql.column_double(st, c)); break;
                case 5:         v = valor_nulo(); break;
                default: {
                    const unsigned char *t = _sql.column_text(st, c);
                    v = valor_texto(t ? (const char *)t : "");
                }
            }
            valor_diccionario_agregar(fila, _sql.column_name(st, c), v);
        }
        lista_agregar(filas, fila);
    }
    if (r != SQL_DONE) {
        char msg[600];
        snprintf(msg, sizeof(msg), "%s", _sql.errmsg(db));
        _sql.finalize(st);
        valor_destruir(filas);
        return sql_error("ErrorBD", msg);
    }
    _sql.finalize(st);
    return filas;
}

static Valor *fn_sql_ultimo_id(Valor **a, int n) {
    void *db = sql_conexion(a, n, 0);
    if (!db) return valor_nulo();
    return valor_numero((double)_sql.last_id(db));
}

static void cargar_sqlite(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"abrir",     fn_sql_abrir,     1},
        {"cerrar",    fn_sql_cerrar,    1},
        {"ejecutar",  fn_sql_ejecutar, -1},
        {"consultar", fn_sql_consultar, -1},
        {"ultimo_id", fn_sql_ultimo_id, 1},
    };
    registrar_modulo_diccionario(entorno, "sqlite",
        fns, 5, NULL, NULL, 0);
}

/* ─────────────────────────────────────────
   MÓDULO: cripto
   Hashes, HMAC, base64 y hexadecimal (src/cripto.h). Trabajan con texto.
───────────────────────────────────────── */
#include "cripto.h"

static Valor *cr_txt_arg(Valor **a, int n, int i, const char *fn) {
    if (i < n && a[i]->tipo == VAL_TEXTO) return a[i];
    char msg[96];
    snprintf(msg, sizeof(msg), "%s() espera texto", fn);
    hay_error = 1;
    valor_error = valor_error_nuevo("ErrorTipo", msg, 0);
    return NULL;
}

static Valor *cr_devolver(char *s) {
    if (!s) return valor_nulo();
    Valor *v = valor_texto(s);
    free(s);
    return v;
}

#define CR_HASH(nombre, tam, fn) \
static Valor *fn_cr_##nombre(Valor **a, int n) { \
    Valor *t = cr_txt_arg(a, n, 0, #nombre); \
    if (!t) return valor_nulo(); \
    unsigned char h[tam]; \
    fn((const unsigned char *)t->texto, strlen(t->texto), h); \
    return cr_devolver(cr_hex(h, tam)); \
}
CR_HASH(sha256, 32, cr_sha256)
CR_HASH(sha1,   20, cr_sha1)
CR_HASH(md5,    16, cr_md5)

static Valor *fn_cr_hmac_sha256(Valor **a, int n) {
    Valor *k = cr_txt_arg(a, n, 0, "hmac_sha256");
    Valor *t = k ? cr_txt_arg(a, n, 1, "hmac_sha256") : NULL;
    if (!k || !t) return valor_nulo();
    unsigned char h[32];
    cr_hmac_sha256((const unsigned char *)k->texto, strlen(k->texto),
                   (const unsigned char *)t->texto, strlen(t->texto), h);
    return cr_devolver(cr_hex(h, 32));
}

static Valor *fn_cr_base64(Valor **a, int n) {
    Valor *t = cr_txt_arg(a, n, 0, "base64");
    return t ? cr_devolver(cr_base64((const unsigned char *)t->texto, strlen(t->texto))) : valor_nulo();
}
static Valor *fn_cr_desbase64(Valor **a, int n) {
    Valor *t = cr_txt_arg(a, n, 0, "desbase64");
    return t ? cr_devolver(cr_desbase64(t->texto)) : valor_nulo();
}
static Valor *fn_cr_hex(Valor **a, int n) {
    Valor *t = cr_txt_arg(a, n, 0, "hex");
    return t ? cr_devolver(cr_hex((const unsigned char *)t->texto, strlen(t->texto))) : valor_nulo();
}
static Valor *fn_cr_deshex(Valor **a, int n) {
    Valor *t = cr_txt_arg(a, n, 0, "deshex");
    return t ? cr_devolver(cr_deshex(t->texto)) : valor_nulo();
}

static void cargar_cripto(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"sha256",      fn_cr_sha256,      1},
        {"sha1",        fn_cr_sha1,        1},
        {"md5",         fn_cr_md5,         1},
        {"hmac_sha256", fn_cr_hmac_sha256, 2},
        {"base64",      fn_cr_base64,      1},
        {"desbase64",   fn_cr_desbase64,   1},
        {"hex",         fn_cr_hex,         1},
        {"deshex",      fn_cr_deshex,      1},
    };
    registrar_modulo_diccionario(entorno, "cripto",
        fns, 8, NULL, NULL, 0);
}

/* ─────────────────────────────────────────
   MÓDULO: tareas
   Concurrencia real con procesos: cada tarea corre una función en un proceso
   hijo (fork) y devuelve su resultado por una tubería. No comparten memoria,
   así que no hay carreras: los datos viajan copiados. Solo POSIX.
   Se pueden enviar números, textos, lógicos, nulo, listas y diccionarios.
───────────────────────────────────────── */
static inline Valor *valor_retener_pub(Valor *v) { if (v) v->refs++; return v; }

#ifndef _WIN32
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>

typedef struct { unsigned char *d; size_t n, cap; } TBuf;

static void tb_poner(TBuf *b, const void *p, size_t k) {
    if (b->n + k > b->cap) {
        b->cap = (b->n + k) * 2 + 64;
        b->d = realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, p, k);
    b->n += k;
}
static void tb_u32(TBuf *b, uint32_t v) { tb_poner(b, &v, 4); }

/* Serializa un valor; 0 si contiene algo que no se puede enviar */
static int tarea_ser(Valor *v, TBuf *b) {
    unsigned char t;
    switch (v ? v->tipo : VAL_NULO) {
        case VAL_NULO:     t = 'Z'; tb_poner(b, &t, 1); return 1;
        case VAL_NUMERO:   t = 'N'; tb_poner(b, &t, 1); tb_poner(b, &v->numero, sizeof(double)); return 1;
        case VAL_BOOLEANO: t = v->booleano ? 'V' : 'F'; tb_poner(b, &t, 1); return 1;
        case VAL_TEXTO: {
            t = 'T'; tb_poner(b, &t, 1);
            uint32_t k = (uint32_t)strlen(v->texto);
            tb_u32(b, k); tb_poner(b, v->texto, k);
            return 1;
        }
        case VAL_LISTA:
            t = 'L'; tb_poner(b, &t, 1); tb_u32(b, (uint32_t)v->lista.cantidad);
            for (int i = 0; i < v->lista.cantidad; i++)
                if (!tarea_ser(v->lista.elementos[i], b)) return 0;
            return 1;
        case VAL_DICCIONARIO:
            if (v->es_modulo) return 0;
            t = 'D'; tb_poner(b, &t, 1); tb_u32(b, (uint32_t)v->diccionario.cantidad);
            for (int i = 0; i < v->diccionario.cantidad; i++) {
                uint32_t k = (uint32_t)strlen(v->diccionario.claves[i]);
                tb_u32(b, k); tb_poner(b, v->diccionario.claves[i], k);
                if (!tarea_ser(v->diccionario.valores[i], b)) return 0;
            }
            return 1;
        default: return 0;
    }
}

static Valor *tarea_deser(const unsigned char *d, size_t n, size_t *p) {
    if (*p >= n) return NULL;
    unsigned char t = d[(*p)++];
    switch (t) {
        case 'Z': return valor_nulo();
        case 'V': return valor_booleano(1);
        case 'F': return valor_booleano(0);
        case 'N': {
            double x;
            if (*p + sizeof x > n) return NULL;
            memcpy(&x, d + *p, sizeof x); *p += sizeof x;
            return valor_numero(x);
        }
        case 'T': {
            uint32_t k;
            if (*p + 4 > n) return NULL;
            memcpy(&k, d + *p, 4); *p += 4;
            if (*p + k > n) return NULL;
            char *s = malloc(k + 1);
            memcpy(s, d + *p, k); s[k] = '\0'; *p += k;
            Valor *r = valor_texto(s);
            free(s);
            return r;
        }
        case 'L': {
            uint32_t c;
            if (*p + 4 > n) return NULL;
            memcpy(&c, d + *p, 4); *p += 4;
            Valor *l = valor_lista_crear();
            for (uint32_t i = 0; i < c; i++) {
                Valor *e = tarea_deser(d, n, p);
                if (!e) { valor_destruir(l); return NULL; }
                lista_agregar(l, e);
            }
            return l;
        }
        case 'D': {
            uint32_t c;
            if (*p + 4 > n) return NULL;
            memcpy(&c, d + *p, 4); *p += 4;
            Valor *dic = valor_diccionario_crear();
            for (uint32_t i = 0; i < c; i++) {
                uint32_t k;
                if (*p + 4 > n) { valor_destruir(dic); return NULL; }
                memcpy(&k, d + *p, 4); *p += 4;
                if (*p + k > n) { valor_destruir(dic); return NULL; }
                char *clave = malloc(k + 1);
                memcpy(clave, d + *p, k); clave[k] = '\0'; *p += k;
                Valor *e = tarea_deser(d, n, p);
                if (!e) { free(clave); valor_destruir(dic); return NULL; }
                valor_diccionario_agregar(dic, clave, e);
                free(clave);
            }
            return dic;
        }
        default: return NULL;
    }
}

#define MAX_TAREAS 256
typedef struct {
    int     usada;
    pid_t   pid;
    int     fd;            /* extremo de lectura de la tubería */
    int     fin;           /* ya se leyó todo (EOF) */
    int     estado;        /* estado de waitpid, cuando se recogió */
    TBuf    datos;
} Tarea;
static Tarea _tareas[MAX_TAREAS];

static Valor *tarea_error(const char *msg) {
    hay_error = 1;
    valor_error = valor_error_nuevo("ErrorTarea", msg, 0);
    return valor_nulo();
}

static Tarea *tarea_de(Valor **a, int n, int i) {
    if (i >= n || a[i]->tipo != VAL_NUMERO) { tarea_error("se esperaba una tarea"); return NULL; }
    int k = (int)a[i]->numero;
    if (k < 0 || k >= MAX_TAREAS || !_tareas[k].usada) { tarea_error("la tarea no existe o ya se recogió su resultado"); return NULL; }
    return &_tareas[k];
}

/* Lee lo que haya en la tubería. bloquear: hasta el EOF */
static void tarea_leer(Tarea *t, int bloquear) {
    while (!t->fin) {
        if (!bloquear) {
            struct pollfd pf = { t->fd, POLLIN, 0 };
            if (poll(&pf, 1, 0) <= 0) return;
        }
        unsigned char tmp[65536];
        ssize_t r = read(t->fd, tmp, sizeof tmp);
        if (r > 0) tb_poner(&t->datos, tmp, (size_t)r);
        else if (r == 0) t->fin = 1;
        else if (errno != EINTR) t->fin = 1;
    }
}

static void tarea_liberar(Tarea *t) {
    if (t->fd >= 0) close(t->fd);
    free(t->datos.d);
    memset(t, 0, sizeof *t);
}

static Valor *fn_tar_iniciar(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_FUNCION) return tarea_error("iniciar() espera una función");
    int nargs = 0;
    if (n > 1) {
        if (a[1]->tipo != VAL_LISTA) return tarea_error("los argumentos de iniciar() van en una lista");
        nargs = a[1]->lista.cantidad;
    }
    int k = 0;
    while (k < MAX_TAREAS && _tareas[k].usada) k++;
    if (k == MAX_TAREAS) return tarea_error("demasiadas tareas a la vez");
    int fds[2];
    if (pipe(fds) != 0) return tarea_error("no se pudo crear la tubería");
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return tarea_error("no se pudo crear el proceso"); }
    if (pid == 0) {
        close(fds[0]);
        for (int i = 0; i < MAX_TAREAS; i++) if (_tareas[i].usada) close(_tareas[i].fd);
        signal(SIGPIPE, SIG_DFL);
        Valor **args = malloc(sizeof(Valor *) * (size_t)(nargs + 1));
        for (int i = 0; i < nargs; i++) args[i] = valor_retener_pub(a[1]->lista.elementos[i]);
        hay_error = 0;
        Valor *r = interprete_llamar_funcion(a[0], args, nargs);
        TBuf b = {0};
        unsigned char marca = 'R';
        if (hay_error) {
            marca = 'E';
            tb_poner(&b, &marca, 1);
            if (valor_error && valor_error->tipo == VAL_ERROR) {
                tb_poner(&b, valor_error->error->tipo, strlen(valor_error->error->tipo));
                tb_poner(&b, ": ", 2);
                tb_poner(&b, valor_error->error->mensaje, strlen(valor_error->error->mensaje));
            } else {
                const char *m = "error desconocido";
                tb_poner(&b, m, strlen(m));
            }
        } else {
            tb_poner(&b, &marca, 1);
            if (!tarea_ser(r, &b)) {
                b.n = 0; marca = 'E';
                tb_poner(&b, &marca, 1);
                const char *m = "el resultado de la tarea no se puede enviar (solo números, textos, lógicos, nulo, listas y diccionarios)";
                tb_poner(&b, m, strlen(m));
            }
        }
        size_t off = 0;
        while (off < b.n) {
            ssize_t w = write(fds[1], b.d + off, b.n - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
        fflush(stdout);
        _exit(0);
    }
    close(fds[1]);
    Tarea *t = &_tareas[k];
    memset(t, 0, sizeof *t);
    t->usada = 1; t->pid = pid; t->fd = fds[0];
    return valor_numero(k);
}

static Valor *fn_tar_listo(Valor **a, int n) {
    Tarea *t = tarea_de(a, n, 0);
    if (!t) return valor_nulo();
    tarea_leer(t, 0);
    return valor_booleano(t->fin);
}

/* Recoge el resultado de la tarea (bloqueando) y la libera */
static Valor *tarea_recoger(Tarea *t) {
    tarea_leer(t, 1);
    int estado = 0;
    waitpid(t->pid, &estado, 0);
    Valor *res;
    if (t->datos.n == 0) {
        res = tarea_error("la tarea terminó sin dar resultado (¿se cayó el proceso?)");
    } else if (t->datos.d[0] == 'E') {
        char *m = malloc(t->datos.n);
        memcpy(m, t->datos.d + 1, t->datos.n - 1); m[t->datos.n - 1] = '\0';
        res = tarea_error(m);
        free(m);
    } else {
        size_t p = 1;
        res = tarea_deser(t->datos.d, t->datos.n, &p);
        if (!res) res = tarea_error("resultado de la tarea dañado");
    }
    tarea_liberar(t);
    return res;
}

static Valor *fn_tar_esperar(Valor **a, int n) {
    Tarea *t = tarea_de(a, n, 0);
    if (!t) return valor_nulo();
    return tarea_recoger(t);
}

static Valor *fn_tar_cancelar(Valor **a, int n) {
    Tarea *t = tarea_de(a, n, 0);
    if (!t) return valor_nulo();
    kill(t->pid, SIGKILL);
    int estado;
    waitpid(t->pid, &estado, 0);
    tarea_liberar(t);
    return valor_nulo();
}

static Valor *fn_tar_cpus(Valor **a, int n) {
    (void)a; (void)n;
    long c = sysconf(_SC_NPROCESSORS_ONLN);
    return valor_numero(c > 0 ? (double)c : 1);
}

/* mapear(lista, funcion[, maximo]): aplica la función a cada elemento en
   paralelo (como mucho 'maximo' a la vez; por defecto, las CPUs) y devuelve
   los resultados en el mismo orden. */
static Valor *fn_tar_mapear(Valor **a, int n) {
    if (n < 2 || a[0]->tipo != VAL_LISTA || a[1]->tipo != VAL_FUNCION)
        return tarea_error("mapear() espera (lista, función[, máximo])");
    int total = a[0]->lista.cantidad;
    int maximo = (n > 2 && a[2]->tipo == VAL_NUMERO && a[2]->numero >= 1) ? (int)a[2]->numero : 0;
    if (!maximo) { long c = sysconf(_SC_NPROCESSORS_ONLN); maximo = c > 0 ? (int)c : 1; }
    if (maximo > MAX_TAREAS - 8) maximo = MAX_TAREAS - 8;
    int *ids = malloc(sizeof(int) * (size_t)(total + 1));
    Valor **res = calloc((size_t)total + 1, sizeof(Valor *));
    int hecho = 0, lanzadas = 0;
    int fallo = 0;
    while (hecho < total && !fallo) {
        while (lanzadas < total && lanzadas - hecho < maximo) {
            Valor *argl = valor_lista_crear();
            lista_agregar(argl, valor_retener_pub(a[0]->lista.elementos[lanzadas]));
            Valor *par[2] = { a[1], argl };
            Valor *id = fn_tar_iniciar(par, 2);
            valor_destruir(argl);
            if (hay_error) { fallo = 1; break; }
            ids[lanzadas++] = (int)id->numero;
            valor_destruir(id);
        }
        if (fallo) break;
        res[hecho] = tarea_recoger(&_tareas[ids[hecho]]);
        if (hay_error) { fallo = 1; hecho++; break; }
        hecho++;
    }
    if (fallo) {
        for (int i = hecho; i < lanzadas; i++) {
            Tarea *t = &_tareas[ids[i]];
            if (t->usada) { kill(t->pid, SIGKILL); int st; waitpid(t->pid, &st, 0); tarea_liberar(t); }
        }
        for (int i = 0; i < total; i++) if (res[i]) valor_destruir(res[i]);
        free(ids); free(res);
        return valor_nulo();
    }
    Valor *lista = valor_lista_crear();
    for (int i = 0; i < total; i++) lista_agregar(lista, res[i]);
    free(ids); free(res);
    return lista;
}
#else
static Valor *tarea_no_disponible(Valor **a, int n) {
    (void)a; (void)n;
    hay_error = 1;
    valor_error = valor_error_nuevo("ErrorTarea", "el módulo tareas no está disponible en Windows", 0);
    return valor_nulo();
}
#define fn_tar_iniciar   tarea_no_disponible
#define fn_tar_esperar   tarea_no_disponible
#define fn_tar_listo     tarea_no_disponible
#define fn_tar_cancelar  tarea_no_disponible
#define fn_tar_mapear    tarea_no_disponible
#define fn_tar_cpus      tarea_no_disponible
#endif

static void cargar_tareas(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"iniciar",  fn_tar_iniciar,  -1},
        {"esperar",  fn_tar_esperar,   1},
        {"listo",    fn_tar_listo,     1},
        {"cancelar", fn_tar_cancelar,  1},
        {"mapear",   fn_tar_mapear,   -1},
        {"cpus",     fn_tar_cpus,      0},
    };
    registrar_modulo_diccionario(entorno, "tareas",
        fns, 6, NULL, NULL, 0);
}

/* ─────────────────────────────────────────
   MÓDULO: plantillas
   Rellena HTML (o cualquier texto) con datos de Lince.

     {{ nombre }}            valor, con los caracteres de HTML escapados
     {{{ nombre }}}          valor tal cual (HTML ya preparado)
     {{ usuario.nombre }}    campo de un diccionario; {{ lista.0 }} elemento
     {{ lista.longitud }}    número de elementos (también de textos y diccionarios)
     {{#si cond}} … {{#sino}} … {{/si}}      ('no cond' niega)
     {{#para x en lista}} … {{/para}}        ({{@indice}} desde 0, {{@numero}} desde 1)
     {{> otra.html}}         incluye otra plantilla (junto a la actual)
     {{! comentario }}
───────────────────────────────────────── */
typedef struct { char *d; size_t n, cap; } PBuf;

static void pb_poner(PBuf *b, const char *p, size_t k) {
    if (b->n + k + 1 > b->cap) {
        b->cap = (b->n + k + 1) * 2;
        b->d = realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, p, k);
    b->n += k;
    b->d[b->n] = '\0';
}

#define PL_MAX_VARS 48
#define PL_MAX_TEMPS 64
typedef struct { const char *nombre; Valor *valor; } PVar;
typedef struct {
    Valor      *raiz;
    PVar        vars[PL_MAX_VARS];
    int         nvars;
    Valor      *temps[PL_MAX_TEMPS];
    int         ntemps;
    const char *dir;            /* carpeta de la plantilla, para {{> parcial}} */
    const char *base;           /* inicio del texto que se está procesando (para la línea) */
    int         prof;
    char        error[480];
} PCtx;

static int pl_fallo(PCtx *c, const char *donde, const char *fmt, const char *arg) {
    int linea = 1;
    for (const char *p = c->base; p && p < donde; p++) if (*p == '\n') linea++;
    char tmp[300];
    snprintf(tmp, sizeof tmp, fmt, arg ? arg : "");
    snprintf(c->error, sizeof c->error, "plantilla, línea %d: %s", linea, tmp);
    return 0;
}

static Valor *pl_temp(PCtx *c, Valor *v) {
    if (c->ntemps < PL_MAX_TEMPS) c->temps[c->ntemps++] = v; else valor_destruir(v);
    return v;
}

/* Valor de un camino «a.b.0» o NULL si no existe (el valor es prestado) */
static Valor *pl_buscar(PCtx *c, const char *ruta, size_t k) {
    char seg[128];
    size_t i = 0;
    Valor *v = NULL;
    int primero = 1;
    while (i <= k) {
        size_t j = i;
        while (j < k && ruta[j] != '.') j++;
        size_t l = j - i;
        if (l == 0 || l >= sizeof seg) return NULL;
        memcpy(seg, ruta + i, l); seg[l] = '\0';
        if (primero) {
            primero = 0;
            for (int x = c->nvars - 1; x >= 0 && !v; x--)
                if (strcmp(c->vars[x].nombre, seg) == 0) v = c->vars[x].valor;
            if (!v && c->raiz && c->raiz->tipo == VAL_DICCIONARIO) {
                int p = valor_diccionario_buscar(c->raiz, seg);
                if (p >= 0) v = c->raiz->diccionario.valores[p];
            }
            if (!v) return NULL;
        } else if (v->tipo == VAL_DICCIONARIO) {
            int p = valor_diccionario_buscar(v, seg);
            if (p >= 0) { v = v->diccionario.valores[p]; }
            else if (strcmp(seg, "longitud") == 0) v = pl_temp(c, valor_numero(v->diccionario.cantidad));
            else return NULL;
        } else if (v->tipo == VAL_LISTA) {
            char *fin;
            long idx = strtol(seg, &fin, 10);
            if (*fin == '\0') {
                if (idx < 0 || idx >= v->lista.cantidad) return NULL;
                v = v->lista.elementos[idx];
            } else if (strcmp(seg, "longitud") == 0) v = pl_temp(c, valor_numero(v->lista.cantidad));
            else return NULL;
        } else if (v->tipo == VAL_TEXTO && strcmp(seg, "longitud") == 0) {
            v = pl_temp(c, valor_numero(utf8_longitud(v->texto)));
        } else return NULL;
        i = j + 1;
        if (j >= k) break;
    }
    return v;
}

static int pl_verdadero(Valor *v) {
    switch (v->tipo) {
        case VAL_NULO:        return 0;
        case VAL_BOOLEANO:    return v->booleano;
        case VAL_NUMERO:      return v->numero != 0;
        case VAL_TEXTO:       return v->texto[0] != '\0';
        case VAL_LISTA:       return v->lista.cantidad > 0;
        case VAL_DICCIONARIO: return v->diccionario.cantidad > 0;
        default:              return 1;
    }
}

static void pl_valor(PBuf *out, Valor *v, int escapar) {
    char tmp[64];
    const char *s = "";
    char *rep = NULL;
    switch (v->tipo) {
        case VAL_NULO: s = ""; break;
        case VAL_TEXTO: s = v->texto; break;
        case VAL_BOOLEANO: s = v->booleano ? "verdadero" : "falso"; break;
        case VAL_NUMERO:
            if (v->numero == (long long)v->numero) snprintf(tmp, sizeof tmp, "%lld", (long long)v->numero);
            else snprintf(tmp, sizeof tmp, "%g", v->numero);
            s = tmp; break;
        default: rep = valor_a_texto_repl(v); s = rep; break;
    }
    if (!escapar) { pb_poner(out, s, strlen(s)); free(rep); return; }
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '&':  pb_poner(out, "&amp;", 5); break;
            case '<':  pb_poner(out, "&lt;", 4); break;
            case '>':  pb_poner(out, "&gt;", 4); break;
            case '"':  pb_poner(out, "&quot;", 6); break;
            case '\'': pb_poner(out, "&#39;", 5); break;
            default:   pb_poner(out, p, 1);
        }
    }
    free(rep);
}

static const char *pl_saltar_esp(const char *p, const char *fin) {
    while (p < fin && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}
static const char *pl_fin_recorte(const char *ini, const char *fin) {
    while (fin > ini && (fin[-1] == ' ' || fin[-1] == '\t' || fin[-1] == '\n' || fin[-1] == '\r')) fin--;
    return fin;
}

/* ¿Está la etiqueta [t0,t1) sola en su línea (sin nada más que espacios)? Así
   las líneas de {{#si}}, {{/si}}, {{#para}}… no dejan líneas vacías en el
   resultado. *desde = inicio de esa línea; *hasta = inicio de la siguiente. */
static int pl_sola(const char *base, const char *t0, const char *t1, const char *fin,
                   const char **desde, const char **hasta) {
    const char *a = t0;
    while (a > base && (a[-1] == ' ' || a[-1] == '\t')) a--;
    if (a > base && a[-1] != '\n') return 0;
    const char *b = t1;
    while (b < fin && (*b == ' ' || *b == '\t' || *b == '\r')) b++;
    if (b < fin && *b != '\n') return 0;
    *desde = a;
    *hasta = b < fin ? b + 1 : b;
    return 1;
}

static int pl_render(PCtx *c, const char *p, const char *fin, PBuf *out);

/* Busca el cierre «{{/tipo}}» que empareja con la apertura ya leída (en p).
   Con 'sino' no nulo, anota dónde está el «{{#sino}}» de primer nivel. */
static const char *pl_cierre(const char *p, const char *fin, const char *tipo,
                             const char **sino, const char **tras_sino) {
    char ap[16], ci[16];
    snprintf(ap, sizeof ap, "{{#%s", tipo);
    snprintf(ci, sizeof ci, "{{/%s}}", tipo);
    size_t lap = strlen(ap), lci = strlen(ci);
    int prof = 1;
    if (sino) *sino = NULL;
    while (p < fin) {
        const char *q = memchr(p, '{', (size_t)(fin - p));
        if (!q) return NULL;
        if (q + lci <= fin && strncmp(q, ci, lci) == 0) {
            if (--prof == 0) return q;
            p = q + lci;
        } else if (q + lap < fin && strncmp(q, ap, lap) == 0 &&
                   (q[lap] == ' ' || q[lap] == '}')) {
            prof++;
            p = q + lap;
        } else if (sino && prof == 1 && q + 9 <= fin && strncmp(q, "{{#sino}}", 9) == 0 &&
                   strcmp(tipo, "si") == 0) {
            *sino = q; *tras_sino = q + 9;
            p = q + 9;
        } else p = q + 1;
    }
    return NULL;
}

static int pl_render(PCtx *c, const char *p, const char *fin, PBuf *out) {
    if (c->prof > 24) return pl_fallo(c, p, "demasiado anidamiento (¿una plantilla se incluye a sí misma?)%s", NULL);
    c->prof++;
    while (p < fin) {
        const char *q = p;
        while (q + 1 < fin && !(q[0] == '{' && q[1] == '{')) q++;
        if (q + 1 >= fin) { pb_poner(out, p, (size_t)(fin - p)); break; }
        pb_poner(out, p, (size_t)(q - p));

        int triple = (q + 2 < fin && q[2] == '{');
        const char *ini = q + (triple ? 3 : 2);
        const char *cierre = triple ? "}}}" : "}}";
        size_t lc = strlen(cierre);
        const char *e = ini;
        while (e + lc <= fin && strncmp(e, cierre, lc) != 0) e++;
        if (e + lc > fin) { c->prof--; return pl_fallo(c, q, "falta cerrar «%s»", triple ? "{{{" : "{{"); }
        p = e + lc;

        if (!triple && *ini == '!') {                                /* comentario */
            const char *d0, *h0;
            if (pl_sola(c->base, q, p, fin, &d0, &h0)) { out->n -= (size_t)(q - d0); out->d[out->n] = '\0'; p = h0; }
            continue;
        }

        const char *a = pl_saltar_esp(ini, e), *b = pl_fin_recorte(a, e);
        if (!triple && *a == '>') {                                  /* parcial */
            const char *n0 = pl_saltar_esp(a + 1, b);
            char nombre[512], ruta[1024];
            size_t l = (size_t)(b - n0);
            if (l == 0 || l >= sizeof nombre) { c->prof--; return pl_fallo(c, q, "parcial sin nombre válido%s", NULL); }
            memcpy(nombre, n0, l); nombre[l] = '\0';
            snprintf(ruta, sizeof ruta, "%s%s%s", c->dir ? c->dir : "", c->dir && c->dir[0] ? "/" : "", nombre);
            size_t leido = 0;
            char *sub = interprete_leer_bytes(ruta, &leido);
            if (!sub) { c->prof--; return pl_fallo(c, q, "no se encontró el parcial '%s'", nombre); }
            const char *base_ant = c->base;
            c->base = sub;
            int ok = pl_render(c, sub, sub + leido, out);
            c->base = base_ant;
            free(sub);
            if (!ok) { c->prof--; return 0; }
            continue;
        }
        if (!triple && *a == '#') {
            if (b - a >= 3 && strncmp(a, "#si", 3) == 0 && (a + 3 == b || a[3] == ' ')) {
                const char *cond = pl_saltar_esp(a + 3, b);
                int niega = 0;
                if (b - cond > 3 && strncmp(cond, "no ", 3) == 0) { niega = 1; cond = pl_saltar_esp(cond + 3, b); }
                if (cond >= b) { c->prof--; return pl_fallo(c, q, "«#si» necesita una condición%s", NULL); }
                const char *sino, *tras;
                const char *fc = pl_cierre(p, fin, "si", &sino, &tras);
                if (!fc) { c->prof--; return pl_fallo(c, q, "«#si» sin «{{/si}}»%s", NULL); }
                char ruta[200]; size_t l = (size_t)(b - cond);
                if (l >= sizeof ruta) l = sizeof ruta - 1;
                memcpy(ruta, cond, l); ruta[l] = '\0';
                Valor *v = pl_buscar(c, ruta, l);
                if (!v) { c->prof--; return pl_fallo(c, q, "«%s» no existe en los datos", ruta); }
                int cierto = pl_verdadero(v) != niega;
                const char *d0, *h0;
                /* apertura sola en su línea */
                if (pl_sola(c->base, q, p, fin, &d0, &h0)) { out->n -= (size_t)(q - d0); out->d[out->n] = '\0'; p = h0; }
                /* «{{#sino}}» y cierre: dónde acaba el trozo «entonces» y dónde empieza el «sino» */
                const char *fin_then = sino ? sino : fc, *ini_else = tras;
                if (sino && pl_sola(c->base, sino, tras, fin, &d0, &h0)) { fin_then = d0; ini_else = h0; }
                const char *fin_else = fc, *tras_cierre = fc + strlen("{{/si}}");
                if (pl_sola(c->base, fc, tras_cierre, fin, &d0, &h0)) { fin_else = d0; tras_cierre = h0; if (!sino) fin_then = d0; }
                int ok = cierto ? pl_render(c, p, fin_then, out)
                                : (sino ? pl_render(c, ini_else, fin_else, out) : 1);
                if (!ok) { c->prof--; return 0; }
                p = tras_cierre;
                continue;
            }
            if (b - a >= 5 && strncmp(a, "#para", 5) == 0 && (a + 5 == b || a[5] == ' ')) {
                /* #para x en ruta */
                const char *v0 = pl_saltar_esp(a + 5, b), *v1 = v0;
                while (v1 < b && *v1 != ' ') v1++;
                const char *en = pl_saltar_esp(v1, b);
                if (v1 == v0 || b - en < 3 || strncmp(en, "en ", 3) != 0)
                    { c->prof--; return pl_fallo(c, q, "se esperaba «{{#para x en lista}}»%s", NULL); }
                const char *r0 = pl_saltar_esp(en + 3, b);
                char var[64], ruta[200];
                size_t lv = (size_t)(v1 - v0), lr = (size_t)(b - r0);
                if (lv == 0 || lv >= sizeof var || lr == 0 || lr >= sizeof ruta)
                    { c->prof--; return pl_fallo(c, q, "se esperaba «{{#para x en lista}}»%s", NULL); }
                memcpy(var, v0, lv); var[lv] = '\0';
                memcpy(ruta, r0, lr); ruta[lr] = '\0';
                const char *fc = pl_cierre(p, fin, "para", NULL, NULL);
                if (!fc) { c->prof--; return pl_fallo(c, q, "«#para» sin «{{/para}}»%s", NULL); }
                Valor *col = pl_buscar(c, ruta, lr);
                if (!col) { c->prof--; return pl_fallo(c, q, "«%s» no existe en los datos", ruta); }
                int n = col->tipo == VAL_LISTA ? col->lista.cantidad
                      : col->tipo == VAL_DICCIONARIO ? col->diccionario.cantidad : -1;
                if (n < 0) { c->prof--; return pl_fallo(c, q, "«%s» no es una lista (ni un diccionario)", ruta); }
                if (c->nvars + 3 > PL_MAX_VARS) { c->prof--; return pl_fallo(c, q, "demasiados bucles anidados%s", NULL); }
                int base_vars = c->nvars;
                const char *d0, *h0, *fin_cuerpo = fc, *tras_cierre = fc + strlen("{{/para}}");
                if (pl_sola(c->base, q, p, fin, &d0, &h0)) { out->n -= (size_t)(q - d0); out->d[out->n] = '\0'; p = h0; }
                if (pl_sola(c->base, fc, tras_cierre, fin, &d0, &h0)) { fin_cuerpo = d0; tras_cierre = h0; }
                for (int i = 0; i < n; i++) {
                    c->nvars = base_vars;
                    Valor *clave = NULL;
                    Valor *elem = col->tipo == VAL_LISTA ? col->lista.elementos[i]
                        : (clave = pl_temp(c, valor_texto(col->diccionario.claves[i])));
                    c->vars[c->nvars++] = (PVar){ var, elem };
                    c->vars[c->nvars++] = (PVar){ "@indice", pl_temp(c, valor_numero(i)) };
                    c->vars[c->nvars++] = (PVar){ "@numero", pl_temp(c, valor_numero(i + 1)) };
                    (void)clave;
                    int ok = pl_render(c, p, fin_cuerpo, out);
                    if (!ok) { c->nvars = base_vars; c->prof--; return 0; }
                }
                c->nvars = base_vars;
                p = tras_cierre;
                continue;
            }
            c->prof--;
            return pl_fallo(c, q, "etiqueta desconocida «%s»", "{{#…}}");
        }
        if (!triple && *a == '/') { c->prof--; return pl_fallo(c, q, "cierre «{{%s}}» sin su apertura", "/…"); }

        /* variable */
        char ruta[200]; size_t l = (size_t)(b - a);
        if (l == 0) { c->prof--; return pl_fallo(c, q, "etiqueta vacía%s", NULL); }
        if (l >= sizeof ruta) l = sizeof ruta - 1;
        memcpy(ruta, a, l); ruta[l] = '\0';
        Valor *v = pl_buscar(c, ruta, l);
        if (!v) { c->prof--; return pl_fallo(c, q, "«%s» no existe en los datos", ruta); }
        pl_valor(out, v, !triple);
    }
    c->prof--;
    return 1;
}

static Valor *pl_ejecutar(const char *texto, size_t largo, Valor *datos, const char *dir) {
    PCtx *c = calloc(1, sizeof(PCtx));
    c->raiz = datos; c->dir = dir; c->base = texto;
    PBuf out = {0};
    pb_poner(&out, "", 0);
    int ok = pl_render(c, texto, texto + largo, &out);
    Valor *res;
    if (ok) {
        res = valor_texto(out.d);
    } else {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorPlantilla", c->error, 0);
        res = valor_nulo();
    }
    for (int i = 0; i < c->ntemps; i++) valor_destruir(c->temps[i]);
    free(out.d); free(c);
    return res;
}

static Valor *fn_pl_renderizar(Valor **a, int n) {
    if (n < 2 || a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_DICCIONARIO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "renderizar() espera (texto de la plantilla, diccionario de datos)", 0);
        return valor_nulo();
    }
    return pl_ejecutar(a[0]->texto, strlen(a[0]->texto), a[1], NULL);
}

static Valor *fn_pl_archivo(Valor **a, int n) {
    if (n < 2 || a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_DICCIONARIO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "archivo() espera (ruta de la plantilla, diccionario de datos)", 0);
        return valor_nulo();
    }
    size_t leido = 0;
    char *txt = interprete_leer_bytes(a[0]->texto, &leido);
    if (!txt) {
        char msg[600];
        snprintf(msg, sizeof msg, "no se encontró la plantilla '%s'", a[0]->texto);
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorPlantilla", msg, 0);
        return valor_nulo();
    }
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", a[0]->texto);
    char *barra = strrchr(dir, '/');
    if (barra) *barra = '\0'; else dir[0] = '\0';
    Valor *r = pl_ejecutar(txt, leido, a[1], dir);
    free(txt);
    return r;
}

static void cargar_plantillas(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"renderizar", fn_pl_renderizar, 2},
        {"archivo",    fn_pl_archivo,    2},
    };
    registrar_modulo_diccionario(entorno, "plantillas",
        fns, 2, NULL, NULL, 0);
}

static void cargar_matematica(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"raiz",       fn_mat_raiz,       1},
        {"potencia",   fn_mat_potencia,   2},
        {"abs",        fn_mat_abs,        1},
        {"redondear",  fn_mat_redondear,  1},
        {"piso",       fn_mat_piso,       1},
        {"techo",      fn_mat_techo,      1},
        {"seno",       fn_mat_seno,       1},
        {"coseno",     fn_mat_coseno,     1},
        {"tangente",   fn_mat_tangente,   1},
        {"logaritmo",  fn_mat_logaritmo,  1},
        {"maximo",     fn_mat_maximo,     2},
        {"minimo",     fn_mat_minimo,     2},
        {"aleatorio",      fn_mat_aleatorio,    -1},
        {"truncar",        fn_mat_truncar,       1},
        {"es_entero",      fn_mat_es_entero,     1},
        {"factorial",      fn_mat_factorial,     1},
        {"combinaciones",  fn_mat_combinaciones, 2},
        {"log2",           fn_mat_log2,          1},
        {"log10",          fn_mat_log10,         1},
    };
    const char *cnames[] = {"PI", "E", "TAU"};
    double      cvals[]  = {M_PI, M_E, M_PI * 2};
    registrar_modulo_diccionario(entorno, "matematica",
        fns, 19, cnames, cvals, 3);
}

static void cargar_texto(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"dividir",       fn_txt_dividir,       2},
        {"unir",          fn_txt_unir,          2},
        {"repetir",       fn_txt_repetir,       2},
        {"invertir",      fn_txt_invertir,      1},
        {"formato",       fn_txt_formato,       2},
        {"a_numero",      fn_txt_a_numero,      1},
        {"de_numero",     fn_txt_de_numero,     1},
        {"a_lista",       fn_txt_a_lista,       1},
        {"posicion",      fn_txt_posicion,      2},
        {"contar",        fn_txt_contar,        2},
        {"extraer",       fn_txt_extraer,       3},
        {"es_numero",     fn_txt_es_numero,     1},
        {"es_letra",      fn_txt_es_letra,      1},
        {"es_vacio",      fn_txt_es_vacio,      1},
        {"empieza_con",   fn_txt_empieza_con,   2},
        {"termina_con",   fn_txt_termina_con,   2},
        {"rellenar_izq",  fn_txt_rellenar_izq,  3},
        {"rellenar_der",  fn_txt_rellenar_der,  3},
        {"centrar",       fn_txt_centrar,       3},
    };
    registrar_modulo_diccionario(entorno, "texto",
        fns, 19, NULL, NULL, 0);
}

static void cargar_archivos(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"leer",              fn_arc_leer,              1},
        {"escribir",          fn_arc_escribir,          2},
        {"agregar",           fn_arc_agregar,           2},
        {"existe",            fn_arc_existe,            1},
        {"eliminar",          fn_arc_eliminar,          1},
        {"crear_directorio",  fn_arc_crear_directorio,  1},
        {"es_directorio",     fn_arc_es_directorio,     1},
        {"copiar",            fn_arc_copiar,            2},
        {"mover",             fn_arc_mover,             2},
        {"listar",            fn_arc_listar,            1},
    };
    registrar_modulo_diccionario(entorno, "archivos",
        fns, 10, NULL, NULL, 0);
}

static void cargar_tiempo(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"ahora",   fn_tpo_ahora,   0},
        {"esperar", fn_tpo_esperar, 1},
        {"fecha",   fn_tpo_fecha,   0},
        {"hora",    fn_tpo_hora,    0},
        {"formatear",     fn_tpo_formatear,     2},
        {"formatear_utc", fn_tpo_formatear_utc, 2},
        {"partes",        fn_tpo_partes,        1},
        {"partes_utc",    fn_tpo_partes_utc,    1},
        {"crear",         fn_tpo_crear,        -1},
        {"crear_utc",     fn_tpo_crear_utc,    -1},
        {"analizar",      fn_tpo_analizar,      2},
        {"analizar_utc",  fn_tpo_analizar_utc,  2},
    };
    registrar_modulo_diccionario(entorno, "tiempo",
        fns, 12, NULL, NULL, 0);
}

static void cargar_sistema(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"ejecutar",           fn_sis_ejecutar,           1},
        {"proceso",            fn_sis_proceso,            1},
        {"entorno",            fn_sis_entorno,           -1},
        {"establecer_entorno", fn_sis_establecer_entorno,  2},
        {"plataforma",         fn_sis_plataforma,          0},
        {"pid",                fn_sis_pid,                 0},
        {"existe",             fn_sis_existe,              1},
        {"salir",              fn_sis_salir,               1},
    };
    registrar_modulo_diccionario(entorno, "sistema",
        fns, 8, NULL, NULL, 0);
}

static void cargar_json(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"parsear", fn_json_parsear, 1},
        {"texto",   fn_json_texto,   1},
    };
    registrar_modulo_diccionario(entorno, "json",
        fns, 2, NULL, NULL, 0);
}

static void cargar_red(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"obtener",    fn_red_obtener,    -1},
        {"enviar",     fn_red_enviar,     -1},
        {"actualizar", fn_red_actualizar, -1},
        {"eliminar",   fn_red_eliminar,   -1},
    };
    registrar_modulo_diccionario(entorno, "red",
        fns, 4, NULL, NULL, 0);
}

static void cargar_expresiones(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"coincidir",  fn_exp_coincidir,  2},
        {"buscar",     fn_exp_buscar,     2},
        {"todos",      fn_exp_todos,      2},
        {"reemplazar", fn_exp_reemplazar, 3},
    };
    registrar_modulo_diccionario(entorno, "expresiones",
        fns, 4, NULL, NULL, 0);
}

static void cargar_servidor(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"ruta",       fn_srv_ruta,           2},
        {"obtener",    fn_srv_obtener,         2},
        {"enviar",     fn_srv_enviar,          2},
        {"borrar",     fn_srv_borrar,          2},
        {"actualizar", fn_srv_actualizar,      2},
        {"estaticos",  fn_srv_estaticos,       1},
        {"html",       fn_srv_html,           -1},
        {"json",       fn_srv_json,           -1},
        {"texto",      fn_srv_texto,          -1},
        {"redirigir",  fn_srv_redirigir,       1},
        {"escuchar",   fn_srv_escuchar,       -1},
        {"ventana",    fn_srv_ventana,        -1},
        {"formulario", fn_srv_formulario,      1},
    };
    registrar_modulo_diccionario(entorno, "servidor",
        fns, 13, NULL, NULL, 0);
}

static void cargar_terminal(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"negro",      fn_ter_negro,      1},
        {"rojo",       fn_ter_rojo,       1},
        {"verde",      fn_ter_verde,      1},
        {"amarillo",   fn_ter_amarillo,   1},
        {"azul",       fn_ter_azul,       1},
        {"magenta",    fn_ter_magenta,    1},
        {"cian",       fn_ter_cian,       1},
        {"blanco",     fn_ter_blanco,     1},
        {"negrita",    fn_ter_negrita,    1},
        {"cursiva",    fn_ter_cursiva,    1},
        {"subrayado",  fn_ter_subrayado,  1},
        {"limpiar",    fn_ter_limpiar,    0},
        {"mover",      fn_ter_mover,      2},
        {"ancho",      fn_ter_ancho,      0},
        {"alto",       fn_ter_alto,       0},
        {"barra",      fn_ter_barra,      3},
        {"tabla",      fn_ter_tabla,      1},
    };
    registrar_modulo_diccionario(entorno, "terminal",
        fns, 17, NULL, NULL, 0);
}

/* Única lista de módulos estándar: cargador y nombre van juntos. */
static const struct { const char *nombre; void (*cargar)(Entorno *); } MODULOS_ESTANDAR[] = {
    {"matematica", cargar_matematica},
    {"texto", cargar_texto},
    {"archivos", cargar_archivos},
    {"tiempo", cargar_tiempo},
    {"sistema", cargar_sistema},
    {"json", cargar_json},
    {"red", cargar_red},
    {"expresiones", cargar_expresiones},
    {"servidor", cargar_servidor},
    {"terminal", cargar_terminal},
    {"pruebas", cargar_pruebas},
    {"sqlite", cargar_sqlite},
    {"cripto", cargar_cripto},
    {"tareas", cargar_tareas},
    {"plantillas", cargar_plantillas},
};
#define NUM_MODULOS_ESTANDAR ((int)(sizeof(MODULOS_ESTANDAR) / sizeof(MODULOS_ESTANDAR[0])))

const char *modulo_estandar(int i) {
    return (i >= 0 && i < NUM_MODULOS_ESTANDAR) ? MODULOS_ESTANDAR[i].nombre : NULL;
}

void modulo_cargar(const char *nombre, Entorno *entorno) {
    srand((unsigned)time(NULL));

    for (int i = 0; i < NUM_MODULOS_ESTANDAR; i++) {
        if (strcmp(nombre, MODULOS_ESTANDAR[i].nombre) == 0) {
            MODULOS_ESTANDAR[i].cargar(entorno);
            return;
        }
    }


    /* ¿Lo trae el anfitrión? */
    if (_modulo_externo && _modulo_externo(nombre, entorno))
        return;

    /* Módulo desconocido */
    hay_error = 1;
    char msg[512];
    snprintf(msg, sizeof(msg),
        "El módulo '%s' no existe. Módulos disponibles: matematica, texto, archivos, tiempo, sistema, terminal, servidor, json, red, expresiones, pruebas, sqlite, cripto, tareas, plantillas",
        nombre);
    valor_error = valor_error_nuevo("Error", msg, 0);
}
