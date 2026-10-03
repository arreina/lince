/*
 * 🐆 Lince — compilador.c
 * Genera código C desde el AST de Lince.
 * El C generado se compila con gcc para producir un binario nativo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compilador.h"
#include "parser.h"

/* ─────────────────────────────────────────
   CONTEXTO DEL COMPILADOR
───────────────────────────────────────── */
typedef struct {
    FILE *salida;
    int   sangria;
    int   tmp_count;
    int   err_count;
    int   lambda_count;
    char  nombre_prog[256];
    char  clases[64][64];
    char  padres[64][64];
    int   num_clases;
    FILE *lambdas;
    char  lambda_file[256];
    /* Tabla de funciones declaradas (no closures) */
    char  funciones[128][64];
    int   num_funciones;
    int   aridad[128];           /* número de parámetros requeridos */
    int   aridad_total[128];     /* número total de parámetros */
    /* Módulos ya emitidos (para no duplicar) */
    char  modulos_emitidos[16][32];
    int   num_modulos;
    /* Contexto de generador en compilación */
    int   en_generador;          /* 1 si estamos dentro de un generador */
    char  gen_lista_var[64];     /* nombre de la variable lista acumuladora */
} Compilador;

static int es_funcion_declarada(Compilador *c, const char *nombre) {
    for (int i = 0; i < c->num_funciones; i++)
        if (strcmp(c->funciones[i], nombre) == 0) return 1;
    return 0;
}

static void registrar_funcion(Compilador *c, const char *nombre) {
    if (c->num_funciones < 128) {
        strncpy(c->funciones[c->num_funciones], nombre, 63);
        c->aridad[c->num_funciones]       = 0;
        c->aridad_total[c->num_funciones] = 0;
        c->num_funciones++;
    }
}

static void registrar_funcion_con_aridad(Compilador *c, const char *nombre,
                                          int req, int total) {
    if (c->num_funciones < 128) {
        strncpy(c->funciones[c->num_funciones], nombre, 63);
        c->aridad[c->num_funciones]       = req;
        c->aridad_total[c->num_funciones] = total;
        c->num_funciones++;
    }
}

static int aridad_requerida(Compilador *c, const char *nombre) {
    for (int i = 0; i < c->num_funciones; i++)
        if (strcmp(c->funciones[i], nombre) == 0)
            return c->aridad[i];
    return -1;
}

static int aridad_total_fn(Compilador *c, const char *nombre) {
    for (int i = 0; i < c->num_funciones; i++)
        if (strcmp(c->funciones[i], nombre) == 0)
            return c->aridad_total[i];
    return -1;
}

static void sangrar(Compilador *c) {
    for (int i = 0; i < c->sangria; i++)
        fprintf(c->salida, "    ");
}

static char *tmp_var(Compilador *c, char *buf) {
    sprintf(buf, "_lince_tmp_%d", c->tmp_count++);
    return buf;
}

