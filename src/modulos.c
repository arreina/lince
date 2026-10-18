/*
 * 🐆 Lince — modulos.c
 * Módulos estándar: matematica, texto, archivos, tiempo, sistema, json, pruebas y juego
 */

#ifndef _WIN32
#define _GNU_SOURCE      /* timegm */
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
#include <sys/wait.h>
#else
#include <windows.h>
#include <direct.h>      /* _mkdir */
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

/* Generador propio (xorshift64*): igual en todas las plataformas, con
   semilla para repetir partidas. Se siembra con la hora la primera vez. */
static uint64_t _mat_estado = 0;
static double mat_azar(void) {
    if (!_mat_estado) _mat_estado = (uint64_t)time(NULL) * 0x9E3779B97F4A7C15ULL | 1;
    _mat_estado ^= _mat_estado >> 12;
    _mat_estado ^= _mat_estado << 25;
    _mat_estado ^= _mat_estado >> 27;
    return (double)((_mat_estado * 0x2545F4914F6CDD1DULL) >> 11) / 9007199254740992.0;   /* [0, 1) */
}

static Valor *fn_mat_aleatorio(Valor **a, int n) {
    if (n == 0) return valor_numero(mat_azar());
    if (n < 2 || a[0]->tipo != VAL_NUMERO || a[1]->tipo != VAL_NUMERO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "aleatorio() espera nada o (mínimo, máximo)", 0);
        return valor_nulo();
    }
    double min = floor(a[0]->numero), max = floor(a[1]->numero);
    if (max < min) { double t = min; min = max; max = t; }
    return valor_numero(min + floor(mat_azar() * (max - min + 1)));
}

/* semilla(n): misma semilla, misma secuencia de aleatorio() */
static Valor *fn_mat_semilla(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_NUMERO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "semilla() espera un número", 0);
        return valor_nulo();
    }
    uint64_t z = (uint64_t)(int64_t)a[0]->numero + 0x9E3779B97F4A7C15ULL;   /* splitmix64 */
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    _mat_estado = (z ^ (z >> 31)) | 1;
    return valor_nulo();
}

static int mat_nums(Valor **a, int n, int k, double *v, const char *fn) {
    for (int i = 0; i < k; i++) {
        if (i >= n || a[i]->tipo != VAL_NUMERO) {
            char msg[96];
            snprintf(msg, sizeof msg, "%s() espera %d números", fn, k);
            hay_error = 1;
            valor_error = valor_error_nuevo("ErrorTipo", msg, 0);
            return 0;
        }
        v[i] = a[i]->numero;
    }
    return 1;
}

/* distancia(x1, y1, x2, y2) */
static Valor *fn_mat_distancia(Valor **a, int n) {
    double v[4];
    if (!mat_nums(a, n, 4, v, "distancia")) return valor_nulo();
    return valor_numero(hypot(v[2] - v[0], v[3] - v[1]));
}
/* angulo(x1, y1, x2, y2): en grados, de (x1, y1) hacia (x2, y2); 0 = derecha, 90 = abajo */
static Valor *fn_mat_angulo(Valor **a, int n) {
    double v[4];
    if (!mat_nums(a, n, 4, v, "angulo")) return valor_nulo();
    return valor_numero(atan2(v[3] - v[1], v[2] - v[0]) * 180.0 / M_PI);
}
/* interpolar(a, b, t): de a (t = 0) a b (t = 1) */
static Valor *fn_mat_interpolar(Valor **a, int n) {
    double v[3];
    if (!mat_nums(a, n, 3, v, "interpolar")) return valor_nulo();
    return valor_numero(v[0] + (v[1] - v[0]) * v[2]);
}
/* limitar(v, minimo, maximo) */
static Valor *fn_mat_limitar(Valor **a, int n) {
    double v[3];
    if (!mat_nums(a, n, 3, v, "limitar")) return valor_nulo();
    return valor_numero(v[0] < v[1] ? v[1] : v[0] > v[2] ? v[2] : v[0]);
}
static Valor *fn_mat_radianes(Valor **a, int n) {
    double v[1];
    if (!mat_nums(a, n, 1, v, "radianes")) return valor_nulo();
    return valor_numero(v[0] * M_PI / 180.0);
}
static Valor *fn_mat_grados(Valor **a, int n) {
    double v[1];
    if (!mat_nums(a, n, 1, v, "grados")) return valor_nulo();
    return valor_numero(v[0] * 180.0 / M_PI);
}
/* ── Vectores: un vector es una lista [x, y] ── */
static Valor *mat_par(double x, double y) {
    Valor *l = valor_lista_crear();
    lista_agregar(l, valor_numero(x));
    lista_agregar(l, valor_numero(y));
    return l;
}
/* longitud(x, y): largo del vector */
static Valor *fn_mat_longitud(Valor **a, int n) {
    double v[2];
    if (!mat_nums(a, n, 2, v, "longitud")) return valor_nulo();
    return valor_numero(hypot(v[0], v[1]));
}
/* normalizar(x, y): el mismo vector con largo 1 ([0, 0] si es nulo) */
static Valor *fn_mat_normalizar(Valor **a, int n) {
    double v[2];
    if (!mat_nums(a, n, 2, v, "normalizar")) return valor_nulo();
    double l = hypot(v[0], v[1]);
    return l > 0 ? mat_par(v[0] / l, v[1] / l) : mat_par(0, 0);
}
/* vector(angulo, largo): [x, y] que apunta hacia ese ángulo (grados, como angulo()) */
static Valor *fn_mat_vector(Valor **a, int n) {
    double v[2];
    if (!mat_nums(a, n, 2, v, "vector")) return valor_nulo();
    double r = v[0] * M_PI / 180.0;
    return mat_par(cos(r) * v[1], sin(r) * v[1]);
}
/* rotar(x, y, grados): gira el vector (positivo = como las agujas del reloj en pantalla) */
static Valor *fn_mat_rotar(Valor **a, int n) {
    double v[3];
    if (!mat_nums(a, n, 3, v, "rotar")) return valor_nulo();
    double r = v[2] * M_PI / 180.0, c = cos(r), s = sin(r);
    return mat_par(v[0] * c - v[1] * s, v[0] * s + v[1] * c);
}
/* acercar(actual, objetivo, paso): avanza hacia el objetivo sin pasarse */
static Valor *fn_mat_acercar(Valor **a, int n) {
    double v[3];
    if (!mat_nums(a, n, 3, v, "acercar")) return valor_nulo();
    double paso = fabs(v[2]);
    if (fabs(v[1] - v[0]) <= paso) return valor_numero(v[1]);
    return valor_numero(v[0] + (v[1] > v[0] ? paso : -paso));
}

