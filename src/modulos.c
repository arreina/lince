/*
 * 🐆 Lince — modulos.c
 * Módulos estándar: matematica, texto, archivos, tiempo
 */

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
#ifdef LINCE_MOTOR
#include "modulo_motor.h"
#endif

/* Función del entorno — declaración externa */

/* ─────────────────────────────────────────
   UTILIDADES INTERNAS
───────────────────────────────────────── */

/* Crea un valor número — necesitamos acceso a la API interna */

/* Macro para definir una función nativa en un módulo */
typedef Valor *(*FnNativa)(Valor **args, int num_args);

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
        valor_error = valor_crear_error("ErrorTipo", "raiz() espera un numero", 0);
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
        valor_error = valor_crear_error("ErrorMatematico",
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
        valor_error = valor_crear_error("ErrorMatematico",
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
        valor_error = valor_crear_error("ErrorMatematico",
            "log2() solo acepta números positivos.", 0);
        return valor_nulo();
    }
    return valor_numero(log2(a[0]->numero));
}

static Valor *fn_mat_log10(Valor **a, int n) {
    (void)n;
    if (a[0]->numero <= 0) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorMatematico",
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
        valor_error = valor_crear_error("ErrorTipo",
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
        valor_error = valor_crear_error("ErrorTipo",
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
        valor_error = valor_crear_error("ErrorTipo",
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
        valor_error = valor_crear_error("ErrorTipo",
            "invertir() espera un texto", 0);
        return valor_nulo();
    }
    char *s = strdup(a[0]->texto);
    int   l = strlen(s);
    for (int i = 0; i < l / 2; i++) {
        char tmp = s[i]; s[i] = s[l-1-i]; s[l-1-i] = tmp;
    }
    Valor *r = valor_texto(s);
    free(s);
    return r;
}

static Valor *fn_txt_formato(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_LISTA) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
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
        valor_error = valor_crear_error("ErrorTipo",
            "a_numero() espera un texto", 0);
        return valor_nulo();
    }
    char *fin;
    double d = strtod(a[0]->texto, &fin);
    if (fin == a[0]->texto) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
            "No se puede convertir ese texto a número", 0);
        return valor_nulo();
    }
    return valor_numero(d);
}

static Valor *fn_txt_de_numero(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_NUMERO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
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
        valor_error = valor_crear_error("ErrorTipo",
            "a_lista() espera un texto", 0);
        return valor_nulo();
    }
    Valor *lista = valor_lista_crear();
    for (int i = 0; a[0]->texto[i]; i++) {
        char tmp[2] = { a[0]->texto[i], '\0' };
        lista_agregar(lista, valor_texto(tmp));
    }
    return lista;
}

static Valor *fn_txt_posicion(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
            "posicion() espera dos textos", 0);
        return valor_nulo();
    }
    const char *found = strstr(a[0]->texto, a[1]->texto);
    if (!found) return valor_numero(-1);
    return valor_numero(found - a[0]->texto);
}

static Valor *fn_txt_contar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
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
        valor_error = valor_crear_error("ErrorTipo",
            "extraer() espera texto, inicio y fin", 0);
        return valor_nulo();
    }
    int len   = strlen(a[0]->texto);
    int ini   = (int)a[1]->numero;
    int fin   = (int)a[2]->numero;
    if (ini < 0) ini = 0;
    if (fin > len) fin = len;
    if (ini >= fin) return valor_texto("");
    int sz   = fin - ini;
    char *buf = malloc(sz + 1);
    strncpy(buf, a[0]->texto + ini, sz);
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
    for (int i = 0; a[0]->texto[i]; i++)
        if (!isalpha((unsigned char)a[0]->texto[i])) return valor_booleano(0);
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
        valor_error = valor_crear_error("ErrorTipo",
            "rellenar_izq() espera texto, ancho y caracter de relleno", 0);
        return valor_nulo();
    }
    int ancho = (int)a[1]->numero;
    const char *s = a[0]->texto, *r = a[2]->texto;
    int len = strlen(s);
    if (len >= ancho) return valor_texto(s);
    char *buf = malloc(ancho + 1);
    int pad = ancho - len;
    for (int i = 0; i < pad; i++) buf[i] = r[0];
    strcpy(buf + pad, s);
    Valor *v = valor_texto(buf); free(buf);
    return v;
}

