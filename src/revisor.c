/*
 * 🐆 Lince — revisor.c
 * Revisión del programa sin ejecutarlo (ver revisor.h).
 *
 * Recorre el árbol del parser con los ámbitos de nombres a mano, como haría
 * el intérprete, pero sin valores: de cada expresión solo sabe su tipo cuando
 * se deduce sin dudas (un literal, una variable o un parámetro con tipo, lo
 * que devuelve una función con tipo...). Si no lo sabe, calla: un aviso de
 * más es peor que uno de menos, porque enseña a no hacerles caso.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "revisor.h"
#include "interprete.h"
#include "modulos.h"

/* ── Tipos que se conocen sin ejecutar ── */
enum { T_DESC, T_NUM, T_TXT, T_LOG, T_LIS, T_DIC, T_NULO, T_FUN, T_OBJ };
typedef struct { int t; const char *clase; } Tipo;    /* clase: si t == T_OBJ */

static const Tipo DESC = { T_DESC, NULL };

static Tipo tipo(int t) { Tipo r = { t, NULL }; return r; }

static Tipo de_tipodato(TipoDato t, const char *clase) {
    switch (t) {
        case TIPO_NUMERO:      return tipo(T_NUM);
        case TIPO_TEXTO:       return tipo(T_TXT);
        case TIPO_LOGICO:      return tipo(T_LOG);
        case TIPO_LISTA:       return tipo(T_LIS);
        case TIPO_DICCIONARIO: return tipo(T_DIC);
        case TIPO_NULO:        return tipo(T_NULO);
        case TIPO_FUNCION:     return tipo(T_FUN);
        case TIPO_CLASE: { Tipo r = { T_OBJ, clase }; return clase ? r : DESC; }
        default:               return DESC;
    }
}

/* Como se escribe el tipo en Lince: "'vida' es numero" */
static const char *nombre_tipo(Tipo t) {
    static const char *n[] = { "?", "numero", "texto", "logico", "lista", "diccionario",
                               "nulo", "funcion", "?" };
    return t.t == T_OBJ ? t.clase : n[t.t];
}

/* Lo que es un valor: "y aquí le das un texto" */
static const char *describir(Tipo t) {
    static char buf[300];
    static const char *n[] = { "algo", "un número", "un texto", "un lógico", "una lista",
                               "un diccionario", "nulo", "una función", "?" };
    if (t.t != T_OBJ) return n[t.t];
    snprintf(buf, sizeof buf, "un objeto %s", t.clase);
    return buf;
}

/* ── Conjuntos de nombres ── */
typedef struct { char **v; int n, cap; } Nombres;

static int tiene(const Nombres *c, const char *s) {
    for (int i = 0; c && i < c->n; i++) if (strcmp(c->v[i], s) == 0) return 1;
    return 0;
}
static void meter(Nombres *c, const char *s) {
    if (tiene(c, s)) return;
    if (c->n == c->cap) c->v = realloc(c->v, sizeof(char*) * (size_t)(c->cap = c->cap ? c->cap * 2 : 16));
    c->v[c->n++] = strdup(s);
}
static void vaciar(Nombres *c) {
    for (int i = 0; i < c->n; i++) free(c->v[i]);
    free(c->v);
    c->v = NULL; c->n = c->cap = 0;
}

/* ── Ámbitos ── */
enum { S_VAR, S_FIJO, S_FUNCION, S_CLASE, S_INTERFAZ, S_ENUM, S_MODULO, S_OTRO, S_PARAM, S_BUCLE };

typedef struct {
    const char *nombre;
    int      que;            /* S_* */
    int      linea;
    int      usos;           /* veces que se lee */
    int      avisar_sin_uso;
    int      tipado;         /* tipo declarado: se le avisa al darle otro */
    Tipo     tipo;           /* el declarado, o el deducido (ver 'deducido') */
    int      deducido;       /* 'tipo' sale del valor inicial: vale hasta que se le da otro */
    Nodo    *nodo;           /* su NODO_FUNCION / NODO_CLASE / NODO_ENUMERACION / ... */
    Nombres *miembros;       /* módulo o alias: lo que tiene (NULL: no se sabe) */
    int      pendiente;      /* definido más abajo en su bloque: aún no existe */
} Simbolo;

typedef struct Ambito {
    Simbolo *s;
    int      n, cap;
    struct Ambito *padre;
    int      local;          /* dentro de una función o de un bloque: avisa de lo que no se usa */
    int      prof;           /* en cuántas funciones está (lo de dentro se ejecuta luego) */
} Ambito;

static void (*_avisar)(int, const char *);
static int    _incompleto;      /* hay importaciones que no se pudieron leer: no avisar de nombres */
static Nodo **_clases;  static int _nclases;
static Nodo **_ifaces;  static int _nifaces;
static Nombres _asignados;      /* x.campo = ... en cualquier sitio: campos que se crean así */
static Nodo  *_fn;              /* función, lambda o generador en el que se está */
static Nodo  *_clase;           /* clase cuyos métodos se están revisando */
static int    _prof;            /* funciones dentro de las que se está */
static const char *_ruta;
static Nombres _modulos_vistos; /* nombres de módulos con miembros ya cargados */
static Nombres *_miembros_mod[32];

static void avisar(int linea, const char *fmt, ...) {
    char msg[600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    _avisar(linea, msg);
}

static Ambito *ambito_nuevo(Ambito *padre, int local) {
    Ambito *a = calloc(1, sizeof(Ambito));
    a->padre = padre;
    a->local = local;
    a->prof  = _prof;
    return a;
}

static Simbolo *declarar(Ambito *a, const char *nombre, int que, int linea) {
    for (int i = 0; i < a->n; i++)
        if (strcmp(a->s[i].nombre, nombre) == 0) return &a->s[i];
    if (a->n == a->cap) a->s = realloc(a->s, sizeof(Simbolo) * (size_t)(a->cap = a->cap ? a->cap * 2 : 16));
    Simbolo *s = &a->s[a->n++];
    memset(s, 0, sizeof *s);
    s->nombre = nombre;
    s->que    = que;
    s->linea  = linea;
    s->tipo   = DESC;
    s->avisar_sin_uso = a->local && (que == S_VAR || que == S_FIJO) && nombre[0] != '_';
    return s;
}

/* Lo que vale 'nombre' donde se usa ahora: lo definido más abajo en un bloque
   que se está ejecutando no existe aún (sí desde dentro de una función, que
   se ejecuta después). Si solo está eso, lo deja en *pendiente. */
static Simbolo *buscar_uso(Ambito *a, const char *nombre, Simbolo **pendiente) {
    for (; a; a = a->padre)
        for (int i = a->n - 1; i >= 0; i--)
            if (strcmp(a->s[i].nombre, nombre) == 0) {
                if (a->s[i].pendiente && a->prof == _prof) {
                    if (!*pendiente) *pendiente = &a->s[i];
                    break;
                }
                return &a->s[i];
            }
    return NULL;
}

/* Desde aquí ya existe (se ha llegado a su definición) */
static void definido(Ambito *a, const char *nombre) {
    for (int i = 0; i < a->n; i++)
        if (strcmp(a->s[i].nombre, nombre) == 0) a->s[i].pendiente = 0;
}

static Simbolo *buscar(Ambito *a, const char *nombre) {
    for (; a; a = a->padre)
        for (int i = a->n - 1; i >= 0; i--)
            if (strcmp(a->s[i].nombre, nombre) == 0) return &a->s[i];
    return NULL;
}

static void ambito_cerrar(Ambito *a) {
    for (int i = 0; i < a->n; i++) {
        Simbolo *s = &a->s[i];
        if (s->avisar_sin_uso && s->usos == 0)
            avisar(s->linea, "'%s' se define pero no se usa (si es a propósito, empieza su nombre por _).",
                   s->nombre);
    }
    free(a->s);
    free(a);
}

/* ── «¿Querías decir…?» ── */
static int distancia(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > 40 || lb > 40 || abs(la - lb) > 2) return 99;
    int d[41][41];
    for (int i = 0; i <= la; i++) d[i][0] = i;
    for (int j = 0; j <= lb; j++) d[0][j] = j;
    for (int i = 1; i <= la; i++)
        for (int j = 1; j <= lb; j++) {
            int c = d[i-1][j-1] + (tolower((unsigned char)a[i-1]) != tolower((unsigned char)b[j-1]));
            if (d[i-1][j] + 1 < c) c = d[i-1][j] + 1;
            if (d[i][j-1] + 1 < c) c = d[i][j-1] + 1;
            if (i > 1 && j > 1 && a[i-1] == b[j-2] && a[i-2] == b[j-1] && d[i-2][j-2] + 1 < c)
                c = d[i-2][j-2] + 1;                              /* dos letras cambiadas */
            d[i][j] = c;
        }
    return d[la][lb];
}