static Valor *fn_mat_signo(Valor **a, int n) {
    double v[1];
    if (!mat_nums(a, n, 1, v, "signo")) return valor_nulo();
    return valor_numero(v[0] > 0 ? 1 : v[0] < 0 ? -1 : 0);
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
    if (remove(a[0]->texto) == 0) return valor_booleano(1);
    return valor_booleano(rmdir(a[0]->texto) == 0);      /* carpeta vacía (Windows) */
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

/* ahora(): segundos desde 1970, con decimales (resolución de microsegundos) */
static Valor *fn_tpo_ahora(Valor **a, int n) {
    (void)a; (void)n;
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return valor_numero((double)(t - 116444736000000000ULL) / 1e7);   /* de 1601 a 1970 */
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return valor_numero((double)ts.tv_sec + ts.tv_nsec / 1e9);
#endif
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

/* Lee un número de hasta 'max' cifras; NULL si no hay ninguna */
static const char *tpo_leer_num(const char *p, int max, int *out) {
    int v = 0, k = 0;
    while (k < max && isdigit((unsigned char)*p)) { v = v * 10 + (*p - '0'); p++; k++; }
    if (!k) return NULL;
    *out = v;
    return p;
}

/* strptime propio (igual en todas las plataformas): %Y %y %m %d %H %M %S %%;
   un espacio del formato acepta cualquier cantidad de espacios. */
static int tpo_strptime(const char *t, const char *f, struct tm *tm) {
    while (*f) {
        if (*f == '%') {
            int v, max = 2;
            char c = f[1];
            if (!c) return 0;
            f += 2;
            if (c == '%') { if (*t++ != '%') return 0; continue; }
            if (c == 'Y') max = 4;
            if (!strchr("YymdHMS", c) || !(t = tpo_leer_num(t, max, &v))) return 0;
            switch (c) {
                case 'Y': tm->tm_year = v - 1900; break;
                case 'y': tm->tm_year = v < 69 ? v + 100 : v; break;
                case 'm': if (v < 1 || v > 12) return 0; tm->tm_mon = v - 1; break;
                case 'd': if (v < 1 || v > 31) return 0; tm->tm_mday = v; break;
                case 'H': if (v > 23) return 0; tm->tm_hour = v; break;
                case 'M': if (v > 59) return 0; tm->tm_min = v; break;
                case 'S': if (v > 60) return 0; tm->tm_sec = v; break;
            }
        } else if (isspace((unsigned char)*f)) {
            while (isspace((unsigned char)*t)) t++;
            f++;
        } else if (*t++ != *f++) {
            return 0;
        }
    }
    return *t == '\0';
}

static Valor *tpo_analizar(Valor **a, int n, int utc) {
    if (n < 2 || a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "analizar() espera (texto, formato)", 0);
        return valor_nulo();
    }
    struct tm tmv;
    memset(&tmv, 0, sizeof tmv);
    tmv.tm_mday = 1;
    tmv.tm_year = 70;
    tmv.tm_isdst = -1;
    if (!tpo_strptime(a[0]->texto, a[1]->texto, &tmv)) return valor_nulo();   /* no encaja */
    return valor_numero((double)(utc ? lince_timegm(&tmv) : mktime(&tmv)));
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

/* Archivo temporal propio para la salida de errores de un comando */
static int sis_temporal(char *ruta, size_t max) {
#ifdef _WIN32
    char dir[MAX_PATH];
    if (max < MAX_PATH || !GetTempPathA(sizeof dir, dir) || !GetTempFileNameA(dir, "lnc", 0, ruta)) return 0;
    return 1;
#else
    const char *dir = getenv("TMPDIR");
    snprintf(ruta, max, "%s/lince_err_XXXXXX", dir && *dir ? dir : "/tmp");
    int fd = mkstemp(ruta);
    if (fd < 0) return 0;
    close(fd);
    return 1;
#endif
}

static Valor *fn_sis_proceso(Valor **a, int n) {
    /* proceso(cmd) → diccionario {salida, error, codigo, ok} */
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) return valor_nulo();

    char ruta_err[4096] = "";
    int con_err = sis_temporal(ruta_err, sizeof ruta_err);
    size_t largo_cmd = strlen(a[0]->texto) + strlen(ruta_err) + 32;
    char *cmd = malloc(largo_cmd);
    if (!cmd) return valor_nulo();
    /* El comando va en su propio proceso para que la redirección recoja todo
       lo que escriba en la salida de errores, también con '1>&2' dentro */
#ifdef _WIN32
    if (con_err) snprintf(cmd, largo_cmd, "cmd /s /c \"%s\" 2>\"%s\"", a[0]->texto, ruta_err);
#else
    if (con_err) snprintf(cmd, largo_cmd, "(%s\n) 2>\"%s\"", a[0]->texto, ruta_err);
#endif
    else         snprintf(cmd, largo_cmd, "%s", a[0]->texto);
    FILE *proc = popen(cmd, "r");
    free(cmd);
    if (!proc) { if (con_err) remove(ruta_err); return valor_nulo(); }

    char buf[65536] = "";
    char linea[4096];
    while (fgets(linea, sizeof(linea), proc))
        strncat(buf, linea, sizeof(buf) - strlen(buf) - 1);
    int estado = pclose(proc);
#ifdef _WIN32
    int codigo = estado;                       /* en Windows ya es el código de salida */
#else
    int codigo = WIFEXITED(estado) ? WEXITSTATUS(estado) : -1;
#endif

    /* Leer stderr */
    char buf_err[65536] = "";
    if (con_err) {
        FILE *ferr = fopen(ruta_err, "r");
        if (ferr) {
            while (fgets(linea, sizeof(linea), ferr))
                strncat(buf_err, linea, sizeof(buf_err) - strlen(buf_err) - 1);
            fclose(ferr);
        }
        remove(ruta_err);
    }

    Valor *res = valor_diccionario_crear();
    valor_diccionario_agregar(res, "salida", valor_texto(buf));
    valor_diccionario_agregar(res, "error",  valor_texto(buf_err));
    valor_diccionario_agregar(res, "codigo", valor_numero(codigo));
    valor_diccionario_agregar(res, "ok",     valor_booleano(codigo == 0));
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

/* ── JSON ──
   json_a_valor: el valor de Lince de un texto JSON (NULL si no es JSON
   válido); json_de_valor: el texto JSON de un valor (malloc). Lo usan el
   módulo json y las partidas guardadas de juego. */
typedef struct { const char *s; size_t i; int hondo; } JsonLector;
typedef struct { char *p; size_t n, cap; } JsonBuf;

static void jb_mas(JsonBuf *b, const char *p, size_t n) {
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, p, n);
    b->n += n;
    b->p[b->n] = '\0';
}
static void jb_txt(JsonBuf *b, const char *t) { jb_mas(b, t, strlen(t)); }

static void json_espacios(JsonLector *l) {
    while (l->s[l->i] == ' ' || l->s[l->i] == '\t' || l->s[l->i] == '\n' || l->s[l->i] == '\r') l->i++;
}

static void jb_utf8(JsonBuf *b, unsigned c) {
    char u[4];
    if (c < 0x80) { u[0] = (char)c; jb_mas(b, u, 1); }
    else if (c < 0x800) { u[0] = (char)(0xC0 | c >> 6); u[1] = (char)(0x80 | (c & 0x3F)); jb_mas(b, u, 2); }
    else if (c < 0x10000) { u[0] = (char)(0xE0 | c >> 12); u[1] = (char)(0x80 | ((c >> 6) & 0x3F)); u[2] = (char)(0x80 | (c & 0x3F)); jb_mas(b, u, 3); }
    else { u[0] = (char)(0xF0 | c >> 18); u[1] = (char)(0x80 | ((c >> 12) & 0x3F)); u[2] = (char)(0x80 | ((c >> 6) & 0x3F)); u[3] = (char)(0x80 | (c & 0x3F)); jb_mas(b, u, 4); }
}

static int json_hex4(JsonLector *l, unsigned *c) {
    *c = 0;
    for (int k = 0; k < 4; k++) {
        char h = l->s[l->i++];
        *c <<= 4;
        if (h >= '0' && h <= '9') *c |= (unsigned)(h - '0');
        else if (h >= 'a' && h <= 'f') *c |= (unsigned)(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') *c |= (unsigned)(h - 'A' + 10);
        else return 0;
    }
    return 1;
}

/* El texto entre comillas (malloc), o NULL si está mal */
static char *json_leer_texto(JsonLector *l) {
    if (l->s[l->i] != '"') return NULL;
    l->i++;
    JsonBuf b = {0};
    jb_mas(&b, "", 0);
    for (;;) {
        unsigned char c = (unsigned char)l->s[l->i];
        if (c == '\0') { free(b.p); return NULL; }
        l->i++;
        if (c == '"') return b.p;
        if (c != '\\') { jb_mas(&b, (const char *)&c, 1); continue; }
        char e = l->s[l->i++];
        switch (e) {
            case '"': jb_txt(&b, "\""); break;
            case '\\': jb_txt(&b, "\\"); break;
            case '/': jb_txt(&b, "/"); break;
            case 'b': jb_txt(&b, "\b"); break;
            case 'f': jb_txt(&b, "\f"); break;
            case 'n': jb_txt(&b, "\n"); break;
            case 'r': jb_txt(&b, "\r"); break;
            case 't': jb_txt(&b, "\t"); break;
            case 'u': {
                unsigned u, u2;
                if (!json_hex4(l, &u)) { free(b.p); return NULL; }
                if (u >= 0xD800 && u < 0xDC00 && l->s[l->i] == '\\' && l->s[l->i + 1] == 'u') {
                    l->i += 2;
                    if (!json_hex4(l, &u2)) { free(b.p); return NULL; }
                    if (u2 >= 0xDC00 && u2 < 0xE000) u = 0x10000 + ((u - 0xD800) << 10) + (u2 - 0xDC00);
                }
                jb_utf8(&b, u);
                break;
            }
            default: free(b.p); return NULL;
        }
    }
}

static Valor *json_leer(JsonLector *l) {
    json_espacios(l);
    const char *s = l->s;
    char c = s[l->i];
    if (++l->hondo > 500) return NULL;
    Valor *r = NULL;
    if (c == '"') {
        char *t = json_leer_texto(l);
        if (t) { r = valor_texto(t); free(t); }
    } else if (c == '[') {
        l->i++;
        r = valor_lista_crear();
        json_espacios(l);
        if (s[l->i] == ']') l->i++;
        else for (;;) {
            Valor *e = json_leer(l);
            if (!e) { valor_destruir(r); r = NULL; break; }
            lista_agregar(r, e);
            json_espacios(l);
            if (s[l->i] == ',') { l->i++; continue; }
            if (s[l->i] == ']') { l->i++; break; }
            valor_destruir(r); r = NULL; break;
        }
    } else if (c == '{') {
        l->i++;
        r = valor_diccionario_crear();
        json_espacios(l);
        if (s[l->i] == '}') l->i++;
        else for (;;) {
            json_espacios(l);
            char *clave = json_leer_texto(l);
            json_espacios(l);
            Valor *v = NULL;
            if (clave && s[l->i] == ':') { l->i++; v = json_leer(l); }
            if (!v) { free(clave); valor_destruir(r); r = NULL; break; }
            int ya = valor_diccionario_buscar(r, clave);
            if (ya >= 0) { valor_destruir(r->diccionario.valores[ya]); r->diccionario.valores[ya] = v; }
            else valor_diccionario_agregar(r, clave, v);
            free(clave);
            json_espacios(l);
            if (s[l->i] == ',') { l->i++; continue; }
            if (s[l->i] == '}') { l->i++; break; }
            valor_destruir(r); r = NULL; break;
        }
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        char *fin;
        double d = strtod(s + l->i, &fin);
        if (fin > s + l->i) { l->i = (size_t)(fin - s); r = valor_numero(d); }
    } else if (strncmp(s + l->i, "true", 4) == 0) { l->i += 4; r = valor_booleano(1); }
    else if (strncmp(s + l->i, "false", 5) == 0) { l->i += 5; r = valor_booleano(0); }
    else if (strncmp(s + l->i, "null", 4) == 0) { l->i += 4; r = valor_nulo(); }
    l->hondo--;
    return r;
}

Valor *json_a_valor(const char *s) {
    JsonLector l = {s, 0, 0};
    Valor *v = json_leer(&l);
    if (!v) return NULL;
    json_espacios(&l);
    if (s[l.i] != '\0') { valor_destruir(v); return NULL; }
    return v;
}

static void json_escribir_texto(JsonBuf *b, const char *t) {
    jb_txt(b, "\"");
    for (const unsigned char *p = (const unsigned char *)t; *p; p++) {
        char esc[8];
        switch (*p) {
            case '"':  jb_txt(b, "\\\""); break;
            case '\\': jb_txt(b, "\\\\"); break;
            case '\n': jb_txt(b, "\\n"); break;
            case '\r': jb_txt(b, "\\r"); break;
            case '\t': jb_txt(b, "\\t"); break;
            default:
                if (*p < 0x20) { snprintf(esc, sizeof esc, "\\u%04x", *p); jb_txt(b, esc); }
                else jb_mas(b, (const char *)p, 1);
        }
    }
    jb_txt(b, "\"");
}

/* 'sangria' < 0: todo en una línea; si no, con saltos y esa sangría */
static void json_escribir(JsonBuf *b, Valor *v, int sangria, int hondo) {
    char tmp[64];
    if (hondo > 500) { jb_txt(b, "null"); return; }
    switch (v ? v->tipo : VAL_NULO) {
        case VAL_NUMERO:
            if (v->numero != v->numero || v->numero - v->numero != 0) jb_txt(b, "null");   /* NaN o infinito */
            else if (v->numero == (long long)v->numero && fabs(v->numero) < 1e15) {
                snprintf(tmp, sizeof tmp, "%lld", (long long)v->numero); jb_txt(b, tmp);
            } else {
                for (int cifras = 15; cifras <= 17; cifras++) {          /* lo más corto que vuelve igual */
                    snprintf(tmp, sizeof tmp, "%.*g", cifras, v->numero);
                    if (strtod(tmp, NULL) == v->numero) break;
                }
                jb_txt(b, tmp);
            }
            break;
        case VAL_TEXTO: json_escribir_texto(b, v->texto); break;
        case VAL_ENUMERADO: json_escribir_texto(b, v->miembro->texto); break;   /* "Estado.Quieto" */
        case VAL_BOOLEANO: jb_txt(b, v->booleano ? "true" : "false"); break;
        case VAL_LISTA:
        case VAL_DICCIONARIO: {
            int dic = v->tipo == VAL_DICCIONARIO;
            int n = dic ? v->diccionario.cantidad : v->lista.cantidad;
            jb_txt(b, dic ? "{" : "[");
            for (int i = 0; i < n; i++) {
                if (i > 0) jb_txt(b, sangria < 0 ? ", " : ",");
                if (sangria >= 0) {
                    jb_txt(b, "\n");
                    for (int k = 0; k < (hondo + 1) * sangria; k++) jb_txt(b, " ");
                }
                if (dic) {
                    json_escribir_texto(b, v->diccionario.claves[i]);
                    jb_txt(b, ": ");
                    json_escribir(b, v->diccionario.valores[i], sangria, hondo + 1);
                } else json_escribir(b, v->lista.elementos[i], sangria, hondo + 1);
            }
            if (sangria >= 0 && n > 0) {
                jb_txt(b, "\n");
                for (int k = 0; k < hondo * sangria; k++) jb_txt(b, " ");
            }
            jb_txt(b, dic ? "}" : "]");
            break;
        }
        default: jb_txt(b, "null"); break;
    }
}

char *json_de_valor(Valor *v, int sangria) {
    JsonBuf b = {0};
    jb_mas(&b, "", 0);
    json_escribir(&b, v, sangria, 0);
    return b.p;
}

/* Lo que no se puede guardar en JSON (funciones, objetos...), o NULL */
const char *json_lo_que_no_cabe(Valor *v) {
    if (!v) return NULL;
    switch (v->tipo) {
        case VAL_NULO: case VAL_NUMERO: case VAL_TEXTO: case VAL_BOOLEANO: case VAL_ENUMERADO: return NULL;
        case VAL_LISTA:
            for (int i = 0; i < v->lista.cantidad; i++) {
                const char *m = json_lo_que_no_cabe(v->lista.elementos[i]);
                if (m) return m;
            }
            return NULL;
        case VAL_DICCIONARIO:
            for (int i = 0; i < v->diccionario.cantidad; i++) {
                const char *m = json_lo_que_no_cabe(v->diccionario.valores[i]);
                if (m) return m;
            }
            return NULL;
        default: return "solo números, textos, lógicos, nulo, listas y diccionarios";
    }
}

static Valor *fn_json_parsear(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_TEXTO) {
        hay_error = 1;
        valor_error = valor_error_nuevo("ErrorTipo", "json.parsear() espera un texto", 0);
        return valor_nulo();
    }
    Valor *v = json_a_valor(a[0]->texto);
    if (!v) {
        hay_error = 1;
        valor_error = valor_error_nuevo("Error", "json.parsear(): el texto no es JSON válido", 0);
        return valor_nulo();
    }
    return v;
}

static Valor *fn_json_texto(Valor **a, int n) {
    int sangria = -1;
    if (n > 1 && a[1]->tipo == VAL_BOOLEANO && a[1]->booleano) sangria = 2;
    char *t = json_de_valor(a[0], sangria);
    Valor *r = valor_texto(t);
    free(t);
    return r;
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
        case VAL_ENUMERADO: return strcmp(a->miembro->texto, b->miembro->texto) == 0;
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

#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <limits.h>
#include "ogg.h"
#include "imagen.h"
#include "letra.h"
#include "empaquetado.h"
#include "recarga.h"
#include "biblioteca_juego.h"
#include "tiled.h"
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
        {"semilla",        fn_mat_semilla,       1},
        {"distancia",      fn_mat_distancia,     4},
        {"angulo",         fn_mat_angulo,        4},
        {"interpolar",     fn_mat_interpolar,    3},
        {"limitar",        fn_mat_limitar,       3},
        {"radianes",       fn_mat_radianes,      1},
        {"grados",         fn_mat_grados,        1},
        {"signo",          fn_mat_signo,         1},
        {"longitud",       fn_mat_longitud,      2},
        {"normalizar",     fn_mat_normalizar,    2},
        {"vector",         fn_mat_vector,        2},
        {"rotar",          fn_mat_rotar,         3},
        {"acercar",        fn_mat_acercar,       3},
    };
    const char *cnames[] = {"PI", "E", "TAU"};
    double      cvals[]  = {M_PI, M_E, M_PI * 2};
    registrar_modulo_diccionario(entorno, "matematica",
        fns, (int)(sizeof(fns) / sizeof(fns[0])), cnames, cvals, 3);
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
        {"texto",   fn_json_texto,  -1},
    };
    registrar_modulo_diccionario(entorno, "json",
        fns, 2, NULL, NULL, 0);
}