static Valor *fn_txt_rellenar_der(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
            "rellenar_der() espera texto, ancho y caracter de relleno", 0);
        return valor_nulo();
    }
    int ancho = (int)a[1]->numero;
    const char *s = a[0]->texto, *r = a[2]->texto;
    int len = strlen(s);
    if (len >= ancho) return valor_texto(s);
    char *buf = malloc(ancho + 1);
    strcpy(buf, s);
    for (int i = len; i < ancho; i++) buf[i] = r[0];
    buf[ancho] = '\0';
    Valor *v = valor_texto(buf); free(buf);
    return v;
}

static Valor *fn_txt_centrar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_NUMERO || a[2]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
            "centrar() espera texto, ancho y caracter de relleno", 0);
        return valor_nulo();
    }
    int ancho = (int)a[1]->numero;
    const char *s = a[0]->texto, *r = a[2]->texto;
    int len = strlen(s);
    if (len >= ancho) return valor_texto(s);
    int pad_total = ancho - len;
    int pad_izq   = pad_total / 2;
    int pad_der   = pad_total - pad_izq;
    char *buf = malloc(ancho + 1);
    for (int i = 0; i < pad_izq; i++) buf[i] = r[0];
    strncpy(buf + pad_izq, s, len);
    for (int i = 0; i < pad_der; i++) buf[pad_izq + len + i] = r[0];
    buf[ancho] = '\0';
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
        valor_error = valor_crear_error("ErrorTipo",
            "leer() espera una ruta de archivo", 0);
        return valor_nulo();
    }
    FILE *f = fopen(a[0]->texto, "r");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "No se pudo abrir el archivo '%s'", a[0]->texto);
        hay_error = 1;
        valor_error = valor_crear_error("Error", msg, 0);
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
        valor_error = valor_crear_error("ErrorTipo",
            "escribir() espera ruta y contenido", 0);
        return valor_nulo();
    }
    FILE *f = fopen(a[0]->texto, "w");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "No se pudo escribir en '%s'", a[0]->texto);
        hay_error = 1;
        valor_error = valor_crear_error("Error", msg, 0);
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
        valor_error = valor_crear_error("ErrorTipo",
            "agregar() espera ruta y contenido", 0);
        return valor_nulo();
    }
    FILE *f = fopen(a[0]->texto, "a");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "No se pudo abrir '%s'", a[0]->texto);
        hay_error = 1;
        valor_error = valor_crear_error("Error", msg, 0);
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

/* Llamar a una función Lince desde C */
static Valor *llamar_funcion(Valor *fn, Valor **args, int nargs, Entorno *ent) {
    return interprete_llamar_funcion(fn, args, nargs, ent);
}
#define SRV_MAX_RUTAS 64
typedef struct {
    char    metodo[8];
    char    ruta[256];
    Valor  *manejador;
    Entorno *entorno_fn;
} SrvRuta;

static SrvRuta   _srv_rutas[SRV_MAX_RUTAS];
static int       _srv_nrutas = 0;
static char      _srv_dir_estaticos[256] = "";
static Entorno  *_srv_entorno_global = NULL;
static Entorno  *_srv_ent_tmp        = NULL;