static const char *parecido(const char *nombre, const char *cand, const char *mejor, int *dmejor) {
    int d = distancia(nombre, cand);
    int tope = strlen(nombre) <= 4 ? 1 : 2;
    if (d > 0 && d <= tope && d < *dmejor) { *dmejor = d; return cand; }
    return mejor;
}

/* " ¿Querías decir 'x'?", o "" */
static const char *sugerencia(const char *nombre, Ambito *a, const Nombres *otros) {
    static char buf[300];
    const char *mejor = NULL;
    int dm = 99;
    for (; a; a = a->padre)
        for (int i = 0; i < a->n; i++) mejor = parecido(nombre, a->s[i].nombre, mejor, &dm);
    for (int i = 0; otros && i < otros->n; i++) mejor = parecido(nombre, otros->v[i], mejor, &dm);
    if (!mejor) return "";
    snprintf(buf, sizeof buf, " ¿Querías decir '%s'?", mejor);
    return buf;
}

/* ── Módulos estándar: lo que tiene cada uno, leído del propio intérprete ── */
static Nombres *miembros_de_modulo(const char *nombre) {
    for (int i = 0; i < _modulos_vistos.n; i++)
        if (strcmp(_modulos_vistos.v[i], nombre) == 0) return _miembros_mod[i];
    int estandar = 0;
    for (int i = 0; modulo_estandar(i); i++)
        if (strcmp(modulo_estandar(i), nombre) == 0) estandar = 1;
    if (!estandar || _modulos_vistos.n >= 32) return NULL;
    Interprete *in = interprete_crear();
    modulo_cargar(nombre, in->global);
    Nombres *m = calloc(1, sizeof(Nombres));
    for (int i = 0; i < in->global->cantidad; i++) {
        EntradaVar *ev = &in->global->vars[i];
        if (strcmp(ev->nombre, nombre) != 0 || !ev->valor || ev->valor->tipo != VAL_DICCIONARIO) continue;
        for (int j = 0; j < ev->valor->diccionario.cantidad; j++)
            meter(m, ev->valor->diccionario.claves[j]);
    }
    meter(&_modulos_vistos, nombre);
    _miembros_mod[_modulos_vistos.n - 1] = m;
    return m;
}

/* ── Lo que define un archivo importado, leído por encima ──
   No se usa el parser, que termina el proceso al primer error: se miran las
   líneas que empiezan fuera de toda llave ('sea x', 'funcion f', 'clase C',
   'importar "..."'). Devuelve 0 si no se pudo leer. */
static const char *saltar_esp(const char *s) { while (*s == ' ' || *s == '\t') s++; return s; }
static int es_id(int c) { return isalnum(c) || c == '_' || (c & 0x80); }
static const char *palabra(const char *s, char *w, size_t max) {
    size_t n = 0;
    while (es_id((unsigned char)*s)) { if (n + 1 < max) w[n++] = *s; s++; }
    w[n] = '\0';
    return s;
}

static int escanear(const char *ruta, Nombres *nombres, Nombres *vistos, int nivel);

static int escanear_import(const char *desde, const char *nombre, const char *alias,
                           Nombres *nombres, Nombres *vistos, int nivel) {
    if (alias) { meter(nombres, alias); return 1; }
    if (strncmp(nombre, "./", 2) != 0 && strncmp(nombre, "../", 3) != 0 &&
        strncmp(nombre, "paquete:", 8) != 0) { meter(nombres, nombre); return 1; }
    char ruta[1024];
    if (!interprete_resolver_import(desde, nombre, ruta, sizeof ruta)) return 0;
    return escanear(ruta, nombres, vistos, nivel + 1);
}

static int escanear(const char *ruta, Nombres *nombres, Nombres *vistos, int nivel) {
    if (tiene(vistos, ruta)) return 1;
    if (nivel > 12) return 0;
    meter(vistos, ruta);
    char *texto = interprete_leer_fuente(ruta);
    if (!texto) return 0;
    int hondo = 0, ok = 1;
    const char *s = texto;
    while (*s) {
        if (hondo == 0) {
            const char *l = saltar_esp(s);
            char w[128], w2[128];
            const char *q = palabra(l, w, sizeof w);
            if (strcmp(w, "abstracta") == 0) q = palabra(saltar_esp(q), w, sizeof w);
            if (!strcmp(w, "funcion") || !strcmp(w, "generador") || !strcmp(w, "clase") ||
                !strcmp(w, "interfaz") || !strcmp(w, "enumeracion") || !strcmp(w, "tarea")) {
                palabra(saltar_esp(q), w2, sizeof w2);
                if (w2[0]) meter(nombres, w2);
            } else if (!strcmp(w, "sea") || !strcmp(w, "fijo")) {
                q = saltar_esp(q);
                if (*q == '[' || *q == '{') {                     /* sea [a, b] = ... */
                    char cierre = *q == '[' ? ']' : '}';
                    q++;
                    while (*q && *q != cierre && *q != '\n') {
                        q = saltar_esp(q);
                        const char *r = palabra(q, w2, sizeof w2);
                        if (w2[0]) meter(nombres, w2);
                        q = r == q ? q + 1 : r;
                    }
                } else {                                         /* el último nombre antes del '=' */
                    char ultimo[128] = "";
                    while (*q && *q != '=' && *q != '\n' && *q != '#') {
                        const char *r = palabra(q, w2, sizeof w2);
                        if (w2[0]) strcpy(ultimo, w2);
                        q = r == q ? q + 1 : r;
                    }
                    if (ultimo[0]) meter(nombres, ultimo);
                }
            } else if (!strcmp(w, "importar")) {
                q = saltar_esp(q);
                if (*q == '"') {
                    const char *fin = strchr(q + 1, '"');
                    if (fin) {
                        char nom[512], alias[128] = "";
                        snprintf(nom, sizeof nom, "%.*s", (int)(fin - q - 1), q + 1);
                        const char *r = saltar_esp(fin + 1);
                        r = palabra(r, w2, sizeof w2);
                        if (!strcmp(w2, "como")) palabra(saltar_esp(r), alias, sizeof alias);
                        if (!escanear_import(ruta, nom, alias[0] ? alias : NULL, nombres, vistos, nivel))
                            ok = 0;
                    }
                }
            }
        }
        /* hasta el final de la línea, contando llaves fuera de textos y comentarios */
        while (*s && *s != '\n') {
            if (*s == '#') { while (*s && *s != '\n') s++; break; }
            if (*s == '"') {
                s++;
                while (*s && *s != '"') { if (*s == '\\' && s[1]) s++; s++; }
                if (*s) s++;
                continue;
            }
            if (*s == '{') hondo++;
            else if (*s == '}' && hondo > 0) hondo--;
            s++;
        }
        if (*s) s++;
    }
    free(texto);
    return ok;
}