/* ─────────────────────────────────────────
   MÓDULO: bytes
   Datos binarios: leer y escribir archivos byte a byte (formatos de imagen,
   niveles comprimidos, partidas guardadas...). Un valor bytes se usa como una
   lista de números de 0 a 255: b[i], b.longitud(), para cada...
───────────────────────────────────────── */
static Valor *bytes_error(const char *tipo, const char *msg) {
    hay_error = 1;
    valor_error = valor_error_nuevo(tipo, msg, 0);
    return valor_nulo();
}

/* crear(n[, valor]): n bytes, todos a 'valor' (0 por defecto) */
static Valor *fn_bytes_crear(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_NUMERO || a[0]->numero < 0 || a[0]->numero > 1e9)
        return bytes_error("ErrorTipo", "crear() espera cuántos bytes (y, si quieres, su valor)");
    int v = 0;
    if (n > 1) {
        if (a[1]->tipo != VAL_NUMERO || a[1]->numero < 0 || a[1]->numero > 255)
            return bytes_error("ErrorTipo", "crear(): el valor es un número de 0 a 255");
        v = (int)a[1]->numero;
    }
    Valor *b = valor_bytes_crear((int)a[0]->numero);
    if (v) memset(b->bytes.datos, v, (size_t)b->bytes.cantidad);
    return b;
}