/* ─────────────────────────────────────────
   CABECERA DEL RUNTIME
   Tipos y funciones de soporte en C
───────────────────────────────────────── */
static void emitir_cabecera(Compilador *c) {
    fprintf(c->salida,
        "/* Generado por Lince — no editar manualmente */\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <math.h>\n"
        "#include <ctype.h>\n"
        "#include <stdint.h>\n\n"

        "/* ── Tipos del runtime ── */\n"
        "typedef enum { L_NUM, L_TXT, L_BOOL, L_NULO, L_LISTA, L_DIC, L_OBJ } LTipo;\n"
        "typedef struct LValor LValor;\n"
        "typedef struct LObj   LObj;\n\n"

        "struct LValor {\n"
        "    LTipo tipo;\n"
        "    union {\n"
        "        double  num;\n"
        "        char   *txt;\n"
        "        int     boolean;\n"
        "        struct { LValor **elems; int cant; int cap; } lista;\n"
        "        struct { char **claves; LValor **vals; int cant; int cap; } dic;\n"
        "        LObj   *obj;\n"
        "    };\n"
        "};\n\n"

        "struct LObj {\n"
        "    char    *clase;\n"
        "    char   **campos;\n"
        "    LValor **valores;\n"
        "    int      num_campos;\n"
        "};\n\n"

        "/* ── Constructores ── */\n"
        "static LValor *lince_num(double n) {\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_NUM; v->num = n; return v;\n"
        "}\n"
        "static LValor *lince_txt(const char *s) {\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_TXT; v->txt = strdup(s ? s : \"\"); return v;\n"
        "}\n"
        "static LValor *lince_bool(int b) {\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_BOOL; v->boolean = b; return v;\n"
        "}\n"
        "static LValor *lince_nulo(void) {\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_NULO; return v;\n"
        "}\n"
        "static LValor *lince_lista(void) {\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_LISTA;\n"
        "    v->lista.elems = malloc(sizeof(LValor*) * 8);\n"
        "    v->lista.cant  = 0;\n"
        "    v->lista.cap   = 8;\n"
        "    return v;\n"
        "}\n"
        "static void lince_lista_agregar(LValor *l, LValor *e) {\n"
        "    if (l->lista.cant >= l->lista.cap) {\n"
        "        l->lista.cap *= 2;\n"
        "        l->lista.elems = realloc(l->lista.elems,\n"
        "            sizeof(LValor*) * l->lista.cap);\n"
        "    }\n"
        "    l->lista.elems[l->lista.cant++] = e;\n"
        "}\n"
        "static LValor *lince_dic(void) {\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_DIC;\n"
        "    v->dic.cap    = 16;\n"
        "    v->dic.claves = malloc(sizeof(char*)   * v->dic.cap);\n"
        "    v->dic.vals   = malloc(sizeof(LValor*) * v->dic.cap);\n"
        "    v->dic.cant   = 0;\n"
        "    return v;\n"
        "}\n"
        "static void lince_dic_set(LValor *d, const char *k, LValor *v) {\n"
        "    for (int i = 0; i < d->dic.cant; i++)\n"
        "        if (strcmp(d->dic.claves[i], k) == 0) {\n"
        "            d->dic.vals[i] = v; return;\n"
        "        }\n"
        "    if (d->dic.cant >= d->dic.cap) {\n"
        "        d->dic.cap *= 2;\n"
        "        d->dic.claves = realloc(d->dic.claves, sizeof(char*)   * d->dic.cap);\n"
        "        d->dic.vals   = realloc(d->dic.vals,   sizeof(LValor*) * d->dic.cap);\n"
        "    }\n"
        "    d->dic.claves[d->dic.cant] = strdup(k);\n"
        "    d->dic.vals[d->dic.cant]   = v;\n"
        "    d->dic.cant++;\n"
        "}\n"
        "static LValor *lince_dic_get(LValor *d, const char *k) {\n"
        "    for (int i = 0; i < d->dic.cant; i++)\n"
        "        if (strcmp(d->dic.claves[i], k) == 0)\n"
        "            return d->dic.vals[i];\n"
        "    return lince_nulo();\n"
        "}\n\n"

        "/* ── Conversión a texto ── */\n"
        "static char *lince_a_txt(LValor *v) {\n"
        "    if (!v || v->tipo == L_NULO) return strdup(\"nulo\");\n"
        "    if (v->tipo == L_TXT)  return strdup(v->txt);\n"
        "    if (v->tipo == L_BOOL) return strdup(v->boolean ? \"verdadero\" : \"falso\");\n"
        "    if (v->tipo == L_NUM) {\n"
        "        char buf[64];\n"
        "        if (v->num == (long long)v->num)\n"
        "            sprintf(buf, \"%%lld\", (long long)v->num);\n"
        "        else\n"
        "            sprintf(buf, \"%%g\", v->num);\n"
        "        return strdup(buf);\n"
        "    }\n"
        "    if (v->tipo == L_LISTA) {\n"
        "        char buf[4096] = \"[\";\n"
        "        for (int i = 0; i < v->lista.cant; i++) {\n"
        "            if (i > 0) strcat(buf, \", \");\n"
        "            char *s = lince_a_txt(v->lista.elems[i]);\n"
        "            strcat(buf, s); free(s);\n"
        "        }\n"
        "        strcat(buf, \"]\");\n"
        "        return strdup(buf);\n"
        "    }\n"
        "    if (v->tipo == L_DIC) {\n"
        "        char buf[4096] = \"{\";\n"
        "        for (int i = 0; i < v->dic.cant; i++) {\n"
        "            if (i > 0) strcat(buf, \", \");\n"
        "            strcat(buf, \"\\\"\"); strcat(buf, v->dic.claves[i]);\n"
        "            strcat(buf, \"\\\": \");\n"
        "            char *s = lince_a_txt(v->dic.vals[i]);\n"
        "            strcat(buf, s); free(s);\n"
        "        }\n"
        "        strcat(buf, \"}\");\n"
        "        return strdup(buf);\n"
        "    }\n"
        "    return strdup(\"?\");\n"
        "}\n\n"

        "/* ── Concatenación ── */\n"
        "static LValor *lince_concat(LValor *a, LValor *b) {\n"
        "    char *sa = lince_a_txt(a), *sb = lince_a_txt(b);\n"
        "    char *res = malloc(strlen(sa) + strlen(sb) + 1);\n"
        "    strcpy(res, sa); strcat(res, sb);\n"
        "    free(sa); free(sb);\n"
        "    LValor *v = lince_txt(res); free(res);\n"
        "    return v;\n"
        "}\n\n"

        "/* ── Métodos de lista ── */\n"
        "static int lince_cmp_natural(const void *pa, const void *pb) {\n"
        "    LValor *a = *(LValor**)pa, *b = *(LValor**)pb;\n"
        "    if (a->tipo == L_NUM && b->tipo == L_NUM)\n"
        "        return a->num < b->num ? -1 : (a->num > b->num ? 1 : 0);\n"
        "    if (a->tipo == L_TXT && b->tipo == L_TXT) return strcmp(a->txt, b->txt);\n"
        "    if (a->tipo == L_BOOL && b->tipo == L_BOOL)\n"
        "        return (a->boolean ? 1 : 0) - (b->boolean ? 1 : 0);\n"
        "    return (int)a->tipo - (int)b->tipo;\n"
        "}\n\n"

        "static LValor *lince_lista_copia(LValor *l) {\n"
        "    LValor *r = lince_lista();\n"
        "    for (int i = 0; i < l->lista.cant; i++) lince_lista_agregar(r, l->lista.elems[i]);\n"
        "    return r;\n"
        "}\n\n"

        "static LValor *lince_lista_metodo(LValor *l, const char *met, LValor **args, int nargs) {\n"
        "    (void)nargs;\n"
        "    if (strcmp(met, \"longitud\") == 0) return lince_num(l->lista.cant);\n"
        "    if (strcmp(met, \"agregar\") == 0 && nargs >= 1) {\n"
        "        lince_lista_agregar(l, args[0]); return lince_nulo();\n"
        "    }\n"
        "    if (strcmp(met, \"contiene\") == 0 && nargs >= 1) {\n"
        "        for (int i = 0; i < l->lista.cant; i++) {\n"
        "            LValor *e = l->lista.elems[i];\n"
        "            if (e->tipo == args[0]->tipo) {\n"
        "                if (e->tipo == L_NUM && e->num == args[0]->num) return lince_bool(1);\n"
        "                if (e->tipo == L_TXT && strcmp(e->txt, args[0]->txt) == 0) return lince_bool(1);\n"
        "            }\n"
        "        }\n"
        "        return lince_bool(0);\n"
        "    }\n"
        "    if (strcmp(met, \"eliminar\") == 0 && nargs >= 1) {\n"
        "        int idx = (int)args[0]->num;\n"
        "        if (idx < 0) idx = l->lista.cant + idx;\n"
        "        if (idx >= 0 && idx < l->lista.cant) {\n"
        "            for (int i = idx; i < l->lista.cant - 1; i++)\n"
        "                l->lista.elems[i] = l->lista.elems[i+1];\n"
        "            l->lista.cant--;\n"
        "        }\n"
        "        return lince_nulo();\n"
        "    }\n"
        "    if (strcmp(met, \"ordenar\") == 0) {\n"
        "        LValor *r = lince_lista_copia(l);\n"
        "        if (nargs >= 1) {\n"
        "            fprintf(stderr, \"\\n❌ El binario compilado no admite 'ordenar' con \"\n"
        "                            \"comparador; usa ordenar() sin argumentos.\\n\\n\");\n"
        "            exit(1);\n"
        "        }\n"
        "        if (r->lista.cant > 1)\n"
        "            qsort(r->lista.elems, r->lista.cant, sizeof(LValor*), lince_cmp_natural);\n"
        "        return r;\n"
        "    }\n"
        "    if (strcmp(met, \"invertir\") == 0) {\n"
        "        LValor *r = lince_lista();\n"
        "        for (int i = l->lista.cant - 1; i >= 0; i--) lince_lista_agregar(r, l->lista.elems[i]);\n"
        "        return r;\n"
        "    }\n"
        "    if (strcmp(met, \"copiar\") == 0) return lince_lista_copia(l);\n"
        "    if (strcmp(met, \"posicion\") == 0 && nargs >= 1) {\n"
        "        for (int i = 0; i < l->lista.cant; i++) {\n"
        "            LValor *e = l->lista.elems[i];\n"
        "            if (lince_cmp_natural(&e, &args[0]) == 0) return lince_num(i);\n"
        "        }\n"
        "        return lince_num(-1);\n"
        "    }\n"
        "    if (strcmp(met, \"insertar\") == 0 && nargs >= 2) {\n"
        "        int idx = (int)args[0]->num;\n"
        "        if (idx < 0 || idx > l->lista.cant) {\n"
        "            fprintf(stderr, \"\\n❌ Índice fuera de rango en 'insertar'.\\n\\n\");\n"
        "            exit(1);\n"
        "        }\n"
        "        lince_lista_agregar(l, args[1]);\n"
        "        for (int i = l->lista.cant - 1; i > idx; i--)\n"
        "            l->lista.elems[i] = l->lista.elems[i-1];\n"
        "        l->lista.elems[idx] = args[1];\n"
        "        return lince_nulo();\n"
        "    }\n"
        "    fprintf(stderr, \"\\n❌ Las listas no tienen el método '%%s'.\\n\\n\", met);\n"
        "    exit(1);\n"
        "}\n\n"
        "/* ── UTF-8 ──\n"
        "   Han de comportarse igual que utf8_* en src/interprete.c: si se\n"
        "   cambian ahí, hay que cambiarlas aquí. */\n"
        "static int lince_utf8_tam(const char *s) {\n"
        "    unsigned char c = (unsigned char)*s;\n"
        "    if (c == 0)             return 0;\n"
        "    if (c < 0x80)           return 1;\n"
        "    if ((c & 0xE0) == 0xC0) return 2;\n"
        "    if ((c & 0xF0) == 0xE0) return 3;\n"
        "    if ((c & 0xF8) == 0xF0) return 4;\n"
        "    return 1;\n"
        "}\n"
        "static int lince_utf8_longitud(const char *s) {\n"
        "    int n = 0;\n"
        "    while (*s) { s += lince_utf8_tam(s); n++; }\n"
        "    return n;\n"
        "}\n"
        "static int lince_utf8_desp(const char *s, int i) {\n"
        "    int pos = 0, n = 0;\n"
        "    while (s[pos]) {\n"
        "        if (n == i) return pos;\n"
        "        pos += lince_utf8_tam(s + pos);\n"
        "        n++;\n"
        "    }\n"
        "    return (n == i) ? pos : -1;\n"
        "}\n"
        "static char *lince_utf8_caja(const char *s, int arriba) {\n"
        "    size_t n = strlen(s);\n"
        "    char *r = malloc(n + 1);\n"
        "    size_t i = 0;\n"
        "    while (i < n) {\n"
        "        unsigned char c = (unsigned char)s[i];\n"
        "        if (c < 0x80) { r[i] = (char)(arriba ? toupper(c) : tolower(c)); i++; }\n"
        "        else if (c == 0xC3 && i + 1 < n) {\n"
        "            unsigned char d = (unsigned char)s[i+1];\n"
        "            r[i] = (char)c;\n"
        "            if (arriba && d >= 0xA0 && d <= 0xBE && d != 0xB7) r[i+1] = (char)(d - 0x20);\n"
        "            else if (!arriba && d >= 0x80 && d <= 0x9E && d != 0x97) r[i+1] = (char)(d + 0x20);\n"
        "            else r[i+1] = (char)d;\n"
        "            i += 2;\n"
        "        } else {\n"
        "            int k = lince_utf8_tam(s + i);\n"
        "            for (int j = 0; j < k && i + (size_t)j < n; j++) r[i+j] = s[i+j];\n"
        "            i += (size_t)k;\n"
        "        }\n"
        "    }\n"
        "    r[n] = '\\0';\n"
        "    return r;\n"
        "}\n\n"

        "/* Carácter número 'i' de un texto, contando caracteres y no\n"
        "   bytes. Índice negativo cuenta desde el final. */\n"
        "static LValor *lince_txt_char(LValor *t, int i) {\n"
        "    int len = lince_utf8_longitud(t->txt);\n"
        "    if (i < 0) i = len + i;\n"
        "    if (i < 0 || i >= len) {\n"
        "        fprintf(stderr, \"\\n❌ Índice %%d fuera de rango. El texto tiene %%d caracteres.\\n\\n\", i, len);\n"
        "        exit(1);\n"
        "    }\n"
        "    int off = lince_utf8_desp(t->txt, i);\n"
        "    int tam = lince_utf8_tam(t->txt + off);\n"
        "    char buf[5] = {0};\n"
        "    for (int k = 0; k < tam && k < 4; k++) buf[k] = t->txt[off + k];\n"
        "    return lince_txt(buf);\n"
        "}\n\n"

        "/* Normaliza lo que se recorre con 'para cada' a una lista: las\n"
        "   listas pasan tal cual, un texto da sus caracteres y un\n"
        "   diccionario sus claves. Así el bucle generado siempre itera\n"
        "   sobre una lista. */\n"
        "static LValor *lince_iterable(LValor *v) {\n"
        "    if (v->tipo == L_LISTA) return v;\n"
        "    LValor *l = lince_lista();\n"
        "    if (v->tipo == L_TXT) {\n"
        "        for (int i = 0; v->txt[i]; ) {\n"
        "            int tam = lince_utf8_tam(v->txt + i);\n"
        "            char buf[5] = {0};\n"
        "            for (int k = 0; k < tam && k < 4; k++) buf[k] = v->txt[i + k];\n"
        "            i += tam;\n"
        "            lince_lista_agregar(l, lince_txt(buf));\n"
        "        }\n"
        "    } else if (v->tipo == L_DIC) {\n"
        "        for (int i = 0; i < v->dic.cant; i++)\n"
        "            lince_lista_agregar(l, lince_txt(v->dic.claves[i]));\n"
        "    }\n"
        "    return l;\n"
        "}\n\n"

        "/* ── Métodos de texto ── */\n"
        "static LValor *lince_txt_metodo(LValor *t, const char *met, LValor **args, int nargs) {\n"
        "    (void)nargs; (void)args;\n"
        "    if (strcmp(met, \"longitud\") == 0) return lince_num(lince_utf8_longitud(t->txt));\n"
        "    if (strcmp(met, \"mayusculas\") == 0) {\n"
        "        char *s = lince_utf8_caja(t->txt, 1);\n"
        "        LValor *v = lince_txt(s); free(s); return v;\n"
        "    }\n"
        "    if (strcmp(met, \"minusculas\") == 0) {\n"
        "        char *s = lince_utf8_caja(t->txt, 0);\n"
        "        LValor *v = lince_txt(s); free(s); return v;\n"
        "    }\n"
        "    if (strcmp(met, \"contiene\") == 0 && nargs >= 1)\n"
        "        return lince_bool(strstr(t->txt, args[0]->txt) != NULL);\n"
        "    if (strcmp(met, \"recortar\") == 0) {\n"
        "        char *s = strdup(t->txt);\n"
        "        int i = 0, j = strlen(s) - 1;\n"
        "        while (s[i] == ' ' || s[i] == '\\t') i++;\n"
        "        while (j > i && (s[j] == ' ' || s[j] == '\\t')) j--;\n"
        "        s[j+1] = '\\0';\n"
        "        LValor *v = lince_txt(s+i); free(s); return v;\n"
        "    }\n"
        "    if (strcmp(met, \"reemplazar\") == 0 && nargs >= 2) {\n"
        "        char buf[4096] = \"\";\n"
        "        const char *src = t->txt, *p = args[0]->txt, *rep = args[1]->txt;\n"
        "        int plen = strlen(p);\n"
        "        const char *pos;\n"
        "        while ((pos = strstr(src, p)) != NULL) {\n"
        "            strncat(buf, src, pos - src);\n"
        "            strcat(buf, rep);\n"
        "            src = pos + plen;\n"
        "        }\n"
        "        strcat(buf, src);\n"
        "        return lince_txt(buf);\n"
        "    }\n"
        "    return lince_nulo();\n"
        "}\n\n"

        "/* ── Métodos de diccionario ── */\n"
        "static LValor *lince_dic_metodo(LValor *d, const char *met, LValor **args, int nargs) {\n"
        "    (void)nargs;\n"
        "    if (strcmp(met, \"longitud\") == 0) return lince_num(d->dic.cant);\n"
        "    if (strcmp(met, \"contiene\") == 0 && nargs >= 1) {\n"
        "        for (int i = 0; i < d->dic.cant; i++)\n"
        "            if (strcmp(d->dic.claves[i], args[0]->txt) == 0) return lince_bool(1);\n"
        "        return lince_bool(0);\n"
        "    }\n"
        "    if (strcmp(met, \"eliminar\") == 0 && nargs >= 1) {\n"
        "        for (int i = 0; i < d->dic.cant; i++)\n"
        "            if (strcmp(d->dic.claves[i], args[0]->txt) == 0) {\n"
        "                for (int j = i; j < d->dic.cant - 1; j++) {\n"
        "                    d->dic.claves[j] = d->dic.claves[j+1];\n"
        "                    d->dic.vals[j]   = d->dic.vals[j+1];\n"
        "                }\n"
        "                d->dic.cant--; return lince_nulo();\n"
        "            }\n"
        "    }\n"
        "    return lince_nulo();\n"
        "}\n\n"

        "/* ── Acceso a campo de objeto ── */\n"
        "static LValor *lince_obj_get(LValor *obj, const char *campo) {\n"
        "    if (!obj || obj->tipo != L_OBJ) return lince_nulo();\n"
        "    for (int i = 0; i < obj->obj->num_campos; i++)\n"
        "        if (strcmp(obj->obj->campos[i], campo) == 0)\n"
        "            return obj->obj->valores[i];\n"
        "    return lince_nulo();\n"
        "}\n"
        "static void lince_obj_set(LValor *obj, const char *campo, LValor *val) {\n"
        "    if (!obj || obj->tipo != L_OBJ) return;\n"
        "    for (int i = 0; i < obj->obj->num_campos; i++)\n"
        "        if (strcmp(obj->obj->campos[i], campo) == 0) {\n"
        "            obj->obj->valores[i] = val; return;\n"
        "        }\n"
        "    int n = obj->obj->num_campos++;\n"
        "    obj->obj->campos  = realloc(obj->obj->campos,  sizeof(char*)   * (n+1));\n"
        "    obj->obj->valores = realloc(obj->obj->valores, sizeof(LValor*) * (n+1));\n"
        "    obj->obj->campos[n]  = strdup(campo);\n"
        "    obj->obj->valores[n] = val;\n"
        "}\n"
        "static LValor *lince_obj_crear(const char *clase) {\n"
        "    LObj *o = malloc(sizeof(LObj));\n"
        "    o->clase      = strdup(clase);\n"
        "    o->campos     = malloc(sizeof(char*)   * 16);\n"
        "    o->valores    = malloc(sizeof(LValor*) * 16);\n"
        "    o->num_campos = 0;\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_OBJ; v->obj = o;\n"
        "    return v;\n"
        "}\n\n"

        "/* ── Closures ── */\n"
        "typedef LValor *(*LFnPtr)(LValor **, int, void *);\n"
        "typedef struct {\n"
        "    LFnPtr  fn;\n"
        "    void   *captura;  /* puntero al struct de variables capturadas */\n"
        "} LClosure;\n\n"
        "static LValor *lince_closure(LFnPtr fn, void *cap) {\n"
        "    LValor *v = malloc(sizeof(LValor));\n"
        "    v->tipo = L_OBJ;\n"
        "    v->obj  = malloc(sizeof(LObj));\n"
        "    v->obj->clase      = strdup(\"__closure__\");\n"
        "    v->obj->num_campos = 0;\n"
        "    v->obj->campos     = malloc(sizeof(char*)   * 2);\n"
        "    v->obj->valores    = malloc(sizeof(LValor*) * 2);\n"
        "    /* Guardar punteros como números — hack portátil */\n"
        "    LClosure *cl = malloc(sizeof(LClosure));\n"
        "    cl->fn = fn; cl->captura = cap;\n"
        "    lince_obj_set(v, \"__fn__\",  lince_num((double)(uintptr_t)(void*)fn));\n"
        "    lince_obj_set(v, \"__cap__\", lince_num((double)(uintptr_t)cap));\n"
        "    lince_obj_set(v, \"__cl__\",  lince_num((double)(uintptr_t)cl));\n"
        "    return v;\n"
        "}\n"
        "static LValor *lince_closure_llamar(LValor *cl, LValor **args, int nargs) {\n"
        "    if (!cl || cl->tipo != L_OBJ || !cl->obj) return lince_nulo();\n"
        "    LValor *vcl = lince_obj_get(cl, \"__cl__\");\n"
        "    if (!vcl || vcl->tipo != L_NUM) return lince_nulo();\n"
        "    LClosure *c = (LClosure*)(uintptr_t)(size_t)vcl->num;\n"
        "    return c->fn(args, nargs, c->captura);\n"
        "}\n\n"
        "#include <setjmp.h>\n"
        "typedef struct {\n"
        "    jmp_buf  buf;\n"
        "    LValor  *error;  /* el error capturado */\n"
        "    char     tipo[64];\n"
        "    char     mensaje[512];\n"
        "} LExcepcion;\n"
        "#define LINCE_MAX_ANIDAMIENTO 32\n"
        "static LExcepcion _lince_pila_exc[LINCE_MAX_ANIDAMIENTO];\n"
        "static int        _lince_nivel_exc = -1;\n"
        "static int        _lince_hay_error = 0;\n"
        "static LValor    *_lince_error_actual = NULL;\n\n"
        "#define LINCE_LANZAR(tipo_, msg_) do { \\\n"
        "    _lince_hay_error = 1; \\\n"
        "    if (_lince_nivel_exc >= 0) { \\\n"
        "        strncpy(_lince_pila_exc[_lince_nivel_exc].tipo, tipo_, 63); \\\n"
        "        strncpy(_lince_pila_exc[_lince_nivel_exc].mensaje, msg_, 511); \\\n"
        "        longjmp(_lince_pila_exc[_lince_nivel_exc].buf, 1); \\\n"
        "    } \\\n"
        "    fprintf(stderr, \"Error: %%s: %%s\\n\", tipo_, msg_); \\\n"
        "    exit(1); \\\n"
        "} while(0)\n\n"
        "static void lince_escribir(LValor *v) {\n"
        "    char *s = lince_a_txt(v);\n"
        "    printf(\"%%s\\n\", s);\n"
        "    free(s);\n"
        "}\n\n"
        "/* ── Funciones integradas como _lf_ ── */\n"
        "static LValor *_lf_escribir(LValor *v) {\n"
        "    lince_escribir(v); return lince_nulo(); }\n"
        "static LValor *_lf_leer(LValor *prompt) {\n"
        "    if (prompt && prompt->tipo == L_TXT) { printf(\"%%s\", prompt->txt); fflush(stdout); }\n"
        "    char buf[4096];\n"
        "    if (!fgets(buf, sizeof(buf), stdin)) return lince_txt(\"\");\n"
        "    int len = strlen(buf);\n"
        "    if (len > 0 && buf[len-1] == '\\n') buf[--len] = 0;\n"
        "    return lince_txt(buf); }\n"
        "static LValor *_lf_a_numero(LValor *v) {\n"
        "    if (!v) return lince_num(0);\n"
        "    if (v->tipo == L_NUM)  return lince_num(v->num);\n"
        "    if (v->tipo == L_TXT)  return lince_num(atof(v->txt));\n"
        "    if (v->tipo == L_BOOL) return lince_num(v->boolean ? 1.0 : 0.0);\n"
        "    return lince_num(0); }\n"
        "static LValor *_lf_a_texto(LValor *v) {\n"
        "    char *s = lince_a_txt(v); LValor *r = lince_txt(s); free(s); return r; }\n"
        "static LValor *_lf_a_logico(LValor *v) {\n"
        "    if (!v || v->tipo == L_NULO) return lince_bool(0);\n"
        "    if (v->tipo == L_BOOL) return lince_bool(v->boolean);\n"
        "    if (v->tipo == L_NUM)  return lince_bool(v->num != 0);\n"
        "    if (v->tipo == L_TXT)  return lince_bool(v->txt[0] != 0);\n"
        "    return lince_bool(1); }\n"
        "static LValor *_lf_argumentos(void) { return lince_lista(); }\n\n"
        "static LValor *_lf_rango(LValor **args, int nargs) {\n"
        "    double inicio = 0, fin = 0, paso = 1;\n"
        "    if (nargs == 1) { fin = args[0]->num; }\n"
        "    else if (nargs == 2) { inicio = args[0]->num; fin = args[1]->num; }\n"
        "    else if (nargs >= 3) { inicio = args[0]->num; fin = args[1]->num; paso = args[2]->num; }\n"
        "    if (paso == 0) return lince_lista();\n"
        "    LValor *lista = lince_lista();\n"
        "    int count = 0;\n"
        "    for (double i = inicio;\n"
        "         (paso > 0 ? i < fin : i > fin) && count < 10000;\n"
        "         i += paso, count++)\n"
        "        lince_lista_agregar(lista, lince_num(i));\n"
        "    return lista;\n"
        "}\n\n"
    );
}

/* ─────────────────────────────────────────
   GENERACIÓN DE EXPRESIONES
───────────────────────────────────────── */
static void compilar_nodo(Compilador *c, Nodo *n);
static void compilar_expr(Compilador *c, Nodo *n);
static void compilar_expr_lambda(Compilador *c, Nodo *n);
static void compilar_expr_instancia(Compilador *c, Nodo *n);
static void compilar_expr_unario(Compilador *c, Nodo *n);
static void emitir_modulo_compilado(Compilador *c, const char *nombre);

static int es_numerico(Nodo *n) {
    if (!n) return 0;
    if (n->tipo == NODO_NUMERO) return 1;
    if (n->tipo == NODO_IDENTIFICADOR) return 1;
    if (n->tipo == NODO_LLAMADA) return 1;
    if (n->tipo == NODO_ACCESO) return 1;   /* mi.campo — asumimos numérico */
    if (n->tipo == NODO_METODO) return 1;   /* obj.metodo() — puede ser numérico */
    if (n->tipo == NODO_INDICE) return 1;   /* lista[i] — puede ser numérico */
    if (n->tipo == NODO_BINARIO) {
        const char *op = n->binario.operador;
        if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
            strcmp(op, "*") == 0 || strcmp(op, "/") == 0 ||
            strcmp(op, "%") == 0)
            return es_numerico(n->binario.izquierda) &&
                   es_numerico(n->binario.derecha);
    }
    return 0;
}