/* Forward declaration */
static Valor *_srv_llamar_manejador(Valor *fn, Entorno *ent,
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
            int idx = dic->diccionario.cantidad;
            if (idx < 64) {
                dic->diccionario.claves[idx]  = strdup(key);
                dic->diccionario.valores[idx] = valor_texto(val);
                dic->diccionario.cantidad++;
            } else {
                fprintf(stderr, "⚠  servidor: query string con más de 64 parámetros, "
                                "se descartan los sobrantes\n");
            }
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
            int idx = dic->diccionario.cantidad;
            if (idx < 64) {
                dic->diccionario.claves[idx]  = strdup(nombre);
                dic->diccionario.valores[idx] = valor_texto(valor);
                dic->diccionario.cantidad++;
            } else {
                fprintf(stderr, "⚠  servidor: más de 64 cookies en la petición, "
                                "se descartan las sobrantes\n");
            }
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
            int idx = params->diccionario.cantidad;
            if (idx < 64) {
                params->diccionario.claves[idx]  = strdup(pseg[i] + 1);
                params->diccionario.valores[idx] = valor_texto(rseg[i]);
                params->diccionario.cantidad++;
            }
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
    return "text/plain";
}

/* ── Enviar respuesta HTTP ──────────────── */

static void _srv_enviar_respuesta(int fd, int codigo, const char *tipo,
                                   const char *cuerpo, Valor *cookies) {
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

    size_t cuerpo_len = cuerpo ? strlen(cuerpo) : 0;
    if (!tipo || strpbrk(tipo, "\r\n")) tipo = "text/plain";

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
        "Access-Control-Allow-Origin: *\r\n",
        codigo, estado, tipo, cuerpo_len);
    if (clen < 0 || (size_t)clen >= sizeof(cab)) {
        fprintf(stderr, "⚠  servidor: cabeceras demasiado largas "
                        "(¿'tipo' excesivo?), se usa text/plain\n");
        clen = snprintf(cab, sizeof(cab),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n",
            codigo, estado, cuerpo_len);
        if (clen < 0) return;
    }

    send(fd, cab, (size_t)clen, 0);

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

/* ── Manejar conexión ───────────────────── */

static void _srv_manejar_conexion(int fd, Entorno *ent) {
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
            _srv_entorno_global,
            metodo, ruta, cuerpo, consulta, params, cookies);

        valor_destruir(params);

        Valor *cookies_resp = NULL;
        if (resp && resp->tipo == VAL_DICCIONARIO) {
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
        snprintf(ruta_file, sizeof(ruta_file), "%s%s", _srv_dir_estaticos, r);
        FILE *f = fopen(ruta_file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long tam = ftell(f); rewind(f);
            char *cont = malloc(tam + 1);
            fread(cont, 1, tam, f); cont[tam] = '\0'; fclose(f);
            const char *ext = strrchr(ruta_file, '.');
            _srv_enviar_respuesta(fd, 200, _srv_mime(ext), cont, NULL);
            free(cont); close(fd);
            valor_destruir(consulta); valor_destruir(cookies);
            return;
        }
    }

    _srv_enviar_respuesta(fd, 404, "text/html",
        "<h1>404 — No encontrado</h1><p>La página no existe.</p>", NULL);
    close(fd);
    valor_destruir(consulta); valor_destruir(cookies);
}

/* ── Llamar manejador Lince ─────────────── */

static Valor *_srv_llamar_manejador(Valor *fn, Entorno *ent,
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
    return llamar_funcion(fn, args, 1, ent ? ent : _srv_entorno_global);
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
    _srv_rutas[_srv_nrutas].entorno_fn = _srv_ent_tmp;
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
    return res;
}

/* ── Escuchar (con fork por conexión) ───── */