/* desde_lista([72, 111, 108, 97]) */
static Valor *fn_bytes_desde_lista(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_LISTA) return bytes_error("ErrorTipo", "desde_lista() espera una lista de números de 0 a 255");
    Valor *b = valor_bytes_crear(a[0]->lista.cantidad);
    for (int i = 0; i < a[0]->lista.cantidad; i++) {
        Valor *e = a[0]->lista.elementos[i];
        if (e->tipo != VAL_NUMERO || e->numero < 0 || e->numero > 255 || e->numero != floor(e->numero)) {
            valor_destruir(b);
            char msg[120];
            snprintf(msg, sizeof msg, "desde_lista(): el elemento %d no es un número entero de 0 a 255", i);
            return bytes_error("ErrorTipo", msg);
        }
        b->bytes.datos[i] = (unsigned char)e->numero;
    }
    return b;
}

/* desde_texto("hola"): los bytes del texto en UTF-8 */
static Valor *fn_bytes_desde_texto(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return bytes_error("ErrorTipo", "desde_texto() espera un texto");
    size_t l = strlen(a[0]->texto);
    Valor *b = valor_bytes_crear((int)l);
    memcpy(b->bytes.datos, a[0]->texto, l);
    return b;
}

/* leer(ruta): el archivo entero (también desde un ejecutable de lince compilar) */
static Valor *fn_bytes_leer(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return bytes_error("ErrorTipo", "leer() espera la ruta de un archivo");
    size_t largo = 0;
    char *c = interprete_leer_bytes(a[0]->texto, &largo);
    if (!c) {
        char msg[600];
        snprintf(msg, sizeof msg, "no se pudo leer '%s'", a[0]->texto);
        return bytes_error("Error", msg);
    }
    if (largo > 0x7fffffff) { free(c); return bytes_error("Error", "leer(): el archivo es demasiado grande"); }
    Valor *b = valor_bytes_crear((int)largo);
    memcpy(b->bytes.datos, c, largo);
    free(c);
    return b;
}