static int es_texto(Nodo *n) {
    if (!n) return 0;
    if (n->tipo == NODO_TEXTO) return 1;
    if (n->tipo == NODO_BINARIO && strcmp(n->binario.operador, "+") == 0)
        return es_texto(n->binario.izquierda) || es_texto(n->binario.derecha);
    return 0;
}

static void compilar_binario(Compilador *c, Nodo *n) {
    const char *op = n->binario.operador;

    /* Concatenación — solo si alguno es literalmente texto */
    if (strcmp(op, "+") == 0) {
        int izq_txt = es_texto(n->binario.izquierda);
        int der_txt = es_texto(n->binario.derecha);
        int izq_num = es_numerico(n->binario.izquierda) && !izq_txt;
        int der_num = es_numerico(n->binario.derecha) && !der_txt;
        if (izq_txt || der_txt || (!izq_num && !der_num)) {
            fprintf(c->salida, "lince_concat(");
            compilar_expr(c, n->binario.izquierda);
            fprintf(c->salida, ", ");
            compilar_expr(c, n->binario.derecha);
            fprintf(c->salida, ")");
            return;
        }
        /* Ambos son numéricos — suma */
        fprintf(c->salida, "lince_num(");
        compilar_expr(c, n->binario.izquierda);
        fprintf(c->salida, "->num + ");
        compilar_expr(c, n->binario.derecha);
        fprintf(c->salida, "->num)");
        return;
    }

    /* Operadores de comparación — devuelven bool */
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<")  == 0 || strcmp(op, ">")  == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        fprintf(c->salida, "lince_bool(");
        compilar_expr(c, n->binario.izquierda);
        fprintf(c->salida, "->num %s ", op);
        compilar_expr(c, n->binario.derecha);
        fprintf(c->salida, "->num)");
        return;
    }

    /* Operadores lógicos */
    if (strcmp(op, "y") == 0) {
        fprintf(c->salida, "lince_bool(");
        compilar_expr(c, n->binario.izquierda);
        fprintf(c->salida, "->boolean && ");
        compilar_expr(c, n->binario.derecha);
        fprintf(c->salida, "->boolean)");
        return;
    }
    if (strcmp(op, "o") == 0) {
        fprintf(c->salida, "lince_bool(");
        compilar_expr(c, n->binario.izquierda);
        fprintf(c->salida, "->boolean || ");
        compilar_expr(c, n->binario.derecha);
        fprintf(c->salida, "->boolean)");
        return;
    }

    /* Aritméticos — todos son num */
    if (strcmp(op, "%") == 0) {
        fprintf(c->salida, "lince_num(fmod(");
        compilar_expr(c, n->binario.izquierda);
        fprintf(c->salida, "->num, ");
        compilar_expr(c, n->binario.derecha);
        fprintf(c->salida, "->num))");
        return;
    }
    fprintf(c->salida, "lince_num(");
    compilar_expr(c, n->binario.izquierda);
    fprintf(c->salida, "->num %s ", op);
    compilar_expr(c, n->binario.derecha);
    fprintf(c->salida, "->num)");
}

static void compilar_expr(Compilador *c, Nodo *n) {
    if (!n) { fprintf(c->salida, "lince_nulo()"); return; }

    /* Casos que requieren funciones auxiliares definidas más abajo */
    if (n->tipo == NODO_LAMBDA)    { compilar_expr_lambda(c, n);    return; }
    if (n->tipo == NODO_INSTANCIA) { compilar_expr_instancia(c, n); return; }
    if (n->tipo == NODO_UNARIO)    { compilar_expr_unario(c, n);    return; }

    switch (n->tipo) {
        case NODO_NUMERO:
            if (n->numero == (long long)n->numero)
                fprintf(c->salida, "lince_num(%lld)", (long long)n->numero);
            else
                fprintf(c->salida, "lince_num(%g)", n->numero);
            break;

        case NODO_TEXTO:
            fprintf(c->salida, "lince_txt(\"");
            for (const char *p = n->texto; *p; p++) {
                if (*p == '"')  fprintf(c->salida, "\\\"");
                else if (*p == '\n') fprintf(c->salida, "\\n");
                else if (*p == '\t') fprintf(c->salida, "\\t");
                else if (*p == '\\') fprintf(c->salida, "\\\\");
                else fputc(*p, c->salida);
            }
            fprintf(c->salida, "\")");
            break;

        case NODO_BOOLEANO:
            fprintf(c->salida, "lince_bool(%d)", n->booleano);
            break;

        case NODO_NULO:
            fprintf(c->salida, "lince_nulo()");
            break;

        case NODO_IDENTIFICADOR:
            fprintf(c->salida, "_l_%s", n->identificador);
            break;

        case NODO_BINARIO:
            compilar_binario(c, n);
            break;

        case NODO_LLAMADA: {
            if (!n->llamada.nombre) break;
            int nargs = n->llamada.num_argumentos;

            /* rango() — número variable de argumentos */
            if (strcmp(n->llamada.nombre, "rango") == 0) {
                char arr[64]; tmp_var(c, arr);
                fprintf(c->salida, "({ LValor *%s[%d] = {", arr, nargs > 0 ? nargs : 1);
                for (int i = 0; i < nargs; i++) {
                    if (i > 0) fprintf(c->salida, ", ");
                    compilar_expr(c, n->llamada.argumentos[i]);
                }
                if (nargs == 0) fprintf(c->salida, "lince_nulo()");
                fprintf(c->salida, "}; _lf_rango(%s, %d); })", arr, nargs);
                break;
            }

            if (es_funcion_declarada(c, n->llamada.nombre)) {
                int total = aridad_total_fn(c, n->llamada.nombre);
                /* Si se pasan menos args que parámetros, usar wrapper _N */
                if (total > 0 && nargs < total) {
                    fprintf(c->salida, "_lf_%s_%d(", n->llamada.nombre, nargs);
                } else {
                    fprintf(c->salida, "_lf_%s(", n->llamada.nombre);
                }
                for (int i = 0; i < nargs; i++) {
                    if (i > 0) fprintf(c->salida, ", ");
                    compilar_expr(c, n->llamada.argumentos[i]);
                }
                fprintf(c->salida, ")");
            } else {
                /* Variable que contiene un closure */
                if (nargs > 0) {
                    char arr[64]; tmp_var(c, arr);
                    fprintf(c->salida, "({ LValor *%s[%d] = {", arr, nargs);
                    for (int i = 0; i < nargs; i++) {
                        if (i > 0) fprintf(c->salida, ", ");
                        compilar_expr(c, n->llamada.argumentos[i]);
                    }
                    fprintf(c->salida,
                        "}; lince_closure_llamar(_l_%s, %s, %d); })",
                        n->llamada.nombre, arr, nargs);
                } else {
                    fprintf(c->salida,
                        "lince_closure_llamar(_l_%s, NULL, 0)",
                        n->llamada.nombre);
                }
            }
            break;
        }

        case NODO_LISTA: {
            char tmp[64]; tmp_var(c, tmp);
            /* Necesitamos emitir esto como expresión — usamos función auxiliar */
            fprintf(c->salida, "({ LValor *%s = lince_lista(); ", tmp);
            for (int i = 0; i < n->lista.cantidad; i++) {
                fprintf(c->salida, "lince_lista_agregar(%s, ", tmp);
                compilar_expr(c, n->lista.elementos[i]);
                fprintf(c->salida, "); ");
            }
            fprintf(c->salida, "%s; })", tmp);
            break;
        }

        case NODO_DICCIONARIO: {
            char tmp[64]; tmp_var(c, tmp);
            fprintf(c->salida, "({ LValor *%s = lince_dic(); ", tmp);
            for (int i = 0; i < n->diccionario.cantidad; i++) {
                fprintf(c->salida, "lince_dic_set(%s, \"%s\", ",
                    tmp, n->diccionario.claves[i]);
                compilar_expr(c, n->diccionario.valores[i]);
                fprintf(c->salida, "); ");
            }
            fprintf(c->salida, "%s; })", tmp);
            break;
        }

        case NODO_INDICE: {
            char tmp[64]; tmp_var(c, tmp);
            fprintf(c->salida, "({ LValor *%s = ", tmp);
            compilar_expr(c, n->acceso_indice.objeto);
            fprintf(c->salida, "; LValor *_idx_%s = ", tmp);
            compilar_expr(c, n->acceso_indice.indice);
            fprintf(c->salida,
                "; (%s->tipo == L_LISTA)\n"
                "    ? %s->lista.elems[(int)_idx_%s->num < 0\n"
                "        ? %s->lista.cant + (int)_idx_%s->num\n"
                "        : (int)_idx_%s->num]\n"
                "    : (%s->tipo == L_DIC)\n"
                "    ? lince_dic_get(%s, _idx_%s->txt)\n"
                "    : (%s->tipo == L_TXT)\n"
                "    ? lince_txt_char(%s, (int)_idx_%s->num)\n"
                "    : lince_nulo(); })",
                tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp);
            break;
        }

        case NODO_ACCESO: {
            /* obj.campo — acceso a campo de objeto */
            /* Caso especial: acceso a constante de módulo */
            if (n->acceso.objeto->tipo == NODO_IDENTIFICADOR) {
                const char *mod = n->acceso.objeto->identificador;
                if (strcmp(mod, "matematica") == 0) {
                    fprintf(c->salida, "_lmod_mat_get(\"%s\", NULL, 0)",
                        n->acceso.campo);
                    break;
                }
                /* Enumeración — acceso al diccionario */
                /* No podemos distinguir enum de objeto en tiempo de compilación,
                   pero sí podemos usar lince_dic_get que funciona para ambos */
                char tmp[64]; tmp_var(c, tmp);
                fprintf(c->salida, "({ LValor *%s = _l_%s; "
                    "(%s && %s->tipo == L_DIC) "
                    "? lince_dic_get(%s, \"%s\") "
                    ": lince_obj_get(%s, \"%s\"); })",
                    tmp, mod,
                    tmp, tmp,
                    tmp, n->acceso.campo,
                    tmp, n->acceso.campo);
                break;
            }
            /* Acceso general a campo de objeto */
            char tmp[64]; tmp_var(c, tmp);
            fprintf(c->salida, "({ LValor *%s = ", tmp);
            compilar_expr(c, n->acceso.objeto);
            fprintf(c->salida, "; lince_obj_get(%s, \"%s\"); })",
                tmp, n->acceso.campo);
            break;
        }

        case NODO_METODO: {
            char obj_var[64]; tmp_var(c, obj_var);
            char args_arr[64]; tmp_var(c, args_arr);

            /* Caso especial: acceso a módulo conocido */
            if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR) {
                const char *mod = n->metodo.objeto->identificador;
                int nargs = n->metodo.num_argumentos;
                if (strcmp(mod, "matematica") == 0) {
                    if (nargs > 0) {
                        fprintf(c->salida, "({ LValor *%s[%d] = {", args_arr, nargs);
                        for (int i = 0; i < nargs; i++) {
                            if (i > 0) fprintf(c->salida, ", ");
                            compilar_expr(c, n->metodo.argumentos[i]);
                        }
                        fprintf(c->salida, "}; _lmod_mat_get(\"%s\", %s, %d); })",
                            n->metodo.metodo, args_arr, nargs);
                    } else {
                        fprintf(c->salida, "_lmod_mat_get(\"%s\", NULL, 0)",
                            n->metodo.metodo);
                    }
                    break;
                }
                if (strcmp(mod, "texto") == 0) {
                    if (nargs > 0) {
                        fprintf(c->salida, "({ LValor *%s[%d] = {", args_arr, nargs);
                        for (int i = 0; i < nargs; i++) {
                            if (i > 0) fprintf(c->salida, ", ");
                            compilar_expr(c, n->metodo.argumentos[i]);
                        }
                        fprintf(c->salida, "}; _lmod_txt_get(\"%s\", %s, %d); })",
                            n->metodo.metodo, args_arr, nargs);
                    } else {
                        fprintf(c->salida, "_lmod_txt_get(\"%s\", NULL, 0)",
                            n->metodo.metodo);
                    }
                    break;
                }
                if (strcmp(mod, "json") == 0) {
                    if (nargs > 0) {
                        fprintf(c->salida, "({ LValor *%s[%d] = {", args_arr, nargs);
                        for (int i = 0; i < nargs; i++) {
                            if (i > 0) fprintf(c->salida, ", ");
                            compilar_expr(c, n->metodo.argumentos[i]);
                        }
                        fprintf(c->salida, "}; _lmod_json_get(\"%s\", %s, %d); })",
                            n->metodo.metodo, args_arr, nargs);
                    } else {
                        fprintf(c->salida, "_lmod_json_get(\"%s\", NULL, 0)",
                            n->metodo.metodo);
                    }
                    break;
                }

                /* Módulos con dispatch genérico */
                const char *_fn_mod = NULL;
                if (strcmp(mod, "archivos") == 0)    _fn_mod = "_lmod_arc_get";
                if (strcmp(mod, "tiempo") == 0)      _fn_mod = "_lmod_tpo_get";
                if (strcmp(mod, "sistema") == 0)     _fn_mod = "_lmod_sis_get";
                if (strcmp(mod, "expresiones") == 0) _fn_mod = "_lmod_exp_get";
                if (strcmp(mod, "red") == 0)         _fn_mod = "_lmod_red_get";
                if (strcmp(mod, "terminal") == 0)    _fn_mod = "_lmod_ter_get";
                if (strcmp(mod, "servidor") == 0)    _fn_mod = "_lmod_srv_get";
                if (_fn_mod) {
                    if (nargs > 0) {
                        fprintf(c->salida, "({ LValor *%s[%d] = {", args_arr, nargs);
                        for (int ii = 0; ii < nargs; ii++) {
                            if (ii > 0) fprintf(c->salida, ", ");
                            compilar_expr(c, n->metodo.argumentos[ii]);
                        }
                        fprintf(c->salida, "}; %s(\"%s\", %s, %d); })",
                            _fn_mod, n->metodo.metodo, args_arr, nargs);
                    } else {
                        fprintf(c->salida, "%s(\"%s\", NULL, 0)",
                            _fn_mod, n->metodo.metodo);
                    }
                    break;
                }
            }

            /* Despacho general de métodos */
            fprintf(c->salida, "({ LValor *%s = ", obj_var);
            compilar_expr(c, n->metodo.objeto);
            fprintf(c->salida, ";\n");
            int nargs = n->metodo.num_argumentos;
            if (nargs > 0) {
                fprintf(c->salida, "LValor *%s[%d]; ", args_arr, nargs);
                for (int i = 0; i < nargs; i++) {
                    fprintf(c->salida, "%s[%d] = ", args_arr, i);
                    compilar_expr(c, n->metodo.argumentos[i]);
                    fprintf(c->salida, "; ");
                }
            } else {
                fprintf(c->salida, "LValor **%s = NULL; ", args_arr);
            }
            fprintf(c->salida,
                "((%s->tipo == L_LISTA) ? lince_lista_metodo(%s, \"%s\", %s, %d)\n"
                "    : (%s->tipo == L_TXT)  ? lince_txt_metodo(%s, \"%s\", %s, %d)\n"
                "    : (%s->tipo == L_DIC)  ? lince_dic_metodo(%s, \"%s\", %s, %d)\n"
                "    : (%s->tipo == L_OBJ)  ? _lince_obj_metodo(%s, \"%s\", %s, %d)\n"
                "    : lince_nulo()); })\n",
                obj_var, obj_var, n->metodo.metodo, args_arr, nargs,
                obj_var, obj_var, n->metodo.metodo, args_arr, nargs,
                obj_var, obj_var, n->metodo.metodo, args_arr, nargs,
                obj_var, obj_var, n->metodo.metodo, args_arr, nargs);
            break;
        }

        case NODO_ESTO: {
            fprintf(c->salida, "_lince_mi");
            break;
        }

        default:
            fprintf(c->salida, "lince_nulo() /* expr tipo %d no soportada */", n->tipo);
            break;
    }
}