/* ── Clases e interfaces del archivo (de cualquier sitio, también las de después) ── */
static void apuntar_nodo(Nodo ***v, int *n, Nodo *x) {
    *v = realloc(*v, sizeof(Nodo*) * (size_t)(*n + 1));
    (*v)[(*n)++] = x;
}

static Nodo *clase_llamada(const char *nombre) {
    for (int i = 0; nombre && i < _nclases; i++)
        if (strcmp(_clases[i]->clase.nombre, nombre) == 0) return _clases[i];
    return NULL;
}
static Nodo *interfaz_llamada(const char *nombre) {
    for (int i = 0; nombre && i < _nifaces; i++)
        if (strcmp(_ifaces[i]->interfaz.nombre, nombre) == 0) return _ifaces[i];
    return NULL;
}

/* Los nombres de los miembros de una clase con sus antecesoras e interfaces.
   Devuelve 0 si alguna no está en este archivo (entonces no se sabe todo). */
static int miembros_de_clase(Nodo *c, Nombres *out, int hondo) {
    if (!c || hondo > 32) return 0;
    for (int i = 0; i < c->clase.num_campos; i++) meter(out, c->clase.campos[i]->declaracion.nombre);
    for (int i = 0; i < c->clase.num_metodos; i++) {
        const char *m = c->clase.metodos[i]->funcion.nombre;
        if (!strncmp(m, "obtener ", 8)) m += 8;
        else if (!strncmp(m, "fijar ", 6)) m += 6;
        meter(out, m);
    }
    int completa = 1;
    for (int i = 0; i < c->clase.num_interfaces; i++) {
        Nodo *f = interfaz_llamada(c->clase.interfaces[i]);
        if (!f) { completa = 0; continue; }
        for (int j = 0; j < f->interfaz.num_metodos; j++) meter(out, f->interfaz.metodos[j]);
    }
    if (c->clase.padre) completa &= miembros_de_clase(clase_llamada(c->clase.padre), out, hondo + 1);
    return completa;
}

/* El método 'nombre' (un NODO_FUNCION, no una propiedad) de la clase o sus antecesoras */
static Nodo *metodo_de(Nodo *c, const char *nombre, int hondo) {
    if (!c || hondo > 32) return NULL;
    for (int i = 0; i < c->clase.num_metodos; i++)
        if (strcmp(c->clase.metodos[i]->funcion.nombre, nombre) == 0) return c->clase.metodos[i];
    for (int i = 0; i < c->clase.num_interfaces; i++) {
        Nodo *f = interfaz_llamada(c->clase.interfaces[i]);
        for (int j = 0; f && j < f->interfaz.num_metodos; j++)
            if (f->interfaz.funciones && f->interfaz.funciones[j] &&
                strcmp(f->interfaz.metodos[j], nombre) == 0) return f->interfaz.funciones[j];
    }
    return metodo_de(clase_llamada(c->clase.padre), nombre, hondo + 1);
}

/* El campo declarado 'nombre' (su NODO_DECLARACION) */
static Nodo *campo_de(Nodo *c, const char *nombre, int hondo) {
    if (!c || hondo > 32) return NULL;
    for (int i = 0; i < c->clase.num_campos; i++)
        if (strcmp(c->clase.campos[i]->declaracion.nombre, nombre) == 0) return c->clase.campos[i];
    return campo_de(clase_llamada(c->clase.padre), nombre, hondo + 1);
}

/* ¿Un objeto de la clase 'a' vale donde se pide 'b'? Si no se sabe, sí. */
static int deriva(const char *a, const char *b, int hondo) {
    if (!a || !b || strcmp(a, b) == 0 || strchr(a, '.') || strchr(b, '.') || hondo > 32) return 1;
    Nodo *c = clase_llamada(a);
    if (!c) return 1;
    for (int i = 0; i < c->clase.num_interfaces; i++) {
        if (strcmp(c->clase.interfaces[i], b) == 0) return 1;
        if (!interfaz_llamada(c->clase.interfaces[i])) return 1;
    }
    if (!c->clase.padre) return !clase_llamada(b) && !interfaz_llamada(b);   /* b no es de aquí */
    return deriva(c->clase.padre, b, hondo + 1);
}

static int compatible(Tipo dec, Tipo val) {
    if (dec.t == T_DESC || val.t == T_DESC) return 1;
    if (dec.t == T_OBJ) {
        if (val.t == T_NULO) return 1;                   /* 'sea Arma arma = nulo': sin arma aún */
        return val.t == T_OBJ && deriva(val.clase, dec.clase, 0);
    }
    return dec.t == val.t;
}

/* ── El recorrido ── */
static Tipo ver(Nodo *n, Ambito *a);

static int tipo_basico(const char *n) {
    static const char *b[] = { "numero", "texto", "logico", "lista", "diccionario", "nulo",
                               "funcion", "tarea", "bytes", "cualquiera", NULL };
    for (int i = 0; b[i]; i++) if (strcmp(b[i], n) == 0) return 1;
    return 0;
}

static void nombre_desconocido(const char *nombre, int linea, Ambito *a) {
    if (_incompleto || !strncmp(nombre, "Error", 5)) return;
    avisar(linea, "No hay nada llamado '%s'.%s", nombre, sugerencia(nombre, a, NULL));
}

/* Un nombre que se usa; 'leer' cuenta como uso (no lo es darle un valor con '=') */
static Simbolo *usar(Ambito *a, const char *nombre, int linea, int leer) {
    Simbolo *pend = NULL;
    Simbolo *s = buscar_uso(a, nombre, &pend);
    if (!s && pend) {
        avisar(linea, "'%s' se usa antes de definirlo (en la línea %d): aquí aún no existe.", nombre, pend->linea);
        s = pend;
        pend->pendiente = 0;           /* un aviso basta */
    }
    if (!s) { nombre_desconocido(nombre, linea, a); return NULL; }
    if (leer) s->usos++;
    return s;
}