/* guardar(ruta, bytes) */
static Valor *fn_bytes_guardar(Valor **a, int n) {
    if (n < 2 || a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_BYTES)
        return bytes_error("ErrorTipo", "guardar() espera (ruta, bytes)");
    FILE *f = fopen(a[0]->texto, "wb");
    if (!f) {
        char msg[600];
        snprintf(msg, sizeof msg, "no se pudo crear '%s'", a[0]->texto);
        return bytes_error("Error", msg);
    }
    int ok = fwrite(a[1]->bytes.datos, 1, (size_t)a[1]->bytes.cantidad, f) == (size_t)a[1]->bytes.cantidad;
    ok = fclose(f) == 0 && ok;
    if (!ok) return bytes_error("Error", "guardar(): no se pudo escribir el archivo");
    return valor_nulo();
}

/* unir([b1, b2, ...]): unos bytes detrás de otros */
static Valor *fn_bytes_unir(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_LISTA) return bytes_error("ErrorTipo", "unir() espera una lista de bytes");
    long total = 0;
    for (int i = 0; i < a[0]->lista.cantidad; i++) {
        if (a[0]->lista.elementos[i]->tipo != VAL_BYTES) return bytes_error("ErrorTipo", "unir(): cada elemento de la lista son bytes");
        total += a[0]->lista.elementos[i]->bytes.cantidad;
    }
    if (total > 0x7fffffff) return bytes_error("Error", "unir(): demasiado grande");
    Valor *b = valor_bytes_crear((int)total);
    long p = 0;
    for (int i = 0; i < a[0]->lista.cantidad; i++) {
        Valor *e = a[0]->lista.elementos[i];
        memcpy(b->bytes.datos + p, e->bytes.datos, (size_t)e->bytes.cantidad);
        p += e->bytes.cantidad;
    }
    return b;
}

