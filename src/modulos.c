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

#include <dlfcn.h>
#include "juego.h"

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

/* Única lista de módulos estándar: cargador y nombre van juntos. */
static const struct { const char *nombre; void (*cargar)(Entorno *); } MODULOS_ESTANDAR[] = {
    {"matematica", cargar_matematica},
    {"texto", cargar_texto},
    {"archivos", cargar_archivos},
    {"tiempo", cargar_tiempo},
    {"sistema", cargar_sistema},
    {"json", cargar_json},
    {"pruebas", cargar_pruebas},
    {"juego", cargar_juego},
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
        "El módulo '%s' no existe. Módulos disponibles: matematica, texto, archivos, tiempo, sistema, json, pruebas, juego",
        nombre);
    valor_error = valor_error_nuevo("Error", msg, 0);
}