/* El nombre de una clase o interfaz donde va un tipo: que exista */
static void usar_tipo(const char *nombre, int linea, Ambito *a) {
    if (!nombre || tipo_basico(nombre)) return;
    char base[256];
    snprintf(base, sizeof base, "%s", nombre);
    char *p = strchr(base, '.');
    if (p) *p = '\0';
    Simbolo *s = buscar(a, base);
    if (s) { s->usos++; return; }
    if (_incompleto || !strncmp(base, "Error", 5)) return;
    avisar(linea, "No hay ninguna clase ni interfaz llamada '%s'.%s", base, sugerencia(base, a, NULL));
}

static void comprobar_args(Parametro *ps, int np, const char *nombre, Nodo **args, Tipo *tipos,
                           int nargs, int linea) {
    int min = 0;
    for (int i = 0; i < np; i++) if (!ps[i].valor_defecto) min = i + 1;
    if (nargs < min || nargs > np) {
        if (min == np)
            avisar(linea, "'%s' necesita %d argumento%s, y aquí le das %d.", nombre, np, np == 1 ? "" : "s", nargs);
        else
            avisar(linea, "'%s' necesita entre %d y %d argumentos, y aquí le das %d.", nombre, min, np, nargs);
        return;
    }
    (void)args;
    for (int i = 0; i < nargs; i++) {
        Tipo dec = de_tipodato(ps[i].tipo, ps[i].tipo_clase);
        if (!compatible(dec, tipos[i]))
            avisar(linea, "El parámetro '%s' de '%s' es %s, y aquí le das %s.",
                   ps[i].nombre, nombre, nombre_tipo(dec), describir(tipos[i]));
    }
}

/* Los argumentos: se revisan y se apuntan sus tipos (malloc) */
static Tipo *ver_args(Nodo **args, int n, Ambito *a) {
    Tipo *t = malloc(sizeof(Tipo) * (size_t)(n + 1));
    for (int i = 0; i < n; i++) t[i] = ver(args[i], a);
    return t;
}

/* Declara de antemano lo que define un bloque: así una función puede usar
   otra que se escribe más abajo, como pasa al ejecutar */
static void preparar_import(Nodo *s, Ambito *a) {
    const char *nom = s->importar.nombre, *alias = s->importar.alias;
    int de_archivo = !strncmp(nom, "./", 2) || !strncmp(nom, "../", 3) || !strncmp(nom, "paquete:", 8);
    if (!de_archivo) {
        Simbolo *m = declarar(a, alias ? alias : nom, S_MODULO, s->linea);
        m->miembros = miembros_de_modulo(nom);
        return;
    }
    Nombres *nombres = calloc(1, sizeof(Nombres));
    Nombres vistos = {0};
    int ok = 0;
    char ruta[1024];
    if (_ruta && interprete_resolver_import(_ruta, nom, ruta, sizeof ruta)) {
        meter(&vistos, _ruta);
        ok = escanear(ruta, nombres, &vistos, 0);
    }
    vaciar(&vistos);
    if (alias) {
        Simbolo *m = declarar(a, alias, S_MODULO, s->linea);
        m->miembros = ok ? nombres : NULL;
        if (!ok) { vaciar(nombres); free(nombres); }
        return;
    }
    if (!ok) _incompleto = 1;
    for (int i = 0; i < nombres->n; i++) {
        Simbolo *x = declarar(a, nombres->v[i], S_OTRO, s->linea);
        x->usos++;
        nombres->v[i] = NULL;       /* el símbolo se queda con el texto */
        (void)x;
    }
    free(nombres->v);
    free(nombres);
}

static void preparar(Nodo *b, Ambito *a) {
    for (int i = 0; i < b->bloque.cantidad; i++) {
        Nodo *s = b->bloque.sentencias[i];
        if (!s) continue;
        switch (s->tipo) {
            case NODO_DECLARACION: {
                Simbolo *x = declarar(a, s->declaracion.nombre,
                                      s->declaracion.constante ? S_FIJO : S_VAR, s->linea);
                x->pendiente = 1;
                if (s->declaracion.tipado) {
                    x->tipado = 1;
                    x->tipo = de_tipodato(s->declaracion.tipo, s->declaracion.tipo_clase);
                }
                break;
            }
            case NODO_DESESTRUCTURAR:
                if (s->desestructurar.declarar)
                    for (int k = 0; k < s->desestructurar.num_nombres; k++)
                        declarar(a, s->desestructurar.nombres[k],
                                 s->desestructurar.constante ? S_FIJO : S_VAR, s->linea)->pendiente = 1;
                break;
            case NODO_FUNCION: {
                Simbolo *x = declarar(a, s->funcion.nombre, S_FUNCION, s->linea);
                x->nodo = s;
                x->pendiente = 1;
                break;
            }
            case NODO_GENERADOR: declarar(a, s->generador.nombre, S_FUNCION, s->linea)->pendiente = 1; break;
            case NODO_CLASE: {
                Simbolo *x = declarar(a, s->clase.nombre, S_CLASE, s->linea);
                x->nodo = s;
                x->pendiente = 1;
                break;
            }
            case NODO_INTERFAZ: declarar(a, s->interfaz.nombre, S_INTERFAZ, s->linea)->pendiente = 1; break;
            case NODO_ENUMERACION: {
                Simbolo *x = declarar(a, s->enumeracion.nombre, S_ENUM, s->linea);
                x->nodo = s;
                x->pendiente = 1;
                break;
            }
            case NODO_IMPORTAR: preparar_import(s, a); break;
            default: break;
        }
    }
}

static int termina(Nodo *s) {
    return s->tipo == NODO_DEVOLVER || s->tipo == NODO_LANZAR ||
           s->tipo == NODO_ROMPER || s->tipo == NODO_CONTINUAR;
}

static const char *palabra_de(Nodo *s) {
    switch (s->tipo) {
        case NODO_DEVOLVER: return "devolver";
        case NODO_LANZAR:   return "lanzar";
        case NODO_ROMPER:   return "romper";
        default:            return "continuar";
    }
}

static void sentencias(Nodo *b, Ambito *a) {
    Nodo *fin = NULL;
    for (int i = 0; i < b->bloque.cantidad; i++) {
        Nodo *s = b->bloque.sentencias[i];
        if (!s) continue;
        if (fin) {
            avisar(s->linea_ini ? s->linea_ini : s->linea,
                   "Esto no se ejecuta nunca: va detrás de un '%s'.", palabra_de(fin));
            fin = NULL;                        /* un aviso por trozo */
            ver(s, a);
            continue;
        }
        ver(s, a);
        if (termina(s)) fin = s;
    }
}

static void bloque(Nodo *b, Ambito *a, int local) {
    if (!b) return;
    if (b->tipo != NODO_BLOQUE) { ver(b, a); return; }
    Ambito *x = ambito_nuevo(a, local);
    preparar(b, x);
    sentencias(b, x);
    ambito_cerrar(x);
}