/* ─────────────────────────────────────────
   ANÁLISIS DE VARIABLES LIBRES
───────────────────────────────────────── */
static void recolectar_ids(Nodo *n, char **ids, int *nids, int max) {
    if (!n || *nids >= max) return;
    if (n->tipo == NODO_IDENTIFICADOR) {
        /* Ver si ya está */
        for (int i = 0; i < *nids; i++)
            if (strcmp(ids[i], n->identificador) == 0) return;
        ids[(*nids)++] = n->identificador;
        return;
    }
    /* Recorrer hijos */
    switch (n->tipo) {
        case NODO_BLOQUE:
            for (int i = 0; i < n->bloque.cantidad; i++)
                recolectar_ids(n->bloque.sentencias[i], ids, nids, max);
            break;
        case NODO_BINARIO:
            recolectar_ids(n->binario.izquierda, ids, nids, max);
            recolectar_ids(n->binario.derecha, ids, nids, max);
            break;
        case NODO_UNARIO:
            recolectar_ids(n->unario.operando, ids, nids, max);
            break;
        case NODO_DECLARACION:
            recolectar_ids(n->declaracion.valor, ids, nids, max);
            break;
        case NODO_ASIGNACION:
            recolectar_ids(n->asignacion.valor, ids, nids, max);
            break;
        case NODO_ESCRIBIR:
            recolectar_ids(n->escribir, ids, nids, max);
            break;
        case NODO_DEVOLVER:
            recolectar_ids(n->devolver, ids, nids, max);
            break;
        case NODO_SI:
            recolectar_ids(n->si.condicion, ids, nids, max);
            recolectar_ids(n->si.entonces, ids, nids, max);
            recolectar_ids(n->si.sino, ids, nids, max);
            break;
        case NODO_LLAMADA:
            /* El nombre de la función puede ser una variable libre (closure) */
            if (n->llamada.nombre) {
                int ya = 0;
                for (int i = 0; i < *nids; i++)
                    if (strcmp(ids[i], n->llamada.nombre) == 0) { ya = 1; break; }
                if (!ya && *nids < max)
                    ids[(*nids)++] = n->llamada.nombre;
            }
            for (int i = 0; i < n->llamada.num_argumentos; i++)
                recolectar_ids(n->llamada.argumentos[i], ids, nids, max);
            break;
        case NODO_METODO:
            recolectar_ids(n->metodo.objeto, ids, nids, max);
            for (int i = 0; i < n->metodo.num_argumentos; i++)
                recolectar_ids(n->metodo.argumentos[i], ids, nids, max);
            break;
        case NODO_MIENTRAS:
            recolectar_ids(n->mientras.condicion, ids, nids, max);
            recolectar_ids(n->mientras.cuerpo, ids, nids, max);
            break;
        case NODO_PARA:
            recolectar_ids(n->para.condicion, ids, nids, max);
            recolectar_ids(n->para.cuerpo, ids, nids, max);
            break;
        case NODO_PARA_CADA:
            recolectar_ids(n->para_cada.coleccion, ids, nids, max);
            recolectar_ids(n->para_cada.cuerpo, ids, nids, max);
            break;
        case NODO_HACER:
            recolectar_ids(n->hacer.condicion, ids, nids, max);
            recolectar_ids(n->hacer.cuerpo, ids, nids, max);
            break;
        case NODO_INDICE:
            recolectar_ids(n->acceso_indice.objeto, ids, nids, max);
            recolectar_ids(n->acceso_indice.indice, ids, nids, max);
            break;
        case NODO_ACCESO:
            recolectar_ids(n->acceso.objeto, ids, nids, max);
            break;
        default: break;
    }
}

static void lambda_vars_libres(Nodo *lambda_cuerpo,
                               Parametro *params, int nparams,
                               char **libres, int *nlibres) {
    /* Recolectar todos los identificadores usados */
    char *todos[64];
    int   ntodos = 0;
    recolectar_ids(lambda_cuerpo, todos, &ntodos, 64);

    /* Recolectar variables declaradas localmente en el cuerpo */
    char *locales[64];
    int   nlocales = 0;
    /* Buscar NODO_DECLARACION en el cuerpo */
    if (lambda_cuerpo) {
        Nodo *cuerpo = lambda_cuerpo;
        if (cuerpo->tipo == NODO_BLOQUE) {
            for (int i = 0; i < cuerpo->bloque.cantidad; i++) {
                Nodo *s = cuerpo->bloque.sentencias[i];
                if (s && s->tipo == NODO_DECLARACION && nlocales < 64)
                    locales[nlocales++] = s->declaracion.nombre;
            }
        }
    }

    *nlibres = 0;
    for (int i = 0; i < ntodos; i++) {
        /* ¿Es un parámetro de la lambda? */
        int es_param = 0;
        for (int j = 0; j < nparams; j++)
            if (strcmp(params[j].nombre, todos[i]) == 0) { es_param = 1; break; }
        if (es_param) continue;

        /* ¿Es una variable local del cuerpo? */
        int es_local = 0;
        for (int j = 0; j < nlocales; j++)
            if (strcmp(locales[j], todos[i]) == 0) { es_local = 1; break; }
        if (es_local) continue;

        libres[(*nlibres)++] = todos[i];
    }
}

static void compilar_expr_lambda(Compilador *c, Nodo *n);

static void compilar_expr_lambda(Compilador *c, Nodo *n) {
            int id = c->lambda_count++;
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "_lince_lambda_%d", id);

            /* Analizar variables libres */
            char *libres[32];
            int   nlibres = 0;
            lambda_vars_libres(n->lambda.cuerpo,
                n->lambda.parametros, n->lambda.num_parametros,
                libres, &nlibres);

            /* Emitir struct de captura */
            fprintf(c->lambdas,
                "typedef struct { LValor *v[%d]; } _Cap%d;\n",
                nlibres > 0 ? nlibres : 1, id);

            /* Emitir la función lambda */
            fprintf(c->lambdas,
                "static LValor *%s(LValor **_args, int _nargs, void *_vcap) {\n"
                "    _Cap%d *_cap = (_Cap%d*)_vcap;\n",
                fn_name, id, id);

            /* Parámetros */
            for (int i = 0; i < n->lambda.num_parametros; i++) {
                fprintf(c->lambdas,
                    "    LValor *_l_%s = (_nargs > %d) ? _args[%d] : lince_nulo();\n",
                    n->lambda.parametros[i].nombre, i, i);
            }

            /* Restaurar variables capturadas desde el struct */
            for (int i = 0; i < nlibres; i++) {
                fprintf(c->lambdas,
                    "    LValor *_l_%s = _cap->v[%d];\n",
                    libres[i], i);
            }

            /* Compilar el cuerpo de la lambda */
            FILE *prev = c->salida;
            int prev_sangria = c->sangria;
            c->salida  = c->lambdas;
            c->sangria = 1;
            compilar_nodo(c, n->lambda.cuerpo);
            c->salida  = prev;
            c->sangria = prev_sangria;

            fprintf(c->lambdas, "    return lince_nulo();\n}\n\n");

            /* En el sitio de la expresión: crear closure capturando variables */
            fprintf(c->salida,
                "({ _Cap%d *_cl = calloc(1, sizeof(_Cap%d));\n",
                id, id);
            for (int i = 0; i < nlibres; i++) {
                fprintf(c->salida,
                    "   _cl->v[%d] = _l_%s;\n", i, libres[i]);
            }
            fprintf(c->salida,
                "   lince_closure((LFnPtr)%s, _cl); })",
                fn_name);
}

static void compilar_expr_instancia(Compilador *c, Nodo *n) {
    fprintf(c->salida, "_lf_%s(", n->instancia.clase);
    for (int i = 0; i < n->instancia.num_argumentos; i++) {
        if (i > 0) fprintf(c->salida, ", ");
        compilar_expr(c, n->instancia.argumentos[i]);
    }
    fprintf(c->salida, ")");
}

static void compilar_expr_unario(Compilador *c, Nodo *n) {
    if (strcmp(n->unario.operador, "no") == 0) {
        fprintf(c->salida, "lince_bool(!(");
        compilar_expr(c, n->unario.operando);
        fprintf(c->salida, "->boolean))");
    } else if (strcmp(n->unario.operador, "-") == 0) {
        fprintf(c->salida, "lince_num(-(");
        compilar_expr(c, n->unario.operando);
        fprintf(c->salida, "->num))");
    } else {
        compilar_expr(c, n->unario.operando);
    }
}