static void cargar_bytes(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"crear",       fn_bytes_crear,       -1},
        {"desde_lista", fn_bytes_desde_lista,  1},
        {"desde_texto", fn_bytes_desde_texto,  1},
        {"leer",        fn_bytes_leer,         1},
        {"guardar",     fn_bytes_guardar,      2},
        {"unir",        fn_bytes_unir,         1},
    };
    registrar_modulo_diccionario(entorno, "bytes", fns, (int)(sizeof fns / sizeof fns[0]), NULL, NULL, 0);
}

/* ─────────────────────────────────────────
   MÓDULO: bits
   Operaciones bit a bit sobre enteros de 32 bits sin signo (de 0 a
   4294967295). Los negativos se toman en complemento a dos.
───────────────────────────────────────── */
static int bits_num(Valor **a, int n, int i, uint32_t *out, const char *fn) {
    if (i >= n || a[i]->tipo != VAL_NUMERO || a[i]->numero != floor(a[i]->numero) ||
        a[i]->numero < -2147483648.0 || a[i]->numero > 4294967295.0) {
        char msg[160];
        snprintf(msg, sizeof msg, "%s(): el argumento %d tiene que ser un número entero de 32 bits", fn, i + 1);
        bytes_error("ErrorTipo", msg);
        return 0;
    }
    *out = (uint32_t)(int64_t)a[i]->numero;
    return 1;
}