/* Una función, método o lambda: sus parámetros y su cuerpo */
static void funcion(Nodo *n, Parametro *ps, int np, Nodo *cuerpo, Ambito *a) {
    for (int i = 0; i < np; i++)
        if (ps[i].valor_defecto) ver(ps[i].valor_defecto, a);
    _prof++;
    Ambito *x = ambito_nuevo(a, 0);
    for (int i = 0; i < np; i++) {
        if (ps[i].tipo == TIPO_CLASE) usar_tipo(ps[i].tipo_clase, n->linea, a);
        Simbolo *s = declarar(x, ps[i].nombre, S_PARAM, n->linea);
        Tipo t = de_tipodato(ps[i].tipo, ps[i].tipo_clase);
        if (t.t != T_DESC) { s->tipado = 1; s->tipo = t; }
    }
    Nodo *fn_ant = _fn;
    _fn = n;
    bloque(cuerpo, x, 1);
    _fn = fn_ant;
    ambito_cerrar(x);
    _prof--;
}

static Tipo tipo_de_simbolo(Simbolo *s) {
    return s && (s->tipado || s->deducido) ? s->tipo : DESC;
}

/* El objeto de 'x.algo': si es una clase de este archivo cuyos miembros se
   conocen todos, la clase; si no, NULL */
static Nodo *clase_conocida(Tipo t, Nombres *miembros) {
    if (t.t != T_OBJ) return NULL;
    Nodo *c = clase_llamada(t.clase);
    if (!c || !miembros_de_clase(c, miembros, 0)) return NULL;
    return c;
}

/* x.nombre o x.nombre(): ¿lo tiene? */
static void comprobar_miembro(Tipo t, const char *nombre, int linea) {
    Nombres m = {0};
    Nodo *c = clase_conocida(t, &m);
    if (c && !tiene(&m, nombre) && !tiene(&_asignados, nombre))
        avisar(linea, "La clase '%s' no tiene '%s'.%s", t.clase, nombre, sugerencia(nombre, NULL, &m));
    vaciar(&m);
}

/* 'modulo.algo' / 'Enumeracion.Valor' / 'Clase.estatico': lo que se sabe de él */
static int miembro_de_nombre(Simbolo *s, const char *nombre, int linea) {
    if (!s) return 0;
    if (s->que == S_MODULO) {
        if (s->miembros && !tiene(s->miembros, nombre) && nombre[0] != '_')   /* _algo: interno */
            avisar(linea, "'%s' no tiene nada llamado '%s'.%s", s->nombre, nombre,
                   sugerencia(nombre, NULL, s->miembros));
        return 1;
    }
    if (s->que == S_ENUM && s->nodo) {
        Nombres m = {0};
        for (int i = 0; i < s->nodo->enumeracion.num_valores; i++) meter(&m, s->nodo->enumeracion.valores[i]);
        if (!tiene(&m, nombre))
            avisar(linea, "La enumeración '%s' no tiene '%s'.%s", s->nombre, nombre, sugerencia(nombre, NULL, &m));
        vaciar(&m);
        return 1;
    }
    if (s->que == S_CLASE && s->nodo) {
        Nombres m = {0};
        if (miembros_de_clase(s->nodo, &m, 0) && !tiene(&m, nombre))
            avisar(linea, "La clase '%s' no tiene '%s'.%s", s->nombre, nombre, sugerencia(nombre, NULL, &m));
        vaciar(&m);
        return 1;
    }
    return 0;
}

static Tipo binario(Nodo *n, Ambito *a) {
    const char *op = n->binario.operador;
    if (!strcmp(op, "es")) {
        ver(n->binario.izquierda, a);
        usar_tipo(n->binario.derecha->identificador, n->linea, a);
        return tipo(T_LOG);
    }
    Tipo l = ver(n->binario.izquierda, a), r = ver(n->binario.derecha, a);
    if (!strcmp(op, "??")) return l.t == T_NULO ? r : (l.t == r.t && l.t != T_OBJ ? l : DESC);
    if (!strcmp(op, "y") || !strcmp(op, "o") || op[0] == '<' || op[0] == '>' ||
        !strcmp(op, "==") || !strcmp(op, "!=")) return tipo(T_LOG);
    if (!strcmp(op, "+")) {
        if (l.t == T_TXT || r.t == T_TXT) return tipo(T_TXT);
        if (l.t == T_NUM && r.t == T_NUM) return tipo(T_NUM);
        if (l.t == T_LIS && r.t == T_LIS) return tipo(T_LIS);
        return DESC;
    }
    if (!strcmp(op, "*")) {
        if (l.t == T_NUM && r.t == T_NUM) return tipo(T_NUM);
        if ((l.t == T_TXT && r.t == T_NUM) || (l.t == T_NUM && r.t == T_TXT)) return tipo(T_TXT);
        if ((l.t == T_LIS && r.t == T_NUM) || (l.t == T_NUM && r.t == T_LIS)) return tipo(T_LIS);
        return DESC;
    }
    if (l.t == T_NUM && r.t == T_NUM) return tipo(T_NUM);
    return DESC;
}