static Valor *fn_srv_escuchar(Valor **a, int n, Entorno *ent) {
    int puerto = (n >= 1 && a[0]->tipo == VAL_NUMERO) ? (int)a[0]->numero : 8080;
    _srv_entorno_global = ent;

#ifdef _WIN32
    printf("⚠  El módulo servidor no está disponible en Windows aún.\n");
    return valor_nulo();
#else
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);  /* recolectar hijos automáticamente */

    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        hay_error = 1;
        valor_error = valor_crear_error("Error", "No se pudo crear el socket", 0);
        return valor_nulo();
    }
    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(puerto);

    if (bind(srv_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(srv_fd);
        hay_error = 1;
        valor_error = valor_crear_error("Error",
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
            _srv_manejar_conexion(cli_fd, ent);
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

static Valor *fn_srv_escuchar_wrap(Valor **a, int n) {
    return fn_srv_escuchar(a, n, _srv_ent_tmp);
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
        valor_error = valor_crear_error("ErrorTipo",
            "ejecutar() espera un texto con el comando", 0);
        return valor_nulo();
    }

    FILE *proc = popen(a[0]->texto, "r");
    if (!proc) {
        hay_error = 1;
        valor_error = valor_crear_error("Error",
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
        valor_error = valor_crear_error("ErrorTipo",
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
        valor_error = valor_crear_error("Error",
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
        valor_error = valor_crear_error("ErrorTipo", "obtener() espera una URL", 0);
        return valor_nulo();
    }
    Valor *cabeceras = (n >= 2) ? a[1] : NULL;
    return hacer_peticion("GET", a[0]->texto, "", cabeceras);
}

static Valor *fn_red_enviar(Valor **a, int n) {
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo", "enviar() espera una URL", 0);
        return valor_nulo();
    }
    const char *cuerpo   = (n >= 2 && a[1]->tipo == VAL_TEXTO) ? a[1]->texto : "";
    Valor      *cabeceras = (n >= 3) ? a[2] : NULL;
    return hacer_peticion("POST", a[0]->texto, cuerpo, cabeceras);
}

static Valor *fn_red_actualizar(Valor **a, int n) {
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo", "actualizar() espera una URL", 0);
        return valor_nulo();
    }
    const char *cuerpo   = (n >= 2 && a[1]->tipo == VAL_TEXTO) ? a[1]->texto : "";
    Valor      *cabeceras = (n >= 3) ? a[2] : NULL;
    return hacer_peticion("PUT", a[0]->texto, cuerpo, cabeceras);
}

static Valor *fn_red_eliminar(Valor **a, int n) {
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo", "eliminar() espera una URL", 0);
        return valor_nulo();
    }
    Valor *cabeceras = (n >= 2) ? a[1] : NULL;
    return hacer_peticion("DELETE", a[0]->texto, "", cabeceras);
}
typedef struct {
    char     *nombre;
    FnNativa  fn;
    int       num_args;
} FuncionNativa;

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
    modulo->diccionario.cantidad = 0;
    modulo->diccionario.claves   = malloc(sizeof(char*) * 128);
    modulo->diccionario.valores  = malloc(sizeof(Valor*) * 128);

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

        Valor *vf = malloc(sizeof(Valor));
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

void modulo_cargar(const char *nombre, Entorno *entorno) {
    srand((unsigned)time(NULL));

    if (strcmp(nombre, "matematica") == 0) {
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
        return;
    }

    if (strcmp(nombre, "texto") == 0) {
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
        return;
    }

    if (strcmp(nombre, "archivos") == 0) {
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
        return;
    }

    if (strcmp(nombre, "tiempo") == 0) {
        FuncionNativa fns[] = {
            {"ahora",   fn_tpo_ahora,   0},
            {"esperar", fn_tpo_esperar, 1},
            {"fecha",   fn_tpo_fecha,   0},
            {"hora",    fn_tpo_hora,    0},
        };
        registrar_modulo_diccionario(entorno, "tiempo",
            fns, 4, NULL, NULL, 0);
        return;
    }

    if (strcmp(nombre, "sistema") == 0) {
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
        return;
    }

    if (strcmp(nombre, "json") == 0) {
        FuncionNativa fns[] = {
            {"parsear", fn_json_parsear, 1},
            {"texto",   fn_json_texto,   1},
        };
        registrar_modulo_diccionario(entorno, "json",
            fns, 2, NULL, NULL, 0);
        return;
    }

    if (strcmp(nombre, "red") == 0) {
        FuncionNativa fns[] = {
            {"obtener",    fn_red_obtener,    -1},
            {"enviar",     fn_red_enviar,     -1},
            {"actualizar", fn_red_actualizar, -1},
            {"eliminar",   fn_red_eliminar,   -1},
        };
        registrar_modulo_diccionario(entorno, "red",
            fns, 4, NULL, NULL, 0);
        return;
    }

    if (strcmp(nombre, "expresiones") == 0) {
        FuncionNativa fns[] = {
            {"coincidir",  fn_exp_coincidir,  2},
            {"buscar",     fn_exp_buscar,     2},
            {"todos",      fn_exp_todos,      2},
            {"reemplazar", fn_exp_reemplazar, 3},
        };
        registrar_modulo_diccionario(entorno, "expresiones",
            fns, 4, NULL, NULL, 0);
        return;
    }

    if (strcmp(nombre, "servidor") == 0) {
        /* escuchar necesita acceso al entorno — lo pasamos via global */
        _srv_ent_tmp = entorno;
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
            {"escuchar",   fn_srv_escuchar_wrap,  -1},
        };
        registrar_modulo_diccionario(entorno, "servidor",
            fns, 11, NULL, NULL, 0);
        return;
    }

    if (strcmp(nombre, "terminal") == 0) {
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
        return;
    }

#ifdef LINCE_MOTOR
    if (strcmp(nombre, "motor") == 0) {
        modulo_motor_fijar_entorno(entorno);
        FuncionNativa fns[] = {
            /* Ciclo de vida y bucle */
            {"iniciar",             fn_motor_iniciar,             -1},
            {"terminar",            fn_motor_terminar,             0},
            {"error",               fn_motor_error,                0},
            {"al_actualizar",       fn_motor_al_actualizar,        1},
            {"correr",              fn_motor_correr,               0},
            {"parar",               fn_motor_parar,                0},
            /* Tiempo */
            {"delta",               fn_motor_delta,                0},
            {"tiempo",              fn_motor_tiempo,               0},
            {"fps",                 fn_motor_fps,                  0},
            {"fijar_limite_fps",    fn_motor_fijar_limite_fps,     1},
            /* Entrada */
            {"tecla_pulsada",       fn_motor_tecla_pulsada,        1},
            {"tecla_recien_pulsada",fn_motor_tecla_recien_pulsada, 1},
            {"raton_posicion",      fn_motor_raton_posicion,       0},
            {"raton_pulsado",       fn_motor_raton_pulsado,        1},
            /* Ventana */
            {"ventana_tamano",      fn_motor_ventana_tamano,       0},
            {"ventana_titulo",      fn_motor_ventana_titulo,       1},
            {"fondo",               fn_motor_fondo,                3},
            /* Texturas y sprites */
            {"textura_cargar",      fn_motor_textura_cargar,       1},
            {"textura_destruir",    fn_motor_textura_destruir,     1},
            {"textura_tamano",      fn_motor_textura_tamano,       1},
            {"dibujar_textura",     fn_motor_dibujar_textura,      3},
            {"dibujar_sprite",      fn_motor_dibujar_sprite,      11},
        };
        registrar_modulo_diccionario(entorno, "motor",
            fns, sizeof(fns) / sizeof(fns[0]), NULL, NULL, 0);
        return;
    }
#endif

    /* Módulo desconocido */
    hay_error = 1;
    char msg[512];
    snprintf(msg, sizeof(msg),
        "El módulo '%s' no existe. Módulos disponibles: matematica, texto, archivos, tiempo, sistema, terminal, servidor, json, red, expresiones"
#ifdef LINCE_MOTOR
        ", motor"
#endif
        ,
        nombre);
    valor_error = valor_crear_error("Error", msg, 0);
}