#define BITS_DOS(nombre, expr) \
    static Valor *fn_bits_##nombre(Valor **a, int n) { \
        uint32_t x, y; \
        if (!bits_num(a, n, 0, &x, #nombre) || !bits_num(a, n, 1, &y, #nombre)) return valor_nulo(); \
        return valor_numero((double)(uint32_t)(expr)); }
BITS_DOS(ambos, x & y)            /* AND: 1 donde los dos tienen 1 */
BITS_DOS(alguno, x | y)           /* OR: 1 donde alguno tiene 1 */
BITS_DOS(solo_uno, x ^ y)         /* XOR: 1 donde solo uno tiene 1 */
BITS_DOS(izquierda, y >= 32 ? 0 : x << y)
BITS_DOS(derecha, y >= 32 ? 0 : x >> y)
#undef BITS_DOS

static Valor *fn_bits_invertir(Valor **a, int n) {
    uint32_t x;
    if (!bits_num(a, n, 0, &x, "invertir")) return valor_nulo();
    return valor_numero((double)(uint32_t)~x);
}

/* leer(numero, posicion): el bit en esa posición (0 = el de menos valor) */
static Valor *fn_bits_leer(Valor **a, int n) {
    uint32_t x, p;
    if (!bits_num(a, n, 0, &x, "leer") || !bits_num(a, n, 1, &p, "leer")) return valor_nulo();
    return valor_numero(p < 32 ? (x >> p) & 1 : 0);
}

/* poner(numero, posicion, 0 o 1): el número con ese bit cambiado */
static Valor *fn_bits_poner(Valor **a, int n) {
    uint32_t x, p, v;
    if (!bits_num(a, n, 0, &x, "poner") || !bits_num(a, n, 1, &p, "poner") || !bits_num(a, n, 2, &v, "poner")) return valor_nulo();
    if (p >= 32) return bytes_error("ErrorRango", "poner(): la posición va de 0 a 31");
    return valor_numero((double)(v ? x | (1u << p) : x & ~(1u << p)));
}

static Valor *fn_bits_contar(Valor **a, int n) {
    uint32_t x;
    if (!bits_num(a, n, 0, &x, "contar")) return valor_nulo();
    int c = 0;
    for (; x; x &= x - 1) c++;
    return valor_numero(c);
}

/* hex(n[, cifras]) y binario(n[, cifras]): como texto, con ceros delante hasta 'cifras' */
static Valor *bits_en_base(Valor **a, int n, int base, const char *fn) {
    uint32_t x;
    if (!bits_num(a, n, 0, &x, fn)) return valor_nulo();
    int cifras = n > 1 && a[1]->tipo == VAL_NUMERO ? (int)a[1]->numero : 1;
    if (cifras < 1) cifras = 1;
    if (cifras > 32) cifras = 32;
    char buf[40];
    int i = 39;
    buf[i] = '\0';
    do { buf[--i] = "0123456789abcdef"[x % (uint32_t)base]; x /= (uint32_t)base; } while (x && i > 0);
    while (39 - i < cifras) buf[--i] = '0';
    return valor_texto(buf + i);
}
static Valor *fn_bits_hex(Valor **a, int n)     { return bits_en_base(a, n, 16, "hex"); }
static Valor *fn_bits_binario(Valor **a, int n) { return bits_en_base(a, n, 2, "binario"); }

/* de_hex("ff") y de_binario("1010"): el número */
static Valor *bits_de_base(Valor **a, int n, int base, const char *fn) {
    char msg[160];
    snprintf(msg, sizeof msg, "%s() espera un texto con cifras %s", fn, base == 16 ? "hexadecimales (0-9, a-f)" : "binarias (0 y 1)");
    if (n < 1 || a[0]->tipo != VAL_TEXTO || !a[0]->texto[0]) return bytes_error("ErrorTipo", msg);
    double v = 0;
    for (const char *c = a[0]->texto; *c; c++) {
        int d = isdigit((unsigned char)*c) ? *c - '0' : (*c | 32) >= 'a' && (*c | 32) <= 'f' ? (*c | 32) - 'a' + 10 : 99;
        if (d >= base) return bytes_error("ErrorTipo", msg);
        v = v * base + d;
    }
    if (v > 4294967295.0) return bytes_error("ErrorRango", "el número no cabe en 32 bits");
    return valor_numero(v);
}
static Valor *fn_bits_de_hex(Valor **a, int n)     { return bits_de_base(a, n, 16, "de_hex"); }
static Valor *fn_bits_de_binario(Valor **a, int n) { return bits_de_base(a, n, 2, "de_binario"); }

static void cargar_bits(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"ambos",      fn_bits_ambos,      2},
        {"alguno",     fn_bits_alguno,     2},
        {"solo_uno",   fn_bits_solo_uno,   2},
        {"invertir",   fn_bits_invertir,   1},
        {"izquierda",  fn_bits_izquierda,  2},
        {"derecha",    fn_bits_derecha,    2},
        {"leer",       fn_bits_leer,       2},
        {"poner",      fn_bits_poner,      3},
        {"contar",     fn_bits_contar,     1},
        {"hex",        fn_bits_hex,       -1},
        {"binario",    fn_bits_binario,   -1},
        {"de_hex",     fn_bits_de_hex,     1},
        {"de_binario", fn_bits_de_binario, 1},
    };
    registrar_modulo_diccionario(entorno, "bits", fns, (int)(sizeof fns / sizeof fns[0]), NULL, NULL, 0);
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
    {"bytes", cargar_bytes},
    {"bits", cargar_bits},
};
#define NUM_MODULOS_ESTANDAR ((int)(sizeof(MODULOS_ESTANDAR) / sizeof(MODULOS_ESTANDAR[0])))

const char *modulo_estandar(int i) {
    return (i >= 0 && i < NUM_MODULOS_ESTANDAR) ? MODULOS_ESTANDAR[i].nombre : NULL;
}

void modulo_cargar(const char *nombre, Entorno *entorno) {

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
        "El módulo '%s' no existe. Módulos disponibles: juego, matematica, texto, archivos, tiempo, sistema, json, pruebas, bytes y bits",
        nombre);
    valor_error = valor_error_nuevo("Error", msg, 0);
}