static Tipo ver(Nodo *n, Ambito *a) {
    if (!n) return DESC;
    switch (n->tipo) {
        case NODO_NUMERO:   return tipo(T_NUM);
        case NODO_TEXTO:    return tipo(T_TXT);
        case NODO_BOOLEANO: return tipo(T_LOG);
        case NODO_NULO:     return tipo(T_NULO);
        case NODO_PLANTILLA:
            for (int i = 0; i < n->lista.cantidad; i++) ver(n->lista.elementos[i], a);
            return tipo(T_TXT);
        case NODO_IDENTIFICADOR: {
            Simbolo *s = usar(a, n->identificador, n->linea, 1);
            return tipo_de_simbolo(s);
        }
        case NODO_BINARIO: return binario(n, a);
        case NODO_UNARIO: {
            Tipo t = ver(n->unario.operando, a);
            if (!strcmp(n->unario.operador, "-")) return t.t == T_NUM ? t : DESC;
            return tipo(T_LOG);
        }
        case NODO_DECLARACION: {
            Tipo t = ver(n->declaracion.valor, a);
            definido(a, n->declaracion.nombre);
            Simbolo *s = buscar(a, n->declaracion.nombre);
            if (n->declaracion.tipado) {
                Tipo dec = de_tipodato(n->declaracion.tipo, n->declaracion.tipo_clase);
                if (n->declaracion.tipo == TIPO_CLASE) usar_tipo(n->declaracion.tipo_clase, n->linea, a);
                if (n->declaracion.valor && !compatible(dec, t))
                    avisar(n->linea, "'%s' es %s, y aquí le das %s.",
                           n->declaracion.nombre, nombre_tipo(dec), describir(t));
            } else if (s && (t.t == T_OBJ || n->declaracion.constante)) {
                s->tipo = t;            /* sea p = Punto(): p es un Punto (mientras no cambie) */
                s->deducido = 1;
            }
            return DESC;
        }
        case NODO_DESESTRUCTURAR:
            ver(n->desestructurar.valor, a);
            if (n->desestructurar.declarar)
                for (int i = 0; i < n->desestructurar.num_nombres; i++) definido(a, n->desestructurar.nombres[i]);
            if (!n->desestructurar.declarar)
                for (int i = 0; i < n->desestructurar.num_nombres; i++)
                    usar(a, n->desestructurar.nombres[i], n->linea, 0);
            return DESC;
        case NODO_ASIGNACION: {
            int compuesta = n->asignacion.op[0] != '\0';
            Simbolo *s = usar(a, n->asignacion.nombre, n->linea, compuesta);
            Tipo t = ver(n->asignacion.valor, a);
            if (!s) return DESC;
            if (s->que == S_FIJO)
                avisar(n->linea, "'%s' es fijo: no se le puede dar otro valor.", s->nombre);
            else if (s->que == S_FUNCION || s->que == S_CLASE)
                avisar(n->linea, "'%s' es una %s: no se le puede dar un valor.", s->nombre,
                       s->que == S_FUNCION ? "función" : "clase");
            else if (s->tipado && !compuesta && !compatible(s->tipo, t))
                avisar(n->linea, "'%s' es %s, y aquí le das %s.", s->nombre, nombre_tipo(s->tipo), describir(t));
            else if (s->deducido && !s->tipado && (compuesta || t.t != s->tipo.t ||
                     (t.t == T_OBJ && strcmp(t.clase, s->tipo.clase) != 0))) {
                s->tipo = DESC;         /* ya puede ser otra cosa */
                s->deducido = 0;
            }
            return DESC;
        }
        case NODO_INCREMENTO:
            usar(a, n->incremento.nombre, n->linea, 1);
            return DESC;
        case NODO_ESCRIBIR: ver(n->escribir, a); return DESC;
        case NODO_BLOQUE:   bloque(n, a, 1); return DESC;
        case NODO_SI:
            ver(n->si.condicion, a);
            bloque(n->si.entonces, a, 1);
            for (int i = 0; i < n->si.cantidad_sino_si; i++) {
                ver(n->si.condiciones_sino_si[i], a);
                bloque(n->si.bloques_sino_si[i], a, 1);
            }
            bloque(n->si.sino, a, 1);
            return DESC;
        case NODO_SI_EXPR: {
            ver(n->si.condicion, a);
            Tipo t1 = ver(n->si.entonces, a), t2 = ver(n->si.sino, a);
            return t1.t == t2.t && t1.t != T_OBJ ? t1 : DESC;
        }
        case NODO_MIENTRAS: ver(n->mientras.condicion, a); bloque(n->mientras.cuerpo, a, 1); return DESC;
        case NODO_HACER:    bloque(n->hacer.cuerpo, a, 1); ver(n->hacer.condicion, a); return DESC;
        case NODO_PARA: {
            Ambito *x = ambito_nuevo(a, 0);
            ver(n->para.val_inicio, a);
            Simbolo *s = buscar(a, n->para.var_inicio);
            if (s) s->usos++;
            else declarar(x, n->para.var_inicio, S_BUCLE, n->linea);
            ver(n->para.condicion, x);
            if (n->para.paso) ver(n->para.paso, x);
            else usar(x, n->para.var_incremento, n->linea, 1);
            bloque(n->para.cuerpo, x, 1);
            ambito_cerrar(x);
            return DESC;
        }
        case NODO_PARA_CADA: {
            ver(n->para_cada.coleccion, a);
            if (n->para_cada.tipo == TIPO_CLASE) usar_tipo(n->para_cada.tipo_clase, n->linea, a);
            Ambito *x = ambito_nuevo(a, 0);
            Simbolo *s = declarar(x, n->para_cada.variable, S_BUCLE, n->linea);
            Tipo t = de_tipodato(n->para_cada.tipo, n->para_cada.tipo_clase);
            if (t.t != T_DESC) { s->tipado = 1; s->tipo = t; }
            bloque(n->para_cada.cuerpo, x, 1);
            ambito_cerrar(x);
            return DESC;
        }
        case NODO_FUNCION:
            definido(a, n->funcion.nombre);
            if (n->funcion.tipo_retorno == TIPO_CLASE) usar_tipo(n->funcion.tipo_retorno_clase, n->linea, a);
            funcion(n, n->funcion.parametros, n->funcion.num_parametros, n->funcion.cuerpo, a);
            return DESC;
        case NODO_GENERADOR:
            definido(a, n->generador.nombre);
            funcion(n, n->generador.parametros, n->generador.num_parametros, n->generador.cuerpo, a);
            return DESC;
        case NODO_LAMBDA:
            if (n->lambda.tipo_retorno == TIPO_CLASE) usar_tipo(n->lambda.tipo_retorno_clase, n->linea, a);
            funcion(n, n->lambda.parametros, n->lambda.num_parametros, n->lambda.cuerpo, a);
            return tipo(T_FUN);
        case NODO_LLAMADA: {
            Simbolo *s = n->llamada.funcion ? NULL : usar(a, n->llamada.nombre, n->linea, 1);
            if (n->llamada.funcion) ver(n->llamada.funcion, a);
            Tipo *t = ver_args(n->llamada.argumentos, n->llamada.num_argumentos, a);
            Tipo r = DESC;
            if (s && s->que == S_FUNCION && s->nodo) {
                Nodo *f = s->nodo;
                comprobar_args(f->funcion.parametros, f->funcion.num_parametros, f->funcion.nombre,
                               n->llamada.argumentos, t, n->llamada.num_argumentos, n->linea);
                if (!f->funcion.es_tarea) r = de_tipodato(f->funcion.tipo_retorno, f->funcion.tipo_retorno_clase);
            }
            free(t);
            return r;
        }
        case NODO_DEVOLVER: {
            Tipo t = ver(n->devolver, a);
            Tipo dec = DESC;
            const char *quien = "Esta función";
            if (_fn && _fn->tipo == NODO_FUNCION && !_fn->funcion.es_tarea) {
                dec = de_tipodato(_fn->funcion.tipo_retorno, _fn->funcion.tipo_retorno_clase);
                quien = _fn->funcion.nombre;
            } else if (_fn && _fn->tipo == NODO_LAMBDA && !_fn->lambda.es_tarea)
                dec = de_tipodato(_fn->lambda.tipo_retorno, _fn->lambda.tipo_retorno_clase);
            if (!compatible(dec, t)) {
                if (!strcmp(quien, "Esta función"))
                    avisar(n->linea, "Esta función devuelve %s, y aquí devuelve %s.", nombre_tipo(dec), describir(t));
                else
                    avisar(n->linea, "'%s' devuelve %s, y aquí devuelve %s.", quien, nombre_tipo(dec), describir(t));
            }
            return DESC;
        }
        case NODO_LISTA:
            for (int i = 0; i < n->lista.cantidad; i++) ver(n->lista.elementos[i], a);
            return tipo(T_LIS);
        case NODO_DICCIONARIO:
            for (int i = 0; i < n->diccionario.cantidad; i++) ver(n->diccionario.valores[i], a);
            return tipo(T_DIC);
        case NODO_INDICE:
            ver(n->acceso_indice.objeto, a);
            ver(n->acceso_indice.indice, a);
            return DESC;
        case NODO_METODO: {
            Nodo *o = n->metodo.objeto;
            Tipo t = DESC;
            Simbolo *s = NULL;
            if (o->tipo == NODO_IDENTIFICADOR) {
                s = usar(a, o->identificador, o->linea, 1);
                if (!miembro_de_nombre(s, n->metodo.metodo, n->linea)) t = tipo_de_simbolo(s);
            } else t = ver(o, a);
            Tipo *ta = ver_args(n->metodo.argumentos, n->metodo.num_argumentos, a);
            Tipo r = DESC;
            Nombres m = {0};
            Nodo *c = clase_conocida(t, &m);
            if (c) {
                Nodo *f = metodo_de(c, n->metodo.metodo, 0);
                if (f) {
                    comprobar_args(f->funcion.parametros, f->funcion.num_parametros, f->funcion.nombre,
                                   n->metodo.argumentos, ta, n->metodo.num_argumentos, n->linea);
                    if (!f->funcion.es_tarea) r = de_tipodato(f->funcion.tipo_retorno, f->funcion.tipo_retorno_clase);
                } else if (!tiene(&m, n->metodo.metodo) && !tiene(&_asignados, n->metodo.metodo))
                    avisar(n->linea, "La clase '%s' no tiene '%s'.%s", t.clase, n->metodo.metodo,
                           sugerencia(n->metodo.metodo, NULL, &m));
            }
            vaciar(&m);
            free(ta);
            return r;
        }
        case NODO_ACCESO: {
            Nodo *o = n->acceso.objeto;
            Tipo t;
            if (o->tipo == NODO_IDENTIFICADOR) {
                Simbolo *s = usar(a, o->identificador, o->linea, 1);
                if (miembro_de_nombre(s, n->acceso.campo, n->linea)) return DESC;
                t = tipo_de_simbolo(s);
            } else t = ver(o, a);
            comprobar_miembro(t, n->acceso.campo, n->linea);
            Nodo *c = t.t == T_OBJ ? clase_llamada(t.clase) : NULL;
            Nodo *cd = c ? campo_de(c, n->acceso.campo, 0) : NULL;
            return cd && cd->declaracion.tipado
                 ? de_tipodato(cd->declaracion.tipo, cd->declaracion.tipo_clase) : DESC;
        }
        case NODO_CLASE: {
            definido(a, n->clase.nombre);
            if (n->clase.padre) usar_tipo(n->clase.padre, n->linea, a);
            for (int i = 0; i < n->clase.num_interfaces; i++) usar_tipo(n->clase.interfaces[i], n->linea, a);
            Nodo *ant = _clase;
            _clase = n;
            _prof++;                     /* los valores iniciales se calculan al crear cada objeto */
            for (int i = 0; i < n->clase.num_campos; i++) {
                Nodo *cd = n->clase.campos[i];
                Tipo t = ver(cd->declaracion.valor, a);
                if (cd->declaracion.tipado) {
                    Tipo dec = de_tipodato(cd->declaracion.tipo, cd->declaracion.tipo_clase);
                    if (cd->declaracion.tipo == TIPO_CLASE) usar_tipo(cd->declaracion.tipo_clase, cd->linea, a);
                    if (cd->declaracion.valor && !compatible(dec, t))
                        avisar(cd->linea, "El campo '%s' es %s, y aquí le das %s.",
                               cd->declaracion.nombre, nombre_tipo(dec), describir(t));
                }
            }
            _prof--;
            for (int i = 0; i < n->clase.num_metodos; i++) ver(n->clase.metodos[i], a);
            _clase = ant;
            return DESC;
        }
        case NODO_INSTANCIA: {
            Simbolo *s = usar(a, n->instancia.clase, n->linea, 1);
            Tipo *ta = ver_args(n->instancia.argumentos, n->instancia.num_argumentos, a);
            if (s && s->que == S_CLASE && s->nodo) {
                Nombres m = {0};
                Nodo *f = metodo_de(s->nodo, "crear", 0);
                if (f)
                    comprobar_args(f->funcion.parametros, f->funcion.num_parametros, n->instancia.clase,
                                   n->instancia.argumentos, ta, n->instancia.num_argumentos, n->linea);
                else if (miembros_de_clase(s->nodo, &m, 0) && n->instancia.num_argumentos > 0)
                    avisar(n->linea, "'%s' no tiene 'crear', así que no recibe argumentos, y aquí le das %d.",
                           n->instancia.clase, n->instancia.num_argumentos);
                vaciar(&m);
            }
            free(ta);
            Tipo r = { T_OBJ, n->instancia.clase };
            return s && s->que == S_CLASE ? r : DESC;
        }
        case NODO_ESTO: {
            Tipo r = { T_OBJ, _clase ? _clase->clase.nombre : NULL };
            return _clase ? r : DESC;
        }
        case NODO_ASIGNACION_CAMPO: {
            Tipo t = ver(n->asignacion_campo.objeto, a);
            Tipo v = ver(n->asignacion_campo.valor, a);
            if (n->asignacion_campo.op[0]) comprobar_miembro(t, n->asignacion_campo.campo, n->linea);
            Nodo *c = t.t == T_OBJ ? clase_llamada(t.clase) : NULL;
            Nodo *cd = c ? campo_de(c, n->asignacion_campo.campo, 0) : NULL;
            if (cd && cd->declaracion.tipado && !n->asignacion_campo.op[0]) {
                Tipo dec = de_tipodato(cd->declaracion.tipo, cd->declaracion.tipo_clase);
                if (!compatible(dec, v))
                    avisar(n->linea, "El campo '%s' es %s, y aquí le das %s.",
                           n->asignacion_campo.campo, nombre_tipo(dec), describir(v));
            }
            return DESC;
        }
        case NODO_ASIGNACION_INDICE:
            ver(n->asignacion_indice.objeto, a);
            ver(n->asignacion_indice.indice, a);
            ver(n->asignacion_indice.valor, a);
            return DESC;
        case NODO_PADRE:
            free(ver_args(n->padre.argumentos, n->padre.num_argumentos, a));
            return DESC;
        case NODO_INTENTAR:
            bloque(n->intentar.cuerpo, a, 1);
            for (int i = 0; i < n->intentar.num_capturas; i++) {
                Ambito *x = ambito_nuevo(a, 0);
                if (n->intentar.vars_captura[i]) declarar(x, n->intentar.vars_captura[i], S_BUCLE, n->linea);
                bloque(n->intentar.cuerpos_captura[i], x, 1);
                ambito_cerrar(x);
            }
            bloque(n->intentar.finalmente, a, 1);
            return DESC;
        case NODO_LANZAR:   ver(n->lanzar, a); return DESC;
        case NODO_PRODUCIR: ver(n->producir, a); return DESC;
        case NODO_ELEGIR:
            ver(n->elegir.sujeto, a);
            for (int i = 0; i < n->elegir.num_casos; i++) {
                ver(n->elegir.valores[i], a);
                bloque(n->elegir.cuerpos[i], a, 1);
            }
            bloque(n->elegir.otro, a, 1);
            return DESC;
        case NODO_ENUMERACION:
            definido(a, n->enumeracion.nombre);
            for (int i = 0; n->enumeracion.expresiones && i < n->enumeracion.num_valores; i++)
                ver(n->enumeracion.expresiones[i], a);
            return DESC;
        case NODO_INTERFAZ:
            definido(a, n->interfaz.nombre);
            for (int i = 0; n->interfaz.funciones && i < n->interfaz.num_metodos; i++)
                ver(n->interfaz.funciones[i], a);
            return DESC;
        default:
            return DESC;
    }
}