/* ─────────────────────────────────────────
   GENERACIÓN DE SENTENCIAS
───────────────────────────────────────── */
static void compilar_nodo(Compilador *c, Nodo *n) {
    if (!n) return;

    switch (n->tipo) {

        case NODO_BLOQUE:
            for (int i = 0; i < n->bloque.cantidad; i++)
                compilar_nodo(c, n->bloque.sentencias[i]);
            break;

        case NODO_DECLARACION: {
            sangrar(c);
            fprintf(c->salida, "LValor *_l_%s = ", n->declaracion.nombre);
            if (n->declaracion.valor)
                compilar_expr(c, n->declaracion.valor);
            else
                fprintf(c->salida, "lince_nulo()");
            fprintf(c->salida, ";\n");
            break;
        }

        case NODO_ASIGNACION: {
            sangrar(c);
            /* Si la variable no está declarada aún, declararla */
            fprintf(c->salida, "_l_%s = ", n->asignacion.nombre);
            compilar_expr(c, n->asignacion.valor);
            fprintf(c->salida, ";\n");
            break;
        }

        case NODO_ESCRIBIR: {
            sangrar(c);
            fprintf(c->salida, "lince_escribir(");
            compilar_expr(c, n->escribir);
            fprintf(c->salida, ");\n");
            break;
        }

        case NODO_SI: {
            sangrar(c);
            fprintf(c->salida, "if (");
            compilar_expr(c, n->si.condicion);
            fprintf(c->salida, "->boolean) {\n");
            c->sangria++;
            compilar_nodo(c, n->si.entonces);
            c->sangria--;
            if (n->si.sino) {
                sangrar(c);
                fprintf(c->salida, "} else {\n");
                c->sangria++;
                compilar_nodo(c, n->si.sino);
                c->sangria--;
            }
            sangrar(c);
            fprintf(c->salida, "}\n");
            break;
        }

        case NODO_MIENTRAS: {
            sangrar(c);
            fprintf(c->salida, "{\n");
            c->sangria++;
            sangrar(c);
            fprintf(c->salida, "while (");
            compilar_expr(c, n->mientras.condicion);
            fprintf(c->salida, "->boolean) {\n");
            c->sangria++;
            compilar_nodo(c, n->mientras.cuerpo);
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");
            break;
        }

        case NODO_PARA: {
            sangrar(c);
            fprintf(c->salida, "{\n");
            c->sangria++;
            sangrar(c);
            fprintf(c->salida, "LValor *_l_%s = ", n->para.var_inicio);
            compilar_expr(c, n->para.val_inicio);
            fprintf(c->salida, ";\n");
            sangrar(c);
            fprintf(c->salida, "while (");
            compilar_expr(c, n->para.condicion);
            fprintf(c->salida, "->boolean) {\n");
            c->sangria++;
            compilar_nodo(c, n->para.cuerpo);
            sangrar(c);
            if (strcmp(n->para.op_incremento, "++") == 0)
                fprintf(c->salida, "_l_%s = lince_num(_l_%s->num + 1);\n",
                    n->para.var_incremento, n->para.var_incremento);
            else
                fprintf(c->salida, "_l_%s = lince_num(_l_%s->num - 1);\n",
                    n->para.var_incremento, n->para.var_incremento);
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");
            break;
        }

        case NODO_DEVOLVER: {
            sangrar(c);
            fprintf(c->salida, "return ");
            if (n->devolver)
                compilar_expr(c, n->devolver);
            else
                fprintf(c->salida, "lince_nulo()");
            fprintf(c->salida, ";\n");
            break;
        }

        case NODO_FUNCION: {
            registrar_funcion(c, n->funcion.nombre);
            fprintf(c->salida, "static LValor *_lf_%s(", n->funcion.nombre);
            for (int i = 0; i < n->funcion.num_parametros; i++) {
                if (i > 0) fprintf(c->salida, ", ");
                fprintf(c->salida, "LValor *_l_%s", n->funcion.parametros[i].nombre);
            }
            fprintf(c->salida, ") {\n");
            c->sangria++;
            compilar_nodo(c, n->funcion.cuerpo);
            sangrar(c);
            fprintf(c->salida, "return lince_nulo();\n");
            c->sangria--;
            fprintf(c->salida, "}\n\n");

            /* Generar wrapper con valores por defecto si los hay */
            int tiene_defecto = 0;
            for (int i = 0; i < n->funcion.num_parametros; i++)
                if (n->funcion.parametros[i].valor_defecto) { tiene_defecto = 1; break; }

            if (tiene_defecto) {
                /* Encontrar el primer parámetro con defecto */
                int primer_defecto = n->funcion.num_parametros;
                for (int i = 0; i < n->funcion.num_parametros; i++)
                    if (n->funcion.parametros[i].valor_defecto && i < primer_defecto)
                        primer_defecto = i;

                /* Generar versión con args variables usando el nombre + _opt */
                /* En las llamadas con menos args, usamos los valores por defecto */
                /* Redefinimos _lf_nombre para envolver con los defaults */
                fprintf(c->salida,
                    "/* wrapper con valores por defecto para %s */\n",
                    n->funcion.nombre);
                /* Generar versiones para cada cantidad de argumentos posibles */
                for (int nargs = primer_defecto;
                     nargs < n->funcion.num_parametros; nargs++) {
                    fprintf(c->salida,
                        "static LValor *_lf_%s_%d(", n->funcion.nombre, nargs);
                    for (int i = 0; i < nargs; i++) {
                        if (i > 0) fprintf(c->salida, ", ");
                        fprintf(c->salida, "LValor *_l_%s",
                            n->funcion.parametros[i].nombre);
                    }
                    fprintf(c->salida, ") {\n");
                    fprintf(c->salida, "    return _lf_%s(", n->funcion.nombre);
                    for (int i = 0; i < n->funcion.num_parametros; i++) {
                        if (i > 0) fprintf(c->salida, ", ");
                        if (i < nargs) {
                            fprintf(c->salida, "_l_%s",
                                n->funcion.parametros[i].nombre);
                        } else {
                            compilar_expr(c, n->funcion.parametros[i].valor_defecto);
                        }
                    }
                    fprintf(c->salida, ");\n}\n\n");
                }
            }
            break;
        }

        case NODO_LLAMADA: {
            /* Como sentencia */
            sangrar(c);
            if (strcmp(n->llamada.nombre, "escribir") == 0) {
                fprintf(c->salida, "lince_escribir(");
                if (n->llamada.num_argumentos > 0)
                    compilar_expr(c, n->llamada.argumentos[0]);
                else
                    fprintf(c->salida, "lince_nulo()");
                fprintf(c->salida, ");\n");
            } else {
                compilar_expr(c, n);
                fprintf(c->salida, ";\n");
            }
            break;
        }
        case NODO_ELEGIR: {
            char tmp[64]; tmp_var(c, tmp);
            sangrar(c);
            fprintf(c->salida, "LValor *%s = ", tmp);
            compilar_expr(c, n->elegir.sujeto);
            fprintf(c->salida, ";\n");
            for (int i = 0; i < n->elegir.num_casos; i++) {
                sangrar(c);
                if (i == 0) fprintf(c->salida, "if (");
                else        fprintf(c->salida, "} else if (");
                /* Comparar por tipo */
                Nodo *val = n->elegir.valores[i];
                if (val->tipo == NODO_NUMERO)
                    fprintf(c->salida, "%s->num == %g", tmp, val->numero);
                else if (val->tipo == NODO_TEXTO)
                    fprintf(c->salida, "strcmp(%s->txt, \"%s\") == 0", tmp, val->texto);
                else if (val->tipo == NODO_BOOLEANO)
                    fprintf(c->salida, "%s->boolean == %d", tmp, val->booleano);
                else {
                    /* Enumeración — se compara como texto */
                    fprintf(c->salida, "%s->txt && strcmp(%s->txt, ", tmp, tmp);
                    compilar_expr(c, val);
                    fprintf(c->salida, "->txt) == 0");
                }
                fprintf(c->salida, ") {\n");
                c->sangria++;
                compilar_nodo(c, n->elegir.cuerpos[i]);
                c->sangria--;
            }
            if (n->elegir.num_casos > 0) {
                if (n->elegir.otro) {
                    sangrar(c);
                    fprintf(c->salida, "} else {\n");
                    c->sangria++;
                    compilar_nodo(c, n->elegir.otro);
                    c->sangria--;
                }
                sangrar(c);
                fprintf(c->salida, "}\n");
            }
            break;
        }

        case NODO_HACER: {
            sangrar(c);
            fprintf(c->salida, "do {\n");
            c->sangria++;
            compilar_nodo(c, n->hacer.cuerpo);
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "} while (");
            compilar_expr(c, n->hacer.condicion);
            fprintf(c->salida, "->boolean);\n");
            break;
        }

        case NODO_INCREMENTO: {
            sangrar(c);
            if (strcmp(n->incremento.operador, "++") == 0)
                fprintf(c->salida, "_l_%s = lince_num(_l_%s->num + 1);\n",
                    n->incremento.nombre, n->incremento.nombre);
            else
                fprintf(c->salida, "_l_%s = lince_num(_l_%s->num - 1);\n",
                    n->incremento.nombre, n->incremento.nombre);
            break;
        }

        case NODO_INTENTAR: {
            sangrar(c);
            fprintf(c->salida, "{\n");
            c->sangria++;
            sangrar(c);
            fprintf(c->salida, "_lince_nivel_exc++;\n");
            sangrar(c);
            fprintf(c->salida,
                "if (setjmp(_lince_pila_exc[_lince_nivel_exc].buf) == 0) {\n");
            c->sangria++;
            compilar_nodo(c, n->intentar.cuerpo);
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");

            if (n->intentar.num_capturas > 0) {
                sangrar(c);
                fprintf(c->salida, "else {\n");
                c->sangria++;
                sangrar(c);
                fprintf(c->salida, "_lince_hay_error = 0;\n");
                sangrar(c);
                fprintf(c->salida, "int _capturado = 0;\n");
                for (int i = 0; i < n->intentar.num_capturas; i++) {
                    const char *tipo_cap = n->intentar.tipos_captura[i];
                    const char *var_cap  = n->intentar.vars_captura[i];
                    int generico = (strcmp(tipo_cap, "error") == 0 ||
                                    strcmp(tipo_cap, "Error") == 0);
                    sangrar(c);
                    if (!generico)
                        fprintf(c->salida,
                            "if (!_capturado && strcmp("
                            "_lince_pila_exc[_lince_nivel_exc].tipo,"
                            " \"%s\") == 0) {\n", tipo_cap);
                    else
                        fprintf(c->salida, "if (!_capturado) {\n");
                    c->sangria++;
                    sangrar(c);
                    fprintf(c->salida, "_capturado = 1;\n");
                    sangrar(c);
                    fprintf(c->salida,
                        "LValor *_l_%s = lince_obj_crear(\"%s\");\n",
                        var_cap, tipo_cap);
                    sangrar(c);
                    fprintf(c->salida,
                        "lince_obj_set(_l_%s, \"tipo\", "
                        "lince_txt(_lince_pila_exc[_lince_nivel_exc].tipo));\n",
                        var_cap);
                    sangrar(c);
                    fprintf(c->salida,
                        "lince_obj_set(_l_%s, \"mensaje\", "
                        "lince_txt(_lince_pila_exc[_lince_nivel_exc].mensaje));\n",
                        var_cap);
                    compilar_nodo(c, n->intentar.cuerpos_captura[i]);
                    c->sangria--;
                    sangrar(c);
                    fprintf(c->salida, "}\n");
                }
                c->sangria--;
                sangrar(c);
                fprintf(c->salida, "}\n");
            }

            if (n->intentar.finalmente)
                compilar_nodo(c, n->intentar.finalmente);

            sangrar(c);
            fprintf(c->salida, "_lince_nivel_exc--;\n");
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");
            break;
        }

        case NODO_LANZAR: {
            sangrar(c);
            /* lanzar Error("mensaje") → LINCE_LANZAR("Error", "mensaje") */
            if (n->lanzar->tipo == NODO_INSTANCIA) {
                fprintf(c->salida, "{ ");
                /* Evaluar el mensaje */
                char tmp_msg[64]; tmp_var(c, tmp_msg);
                fprintf(c->salida, "LValor *%s = ", tmp_msg);
                if (n->lanzar->instancia.num_argumentos > 0)
                    compilar_expr(c, n->lanzar->instancia.argumentos[0]);
                else
                    fprintf(c->salida, "lince_txt(\"\")");
                fprintf(c->salida, ";\n");
                sangrar(c);
                char *tipo = n->lanzar->instancia.clase;
                fprintf(c->salida,
                    "char *_msg_%s = lince_a_txt(%s);\n",
                    tmp_msg, tmp_msg);
                sangrar(c);
                fprintf(c->salida,
                    "LINCE_LANZAR(\"%s\", _msg_%s); }\n",
                    tipo, tmp_msg);
            } else {
                fprintf(c->salida, "LINCE_LANZAR(\"Error\", \"error\");\n");
            }
            break;
        }

        case NODO_PARA_CADA: {
            char tmp[64]; tmp_var(c, tmp);
            char idx[64]; tmp_var(c, idx);
            sangrar(c);
            fprintf(c->salida, "{\n");
            c->sangria++;
            sangrar(c);
            fprintf(c->salida, "LValor *%s = lince_iterable(", tmp);
            compilar_expr(c, n->para_cada.coleccion);
            fprintf(c->salida, ");\n");
            sangrar(c);
            fprintf(c->salida, "for (int %s = 0; %s < %s->lista.cant; %s++) {\n",
                idx, idx, tmp, idx);
            c->sangria++;
            sangrar(c);
            fprintf(c->salida, "LValor *_l_%s = %s->lista.elems[%s];\n",
                n->para_cada.variable, tmp, idx);
            compilar_nodo(c, n->para_cada.cuerpo);
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");
            c->sangria--;
            sangrar(c);
            fprintf(c->salida, "}\n");
            break;
        }

        case NODO_ASIGNACION_INDICE: {
            sangrar(c);
            char tmp[64]; tmp_var(c, tmp);
            fprintf(c->salida, "{ LValor *%s = ", tmp);
            compilar_expr(c, n->asignacion_indice.objeto);
            fprintf(c->salida, ";\n");
            sangrar(c);
            fprintf(c->salida, "  LValor *_idx = ");
            compilar_expr(c, n->asignacion_indice.indice);
            fprintf(c->salida, ";\n");
            sangrar(c);
            fprintf(c->salida, "  LValor *_val = ");
            compilar_expr(c, n->asignacion_indice.valor);
            fprintf(c->salida, ";\n");
            sangrar(c);
            fprintf(c->salida,
                "  if (%s->tipo == L_LISTA) %s->lista.elems[(int)_idx->num] = _val;\n",
                tmp, tmp);
            sangrar(c);
            fprintf(c->salida,
                "  else if (%s->tipo == L_DIC) lince_dic_set(%s, _idx->txt, _val); }\n",
                tmp, tmp);
            break;
        }

        case NODO_ASIGNACION_CAMPO: {
            sangrar(c);
            char tmp[64]; tmp_var(c, tmp);
            fprintf(c->salida, "{ LValor *%s = ", tmp);
            compilar_expr(c, n->asignacion_campo.objeto);
            fprintf(c->salida, "; lince_obj_set(%s, \"%s\", ", tmp, n->asignacion_campo.campo);
            compilar_expr(c, n->asignacion_campo.valor);
            fprintf(c->salida, "); }\n");
            break;
        }

        case NODO_CLASE: {
            const char *cnom = n->clase.nombre;
            /* Registrar clase con su padre */
            strncpy(c->clases[c->num_clases], cnom, 63);
            strncpy(c->padres[c->num_clases],
                n->clase.padre ? n->clase.padre : "", 63);
            c->num_clases++;

            /* Si tiene padre, generar alias para padre() */
            if (n->clase.padre) {
                fprintf(c->salida,
                    "#define _lince_padre_crear _lf_%s_crear\n\n",
                    n->clase.padre);
            }

            /* Generar métodos */
            for (int i = 0; i < n->clase.num_metodos; i++) {
                Nodo *mn = n->clase.metodos[i];
                fprintf(c->salida, "static LValor *_lf_%s_%s(LValor *_lince_mi",
                    cnom, mn->funcion.nombre);
                for (int j = 0; j < mn->funcion.num_parametros; j++)
                    fprintf(c->salida, ", LValor *_l_%s",
                        mn->funcion.parametros[j].nombre);
                fprintf(c->salida, ") {\n");
                c->sangria++;
                compilar_nodo(c, mn->funcion.cuerpo);
                sangrar(c);
                fprintf(c->salida, "return lince_nulo();\n");
                c->sangria--;
                fprintf(c->salida, "}\n\n");
            }

            /* Limpiar alias padre */
            if (n->clase.padre)
                fprintf(c->salida, "#undef _lince_padre_crear\n\n");

            /* Generar función constructora */
            fprintf(c->salida, "static LValor *_lf_%s(", cnom);
            for (int i = 0; i < n->clase.num_metodos; i++) {
                if (strcmp(n->clase.metodos[i]->funcion.nombre, "crear") == 0) {
                    Nodo *cr = n->clase.metodos[i];
                    for (int j = 0; j < cr->funcion.num_parametros; j++) {
                        if (j > 0) fprintf(c->salida, ", ");
                        fprintf(c->salida, "LValor *_l_%s",
                            cr->funcion.parametros[j].nombre);
                    }
                    break;
                }
            }
            fprintf(c->salida, ") {\n");
            fprintf(c->salida, "    LValor *_lince_mi = lince_obj_crear(\"%s\");\n", cnom);
            for (int i = 0; i < n->clase.num_metodos; i++) {
                if (strcmp(n->clase.metodos[i]->funcion.nombre, "crear") == 0) {
                    fprintf(c->salida, "    _lf_%s_crear(_lince_mi", cnom);
                    Nodo *cr = n->clase.metodos[i];
                    for (int j = 0; j < cr->funcion.num_parametros; j++)
                        fprintf(c->salida, ", _l_%s", cr->funcion.parametros[j].nombre);
                    fprintf(c->salida, ");\n");
                    break;
                }
            }
            fprintf(c->salida, "    return _lince_mi;\n}\n\n");
            break;
        }

        case NODO_PADRE: {
            /* padre(args) — llama al crear del padre */
            /* Buscamos el nombre de la clase padre en la tabla */
            sangrar(c);
            fprintf(c->salida, "/* padre() */\n");
            /* Emitir como llamada a _lf_PADRE_crear(_lince_mi, args...) */
            /* El nombre del padre lo buscamos en la tabla de clases */
            /* Por ahora usamos una variable especial _lince_clase_padre */
            fprintf(c->salida, "_lince_padre_crear(_lince_mi");
            for (int i = 0; i < n->padre.num_argumentos; i++) {
                fprintf(c->salida, ", ");
                compilar_expr(c, n->padre.argumentos[i]);
            }
            fprintf(c->salida, ");\n");
            break;
        }

        case NODO_METODO: {
            /* Método como sentencia */
            sangrar(c);
            compilar_expr(c, n);
            fprintf(c->salida, ";\n");
            break;
        }

        case NODO_ACCESO: {
            /* Acceso a campo como sentencia — raro pero posible */
            sangrar(c);
            compilar_expr(c, n);
            fprintf(c->salida, ";\n");
            break;
        }

        case NODO_INTERFAZ: {
            /* Las interfaces solo existen en tiempo de compilación —
               verificación ya hecha por el intérprete/parser.
               En el binario no generamos código. */
            sangrar(c);
            fprintf(c->salida, "/* interfaz %s — solo compile-time */\n",
                n->interfaz.nombre);
            break;
        }

        case NODO_ENUMERACION: {
            /* Una enumeración se convierte en un objeto con campos de texto */
            sangrar(c);
            fprintf(c->salida, "/* enumeracion %s */\n", n->enumeracion.nombre);
            sangrar(c);
            fprintf(c->salida, "LValor *_l_%s = lince_dic();\n", n->enumeracion.nombre);
            for (int i = 0; i < n->enumeracion.num_valores; i++) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%s.%s",
                    n->enumeracion.nombre, n->enumeracion.valores[i]);
                sangrar(c);
                fprintf(c->salida,
                    "lince_dic_set(_l_%s, \"%s\", lince_txt(\"%s\"));\n",
                    n->enumeracion.nombre,
                    n->enumeracion.valores[i],
                    buf);
            }
            break;
        }

        case NODO_GENERADOR: {
            /* Un generador compilado es una función que devuelve una lista.
               El cuerpo se ejecuta completamente, acumulando valores producidos. */
            registrar_funcion(c, n->generador.nombre);
            fprintf(c->salida, "static LValor *_lf_%s(", n->generador.nombre);
            for (int i = 0; i < n->generador.num_parametros; i++) {
                if (i > 0) fprintf(c->salida, ", ");
                fprintf(c->salida, "LValor *_l_%s",
                    n->generador.parametros[i].nombre);
            }
            fprintf(c->salida, ") {\n");
            c->sangria++;

            /* Lista que acumula los valores producidos */
            char lista_var[64];
            snprintf(lista_var, sizeof(lista_var), "_gen_lista_%d", c->tmp_count++);
            sangrar(c);
            fprintf(c->salida, "LValor *%s = lince_lista();\n", lista_var);

            /* Guardar contexto anterior y activar modo generador */
            int prev_en_gen = c->en_generador;
            char prev_lista[64];
            strncpy(prev_lista, c->gen_lista_var, 63);
            c->en_generador = 1;
            strncpy(c->gen_lista_var, lista_var, 63);

            compilar_nodo(c, n->generador.cuerpo);

            /* Restaurar contexto */
            c->en_generador = prev_en_gen;
            strncpy(c->gen_lista_var, prev_lista, 63);

            sangrar(c);
            fprintf(c->salida, "return %s;\n", lista_var);
            c->sangria--;
            fprintf(c->salida, "}\n\n");
            break;
        }

        case NODO_PRODUCIR: {
            /* Añadir el valor a la lista acumuladora */
            if (c->en_generador && c->gen_lista_var[0]) {
                sangrar(c);
                fprintf(c->salida, "lince_lista_agregar(%s, ", c->gen_lista_var);
                compilar_expr(c, n->producir);
                fprintf(c->salida, ");\n");
            }
            break;
        }

        case NODO_IMPORTAR: {
            /* Ya emitido en el pre-scan — solo un comentario */
            sangrar(c);
            fprintf(c->salida, "/* importar \"%s\" */\n", n->importar);
            break;
        }

        default:
            sangrar(c);
            fprintf(c->salida, "/* nodo tipo %d no soportado */\n", n->tipo);
            break;
    }
}