/* ── Antes de recorrer: las clases, las interfaces y los campos que se crean
   con 'x.campo = ...' en cualquier sitio ── */
static void apuntar_todo(Nodo *n, int hondo);

static void apuntar_lista(Nodo **v, int k, int hondo) {
    for (int i = 0; v && i < k; i++) apuntar_todo(v[i], hondo);
}

static void apuntar_params(Parametro *ps, int np, int hondo) {
    for (int i = 0; i < np; i++) apuntar_todo(ps[i].valor_defecto, hondo);
}

static void apuntar_todo(Nodo *n, int hondo) {
    if (!n || hondo > 400) return;
    hondo++;
    switch (n->tipo) {
        case NODO_CLASE:
            apuntar_nodo(&_clases, &_nclases, n);
            apuntar_lista(n->clase.campos, n->clase.num_campos, hondo);
            apuntar_lista(n->clase.metodos, n->clase.num_metodos, hondo);
            break;
        case NODO_INTERFAZ:
            apuntar_nodo(&_ifaces, &_nifaces, n);
            if (n->interfaz.funciones) apuntar_lista(n->interfaz.funciones, n->interfaz.num_metodos, hondo);
            break;
        case NODO_ASIGNACION_CAMPO:
            if (!n->asignacion_campo.op[0])        /* 'x.c += 1' no crea 'c': lo lee */
                meter(&_asignados, n->asignacion_campo.campo);
            apuntar_todo(n->asignacion_campo.objeto, hondo);
            apuntar_todo(n->asignacion_campo.valor, hondo);
            break;
        case NODO_PLANTILLA: case NODO_LISTA: apuntar_lista(n->lista.elementos, n->lista.cantidad, hondo); break;
        case NODO_BINARIO: apuntar_todo(n->binario.izquierda, hondo); apuntar_todo(n->binario.derecha, hondo); break;
        case NODO_UNARIO: apuntar_todo(n->unario.operando, hondo); break;
        case NODO_DECLARACION: apuntar_todo(n->declaracion.valor, hondo); break;
        case NODO_DESESTRUCTURAR: apuntar_todo(n->desestructurar.valor, hondo); break;
        case NODO_ASIGNACION: apuntar_todo(n->asignacion.valor, hondo); break;
        case NODO_ESCRIBIR: apuntar_todo(n->escribir, hondo); break;
        case NODO_BLOQUE: apuntar_lista(n->bloque.sentencias, n->bloque.cantidad, hondo); break;
        case NODO_SI: case NODO_SI_EXPR:
            apuntar_todo(n->si.condicion, hondo); apuntar_todo(n->si.entonces, hondo);
            apuntar_lista(n->si.condiciones_sino_si, n->si.cantidad_sino_si, hondo);
            apuntar_lista(n->si.bloques_sino_si, n->si.cantidad_sino_si, hondo);
            apuntar_todo(n->si.sino, hondo);
            break;
        case NODO_MIENTRAS: apuntar_todo(n->mientras.condicion, hondo); apuntar_todo(n->mientras.cuerpo, hondo); break;
        case NODO_HACER: apuntar_todo(n->hacer.cuerpo, hondo); apuntar_todo(n->hacer.condicion, hondo); break;
        case NODO_PARA:
            apuntar_todo(n->para.val_inicio, hondo); apuntar_todo(n->para.condicion, hondo);
            apuntar_todo(n->para.paso, hondo); apuntar_todo(n->para.cuerpo, hondo);
            break;
        case NODO_PARA_CADA: apuntar_todo(n->para_cada.coleccion, hondo); apuntar_todo(n->para_cada.cuerpo, hondo); break;
        case NODO_FUNCION:
            apuntar_params(n->funcion.parametros, n->funcion.num_parametros, hondo);
            apuntar_todo(n->funcion.cuerpo, hondo);
            break;
        case NODO_GENERADOR:
            apuntar_params(n->generador.parametros, n->generador.num_parametros, hondo);
            apuntar_todo(n->generador.cuerpo, hondo);
            break;
        case NODO_LAMBDA:
            apuntar_params(n->lambda.parametros, n->lambda.num_parametros, hondo);
            apuntar_todo(n->lambda.cuerpo, hondo);
            break;
        case NODO_LLAMADA:
            apuntar_todo(n->llamada.funcion, hondo);
            apuntar_lista(n->llamada.argumentos, n->llamada.num_argumentos, hondo);
            break;
        case NODO_DEVOLVER: apuntar_todo(n->devolver, hondo); break;
        case NODO_DICCIONARIO: apuntar_lista(n->diccionario.valores, n->diccionario.cantidad, hondo); break;
        case NODO_INDICE: apuntar_todo(n->acceso_indice.objeto, hondo); apuntar_todo(n->acceso_indice.indice, hondo); break;
        case NODO_METODO:
            apuntar_todo(n->metodo.objeto, hondo);
            apuntar_lista(n->metodo.argumentos, n->metodo.num_argumentos, hondo);
            break;
        case NODO_ACCESO: apuntar_todo(n->acceso.objeto, hondo); break;
        case NODO_INSTANCIA: apuntar_lista(n->instancia.argumentos, n->instancia.num_argumentos, hondo); break;
        case NODO_ASIGNACION_INDICE:
            apuntar_todo(n->asignacion_indice.objeto, hondo); apuntar_todo(n->asignacion_indice.indice, hondo);
            apuntar_todo(n->asignacion_indice.valor, hondo);
            break;
        case NODO_PADRE: apuntar_lista(n->padre.argumentos, n->padre.num_argumentos, hondo); break;
        case NODO_INTENTAR:
            apuntar_todo(n->intentar.cuerpo, hondo);
            apuntar_lista(n->intentar.cuerpos_captura, n->intentar.num_capturas, hondo);
            apuntar_todo(n->intentar.finalmente, hondo);
            break;
        case NODO_LANZAR: apuntar_todo(n->lanzar, hondo); break;
        case NODO_PRODUCIR: apuntar_todo(n->producir, hondo); break;
        case NODO_ELEGIR:
            apuntar_todo(n->elegir.sujeto, hondo);
            apuntar_lista(n->elegir.valores, n->elegir.num_casos, hondo);
            apuntar_lista(n->elegir.cuerpos, n->elegir.num_casos, hondo);
            apuntar_todo(n->elegir.otro, hondo);
            break;
        default: break;
    }
}

void revisar(Nodo *programa, const char *ruta, void (*avisar_fn)(int linea, const char *mensaje)) {
    if (!programa || programa->tipo != NODO_BLOQUE) return;
    _avisar = avisar_fn;
    _ruta = ruta;
    _incompleto = 0;
    _fn = _clase = NULL;
    _prof = 0;
    apuntar_todo(programa, 0);

    /* Lo que trae Lince sin importar nada: escribir, rango, a_texto... */
    Ambito *base = ambito_nuevo(NULL, 0);
    Interprete *in = interprete_crear();
    for (int i = 0; i < in->global->cantidad; i++) declarar(base, in->global->vars[i].nombre, S_OTRO, 0);

    Ambito *global = ambito_nuevo(base, 0);
    preparar(programa, global);
    sentencias(programa, global);
    ambito_cerrar(global);
    ambito_cerrar(base);

    free(_clases); _clases = NULL; _nclases = 0;
    free(_ifaces); _ifaces = NULL; _nifaces = 0;
    vaciar(&_asignados);
}