/* ─────────────────────────────────────────
   PUNTO DE ENTRADA DEL COMPILADOR
───────────────────────────────────────── */
/* ─────────────────────────────────────────
   EMISIÓN DE MÓDULOS COMPILADOS
   Cada módulo se convierte en un objeto C
   con sus funciones como métodos.
───────────────────────────────────────── */
static void emitir_modulo_compilado(Compilador *c, const char *nombre) {
    /* No duplicar */
    for (int i = 0; i < c->num_modulos; i++)
        if (strcmp(c->modulos_emitidos[i], nombre) == 0) return;
    if (c->num_modulos < 16)
        strncpy(c->modulos_emitidos[c->num_modulos++], nombre, 31);

    /* Emitir al archivo principal (c->salida apunta a f en este momento) */
    /* Los módulos se emiten como comentario + inicialización de objeto */

    if (strcmp(nombre, "matematica") == 0) {
        fprintf(c->salida,
            "/* módulo matematica */\n"
            "#include <math.h>\n"
            "static LValor *_lmod_matematica = NULL;\n"
            "static void _init_matematica(void) {\n"
            "    if (_lmod_matematica) return;\n"
            "    _lmod_matematica = lince_dic();\n"
            "    lince_dic_set(_lmod_matematica, \"PI\",   lince_num(3.14159265358979323846));\n"
            "    lince_dic_set(_lmod_matematica, \"E\",    lince_num(2.71828182845904523536));\n"
            "    lince_dic_set(_lmod_matematica, \"TAU\",  lince_num(6.28318530717958647692));\n"
            "}\n"
        );
        /* Funciones matemáticas como macros C */
        fprintf(c->salida,
            "static LValor *_mat_abs(LValor *x)   { return lince_num(fabs(x->num)); }\n"
            "static LValor *_mat_raiz(LValor *x)  { return lince_num(sqrt(x->num)); }\n"
            "static LValor *_mat_piso(LValor *x)  { return lince_num(floor(x->num)); }\n"
            "static LValor *_mat_techo(LValor *x) { return lince_num(ceil(x->num)); }\n"
            "static LValor *_mat_redondear(LValor *x) { return lince_num(round(x->num)); }\n"
            "static LValor *_mat_truncar(LValor *x)   { return lince_num(trunc(x->num)); }\n"
            "static LValor *_mat_es_entero(LValor *x) { return lince_bool(x->num == trunc(x->num)); }\n"
            "static LValor *_mat_seno(LValor *x)      { return lince_num(sin(x->num)); }\n"
            "static LValor *_mat_coseno(LValor *x)    { return lince_num(cos(x->num)); }\n"
            "static LValor *_mat_tangente(LValor *x)  { return lince_num(tan(x->num)); }\n"
            "static LValor *_mat_logaritmo(LValor *x) { return lince_num(log(x->num)); }\n"
            "static LValor *_mat_log2(LValor *x)      { return lince_num(log2(x->num)); }\n"
            "static LValor *_mat_log10(LValor *x)     { return lince_num(log10(x->num)); }\n"
            "static LValor *_mat_potencia(LValor *b, LValor *e) { return lince_num(pow(b->num, e->num)); }\n"
            "static LValor *_mat_maximo(LValor *a, LValor *b) { return lince_num(a->num > b->num ? a->num : b->num); }\n"
            "static LValor *_mat_minimo(LValor *a, LValor *b) { return lince_num(a->num < b->num ? a->num : b->num); }\n"
            "static LValor *_mat_factorial(LValor *n) {\n"
            "    long long r = 1, x = (long long)n->num;\n"
            "    for (long long i = 2; i <= x; i++) r *= i;\n"
            "    return lince_num((double)r); }\n"
            "static LValor *_mat_combinaciones(LValor *nv, LValor *k) {\n"
            "    long long n = (long long)nv->num, ki = (long long)k->num;\n"
            "    if (ki < 0 || ki > n) return lince_num(0);\n"
            "    if (ki > n - ki) ki = n - ki;\n"
            "    long long r = 1;\n"
            "    for (long long i = 0; i < ki; i++) r = r * (n - i) / (i + 1);\n"
            "    return lince_num((double)r); }\n"
            "static LValor *_mat_aleatorio(LValor **args, int n) {\n"
            "    if (n == 0) return lince_num((double)rand() / RAND_MAX);\n"
            "    int mn = (int)args[0]->num, mx = (int)args[1]->num;\n"
            "    return lince_num(mn + rand() %% (mx - mn + 1)); }\n"
            "\n"
        );
        /* Acceso a propiedades del módulo via lince_dic_metodo */
        fprintf(c->salida,
            "static LValor *_lmod_mat_get(const char *prop, LValor **args, int nargs) {\n"
            "    _init_matematica();\n"
            "    if (strcmp(prop, \"PI\") == 0)  return lince_num(3.14159265358979323846);\n"
            "    if (strcmp(prop, \"E\") == 0)   return lince_num(2.71828182845904523536);\n"
            "    if (strcmp(prop, \"TAU\") == 0) return lince_num(6.28318530717958647692);\n"
            "    if (strcmp(prop, \"abs\") == 0 && nargs >= 1)       return _mat_abs(args[0]);\n"
            "    if (strcmp(prop, \"raiz\") == 0 && nargs >= 1)      return _mat_raiz(args[0]);\n"
            "    if (strcmp(prop, \"piso\") == 0 && nargs >= 1)      return _mat_piso(args[0]);\n"
            "    if (strcmp(prop, \"techo\") == 0 && nargs >= 1)     return _mat_techo(args[0]);\n"
            "    if (strcmp(prop, \"redondear\") == 0 && nargs >= 1) return _mat_redondear(args[0]);\n"
            "    if (strcmp(prop, \"truncar\") == 0 && nargs >= 1)   return _mat_truncar(args[0]);\n"
            "    if (strcmp(prop, \"es_entero\") == 0 && nargs >= 1) return _mat_es_entero(args[0]);\n"
            "    if (strcmp(prop, \"seno\") == 0 && nargs >= 1)      return _mat_seno(args[0]);\n"
            "    if (strcmp(prop, \"coseno\") == 0 && nargs >= 1)    return _mat_coseno(args[0]);\n"
            "    if (strcmp(prop, \"tangente\") == 0 && nargs >= 1)  return _mat_tangente(args[0]);\n"
            "    if (strcmp(prop, \"logaritmo\") == 0 && nargs >= 1) return _mat_logaritmo(args[0]);\n"
            "    if (strcmp(prop, \"log2\") == 0 && nargs >= 1)      return _mat_log2(args[0]);\n"
            "    if (strcmp(prop, \"log10\") == 0 && nargs >= 1)     return _mat_log10(args[0]);\n"
            "    if (strcmp(prop, \"potencia\") == 0 && nargs >= 2)  return _mat_potencia(args[0], args[1]);\n"
            "    if (strcmp(prop, \"maximo\") == 0 && nargs >= 2)    return _mat_maximo(args[0], args[1]);\n"
            "    if (strcmp(prop, \"minimo\") == 0 && nargs >= 2)    return _mat_minimo(args[0], args[1]);\n"
            "    if (strcmp(prop, \"factorial\") == 0 && nargs >= 1) return _mat_factorial(args[0]);\n"
            "    if (strcmp(prop, \"combinaciones\") == 0 && nargs >= 2) return _mat_combinaciones(args[0], args[1]);\n"
            "    if (strcmp(prop, \"aleatorio\") == 0) return _mat_aleatorio(args, nargs);\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "matematica");
        fprintf(c->salida,
            "static LValor *_lf_matematica(void) {\n"
            "    _init_matematica(); return _lmod_matematica; }\n\n");
    }

    else if (strcmp(nombre, "texto") == 0) {
        fprintf(c->salida,
            "/* módulo texto */\n"
            "static LValor *_txt_dividir(LValor *s, LValor *sep) {\n"
            "    LValor *lista = lince_lista();\n"
            "    const char *src = s->txt, *p = sep->txt;\n"
            "    int plen = strlen(p);\n"
            "    const char *pos;\n"
            "    if (plen == 0) { lince_lista_agregar(lista, lince_txt(src)); return lista; }\n"
            "    while ((pos = strstr(src, p)) != NULL) {\n"
            "        int len = pos - src;\n"
            "        char *buf = malloc(len + 1); strncpy(buf, src, len); buf[len] = 0;\n"
            "        lince_lista_agregar(lista, lince_txt(buf)); free(buf);\n"
            "        src = pos + plen;\n"
            "    }\n"
            "    lince_lista_agregar(lista, lince_txt(src));\n"
            "    return lista; }\n"
            "static LValor *_txt_unir(LValor *lista, LValor *sep) {\n"
            "    char buf[65536] = \"\";\n"
            "    for (int i = 0; i < lista->lista.cant; i++) {\n"
            "        if (i > 0) strncat(buf, sep->txt, sizeof(buf)-strlen(buf)-1);\n"
            "        char *s = lince_a_txt(lista->lista.elems[i]);\n"
            "        strncat(buf, s, sizeof(buf)-strlen(buf)-1); free(s);\n"
            "    }\n"
            "    return lince_txt(buf); }\n"
            "static LValor *_txt_invertir(LValor *s) {\n"
            "    size_t bytes = strlen(s->txt);\n"
            "    char *r = malloc(bytes + 1);\n"
            "    size_t destino = bytes, i = 0;\n"
            "    while (s->txt[i]) {\n"
            "        int tam = lince_utf8_tam(s->txt + i);\n"
            "        destino -= (size_t)tam;\n"
            "        memcpy(r + destino, s->txt + i, (size_t)tam);\n"
            "        i += (size_t)tam;\n"
            "    }\n"
            "    r[bytes] = 0; LValor *v = lince_txt(r); free(r); return v; }\n"
            "static LValor *_txt_posicion(LValor *s, LValor *sub) {\n"
            "    char *p = strstr(s->txt, sub->txt);\n"
            "    if (!p) return lince_num(-1);\n"
            "    int chars = 0;\n"
            "    for (const char *q = s->txt; q < p; q += lince_utf8_tam(q)) chars++;\n"
            "    return lince_num(chars); }\n"
            "static LValor *_lmod_txt_get(const char *prop, LValor **args, int nargs) {\n"
            "    if (strcmp(prop, \"dividir\") == 0 && nargs >= 2)  return _txt_dividir(args[0], args[1]);\n"
            "    if (strcmp(prop, \"unir\") == 0 && nargs >= 2)     return _txt_unir(args[0], args[1]);\n"
            "    if (strcmp(prop, \"invertir\") == 0 && nargs >= 1) return _txt_invertir(args[0]);\n"
            "    if (strcmp(prop, \"posicion\") == 0 && nargs >= 2) return _txt_posicion(args[0], args[1]);\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "texto");
    }

    else if (strcmp(nombre, "json") == 0) {
        fprintf(c->salida,
            "/* módulo json — parseo básico */\n"
            "static LValor *_json_texto(LValor *v) {\n"
            "    if (!v) return lince_txt(\"nulo\");\n"
            "    char *s = lince_a_txt(v);\n"
            "    LValor *r = lince_txt(s); free(s); return r; }\n"
            "static LValor *_lmod_json_get(const char *prop, LValor **args, int nargs) {\n"
            "    if (strcmp(prop, \"texto\") == 0 && nargs >= 1) return _json_texto(args[0]);\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "json");
    }

    else if (strcmp(nombre, "archivos") == 0) {
        fprintf(c->salida,
            "/* módulo archivos */\n"
            "#include <sys/stat.h>\n"
            "static LValor *_arc_leer(LValor *ruta) {\n"
            "    FILE *f = fopen(ruta->txt, \"r\");\n"
            "    if (!f) return lince_txt(\"\");\n"
            "    fseek(f, 0, SEEK_END); long tam = ftell(f); rewind(f);\n"
            "    char *buf = malloc(tam + 1);\n"
            "    size_t n = fread(buf, 1, tam, f); buf[n] = 0; fclose(f);\n"
            "    LValor *v = lince_txt(buf); free(buf); return v; }\n"
            "static LValor *_arc_escribir(LValor *ruta, LValor *cont) {\n"
            "    FILE *f = fopen(ruta->txt, \"w\");\n"
            "    if (!f) return lince_bool(0);\n"
            "    fputs(cont->txt, f); fclose(f); return lince_bool(1); }\n"
            "static LValor *_arc_agregar(LValor *ruta, LValor *cont) {\n"
            "    FILE *f = fopen(ruta->txt, \"a\");\n"
            "    if (!f) return lince_bool(0);\n"
            "    fputs(cont->txt, f); fclose(f); return lince_bool(1); }\n"
            "static LValor *_arc_existe(LValor *ruta) {\n"
            "    FILE *f = fopen(ruta->txt, \"r\");\n"
            "    if (f) { fclose(f); return lince_bool(1); }\n"
            "    return lince_bool(0); }\n"
            "static LValor *_arc_eliminar(LValor *ruta) {\n"
            "    return lince_bool(remove(ruta->txt) == 0); }\n"
            "static LValor *_arc_copiar(LValor *orig, LValor *dest) {\n"
            "    FILE *s = fopen(orig->txt, \"rb\");\n"
            "    if (!s) return lince_bool(0);\n"
            "    FILE *d = fopen(dest->txt, \"wb\");\n"
            "    if (!d) { fclose(s); return lince_bool(0); }\n"
            "    char buf[4096]; size_t n;\n"
            "    while ((n = fread(buf,1,sizeof(buf),s)) > 0) fwrite(buf,1,n,d);\n"
            "    fclose(s); fclose(d); return lince_bool(1); }\n"
            "static LValor *_arc_mover(LValor *orig, LValor *dest) {\n"
            "    return lince_bool(rename(orig->txt, dest->txt) == 0); }\n"
            "static LValor *_arc_es_directorio(LValor *ruta) {\n"
            "    struct stat st;\n"
            "    if (stat(ruta->txt, &st) == 0) return lince_bool(S_ISDIR(st.st_mode));\n"
            "    return lince_bool(0); }\n"
            "static LValor *_arc_crear_directorio(LValor *ruta) {\n"
            "    return lince_bool(mkdir(ruta->txt, 0755) == 0); }\n"
            "static LValor *_arc_listar(LValor *ruta) {\n"
            "    char cmd[1024]; LValor *lista = lince_lista();\n"
            "    snprintf(cmd, sizeof(cmd), \"ls \\\"%%s\\\" 2>/dev/null\", ruta->txt);\n"
            "    FILE *p = popen(cmd, \"r\");\n"
            "    if (!p) return lista;\n"
            "    char linea[512];\n"
            "    while (fgets(linea, sizeof(linea), p)) {\n"
            "        linea[strcspn(linea, \"\\n\\r\")] = 0;\n"
            "        if (linea[0]) lince_lista_agregar(lista, lince_txt(linea));\n"
            "    }\n"
            "    pclose(p); return lista; }\n"
            "static LValor *_lmod_arc_get(const char *prop, LValor **args, int nargs) {\n"
            "    if (strcmp(prop, \"leer\") == 0 && nargs >= 1)             return _arc_leer(args[0]);\n"
            "    if (strcmp(prop, \"escribir\") == 0 && nargs >= 2)         return _arc_escribir(args[0], args[1]);\n"
            "    if (strcmp(prop, \"agregar\") == 0 && nargs >= 2)          return _arc_agregar(args[0], args[1]);\n"
            "    if (strcmp(prop, \"existe\") == 0 && nargs >= 1)           return _arc_existe(args[0]);\n"
            "    if (strcmp(prop, \"eliminar\") == 0 && nargs >= 1)         return _arc_eliminar(args[0]);\n"
            "    if (strcmp(prop, \"copiar\") == 0 && nargs >= 2)           return _arc_copiar(args[0], args[1]);\n"
            "    if (strcmp(prop, \"mover\") == 0 && nargs >= 2)            return _arc_mover(args[0], args[1]);\n"
            "    if (strcmp(prop, \"es_directorio\") == 0 && nargs >= 1)    return _arc_es_directorio(args[0]);\n"
            "    if (strcmp(prop, \"crear_directorio\") == 0 && nargs >= 1) return _arc_crear_directorio(args[0]);\n"
            "    if (strcmp(prop, \"listar\") == 0 && nargs >= 1)           return _arc_listar(args[0]);\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "archivos");
    }

    else if (strcmp(nombre, "tiempo") == 0) {
        fprintf(c->salida,
            "/* módulo tiempo */\n"
            "#include <time.h>\n"
            "#ifdef _WIN32\n"
            "#include <windows.h>\n"
            "#define _lince_sleep_ms(ms) Sleep(ms)\n"
            "#else\n"
            "#include <unistd.h>\n"
            "#define _lince_sleep_ms(ms) usleep((ms)*1000)\n"
            "#endif\n"
            "static LValor *_lmod_tpo_get(const char *prop, LValor **args, int nargs) {\n"
            "    if (strcmp(prop, \"ahora\") == 0)  return lince_num((double)time(NULL));\n"
            "    if (strcmp(prop, \"esperar\") == 0 && nargs >= 1) {\n"
            "        _lince_sleep_ms((int)args[0]->num); return lince_nulo(); }\n"
            "    if (strcmp(prop, \"fecha\") == 0) {\n"
            "        time_t t = time(NULL); struct tm *tm = localtime(&t);\n"
            "        char buf[64]; strftime(buf, sizeof(buf), \"%%Y-%%m-%%d\", tm);\n"
            "        return lince_txt(buf); }\n"
            "    if (strcmp(prop, \"hora\") == 0) {\n"
            "        time_t t = time(NULL); struct tm *tm = localtime(&t);\n"
            "        char buf[64]; strftime(buf, sizeof(buf), \"%%H:%%M:%%S\", tm);\n"
            "        return lince_txt(buf); }\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "tiempo");
    }

    else if (strcmp(nombre, "sistema") == 0) {
        fprintf(c->salida,
            "/* módulo sistema */\n"
            "static LValor *_lmod_sis_get(const char *prop, LValor **args, int nargs) {\n"
            "    if (strcmp(prop, \"ejecutar\") == 0 && nargs >= 1) {\n"
            "        return lince_num(system(args[0]->txt)); }\n"
            "    if (strcmp(prop, \"existe\") == 0 && nargs >= 1) {\n"
            "        FILE *f = fopen(args[0]->txt, \"r\");\n"
            "        if (f) { fclose(f); return lince_bool(1); }\n"
            "        struct stat st;\n"
            "        return lince_bool(stat(args[0]->txt, &st) == 0); }\n"
            "    if (strcmp(prop, \"salir\") == 0) {\n"
            "        int code = (nargs >= 1) ? (int)args[0]->num : 0;\n"
            "        exit(code); }\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "sistema");
    }

    else if (strcmp(nombre, "expresiones") == 0) {
        fprintf(c->salida,
            "/* módulo expresiones */\n"
            "#ifndef _WIN32\n"
            "#include <regex.h>\n"
            "static LValor *_exp_coincidir(LValor *txt, LValor *pat) {\n"
            "    regex_t re; int r = 0;\n"
            "    if (regcomp(&re, pat->txt, REG_EXTENDED) == 0) {\n"
            "        r = (regexec(&re, txt->txt, 0, NULL, 0) == 0);\n"
            "        regfree(&re); }\n"
            "    return lince_bool(r); }\n"
            "static LValor *_exp_buscar(LValor *txt, LValor *pat) {\n"
            "    regex_t re;\n"
            "    if (regcomp(&re, pat->txt, REG_EXTENDED) != 0) return lince_nulo();\n"
            "    regmatch_t m;\n"
            "    if (regexec(&re, txt->txt, 1, &m, 0) != 0) { regfree(&re); return lince_nulo(); }\n"
            "    int len = m.rm_eo - m.rm_so;\n"
            "    char *buf = malloc(len + 1); strncpy(buf, txt->txt + m.rm_so, len); buf[len] = 0;\n"
            "    LValor *v = lince_txt(buf); free(buf); regfree(&re); return v; }\n"
            "static LValor *_exp_todos(LValor *txt, LValor *pat) {\n"
            "    LValor *lista = lince_lista();\n"
            "    regex_t re;\n"
            "    if (regcomp(&re, pat->txt, REG_EXTENDED) != 0) return lista;\n"
            "    const char *s = txt->txt; regmatch_t m;\n"
            "    while (regexec(&re, s, 1, &m, 0) == 0) {\n"
            "        int len = m.rm_eo - m.rm_so;\n"
            "        char *buf = malloc(len + 1); strncpy(buf, s + m.rm_so, len); buf[len] = 0;\n"
            "        lince_lista_agregar(lista, lince_txt(buf)); free(buf);\n"
            "        s += (m.rm_eo > 0) ? m.rm_eo : 1; }\n"
            "    regfree(&re); return lista; }\n"
            "static LValor *_exp_reemplazar(LValor *txt, LValor *pat, LValor *rep) {\n"
            "    regex_t re;\n"
            "    if (regcomp(&re, pat->txt, REG_EXTENDED) != 0) return lince_txt(txt->txt);\n"
            "    char buf[65536] = \"\"; const char *s = txt->txt; regmatch_t m;\n"
            "    while (regexec(&re, s, 1, &m, 0) == 0) {\n"
            "        strncat(buf, s, m.rm_so);\n"
            "        strncat(buf, rep->txt, sizeof(buf)-strlen(buf)-1);\n"
            "        s += (m.rm_eo > 0) ? m.rm_eo : 1; }\n"
            "    strncat(buf, s, sizeof(buf)-strlen(buf)-1);\n"
            "    regfree(&re); return lince_txt(buf); }\n"
            "#else\n"
            "static LValor *_exp_coincidir(LValor *t, LValor *p) { return lince_bool(strstr(t->txt, p->txt) != NULL); }\n"
            "static LValor *_exp_buscar(LValor *t, LValor *p) { return strstr(t->txt, p->txt) ? lince_txt(p->txt) : lince_nulo(); }\n"
            "static LValor *_exp_todos(LValor *t, LValor *p) {\n"
            "    LValor *l = lince_lista(); const char *s = t->txt; int plen = strlen(p->txt);\n"
            "    while ((s = strstr(s, p->txt)) != NULL) { lince_lista_agregar(l, lince_txt(p->txt)); s += plen; }\n"
            "    return l; }\n"
            "static LValor *_exp_reemplazar(LValor *t, LValor *p, LValor *r) {\n"
            "    char buf[65536] = \"\"; const char *s = t->txt; int plen = strlen(p->txt); const char *pos;\n"
            "    while ((pos = strstr(s, p->txt)) != NULL) { strncat(buf, s, pos-s); strcat(buf, r->txt); s = pos+plen; }\n"
            "    strcat(buf, s); return lince_txt(buf); }\n"
            "#endif\n"
            "static LValor *_lmod_exp_get(const char *prop, LValor **args, int nargs) {\n"
            "    if (strcmp(prop, \"coincidir\") == 0 && nargs >= 2)   return _exp_coincidir(args[0], args[1]);\n"
            "    if (strcmp(prop, \"buscar\") == 0 && nargs >= 2)      return _exp_buscar(args[0], args[1]);\n"
            "    if (strcmp(prop, \"todos\") == 0 && nargs >= 2)       return _exp_todos(args[0], args[1]);\n"
            "    if (strcmp(prop, \"reemplazar\") == 0 && nargs >= 3)  return _exp_reemplazar(args[0], args[1], args[2]);\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "expresiones");
    }

    else if (strcmp(nombre, "red") == 0) {
        fprintf(c->salida,
            "/* módulo red — usa curl */\n"
            "#include <unistd.h>\n"
            "static LValor *_red_peticion(const char *metodo, LValor *url,\n"
            "                             LValor *cuerpo, LValor *cabeceras) {\n"
            "    char out_file[] = \"/tmp/lince_red_XXXXXX\";\n"
            "    int fd = mkstemp(out_file); if (fd < 0) return lince_nulo(); close(fd);\n"
            "    char cmd[65536];\n"
            "    snprintf(cmd, sizeof(cmd),\n"
            "        \"curl -s -o %%s -w '%%%%{http_code}' -X %%s\", out_file, metodo);\n"
            "    if (cuerpo && cuerpo->tipo == L_TXT && cuerpo->txt[0]) {\n"
            "        strncat(cmd, \" -d '\", sizeof(cmd)-strlen(cmd)-1);\n"
            "        strncat(cmd, cuerpo->txt, sizeof(cmd)-strlen(cmd)-1);\n"
            "        strncat(cmd, \"'\", sizeof(cmd)-strlen(cmd)-1);\n"
            "    }\n"
            "    if (cabeceras && cabeceras->tipo == L_DIC) {\n"
            "        for (int i = 0; i < cabeceras->dic.cant; i++) {\n"
            "            strncat(cmd, \" -H '\", sizeof(cmd)-strlen(cmd)-1);\n"
            "            strncat(cmd, cabeceras->dic.claves[i], sizeof(cmd)-strlen(cmd)-1);\n"
            "            strncat(cmd, \": \", sizeof(cmd)-strlen(cmd)-1);\n"
            "            char *sv = lince_a_txt(cabeceras->dic.vals[i]);\n"
            "            strncat(cmd, sv, sizeof(cmd)-strlen(cmd)-1); free(sv);\n"
            "            strncat(cmd, \"'\", sizeof(cmd)-strlen(cmd)-1);\n"
            "        }\n"
            "    }\n"
            "    strncat(cmd, \" '\", sizeof(cmd)-strlen(cmd)-1);\n"
            "    strncat(cmd, url->txt, sizeof(cmd)-strlen(cmd)-1);\n"
            "    strncat(cmd, \"' 2>/dev/null\", sizeof(cmd)-strlen(cmd)-1);\n"
            "    FILE *proc = popen(cmd, \"r\");\n"
            "    int codigo = 0;\n"
            "    if (proc) { fscanf(proc, \"%%d\", &codigo); pclose(proc); }\n"
            "    FILE *f = fopen(out_file, \"r\");\n"
            "    char *respuesta = strdup(\"\");\n"
            "    if (f) {\n"
            "        fseek(f, 0, SEEK_END); long tam = ftell(f); rewind(f);\n"
            "        respuesta = malloc(tam + 1);\n"
            "        size_t n = fread(respuesta, 1, tam, f);\n"
            "        respuesta[n] = 0; fclose(f);\n"
            "    }\n"
            "    remove(out_file);\n"
            "    LValor *res = lince_dic();\n"
            "    lince_dic_set(res, \"codigo\", lince_num(codigo));\n"
            "    lince_dic_set(res, \"cuerpo\", lince_txt(respuesta));\n"
            "    lince_dic_set(res, \"ok\",     lince_bool(codigo >= 200 && codigo < 400));\n"
            "    free(respuesta);\n"
            "    return res;\n"
            "}\n"
            "static LValor *_lmod_red_get(const char *prop, LValor **args, int nargs) {\n"
            "    LValor *cab = (nargs >= 2) ? args[1] : NULL;\n"
            "    if (strcmp(prop, \"obtener\") == 0 && nargs >= 1)\n"
            "        return _red_peticion(\"GET\", args[0], NULL, cab);\n"
            "    if (strcmp(prop, \"enviar\") == 0 && nargs >= 2)\n"
            "        return _red_peticion(\"POST\", args[0], args[1],\n"
            "                            nargs >= 3 ? args[2] : NULL);\n"
            "    if (strcmp(prop, \"actualizar\") == 0 && nargs >= 2)\n"
            "        return _red_peticion(\"PUT\", args[0], args[1],\n"
            "                            nargs >= 3 ? args[2] : NULL);\n"
            "    if (strcmp(prop, \"eliminar\") == 0 && nargs >= 1)\n"
            "        return _red_peticion(\"DELETE\", args[0], NULL,\n"
            "                            nargs >= 2 ? args[1] : NULL);\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "red");
    }

    else if (strcmp(nombre, "terminal") == 0) {
        fprintf(c->salida,
            "/* módulo terminal */\n"
            "static char *_ter_val(LValor *v) {\n"
            "    static char _tb[256];\n"
            "    if (!v) return \"\";\n"
            "    if (v->tipo == L_TXT)  return v->txt;\n"
            "    if (v->tipo == L_NUM)  { snprintf(_tb,sizeof(_tb),\"%%g\",v->num); return _tb; }\n"
            "    if (v->tipo == L_BOOL) return v->boolean ? \"verdadero\" : \"falso\";\n"
            "    return \"\";\n"
            "}\n"
            "static LValor *_ter_color(const char *c, LValor *v) {\n"
            "    char buf[4096];\n"
            "    snprintf(buf,sizeof(buf),\"\\033[%%sm%%s\\033[0m\",c,_ter_val(v));\n"
            "    return lince_txt(buf); }\n"
            "static LValor *_lmod_ter_get(const char *prop, LValor **a, int n) {\n"
            "    if (!a) return lince_nulo();\n"
            "    if (strcmp(prop,\"negro\")==0)     return _ter_color(\"30\",a[0]);\n"
            "    if (strcmp(prop,\"rojo\")==0)      return _ter_color(\"31\",a[0]);\n"
            "    if (strcmp(prop,\"verde\")==0)     return _ter_color(\"32\",a[0]);\n"
            "    if (strcmp(prop,\"amarillo\")==0)  return _ter_color(\"33\",a[0]);\n"
            "    if (strcmp(prop,\"azul\")==0)      return _ter_color(\"34\",a[0]);\n"
            "    if (strcmp(prop,\"magenta\")==0)   return _ter_color(\"35\",a[0]);\n"
            "    if (strcmp(prop,\"cian\")==0)      return _ter_color(\"36\",a[0]);\n"
            "    if (strcmp(prop,\"blanco\")==0)    return _ter_color(\"37\",a[0]);\n"
            "    if (strcmp(prop,\"negrita\")==0)   return _ter_color(\"1\", a[0]);\n"
            "    if (strcmp(prop,\"cursiva\")==0)   return _ter_color(\"3\", a[0]);\n"
            "    if (strcmp(prop,\"subrayado\")==0) return _ter_color(\"4\", a[0]);\n"
            "    if (strcmp(prop,\"limpiar\")==0) { printf(\"\\033[2J\\033[H\"); fflush(stdout); return lince_nulo(); }\n"
            "    if (strcmp(prop,\"barra\")==0 && n>=3) {\n"
            "        double val=a[0]->num, mx=a[1]->num; int w=(int)a[2]->num;\n"
            "        if(w<2)w=2; if(mx<=0)mx=1;\n"
            "        int ll=(int)((val/mx)*w); if(ll<0)ll=0; if(ll>w)ll=w;\n"
            "        char buf[256]=\"[\"; for(int i=0;i<w;i++) strcat(buf,i<ll?\"=\":\" \"); strcat(buf,\"]\");\n"
            "        return lince_txt(buf); }\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "terminal");
    }

    else if (strcmp(nombre, "servidor") == 0) {
        /* El servidor HTTP no es compilable a binario estático fácilmente —
           se recomienda usar el intérprete para programas con servidor */
        fprintf(c->salida,
            "/* módulo servidor — usa el intérprete para servidores HTTP */\n"
            "static LValor *_lmod_srv_get(const char *prop, LValor **a, int n) {\n"
            "    (void)prop; (void)a; (void)n;\n"
            "    fprintf(stderr, \"El módulo servidor requiere el interprete.\\n"
            "Use: lince programa.lince\\n\");\n"
            "    return lince_nulo();\n"
            "}\n\n"
        );
        registrar_funcion(c, "servidor");
    }

    else {
        fprintf(stderr,
            "⚠  El módulo '%s' no está disponible en binarios nativos.\n"
            "   Usa el intérprete para este programa.\n", nombre);
        c->err_count++;
    }
}

/* ─────────────────────────────────────────
   SCANNER RECURSIVO DE LAMBDAS
   Encuentra todas las lambdas en el AST y las
   emite al buffer c->lambdas para que estén
   disponibles antes de las funciones que las usan
───────────────────────────────────────── */
static void escanear_lambdas(Compilador *c, Nodo *n) {
    if (!n) return;
    switch (n->tipo) {
        case NODO_BLOQUE:
            for (int i = 0; i < n->bloque.cantidad; i++)
                escanear_lambdas(c, n->bloque.sentencias[i]);
            break;
        case NODO_LAMBDA: {
            /* Emitir SOLO la función lambda al buffer, descartar la expresión closure */
            FILE *prev_salida = c->salida;
            FILE *dev_null = fopen("/dev/null", "w");
            if (dev_null) {
                c->salida = dev_null; /* la expresión closure va a /dev/null */
                compilar_expr_lambda(c, n);
                fclose(dev_null);
            }
            c->salida = prev_salida;
            break;
        }
        case NODO_FUNCION:
            escanear_lambdas(c, n->funcion.cuerpo);
            break;
        case NODO_GENERADOR:
            escanear_lambdas(c, n->generador.cuerpo);
            break;
        case NODO_CLASE:
            for (int i = 0; i < n->clase.num_metodos; i++)
                if (n->clase.metodos[i])
                    escanear_lambdas(c, n->clase.metodos[i]->funcion.cuerpo);
            break;
        case NODO_DECLARACION:
            escanear_lambdas(c, n->declaracion.valor);
            break;
        case NODO_ASIGNACION:
            escanear_lambdas(c, n->asignacion.valor);
            break;
        case NODO_DEVOLVER:
            escanear_lambdas(c, n->devolver);
            break;
        case NODO_SI:
            escanear_lambdas(c, n->si.condicion);
            escanear_lambdas(c, n->si.entonces);
            escanear_lambdas(c, n->si.sino);
            break;
        case NODO_MIENTRAS:
            escanear_lambdas(c, n->mientras.cuerpo);
            break;
        case NODO_LLAMADA:
            for (int i = 0; i < n->llamada.num_argumentos; i++)
                escanear_lambdas(c, n->llamada.argumentos[i]);
            break;
        case NODO_BINARIO:
            escanear_lambdas(c, n->binario.izquierda);
            escanear_lambdas(c, n->binario.derecha);
            break;
        default: break;
    }
}

int compilador_compilar(Nodo *ast, const char *ruta_c, const char *ruta_bin) {
    FILE *f = fopen(ruta_c, "w");
    if (!f) {
        fprintf(stderr, "\n❌ No se pudo crear '%s'\n\n", ruta_c);
        return 1;
    }

    Compilador c = {0};
    c.salida       = f;
    c.sangria      = 0;
    c.tmp_count    = 0;
    c.err_count    = 0;
    c.lambda_count = 0;

    /* Archivo temporal para lambdas */
    snprintf(c.lambda_file, sizeof(c.lambda_file), "%s_lambdas.c", ruta_bin);
    c.lambdas = fopen(c.lambda_file, "w");
    if (!c.lambdas) c.lambdas = f; /* fallback */

    /* PRE-SCAN: registrar todas las funciones y clases antes de compilar */
    for (int i = 0; i < ast->bloque.cantidad; i++) {
        Nodo *n = ast->bloque.sentencias[i];
        if (!n) continue;
        if (n->tipo == NODO_FUNCION) {
            int req = 0, total = n->funcion.num_parametros;
            for (int j = 0; j < total; j++)
                if (!n->funcion.parametros[j].valor_defecto) req++;
            registrar_funcion_con_aridad(&c, n->funcion.nombre, req, total);
        }
        if (n->tipo == NODO_GENERADOR)
            registrar_funcion(&c, n->generador.nombre);
        if (n->tipo == NODO_CLASE) {
            registrar_funcion(&c, n->clase.nombre);
            for (int j = 0; j < n->clase.num_metodos; j++)
                if (n->clase.metodos[j])
                    registrar_funcion(&c, n->clase.metodos[j]->funcion.nombre);
        }
    }
    /* También registrar funciones integradas conocidas */
    registrar_funcion(&c, "escribir");
    registrar_funcion(&c, "leer");
    registrar_funcion(&c, "a_numero");
    registrar_funcion(&c, "a_texto");
    registrar_funcion(&c, "a_logico");
    registrar_funcion(&c, "mapear");
    registrar_funcion(&c, "filtrar");
    registrar_funcion(&c, "reducir");
    registrar_funcion(&c, "rango");
    registrar_funcion(&c, "argumentos");
    registrar_funcion(&c, "rango");

    /* Emitir cabecera del runtime */
    emitir_cabecera(&c);

    /* Pre-escanear lambdas — emitirlas antes de cualquier función */
    /* Las lambdas se acumulan en c.lambdas durante la compilación */
    /* Las incorporamos ANTES del bloque de funciones */

    /* Primero compilamos todo en c.lambdas para recolectar lambdas */
    /* Luego volcamos lambdas + funciones al archivo final */

    fprintf(f, "/* ── Declaraciones adelantadas ── */\n");
    fprintf(f, "static LValor *_lince_obj_metodo(LValor *obj, const char *met, LValor **args, int nargs);\n");
    for (int i = 0; i < ast->bloque.cantidad; i++) {
        Nodo *n = ast->bloque.sentencias[i];
        if (n->tipo == NODO_FUNCION) {
            fprintf(f, "static LValor *_lf_%s(", n->funcion.nombre);
            for (int j = 0; j < n->funcion.num_parametros; j++) {
                if (j > 0) fprintf(f, ", ");
                fprintf(f, "LValor *_l_%s", n->funcion.parametros[j].nombre);
            }
            fprintf(f, ");\n");
        }
        if (n->tipo == NODO_GENERADOR) {
            fprintf(f, "static LValor *_lf_%s(", n->generador.nombre);
            for (int j = 0; j < n->generador.num_parametros; j++) {
                if (j > 0) fprintf(f, ", ");
                fprintf(f, "LValor *_l_%s", n->generador.parametros[j].nombre);
            }
            fprintf(f, ");\n");
        }
        if (n->tipo == NODO_CLASE) {
            /* Declarar constructor y métodos */
            fprintf(f, "static LValor *_lf_%s(", n->clase.nombre);
            for (int j = 0; j < n->clase.num_metodos; j++) {
                if (strcmp(n->clase.metodos[j]->funcion.nombre, "crear") == 0) {
                    Nodo *cr = n->clase.metodos[j];
                    for (int k = 0; k < cr->funcion.num_parametros; k++) {
                        if (k > 0) fprintf(f, ", ");
                        fprintf(f, "LValor *_l_%s", cr->funcion.parametros[k].nombre);
                    }
                    break;
                }
            }
            fprintf(f, ");\n");
            for (int j = 0; j < n->clase.num_metodos; j++) {
                Nodo *mn = n->clase.metodos[j];
                fprintf(f, "static LValor *_lf_%s_%s(LValor *_lince_mi",
                    n->clase.nombre, mn->funcion.nombre);
                for (int k = 0; k < mn->funcion.num_parametros; k++)
                    fprintf(f, ", LValor *_l_%s", mn->funcion.parametros[k].nombre);
                fprintf(f, ");\n");
            }
        }
    }
    fprintf(f, "\n");

    /* Emitir función de despacho de métodos de objeto */
    fprintf(f,
        "/* ── Despacho de métodos de objeto ── */\n"
        "static LValor *_lince_obj_metodo(LValor *obj, const char *met,\n"
        "                                  LValor **args, int nargs) {\n"
        "    if (!obj || obj->tipo != L_OBJ || !obj->obj) return lince_nulo();\n"
        "    const char *cls = obj->obj->clase;\n"
        "    if (!cls || !met) return lince_nulo();\n");

    for (int i = 0; i < ast->bloque.cantidad; i++) {
        Nodo *n = ast->bloque.sentencias[i];
        if (!n || n->tipo != NODO_CLASE) continue;

        fprintf(f, "    if (strcmp(cls, \"%s\") == 0) {\n", n->clase.nombre);

        /* Recorrer la cadena de herencia para encontrar métodos */
        /* Primero los métodos propios de la clase, luego los del padre */
        const char *buscar = n->clase.nombre;
        int profundidad = 0;
        while (buscar && profundidad < 10) {
            /* Encontrar la clase en el AST */
            for (int j = 0; j < ast->bloque.cantidad; j++) {
                Nodo *m = ast->bloque.sentencias[j];
                if (!m || m->tipo != NODO_CLASE) continue;
                if (strcmp(m->clase.nombre, buscar) != 0) continue;

                for (int k = 0; k < m->clase.num_metodos; k++) {
                    Nodo *mn = m->clase.metodos[k];
                    if (!mn) continue;
                    const char *mnom = mn->funcion.nombre;
                    if (!mnom || strcmp(mnom, "crear") == 0) continue;

                    /* Verificar que no fue sobreescrito por una clase hija */
                    int sobreescrito = 0;
                    if (profundidad > 0) {
                        /* Buscar si la clase original (n) tiene este método */
                        for (int jj = 0; jj < n->clase.num_metodos; jj++) {
                            if (n->clase.metodos[jj] &&
                                strcmp(n->clase.metodos[jj]->funcion.nombre,
                                       mnom) == 0) {
                                sobreescrito = 1;
                                break;
                            }
                        }
                    }
                    if (sobreescrito) continue;

                    fprintf(f,
                        "        if (strcmp(met, \"%s\") == 0) "
                        "return _lf_%s_%s(obj",
                        mnom, m->clase.nombre, mnom);
                    for (int p = 0; p < mn->funcion.num_parametros; p++)
                        fprintf(f, ", (nargs > %d ? args[%d] : lince_nulo())", p, p);
                    fprintf(f, ");\n");
                }

                /* Subir al padre */
                buscar = m->clase.padre;
                break;
            }
            profundidad++;
            if (!buscar || !buscar[0]) break;
        }
        fprintf(f, "    }\n");
    }
    fprintf(f, "    return lince_nulo();\n}\n\n");

    /* Pre-escanear módulos */
    fprintf(f, "/* ── Módulos ── */\n");
    for (int i = 0; i < ast->bloque.cantidad; i++) {
        Nodo *n = ast->bloque.sentencias[i];
        if (n && n->tipo == NODO_IMPORTAR)
            emitir_modulo_compilado(&c, n->importar);
    }

    /* ── Estrategia de lambdas por intercalación ──
       Para cada función/generador/clase:
         1. Compilar su cuerpo a un buffer temporal
         2. Volcar las lambdas acumuladas al archivo principal
         3. Volcar el cuerpo de la función al archivo principal
       Así los ids de lambda siempre coinciden.
    ─────────────────────────────────────────── */
    fprintf(f, "/* ── Funciones, clases y lambdas ── */\n");
    c.lambda_count = 0;

    for (int i = 0; i < ast->bloque.cantidad; i++) {
        Nodo *n = ast->bloque.sentencias[i];
        if (!n) continue;
        if (n->tipo != NODO_FUNCION && n->tipo != NODO_CLASE &&
            n->tipo != NODO_GENERADOR) continue;

        /* 1. Abrir buffer para el cuerpo de la función */
        char buf_fn[256];
        snprintf(buf_fn, sizeof(buf_fn), "%s_fn_%d.c", ruta_bin, i);
        FILE *f_fn = fopen(buf_fn, "w");
        if (!f_fn) { compilar_nodo(&c, n); continue; }

        /* Abrir buffer para las lambdas de esta función */
        char buf_lam[256];
        snprintf(buf_lam, sizeof(buf_lam), "%s_lam_%d.c", ruta_bin, i);
        FILE *f_lam = fopen(buf_lam, "w");
        if (!f_lam) { fclose(f_fn); compilar_nodo(&c, n); continue; }

        /* Redirigir salida y lambdas a los buffers */
        FILE *prev_salida = c.salida;
        FILE *prev_lambdas = c.lambdas;
        c.salida  = f_fn;
        c.lambdas = f_lam;

        compilar_nodo(&c, n);

        c.salida  = prev_salida;
        c.lambdas = prev_lambdas;
        fclose(f_fn);
        fclose(f_lam);

        /* 2. Volcar lambdas al archivo principal */
        FILE *lf = fopen(buf_lam, "r");
        if (lf) {
            char buf[4096]; size_t nb;
            while ((nb = fread(buf, 1, sizeof(buf), lf)) > 0)
                fwrite(buf, 1, nb, f);
            fclose(lf);
            remove(buf_lam);
        }

        /* 3. Volcar el cuerpo de la función */
        FILE *ff = fopen(buf_fn, "r");
        if (ff) {
            char buf[4096]; size_t nb;
            while ((nb = fread(buf, 1, sizeof(buf), ff)) > 0)
                fwrite(buf, 1, nb, f);
            fclose(ff);
            remove(buf_fn);
        }
    }

    /* Cerrar archivo de lambdas principal si sigue abierto */
    if (c.lambdas && c.lambdas != f) {
        fclose(c.lambdas);
        c.lambdas = NULL;
    }

    /* Emitir main() en dos pasadas */
    /* Pasada 1: compilar a buffer para capturar lambdas de nivel superior */
    char buf_main[256], buf_lam_main[256];
    snprintf(buf_main,     sizeof(buf_main),     "%s_main.c",     ruta_bin);
    snprintf(buf_lam_main, sizeof(buf_lam_main), "%s_lam_main.c", ruta_bin);
    FILE *f_main     = fopen(buf_main, "w");
    FILE *f_lam_main = fopen(buf_lam_main, "w");

    if (f_main && f_lam_main) {
        FILE *prev_sal = c.salida;
        FILE *prev_lam = c.lambdas;
        c.salida  = f_main;
        c.lambdas = f_lam_main;
        c.sangria = 1;

        for (int i = 0; i < ast->bloque.cantidad; i++) {
            Nodo *n = ast->bloque.sentencias[i];
            if (n->tipo != NODO_FUNCION && n->tipo != NODO_CLASE &&
                n->tipo != NODO_GENERADOR)
                compilar_nodo(&c, n);
        }

        c.salida  = prev_sal;
        c.lambdas = prev_lam;
        fclose(f_main);
        fclose(f_lam_main);

        /* Pasada 2: volcar lambdas y luego el cuerpo del main */
        fprintf(f, "/* ── Lambdas de nivel superior ── */\n");
        FILE *lf = fopen(buf_lam_main, "r");
        if (lf) {
            char buf[4096]; size_t nb;
            while ((nb = fread(buf, 1, sizeof(buf), lf)) > 0)
                fwrite(buf, 1, nb, f);
            fclose(lf);
            remove(buf_lam_main);
        }

        fprintf(f, "int main(void) {\n");
        FILE *mf = fopen(buf_main, "r");
        if (mf) {
            char buf[4096]; size_t nb;
            while ((nb = fread(buf, 1, sizeof(buf), mf)) > 0)
                fwrite(buf, 1, nb, f);
            fclose(mf);
            remove(buf_main);
        }
        fprintf(f, "    return 0;\n}\n");
    } else {
        /* Fallback */
        if (f_main) fclose(f_main);
        if (f_lam_main) fclose(f_lam_main);
        fprintf(f, "int main(void) {\n");
        c.sangria = 1;
        for (int i = 0; i < ast->bloque.cantidad; i++) {
            Nodo *n = ast->bloque.sentencias[i];
            if (n->tipo != NODO_FUNCION && n->tipo != NODO_CLASE &&
                n->tipo != NODO_GENERADOR)
                compilar_nodo(&c, n);
        }
        fprintf(f, "    return 0;\n}\n");
    }

    fclose(f);

    if (c.err_count > 0) {
        fprintf(stderr,
            "\n⚠  Se encontraron %d construcciones no soportadas.\n"
            "   El código C se generó pero puede no compilar correctamente.\n\n",
            c.err_count);
    }

    /* Compilar el C generado con gcc */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc -O2 -o \"%s\" \"%s\" -lm 2>&1", ruta_bin, ruta_c);
    printf("   Compilando con gcc...\n");
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr,
            "\n❌ Error al compilar el código C generado.\n"
            "   Puedes inspeccionar '%s' para ver el código generado.\n\n", ruta_c);
        return 1;
    }

    return 0;
}
