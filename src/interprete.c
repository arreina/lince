/*
 * 🐆 Lince — interprete.c
 * Recorre el AST y ejecuta el programa.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include "interprete.h"
#include "modulos.h"
#include "paquetes.h"

/* ─────────────────────────────────────────
   SEÑAL DE RETORNO
   Usamos una variable global para propagar
   el valor de 'devolver' sin excepciones.
───────────────────────────────────────── */
static int    hay_retorno    = 0;
static Valor *valor_retorno  = NULL;
static Valor *esto_actual    = NULL;
static ClaseLince *clase_actual = NULL;
static int    profundidad    = 0;
#define MAX_PROFUNDIDAD 500
#define MAX_GENERADOR   10000  /* límite de seguridad para generadores */
static int    hay_producir   = 0;
static Valor *generador_actual = NULL; /* lista acumulando valores producidos */
int    hay_error     = 0;
Valor *valor_error   = NULL;

/* ─────────────────────────────────────────
   VALORES — creación
───────────────────────────────────────── */
static void entorno_destruir(Entorno *e); /* declaración anticipada */
Valor *valor_numero(double n) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo   = VAL_NUMERO;
    v->refs   = 1; v->es_modulo = 0;
    v->numero = n;
    return v;
}

Valor *valor_texto(const char *s) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo  = VAL_TEXTO;
    v->refs  = 1; v->es_modulo = 0;
    v->texto = strdup(s);
    return v;
}

Valor *valor_booleano(int b) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo     = VAL_BOOLEANO;
    v->refs     = 1; v->es_modulo = 0;
    v->booleano = b;
    return v;
}

Valor *valor_nulo(void) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo = VAL_NULO;
    v->refs = 1; v->es_modulo = 0;
    return v;
}

static Valor *valor_funcion(FuncionLince *f) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo    = VAL_FUNCION;
    v->refs    = 1; v->es_modulo = 0;
    v->funcion = f;
    return v;
}

static Valor *valor_clase(ClaseLince *c) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo  = VAL_CLASE;
    v->refs  = 1; v->es_modulo = 0;
    v->clase = c;
    return v;
}

static Valor *valor_objeto(ObjetoLince *o) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo   = VAL_OBJETO;
    v->refs   = 1; v->es_modulo = 0;
    v->objeto = o;
    return v;
}

Valor *valor_crear_error(const char *tipo, const char *mensaje, int linea) {
    ErrorLince *e = malloc(sizeof(ErrorLince));
    e->tipo    = strdup(tipo);
    e->mensaje = strdup(mensaje);
    e->linea   = linea;
    Valor *v = malloc(sizeof(Valor));
    v->tipo  = VAL_ERROR;
    v->refs  = 1; v->es_modulo = 0;
    v->error = e;
    return v;
}

/* Incrementa referencias */
static Valor *valor_retener(Valor *v) {
    if (v) v->refs++;
    return v;
}

/* Decrementa referencias y destruye si llega a 0 */
static void valor_liberar(Valor *v) {
    if (!v) return;
    v->refs--;
    if (v->refs > 0) return;

    if (v->tipo == VAL_TEXTO) free(v->texto);
    if (v->tipo == VAL_LISTA) {
        for (int i = 0; i < v->lista.cantidad; i++)
            valor_liberar(v->lista.elementos[i]);
        free(v->lista.elementos);
    }
    if (v->tipo == VAL_DICCIONARIO) {
        for (int i = 0; i < v->diccionario.cantidad; i++) {
            free(v->diccionario.claves[i]);
            valor_liberar(v->diccionario.valores[i]);
        }
        free(v->diccionario.claves);
        free(v->diccionario.valores);
    }
    if (v->tipo == VAL_OBJETO) {
        for (int i = 0; i < v->objeto->num_campos; i++) {
            free(v->objeto->campos_nombres[i]);
            valor_liberar(v->objeto->campos_valores[i]);
        }
        free(v->objeto);
    }
    if (v->tipo == VAL_FUNCION && v->funcion) {
        /* Liberar la referencia al entorno capturado por el closure */
        if (v->funcion->entorno_closure)
            entorno_destruir(v->funcion->entorno_closure);
        free(v->funcion->nombre);
        free(v->funcion);
    }
    if (v->tipo == VAL_GENERADOR) {
        for (int i = 0; i < v->generador.cantidad; i++)
            valor_liberar(v->generador.elementos[i]);
        free(v->generador.elementos);
    }
    if (v->tipo == VAL_ERROR) {
        free(v->error->tipo);
        free(v->error->mensaje);
        free(v->error);
    }
    free(v);
}

/* Alias para claridad — función real para que sea linkeable */
void valor_destruir(Valor *v) { valor_liberar(v); }

Valor *valor_lista_crear(void) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo             = VAL_LISTA;
    v->refs             = 1; v->es_modulo = 0;
    v->lista.capacidad  = 8;
    v->lista.cantidad   = 0;
    v->lista.elementos  = malloc(sizeof(Valor*) * 8);
    return v;
}

void lista_agregar(Valor *lista, Valor *elem) {
    if (lista->lista.cantidad >= lista->lista.capacidad) {
        lista->lista.capacidad *= 2;
        lista->lista.elementos = realloc(lista->lista.elementos,
            sizeof(Valor*) * lista->lista.capacidad);
    }
    lista->lista.elementos[lista->lista.cantidad++] = elem;
}

#define DIC_CAP_INICIAL 16

Valor *valor_diccionario_crear(void) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo                  = VAL_DICCIONARIO;
    v->refs                  = 1; v->es_modulo = 0;
    v->es_modulo             = 0;
    v->diccionario.cantidad  = 0;
    v->diccionario.capacidad = DIC_CAP_INICIAL;
    v->diccionario.claves    = malloc(sizeof(char*)  * DIC_CAP_INICIAL);
    v->diccionario.valores   = malloc(sizeof(Valor*) * DIC_CAP_INICIAL);
    return v;
}

/* Asegura hueco para 'n' entradas, duplicando como hacen las listas.
   Antes el diccionario reservaba 64 huecos fijos y no crecía nunca, así
   que la clave 65 escribía fuera del array y corrompía el montón. */
void valor_diccionario_asegurar(Valor *dic, int n) {
    if (!dic || dic->tipo != VAL_DICCIONARIO) return;
    if (n <= dic->diccionario.capacidad) return;
    int cap = dic->diccionario.capacidad > 0
                ? dic->diccionario.capacidad : DIC_CAP_INICIAL;
    while (cap < n) cap *= 2;
    dic->diccionario.claves  = realloc(dic->diccionario.claves,
                                       sizeof(char*)  * cap);
    dic->diccionario.valores = realloc(dic->diccionario.valores,
                                       sizeof(Valor*) * cap);
    dic->diccionario.capacidad = cap;
}

/* Añade una entrada al final, creciendo si hace falta. No comprueba si
   la clave ya existe: para eso están los sitios que buscan primero. */
void valor_diccionario_agregar(Valor *dic, const char *clave, Valor *valor) {
    if (!dic || dic->tipo != VAL_DICCIONARIO) return;
    valor_diccionario_asegurar(dic, dic->diccionario.cantidad + 1);
    int i = dic->diccionario.cantidad;
    dic->diccionario.claves[i]  = strdup(clave);
    dic->diccionario.valores[i] = valor;
    dic->diccionario.cantidad++;
}

static Valor *valor_generador_crear(void) {
    Valor *v = malloc(sizeof(Valor));
    v->tipo              = VAL_GENERADOR;
    v->refs              = 1;
    v->es_modulo         = 0;
    v->generador.cantidad  = 0;
    v->generador.capacidad = 8;
    v->generador.elementos = malloc(sizeof(Valor*) * 8);
    return v;
}

static void generador_agregar(Valor *gen, Valor *elem) {
    if (gen->generador.cantidad >= gen->generador.capacidad) {
        gen->generador.capacidad *= 2;
        gen->generador.elementos = realloc(gen->generador.elementos,
            sizeof(Valor*) * gen->generador.capacidad);
    }
    gen->generador.elementos[gen->generador.cantidad++] = elem;
}

static Valor *valor_copiar(Valor *v) {
    if (!v) return valor_nulo();
    switch (v->tipo) {
        case VAL_NUMERO:      return valor_numero(v->numero);
        case VAL_TEXTO:       return valor_texto(v->texto);
        case VAL_BOOLEANO:    return valor_booleano(v->booleano);
        case VAL_NULO:        return valor_nulo();
        case VAL_FUNCION:
        case VAL_LISTA:
        case VAL_DICCIONARIO:
        case VAL_CLASE:
        case VAL_OBJETO:
            return valor_retener(v);
        default:              return valor_nulo();
    }
}

/* Convierte un valor a texto para escribir() */
static char *valor_a_texto(Valor *v) {
    char buf[4096];
    if (!v || v->tipo == VAL_NULO) return strdup("nulo");
    switch (v->tipo) {
        case VAL_NUMERO:
            if (v->numero == (long long)v->numero)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->numero);
            else
                snprintf(buf, sizeof(buf), "%g", v->numero);
            return strdup(buf);
        case VAL_TEXTO:
            return strdup(v->texto);
        case VAL_BOOLEANO:
            return strdup(v->booleano ? "verdadero" : "falso");
        case VAL_FUNCION:
            snprintf(buf, sizeof(buf), "<funcion %s>", v->funcion->nombre);
            return strdup(buf);
        case VAL_CLASE:
            snprintf(buf, sizeof(buf), "<clase %s>", v->clase->nombre);
            return strdup(buf);
        case VAL_OBJETO: {
            char tmp[4096];
            snprintf(tmp, sizeof(tmp), "%s{", v->objeto->clase->nombre);
            for (int i = 0; i < v->objeto->num_campos; i++) {
                strncat(tmp, v->objeto->campos_nombres[i], sizeof(tmp)-strlen(tmp)-1);
                strncat(tmp, ": ", sizeof(tmp)-strlen(tmp)-1);
                char *s = valor_a_texto(v->objeto->campos_valores[i]);
                strncat(tmp, s, sizeof(tmp)-strlen(tmp)-1);
                free(s);
                if (i < v->objeto->num_campos - 1)
                    strncat(tmp, ", ", sizeof(tmp)-strlen(tmp)-1);
            }
            strncat(tmp, "}", sizeof(tmp)-strlen(tmp)-1);
            return strdup(tmp);
        }
        case VAL_LISTA: {
            char tmp[4096] = "[";
            for (int i = 0; i < v->lista.cantidad; i++) {
                char *s = valor_a_texto(v->lista.elementos[i]);
                if (v->lista.elementos[i]->tipo == VAL_TEXTO) {
                    strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                    strncat(tmp, s, sizeof(tmp) - strlen(tmp) - 1);
                    strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                } else {
                    strncat(tmp, s, sizeof(tmp) - strlen(tmp) - 1);
                }
                free(s);
                if (i < v->lista.cantidad - 1)
                    strncat(tmp, ", ", sizeof(tmp) - strlen(tmp) - 1);
            }
            strncat(tmp, "]", sizeof(tmp) - strlen(tmp) - 1);
            return strdup(tmp);
        }
        case VAL_DICCIONARIO: {
            char tmp[4096] = "{";
            for (int i = 0; i < v->diccionario.cantidad; i++) {
                strncat(tmp, "\"", sizeof(tmp) - strlen(tmp) - 1);
                strncat(tmp, v->diccionario.claves[i], sizeof(tmp) - strlen(tmp) - 1);
                strncat(tmp, "\": ", sizeof(tmp) - strlen(tmp) - 1);
                char *s = valor_a_texto(v->diccionario.valores[i]);
                strncat(tmp, s, sizeof(tmp) - strlen(tmp) - 1);
                free(s);
                if (i < v->diccionario.cantidad - 1)
                    strncat(tmp, ", ", sizeof(tmp) - strlen(tmp) - 1);
            }
            strncat(tmp, "}", sizeof(tmp) - strlen(tmp) - 1);
            return strdup(tmp);
        }
        default:
            return strdup("nulo");
    }
}

/* Verdad de un valor */
static int es_verdadero(Valor *v) {
    if (!v || v->tipo == VAL_NULO)     return 0;
    if (v->tipo == VAL_BOOLEANO)       return v->booleano;
    if (v->tipo == VAL_NUMERO)         return v->numero != 0;
    if (v->tipo == VAL_TEXTO)          return strlen(v->texto) > 0;
    return 1;
}

/* ─────────────────────────────────────────
   ENTORNO
───────────────────────────────────────── */
/* Lista de entornos reutilizables. Un Entorno ocupa ~6 KB y se crea y
   destruye en cada llamada, cada bloque y cada iteración de bucle, así
   que reciclarlos evita casi todo ese tráfico de malloc/free. Se enlazan
   por el campo 'padre', que está libre mientras el entorno no se usa. */
static Entorno *_ent_libres     = NULL;
static int      _ent_libres_num = 0;
#define MAX_ENT_LIBRES 256   /* tope de retención: ~1,5 MB */

static Entorno *entorno_crear(Entorno *padre) {
    /* malloc, no calloc: 'vars' ocupa casi todo el struct (MAX_VARS
       entradas) y cada entrada se escribe por completo en
       entorno_definir antes de leerse; nadie mira más allá de
       'cantidad'. Poner a cero ~6 KB en cada creación dominaba el
       tiempo de ejecución. */
    Entorno *e;
    if (_ent_libres) {
        e = _ent_libres;
        _ent_libres = e->padre;
        _ent_libres_num--;
    } else {
        e = malloc(sizeof(Entorno));
    }
    e->padre    = padre;
    e->cantidad = 0;
    e->refs     = 1;
    if (padre) padre->refs++;
    return e;
}

void entorno_definir(Entorno *e, const char *nombre, Valor *valor, int constante) {
    if (e->cantidad >= MAX_VARS) {
        fprintf(stderr, "\n❌ Error interno: demasiadas variables en el mismo ámbito.\n\n");
        exit(1);
    }
    e->vars[e->cantidad].nombre    = strdup(nombre);
    e->vars[e->cantidad].valor     = valor;
    e->vars[e->cantidad].constante = constante;
    e->cantidad++;
}

static void entorno_liberar(Entorno *e);

static void entorno_destruir(Entorno *e) {
    if (!e) return;
    e->refs--;
    if (e->refs > 0) return;
    for (int i = 0; i < e->cantidad; i++) {
        free(e->vars[i].nombre);
        valor_destruir(e->vars[i].valor);
    }
    Entorno *padre = e->padre;
    if (_ent_libres_num < MAX_ENT_LIBRES) {
        e->padre = _ent_libres;
        _ent_libres = e;
        _ent_libres_num++;
    } else {
        free(e);
    }
    if (padre) entorno_destruir(padre);
}

static void entorno_liberar(Entorno *e) { entorno_destruir(e); }

static Valor *entorno_obtener(Entorno *e, const char *nombre, int linea) {
    for (int i = 0; i < e->cantidad; i++) {
        if (strcmp(e->vars[i].nombre, nombre) == 0)
            return valor_copiar(e->vars[i].valor);
    }
    if (e->padre) return entorno_obtener(e->padre, nombre, linea);
    /* Lanzar como error capturable si hay línea */
    char msg[256];
    if (linea > 0) {
        snprintf(msg, sizeof(msg),
            "La variable '%s' no está definida.", nombre);
        valor_error = valor_crear_error("Error", msg, linea);
        hay_error = 1;
        return valor_nulo();
    }
    fprintf(stderr,
        "\n❌ Error en línea %d:\n"
        "   Estás usando la variable '%s', pero nunca la definiste.\n"
        "   ¿Querías escribir 'sea %s = ...' antes de usarla?\n\n",
        linea, nombre, nombre);
    exit(1);
}

static void entorno_asignar(Entorno *e, const char *nombre, Valor *nuevo, int linea) {
    for (int i = 0; i < e->cantidad; i++) {
        if (strcmp(e->vars[i].nombre, nombre) == 0) {
            if (e->vars[i].constante) {
                fprintf(stderr,
                    "\n❌ Error en línea %d:\n"
                    "   '%s' es una constante definida con 'fijo' y no puede cambiar.\n"
                    "   Si necesitas que cambie, usa 'sea' en lugar de 'fijo'.\n\n",
                    linea, nombre);
                exit(1);
            }
            valor_destruir(e->vars[i].valor);
            e->vars[i].valor = nuevo;
            return;
        }
    }
    if (e->padre) { entorno_asignar(e->padre, nombre, nuevo, linea); return; }
    fprintf(stderr,
        "\n❌ Error en línea %d:\n"
        "   Intentas asignar un valor a '%s', pero nunca la definiste.\n"
        "   ¿Querías escribir 'sea %s = ...' primero?\n\n",
        linea, nombre, nombre);
    exit(1);
}

/* ─────────────────────────────────────────
   EJECUCIÓN DE NODOS
───────────────────────────────────────── */
static Valor *ejecutar(Nodo *n, Entorno *e);

/* Busca un método en la clase y sus ancestros */
static FuncionLince *buscar_metodo(ClaseLince *cls, const char *nombre) {
    while (cls) {
        for (int i = 0; i < cls->num_metodos; i++) {
            if (strcmp(cls->metodos[i]->nombre, nombre) == 0)
                return cls->metodos[i];
        }
        cls = cls->padre;
    }
    return NULL;
}

static Valor *exec_binario(Nodo *n, Entorno *e) {
    Valor *izq = ejecutar(n->binario.izquierda, e);
    Valor *der = ejecutar(n->binario.derecha,   e);
    const char *op = n->binario.operador;
    Valor *resultado = NULL;

    /* Concatenación de texto */
    if (strcmp(op, "+") == 0 && (izq->tipo == VAL_TEXTO || der->tipo == VAL_TEXTO)) {
        char *si = valor_a_texto(izq);
        char *sd = valor_a_texto(der);
        char *buf = malloc(strlen(si) + strlen(sd) + 1);
        strcpy(buf, si); strcat(buf, sd);
        resultado = valor_texto(buf);
        free(si); free(sd); free(buf);
        valor_destruir(izq); valor_destruir(der);
        return resultado;
    }

    /* Operaciones numéricas */
    if (izq->tipo == VAL_NUMERO && der->tipo == VAL_NUMERO) {
        double a = izq->numero, b = der->numero;
        if (strcmp(op, "+")  == 0) resultado = valor_numero(a + b);
        else if (strcmp(op, "-")  == 0) resultado = valor_numero(a - b);
        else if (strcmp(op, "*")  == 0) resultado = valor_numero(a * b);
        else if (strcmp(op, "/")  == 0) {
            if (b == 0) {
                valor_error = valor_crear_error("ErrorMatematico", "División por cero.", 0);
                hay_error = 1;
                valor_destruir(izq);
                valor_destruir(der);
                return valor_nulo();
            }
            resultado = valor_numero(a / b);
        }
        else if (strcmp(op, "%")  == 0) resultado = valor_numero(fmod(a, b));
        else if (strcmp(op, "<")  == 0) resultado = valor_booleano(a < b);
        else if (strcmp(op, ">")  == 0) resultado = valor_booleano(a > b);
        else if (strcmp(op, "<=") == 0) resultado = valor_booleano(a <= b);
        else if (strcmp(op, ">=") == 0) resultado = valor_booleano(a >= b);
        else if (strcmp(op, "==") == 0) resultado = valor_booleano(a == b);
        else if (strcmp(op, "!=") == 0) resultado = valor_booleano(a != b);
    }

    /* Comparaciones entre texto */
    if (!resultado && izq->tipo == VAL_TEXTO && der->tipo == VAL_TEXTO) {
        int cmp = strcmp(izq->texto, der->texto);
        if (strcmp(op, "==") == 0) resultado = valor_booleano(cmp == 0);
        else if (strcmp(op, "!=") == 0) resultado = valor_booleano(cmp != 0);
    }

    /* Comparaciones entre lógicos. Sin esto caerían en el caso general
       de más abajo, que sólo sabe comparar nulos, y 'verdadero ==
       verdadero' daba falso. */
    if (!resultado && izq->tipo == VAL_BOOLEANO && der->tipo == VAL_BOOLEANO) {
        int a = izq->booleano ? 1 : 0;
        int b = der->booleano ? 1 : 0;
        if (strcmp(op, "==") == 0) resultado = valor_booleano(a == b);
        else if (strcmp(op, "!=") == 0) resultado = valor_booleano(a != b);
    }

    /* Lógica booleana */
    if (!resultado) {
        if (strcmp(op, "y") == 0)
            resultado = valor_booleano(es_verdadero(izq) && es_verdadero(der));
        else if (strcmp(op, "o") == 0)
            resultado = valor_booleano(es_verdadero(izq) || es_verdadero(der));
        else if (strcmp(op, "==") == 0)
            resultado = valor_booleano(izq->tipo == VAL_NULO && der->tipo == VAL_NULO);
        else if (strcmp(op, "!=") == 0)
            resultado = valor_booleano(!(izq->tipo == VAL_NULO && der->tipo == VAL_NULO));
    }

    valor_destruir(izq);
    valor_destruir(der);

    if (!resultado) {
        fprintf(stderr, "\n❌ Error: Operación '%s' no soportada entre esos tipos.\n\n", op);
        exit(1);
    }
    return resultado;
}

static Valor *ejecutar(Nodo *n, Entorno *e);

/* Valida que un valor coincide con el tipo declarado */
static void validar_tipo(Valor *v, TipoDato tipo, const char *contexto,
                          const char *nombre, int linea) {
    if (tipo == TIPO_CUALQUIERA || tipo == TIPO_CLASE) return;
    int ok = 1;
    switch (tipo) {
        case TIPO_NUMERO:      ok = (v->tipo == VAL_NUMERO);      break;
        case TIPO_TEXTO:       ok = (v->tipo == VAL_TEXTO);       break;
        case TIPO_LOGICO:      ok = (v->tipo == VAL_BOOLEANO);    break;
        case TIPO_LISTA:       ok = (v->tipo == VAL_LISTA);       break;
        case TIPO_DICCIONARIO: ok = (v->tipo == VAL_DICCIONARIO); break;
        case TIPO_NULO:        ok = (v->tipo == VAL_NULO);        break;
        default: break;
    }
    if (!ok) {
        const char *esperado[] = {
            "numero", "texto", "logico", "lista", "diccionario", "nulo", "clase", "cualquiera"
        };
        const char *recibido[] = {
            "numero", "texto", "logico", "nulo", "funcion",
            "lista", "diccionario", "clase", "objeto", "error"
        };
        char msg[256];
        snprintf(msg, sizeof(msg),
            "Error de tipo en %s '%s': se esperaba '%s' pero se recibió '%s'.",
            contexto, nombre,
            tipo < 8 ? esperado[tipo] : "?",
            v->tipo < 10 ? recibido[v->tipo] : "?");
        valor_error = valor_crear_error("ErrorTipo", msg, linea);
        hay_error = 1;
    }
}

static Valor *ejecutar(Nodo *n, Entorno *e) {
    if (!n || hay_retorno || hay_error) return valor_nulo();

    switch (n->tipo) {

        case NODO_NUMERO:
            return valor_numero(n->numero);

        case NODO_TEXTO:
            return valor_texto(n->texto);

        case NODO_BOOLEANO:
            return valor_booleano(n->booleano);

        case NODO_NULO:
            return valor_nulo();

        case NODO_IDENTIFICADOR:
            return entorno_obtener(e, n->identificador, n->linea);

        case NODO_BINARIO:
            return exec_binario(n, e);

        case NODO_UNARIO: {
            Valor *v = ejecutar(n->unario.operando, e);
            if (strcmp(n->unario.operador, "-") == 0 && v->tipo == VAL_NUMERO) {
                v->numero = -v->numero;
                return v;
            }
            if (strcmp(n->unario.operador, "no") == 0) {
                int b = !es_verdadero(v);
                valor_destruir(v);
                return valor_booleano(b);
            }
            return v;
        }

        case NODO_DECLARACION: {
            Valor *v = ejecutar(n->declaracion.valor, e);
            entorno_definir(e, n->declaracion.nombre, v, n->declaracion.constante);
            return valor_nulo();
        }

        case NODO_ASIGNACION: {
            Valor *v = ejecutar(n->asignacion.valor, e);
            entorno_asignar(e, n->asignacion.nombre, v, n->linea);
            return valor_nulo();
        }

        case NODO_INCREMENTO: {
            Valor *v = entorno_obtener(e, n->incremento.nombre, n->linea);
            if (v->tipo != VAL_NUMERO) {
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   Solo puedes usar '++' o '--' con variables numéricas.\n"
                    "   '%s' no contiene un número.\n\n",
                    n->incremento.nombre);
                exit(1);
            }
            Valor *nuevo;
            if (strcmp(n->incremento.operador, "++") == 0)
                nuevo = valor_numero(v->numero + 1);
            else
                nuevo = valor_numero(v->numero - 1);
            entorno_asignar(e, n->incremento.nombre, nuevo, n->linea);
            return valor_nulo();
        }

        case NODO_ESCRIBIR: {
            Valor *v = ejecutar(n->escribir, e);
            char  *s = valor_a_texto(v);
            printf("%s\n", s);
            free(s);
            valor_destruir(v);
            return valor_nulo();
        }

        case NODO_BLOQUE: {
            Valor *ultimo = valor_nulo();
            for (int i = 0; i < n->bloque.cantidad && !hay_retorno && !hay_error; i++) {
                valor_destruir(ultimo);
                ultimo = ejecutar(n->bloque.sentencias[i], e);
            }
            return ultimo;
        }

        case NODO_SI: {
            Valor *cond = ejecutar(n->si.condicion, e);
            int    ok   = es_verdadero(cond);
            valor_destruir(cond);

            if (ok) {
                Entorno *nuevo = entorno_crear(e);
                Valor   *r     = ejecutar(n->si.entonces, nuevo);
                entorno_destruir(nuevo);
                return r;
            }
            for (int i = 0; i < n->si.cantidad_sino_si; i++) {
                Valor *c = ejecutar(n->si.condiciones_sino_si[i], e);
                int    b = es_verdadero(c);
                valor_destruir(c);
                if (b) {
                    Entorno *nuevo = entorno_crear(e);
                    Valor   *r     = ejecutar(n->si.bloques_sino_si[i], nuevo);
                    entorno_destruir(nuevo);
                    return r;
                }
            }
            if (n->si.sino) {
                Entorno *nuevo = entorno_crear(e);
                Valor   *r     = ejecutar(n->si.sino, nuevo);
                entorno_destruir(nuevo);
                return r;
            }
            return valor_nulo();
        }

        case NODO_MIENTRAS: {
            Valor *r = valor_nulo();
            while (!hay_retorno) {
                Valor *cond = ejecutar(n->mientras.condicion, e);
                int    ok   = es_verdadero(cond);
                valor_destruir(cond);
                if (!ok) break;
                valor_destruir(r);
                Entorno *nuevo = entorno_crear(e);
                r = ejecutar(n->mientras.cuerpo, nuevo);
                entorno_destruir(nuevo);
            }
            return r;
        }

        case NODO_HACER: {
            Valor *r = valor_nulo();
            do {
                valor_destruir(r);
                Entorno *nuevo = entorno_crear(e);
                r = ejecutar(n->hacer.cuerpo, nuevo);
                entorno_destruir(nuevo);
                if (hay_retorno) break;
                Valor *cond = ejecutar(n->hacer.condicion, e);
                int    ok   = es_verdadero(cond);
                valor_destruir(cond);
                if (!ok) break;
            } while (1);
            return r;
        }

        case NODO_PARA: {
            Entorno *bucle = entorno_crear(e);
            Valor   *vinicio = ejecutar(n->para.val_inicio, bucle);
            entorno_definir(bucle, n->para.var_inicio, vinicio, 0);

            while (!hay_retorno && !hay_error) {
                Valor *cond = ejecutar(n->para.condicion, bucle);
                int    ok   = es_verdadero(cond);
                valor_destruir(cond);
                if (!ok) break;

                Entorno *cuerpo_e = entorno_crear(bucle);
                Valor   *r = ejecutar(n->para.cuerpo, cuerpo_e);
                valor_destruir(r);
                entorno_destruir(cuerpo_e);

                Valor *vactual = entorno_obtener(bucle, n->para.var_incremento, 0);
                Valor *vnuevo;
                if (strcmp(n->para.op_incremento, "++") == 0)
                    vnuevo = valor_numero(vactual->numero + 1);
                else
                    vnuevo = valor_numero(vactual->numero - 1);
                valor_destruir(vactual);
                entorno_asignar(bucle, n->para.var_incremento, vnuevo, 0);
            }
            entorno_destruir(bucle);
            return valor_nulo();
        }

        case NODO_PARA_CADA: {
            Valor *col = ejecutar(n->para_cada.coleccion, e);

            /* ── Lista o Generador ── */
            if (col->tipo == VAL_LISTA || col->tipo == VAL_GENERADOR) {
                int cant = (col->tipo == VAL_LISTA)
                    ? col->lista.cantidad
                    : col->generador.cantidad;
                for (int i = 0; i < cant && !hay_retorno && !hay_error; i++) {
                    Valor *elem = valor_copiar(
                        col->tipo == VAL_LISTA
                            ? col->lista.elementos[i]
                            : col->generador.elementos[i]);
                    validar_tipo(elem, n->para_cada.tipo,
                                 "variable del bucle", n->para_cada.variable, n->linea);
                    Entorno *iter = entorno_crear(e);
                    entorno_definir(iter, n->para_cada.variable, elem, 0);
                    Valor *r = ejecutar(n->para_cada.cuerpo, iter);
                    valor_destruir(r);
                    entorno_destruir(iter);
                }
                valor_destruir(col);
                return valor_nulo();
            }

            /* ── Diccionario — itera sobre claves ── */
            if (col->tipo == VAL_DICCIONARIO) {
                for (int i = 0; i < col->diccionario.cantidad && !hay_retorno && !hay_error; i++) {
                    Valor *clave = valor_texto(col->diccionario.claves[i]);

                    validar_tipo(clave, n->para_cada.tipo, "variable del bucle", n->para_cada.variable, n->linea);

                    Entorno *iter = entorno_crear(e);
                    entorno_definir(iter, n->para_cada.variable, clave, 0);
                    Valor *r = ejecutar(n->para_cada.cuerpo, iter);
                    valor_destruir(r);
                    entorno_destruir(iter);
                }
                valor_destruir(col);
                return valor_nulo();
            }

            /* ── Texto — itera carácter a carácter ── */
            if (col->tipo == VAL_TEXTO) {
                for (int i = 0; col->texto[i] && !hay_retorno && !hay_error; i++) {
                    char tmp[2] = { col->texto[i], '\0' };
                    Valor *c = valor_texto(tmp);

                    validar_tipo(c, n->para_cada.tipo, "variable del bucle", n->para_cada.variable, n->linea);

                    Entorno *iter = entorno_crear(e);
                    entorno_definir(iter, n->para_cada.variable, c, 0);
                    Valor *r = ejecutar(n->para_cada.cuerpo, iter);
                    valor_destruir(r);
                    entorno_destruir(iter);
                }
                valor_destruir(col);
                return valor_nulo();
            }

            fprintf(stderr,
                "\n❌ Error:\n"
                "   'para cada' solo funciona con listas, diccionarios o texto.\n\n");
            exit(1);
        }

        case NODO_FUNCION: {
            FuncionLince *f = malloc(sizeof(FuncionLince));
            f->nombre           = strdup(n->funcion.nombre);
            f->parametros       = n->funcion.parametros;
            f->num_parametros   = n->funcion.num_parametros;
            f->tipo_retorno     = n->funcion.tipo_retorno;
            f->cuerpo           = n->funcion.cuerpo;
            f->entorno_closure  = e;
            e->refs++;  /* el closure retiene el entorno */
            entorno_definir(e, n->funcion.nombre, valor_funcion(f), 0);
            return valor_nulo();
        }

        case NODO_LLAMADA: {
            if (++profundidad > MAX_PROFUNDIDAD) {
                profundidad = 0;
                char msg[128];
                snprintf(msg, sizeof(msg),
                    "Demasiadas llamadas anidadas — ¿hay una recursión infinita en '%s'?",
                    n->llamada.nombre);
                valor_error = valor_crear_error("ErrorRecursion", msg, n->linea);
                hay_error   = 1;
                return valor_nulo();
            }
            Valor *vfun = entorno_obtener(e, n->llamada.nombre, n->linea);
            if (vfun->tipo != VAL_FUNCION) {
                fprintf(stderr,
                    "\n❌ Error en línea %d:\n"
                    "   '%s' no es una función.\n\n",
                    n->linea, n->llamada.nombre);
                exit(1);
            }
            FuncionLince *f = vfun->funcion;

            /* ¿Es un generador? */
            if (vfun->es_modulo == 99) {
                /* Ejecutar el cuerpo acumulando valores producidos */
                int args_dados   = n->llamada.num_argumentos;
                int params_total = f->num_parametros;
                int params_req   = 0;
                for (int i = 0; i < params_total; i++)
                    if (!f->parametros[i].valor_defecto) params_req++;

                if (args_dados < params_req || args_dados > params_total) {
                    fprintf(stderr,
                        "\n❌ Error en línea %d:\n"
                        "   El generador '%s' espera entre %d y %d argumento(s).\n\n",
                        n->linea, f->nombre, params_req, params_total);
                    exit(1);
                }

                Entorno *fn_e = entorno_crear(f->entorno_closure);
                for (int i = 0; i < params_total; i++) {
                    Valor *arg = (i < args_dados)
                        ? ejecutar(n->llamada.argumentos[i], e)
                        : ejecutar(f->parametros[i].valor_defecto, e);
                    entorno_definir(fn_e, f->parametros[i].nombre, arg, 0);
                }

                /* Crear la lista acumuladora */
                Valor *gen = valor_generador_crear();
                Valor *gen_anterior = generador_actual;
                generador_actual    = gen;

                ejecutar(f->cuerpo, fn_e);

                generador_actual = gen_anterior;
                hay_retorno      = 0;
                valor_retorno    = NULL;
                hay_producir     = 0;
                entorno_destruir(fn_e);
                valor_destruir(vfun);
                profundidad--;
                return gen;
            }

            /* ¿Es una función nativa? (entorno_closure == NULL) */
            if (f->entorno_closure == NULL) {
                typedef Valor *(*FnNativa)(Valor**, int);
                FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
                int nargs = n->llamada.num_argumentos;
                Valor **args = malloc(sizeof(Valor*) * (nargs + 1));
                for (int i = 0; i < nargs; i++)
                    args[i] = ejecutar(n->llamada.argumentos[i], e);
                Valor *resultado = fn(args, nargs);
                for (int i = 0; i < nargs; i++) valor_destruir(args[i]);
                free(args);
                valor_destruir(vfun);
                profundidad--;
                return resultado ? resultado : valor_nulo();
            }

            /* Contar parámetros requeridos (sin valor por defecto) */
            int args_dados   = n->llamada.num_argumentos;
            int params_total = f->num_parametros;
            int params_req   = 0;
            for (int i = 0; i < params_total; i++)
                if (!f->parametros[i].valor_defecto) params_req++;

            if (args_dados < params_req || args_dados > params_total) {
                fprintf(stderr,
                    "\n❌ Error en línea %d:\n"
                    "   La función '%s' espera entre %d y %d argumento(s) pero recibió %d.\n\n",
                    n->linea, f->nombre, params_req, params_total, args_dados);
                exit(1);
            }

            Entorno *fn_e = entorno_crear(f->entorno_closure);
            for (int i = 0; i < params_total; i++) {
                Parametro *param = &f->parametros[i];
                Valor *arg;

                if (i < args_dados) {
                    if (param->por_ref) {
                        arg = ejecutar(n->llamada.argumentos[i], e);
                    } else {
                        Valor *tmp = ejecutar(n->llamada.argumentos[i], e);
                        if (tmp->tipo == VAL_NUMERO || tmp->tipo == VAL_TEXTO ||
                            tmp->tipo == VAL_BOOLEANO || tmp->tipo == VAL_NULO) {
                            arg = valor_copiar(tmp);
                            valor_destruir(tmp);
                        } else {
                            arg = tmp;
                        }
                    }
                } else {
                    /* Usar valor por defecto */
                    arg = ejecutar(param->valor_defecto, e);
                }

                validar_tipo(arg, param->tipo, "parámetro", param->nombre, n->linea);
                entorno_definir(fn_e, param->nombre, arg, 0);
            }

            ejecutar(f->cuerpo, fn_e);
            entorno_destruir(fn_e);

            Valor *retorno = valor_nulo();
            if (hay_retorno) {
                retorno       = valor_retorno;
                valor_retorno = NULL;
                hay_retorno   = 0;
            }

            if (!hay_error)
                validar_tipo(retorno, f->tipo_retorno, "retorno de función", f->nombre, n->linea);

            valor_destruir(vfun);
            profundidad--;
            return retorno;
        }

        case NODO_DEVOLVER: {
            Valor *v = ejecutar(n->devolver, e);
            /* Si la expresión falló, propagar el error en vez de marcar
               retorno: con 'hay_retorno' puesto, el bloque 'capturar'
               que envuelva a este 'devolver' se saltaría sus sentencias
               y el error escaparía del intentar/capturar como nulo. */
            if (hay_error) {
                valor_destruir(v);
                return valor_nulo();
            }
            valor_retorno = v;
            hay_retorno   = 1;
            return valor_nulo();
        }

        case NODO_LISTA: {
            Valor *lista = valor_lista_crear();
            for (int i = 0; i < n->lista.cantidad; i++)
                lista_agregar(lista, ejecutar(n->lista.elementos[i], e));
            return lista;
        }

        case NODO_DICCIONARIO: {
            Valor *dic = valor_diccionario_crear();
            valor_diccionario_asegurar(dic, n->diccionario.cantidad);
            for (int i = 0; i < n->diccionario.cantidad; i++) {
                dic->diccionario.claves[i]  = strdup(n->diccionario.claves[i]);
                dic->diccionario.valores[i] = ejecutar(n->diccionario.valores[i], e);
                dic->diccionario.cantidad++;
            }
            return dic;
        }

        case NODO_INDICE: {
            Valor *obj = ejecutar(n->acceso_indice.objeto, e);
            Valor *idx = ejecutar(n->acceso_indice.indice, e);

            if (obj->tipo == VAL_LISTA) {
                if (idx->tipo != VAL_NUMERO) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "El índice de una lista debe ser un número.");
                    valor_error = valor_crear_error("ErrorTipo", msg, n->linea);
                    hay_error = 1;
                    valor_destruir(obj); valor_destruir(idx);
                    return valor_nulo();
                }
                int i = (int)idx->numero;
                if (i < 0) i = obj->lista.cantidad + i;
                if (i < 0 || i >= obj->lista.cantidad) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                        "Índice %d fuera de rango. La lista tiene %d elementos.",
                        i, obj->lista.cantidad);
                    valor_error = valor_crear_error("ErrorRango", msg, n->linea);
                    hay_error = 1;
                    valor_destruir(obj); valor_destruir(idx);
                    return valor_nulo();
                }
                Valor *res = valor_copiar(obj->lista.elementos[i]);
                valor_destruir(obj); valor_destruir(idx);
                return res;
            }
            if (obj->tipo == VAL_DICCIONARIO) {
                if (idx->tipo != VAL_TEXTO) {
                    valor_error = valor_crear_error("ErrorTipo",
                        "La clave de un diccionario debe ser texto.", n->linea);
                    hay_error = 1;
                    valor_destruir(obj); valor_destruir(idx);
                    return valor_nulo();
                }
                for (int i = 0; i < obj->diccionario.cantidad; i++) {
                    if (strcmp(obj->diccionario.claves[i], idx->texto) == 0) {
                        Valor *res = valor_copiar(obj->diccionario.valores[i]);
                        valor_destruir(obj); valor_destruir(idx);
                        return res;
                    }
                }
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "La clave \"%s\" no existe en el diccionario.", idx->texto);
                valor_error = valor_crear_error("ErrorRango", msg, n->linea);
                hay_error = 1;
                valor_destruir(obj); valor_destruir(idx);
                return valor_nulo();
            }
            if (obj->tipo == VAL_TEXTO) {
                if (idx->tipo != VAL_NUMERO) {
                    valor_error = valor_crear_error("ErrorTipo",
                        "El índice de un texto debe ser un número.", n->linea);
                    hay_error = 1;
                    valor_destruir(obj); valor_destruir(idx);
                    return valor_nulo();
                }
                int i = (int)idx->numero;
                int len = (int)strlen(obj->texto);
                if (i < 0) i = len + i;
                if (i < 0 || i >= len) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                        "Índice %d fuera de rango. El texto tiene %d caracteres.", i, len);
                    valor_error = valor_crear_error("ErrorRango", msg, n->linea);
                    hay_error = 1;
                    valor_destruir(obj); valor_destruir(idx);
                    return valor_nulo();
                }
                char tmp[2] = { obj->texto[i], '\0' };
                Valor *res = valor_texto(tmp);
                valor_destruir(obj); valor_destruir(idx);
                return res;
            }
            valor_error = valor_crear_error("ErrorTipo",
                "Solo puedes usar [] en listas, diccionarios o texto.", n->linea);
            hay_error = 1;
            valor_destruir(obj); valor_destruir(idx);
            return valor_nulo();
        }

        case NODO_ASIGNACION_INDICE: {
            Valor *obj = ejecutar(n->asignacion_indice.objeto, e);
            Valor *idx = ejecutar(n->asignacion_indice.indice, e);
            Valor *val = ejecutar(n->asignacion_indice.valor,  e);

            if (obj->tipo == VAL_LISTA) {
                int i = (int)idx->numero;
                if (i < 0) i = obj->lista.cantidad + i;
                if (i < 0 || i >= obj->lista.cantidad) {
                    fprintf(stderr, "\n❌ Error:\n   Índice fuera de rango.\n\n");
                    exit(1);
                }
                valor_destruir(obj->lista.elementos[i]);
                obj->lista.elementos[i] = val;
                /* Reflejar en el entorno */
                if (n->asignacion_indice.objeto->tipo == NODO_IDENTIFICADOR) {
                    entorno_asignar(e, n->asignacion_indice.objeto->identificador, obj, n->linea);
                }
                valor_destruir(idx);
                return valor_nulo();
            }
            if (obj->tipo == VAL_DICCIONARIO) {
                if (idx->tipo != VAL_TEXTO) {
                    fprintf(stderr, "\n❌ Error:\n   La clave debe ser texto.\n\n");
                    exit(1);
                }
                for (int i = 0; i < obj->diccionario.cantidad; i++) {
                    if (strcmp(obj->diccionario.claves[i], idx->texto) == 0) {
                        valor_destruir(obj->diccionario.valores[i]);
                        obj->diccionario.valores[i] = val;
                        if (n->asignacion_indice.objeto->tipo == NODO_IDENTIFICADOR)
                            entorno_asignar(e, n->asignacion_indice.objeto->identificador, obj, n->linea);
                        valor_destruir(idx);
                        return valor_nulo();
                    }
                }
                /* Clave nueva */
                valor_diccionario_asegurar(obj, obj->diccionario.cantidad + 1);
                int i = obj->diccionario.cantidad;
                obj->diccionario.claves[i]  = strdup(idx->texto);
                obj->diccionario.valores[i] = val;
                obj->diccionario.cantidad++;
                if (n->asignacion_indice.objeto->tipo == NODO_IDENTIFICADOR)
                    entorno_asignar(e, n->asignacion_indice.objeto->identificador, obj, n->linea);
                valor_destruir(idx);
                return valor_nulo();
            }
            fprintf(stderr, "\n❌ Error:\n   Solo puedes asignar por índice en listas o diccionarios.\n\n");
            exit(1);
        }

        case NODO_METODO: {
            Valor *obj = ejecutar(n->metodo.objeto, e);
            const char *met = n->metodo.metodo;

            /* ── Módulo (diccionario con funciones nativas) ── */
            if (obj->tipo == VAL_DICCIONARIO && obj->es_modulo) {
                const char *met = n->metodo.metodo;
                for (int i = 0; i < obj->diccionario.cantidad; i++) {
                    if (strcmp(obj->diccionario.claves[i], met) == 0) {
                        Valor *vf = obj->diccionario.valores[i];
                        if (vf->tipo != VAL_FUNCION) {
                            fprintf(stderr,
                                "\n❌ Error:\n"
                                "   '%s' no es una función del módulo.\n\n", met);
                            exit(1);
                        }
                        FuncionLince *f = vf->funcion;
                        /* Función nativa: cuerpo guarda el puntero */
                        typedef Valor *(*FnNativa)(Valor**, int);
                        FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;

                        int nargs = n->metodo.num_argumentos;
                        Valor **args = malloc(sizeof(Valor*) * (nargs + 1));
                        for (int j = 0; j < nargs; j++)
                            args[j] = ejecutar(n->metodo.argumentos[j], e);

                        Valor *resultado = fn(args, nargs);
                        for (int j = 0; j < nargs; j++)
                            valor_destruir(args[j]);
                        free(args);
                        valor_destruir(obj);
                        return resultado ? resultado : valor_nulo();
                    }
                }
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   El módulo no tiene la función '%s'.\n\n", met);
                exit(1);
            }

            /* ── Métodos de OBJETO ── */
            if (obj->tipo == VAL_OBJETO) {
                ClaseLince   *cls       = obj->objeto->clase;
                const char   *met_nombre = n->metodo.metodo;
                FuncionLince *f          = buscar_metodo(cls, met_nombre);

                if (!f) {
                    fprintf(stderr,
                        "\n❌ Error en línea %d:\n"
                        "   La clase '%s' no tiene el método '%s'.\n\n",
                        n->linea, cls->nombre, met_nombre);
                    exit(1);
                }

                if (n->metodo.num_argumentos != f->num_parametros) {
                    fprintf(stderr,
                        "\n❌ Error en línea %d:\n"
                        "   El método '%s' espera %d argumento(s), recibió %d.\n\n",
                        n->linea, met_nombre, f->num_parametros, n->metodo.num_argumentos);
                    exit(1);
                }

                if (++profundidad > MAX_PROFUNDIDAD) {
                    profundidad = 0;
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                        "Demasiadas llamadas anidadas en método '%s'.", met_nombre);
                    valor_error = valor_crear_error("ErrorRecursion", msg, n->linea);
                    hay_error   = 1;
                    valor_destruir(obj);
                    return valor_nulo();
                }
                Entorno *fn_e = entorno_crear(f->entorno_closure);
                for (int j = 0; j < f->num_parametros; j++) {
                    Valor *arg = ejecutar(n->metodo.argumentos[j], e);
                    validar_tipo(arg, f->parametros[j].tipo, "parámetro", f->parametros[j].nombre, n->linea);
                    entorno_definir(fn_e, f->parametros[j].nombre, arg, 0);
                }
                Valor *esto_anterior   = esto_actual;
                ClaseLince *cls_ant    = clase_actual;
                esto_actual  = obj;
                clase_actual = cls;
                ejecutar(f->cuerpo, fn_e);
                Valor *retorno = valor_nulo();
                if (hay_retorno) {
                    retorno       = valor_retorno;
                    valor_retorno = NULL;
                    hay_retorno   = 0;
                }
                esto_actual  = esto_anterior;
                clase_actual = cls_ant;
                entorno_destruir(fn_e);
                valor_destruir(obj);
                profundidad--;
                return retorno;
            }

            /* ── Métodos de LISTA ── */
            if (obj->tipo == VAL_LISTA) {
                if (strcmp(met, "longitud") == 0) {
                    int len = obj->lista.cantidad;
                    valor_destruir(obj);
                    return valor_numero(len);
                }
                if (strcmp(met, "agregar") == 0) {
                    if (n->metodo.num_argumentos != 1) {
                        fprintf(stderr, "\n❌ Error:\n   'agregar' necesita exactamente 1 argumento.\n\n");
                        exit(1);
                    }
                    Valor *elem = ejecutar(n->metodo.argumentos[0], e);
                    lista_agregar(obj, elem);
                    if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR)
                        entorno_asignar(e, n->metodo.objeto->identificador, obj, n->linea);
                    else valor_destruir(obj);
                    return valor_nulo();
                }
                if (strcmp(met, "eliminar") == 0) {
                    if (n->metodo.num_argumentos != 1) {
                        fprintf(stderr, "\n❌ Error:\n   'eliminar' necesita exactamente 1 argumento (índice).\n\n");
                        exit(1);
                    }
                    Valor *vidx = ejecutar(n->metodo.argumentos[0], e);
                    int idx = (int)vidx->numero;
                    valor_destruir(vidx);
                    if (idx < 0 || idx >= obj->lista.cantidad) {
                        fprintf(stderr, "\n❌ Error:\n   Índice fuera de rango en 'eliminar'.\n\n");
                        exit(1);
                    }
                    valor_destruir(obj->lista.elementos[idx]);
                    for (int i = idx; i < obj->lista.cantidad - 1; i++)
                        obj->lista.elementos[i] = obj->lista.elementos[i+1];
                    obj->lista.cantidad--;
                    if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR)
                        entorno_asignar(e, n->metodo.objeto->identificador, obj, n->linea);
                    else valor_destruir(obj);
                    return valor_nulo();
                }
                if (strcmp(met, "contiene") == 0) {
                    if (n->metodo.num_argumentos != 1) {
                        fprintf(stderr, "\n❌ Error:\n   'contiene' necesita exactamente 1 argumento.\n\n");
                        exit(1);
                    }
                    Valor *buscado = ejecutar(n->metodo.argumentos[0], e);
                    int encontrado = 0;
                    for (int i = 0; i < obj->lista.cantidad; i++) {
                        Valor *el = obj->lista.elementos[i];
                        if (el->tipo == VAL_NUMERO && buscado->tipo == VAL_NUMERO && el->numero == buscado->numero) { encontrado = 1; break; }
                        if (el->tipo == VAL_TEXTO  && buscado->tipo == VAL_TEXTO  && strcmp(el->texto, buscado->texto) == 0) { encontrado = 1; break; }
                    }
                    valor_destruir(buscado); valor_destruir(obj);
                    return valor_booleano(encontrado);
                }
                fprintf(stderr, "\n❌ Error:\n   Las listas no tienen el método '%s'.\n\n", met);
                exit(1);
            }

            /* ── Métodos de TEXTO ── */
            if (obj->tipo == VAL_TEXTO) {
                if (strcmp(met, "longitud") == 0) {
                    int len = strlen(obj->texto);
                    valor_destruir(obj);
                    return valor_numero(len);
                }
                if (strcmp(met, "mayusculas") == 0) {
                    char *s = strdup(obj->texto);
                    for (int i = 0; s[i]; i++) s[i] = toupper((unsigned char)s[i]);
                    Valor *r = valor_texto(s); free(s); valor_destruir(obj);
                    return r;
                }
                if (strcmp(met, "minusculas") == 0) {
                    char *s = strdup(obj->texto);
                    for (int i = 0; s[i]; i++) s[i] = tolower((unsigned char)s[i]);
                    Valor *r = valor_texto(s); free(s); valor_destruir(obj);
                    return r;
                }
                if (strcmp(met, "contiene") == 0) {
                    if (n->metodo.num_argumentos != 1) {
                        fprintf(stderr, "\n❌ Error:\n   'contiene' necesita exactamente 1 argumento.\n\n");
                        exit(1);
                    }
                    Valor *sub = ejecutar(n->metodo.argumentos[0], e);
                    int ok = strstr(obj->texto, sub->texto) != NULL;
                    valor_destruir(sub); valor_destruir(obj);
                    return valor_booleano(ok);
                }
                if (strcmp(met, "reemplazar") == 0) {
                    if (n->metodo.num_argumentos != 2) {
                        fprintf(stderr, "\n❌ Error:\n   'reemplazar' necesita 2 argumentos: buscar y reemplazo.\n\n");
                        exit(1);
                    }
                    Valor *buscar = ejecutar(n->metodo.argumentos[0], e);
                    Valor *repla  = ejecutar(n->metodo.argumentos[1], e);
                    char *src = obj->texto, *pat = buscar->texto, *rep = repla->texto;
                    char resultado[4096] = "";
                    char *pos;
                    while ((pos = strstr(src, pat)) != NULL) {
                        strncat(resultado, src, pos - src);
                        strcat(resultado, rep);
                        src = pos + strlen(pat);
                    }
                    strcat(resultado, src);
                    Valor *r = valor_texto(resultado);
                    valor_destruir(buscar); valor_destruir(repla); valor_destruir(obj);
                    return r;
                }
                if (strcmp(met, "recortar") == 0) {
                    char *s = strdup(obj->texto);
                    int i = 0, j = strlen(s) - 1;
                    while (s[i] == ' ' || s[i] == '\t') i++;
                    while (j > i && (s[j] == ' ' || s[j] == '\t')) j--;
                    s[j+1] = '\0';
                    Valor *r = valor_texto(s + i); free(s); valor_destruir(obj);
                    return r;
                }
                fprintf(stderr, "\n❌ Error:\n   El texto no tiene el método '%s'.\n\n", met);
                exit(1);
            }

            /* ── Métodos de DICCIONARIO ── */
            if (obj->tipo == VAL_DICCIONARIO) {
                if (strcmp(met, "contiene") == 0) {
                    if (n->metodo.num_argumentos != 1) {
                        fprintf(stderr, "\n❌ Error:\n   'contiene' necesita exactamente 1 argumento.\n\n");
                        exit(1);
                    }
                    Valor *clave = ejecutar(n->metodo.argumentos[0], e);
                    int ok = 0;
                    for (int i = 0; i < obj->diccionario.cantidad; i++)
                        if (strcmp(obj->diccionario.claves[i], clave->texto) == 0) { ok = 1; break; }
                    valor_destruir(clave); valor_destruir(obj);
                    return valor_booleano(ok);
                }
                if (strcmp(met, "longitud") == 0) {
                    int len = obj->diccionario.cantidad;
                    valor_destruir(obj);
                    return valor_numero(len);
                }
                if (strcmp(met, "eliminar") == 0) {
                    if (n->metodo.num_argumentos != 1) {
                        fprintf(stderr, "\n❌ Error:\n   'eliminar' necesita exactamente 1 argumento (clave).\n\n");
                        exit(1);
                    }
                    Valor *clave = ejecutar(n->metodo.argumentos[0], e);
                    for (int i = 0; i < obj->diccionario.cantidad; i++) {
                        if (strcmp(obj->diccionario.claves[i], clave->texto) == 0) {
                            free(obj->diccionario.claves[i]);
                            valor_destruir(obj->diccionario.valores[i]);
                            for (int j = i; j < obj->diccionario.cantidad - 1; j++) {
                                obj->diccionario.claves[j]  = obj->diccionario.claves[j+1];
                                obj->diccionario.valores[j] = obj->diccionario.valores[j+1];
                            }
                            obj->diccionario.cantidad--;
                            break;
                        }
                    }
                    if (n->metodo.objeto->tipo == NODO_IDENTIFICADOR)
                        entorno_asignar(e, n->metodo.objeto->identificador, obj, n->linea);
                    valor_destruir(clave);
                    return valor_nulo();
                }
                fprintf(stderr, "\n❌ Error:\n   Los diccionarios no tienen el método '%s'.\n\n", met);
                exit(1);
            }

            fprintf(stderr, "\n❌ Error:\n   El tipo actual no tiene el método '%s'.\n\n", met);
            exit(1);
        }

        case NODO_ELEGIR: {
            Valor *sujeto = ejecutar(n->elegir.sujeto, e);
            int    encontrado = 0;

            for (int i = 0; i < n->elegir.num_casos && !hay_error && !hay_retorno; i++) {
                Valor *caso_val = ejecutar(n->elegir.valores[i], e);

                /* Comparación por igualdad */
                int iguales = 0;
                if (sujeto->tipo == VAL_NUMERO && caso_val->tipo == VAL_NUMERO)
                    iguales = (sujeto->numero == caso_val->numero);
                else if (sujeto->tipo == VAL_TEXTO && caso_val->tipo == VAL_TEXTO)
                    iguales = (strcmp(sujeto->texto, caso_val->texto) == 0);
                else if (sujeto->tipo == VAL_BOOLEANO && caso_val->tipo == VAL_BOOLEANO)
                    iguales = (sujeto->booleano == caso_val->booleano);
                else if (sujeto->tipo == VAL_NULO && caso_val->tipo == VAL_NULO)
                    iguales = 1;

                valor_destruir(caso_val);

                if (iguales) {
                    encontrado = 1;
                    Entorno *caso_e = entorno_crear(e);
                    Valor   *r      = ejecutar(n->elegir.cuerpos[i], caso_e);
                    valor_destruir(r);
                    entorno_destruir(caso_e);
                    break;
                }
            }

            /* Si no coincidió ningún caso, ejecutar otro */
            if (!encontrado && n->elegir.otro && !hay_error && !hay_retorno) {
                Entorno *otro_e = entorno_crear(e);
                Valor   *r      = ejecutar(n->elegir.otro, otro_e);
                valor_destruir(r);
                entorno_destruir(otro_e);
            }

            valor_destruir(sujeto);
            return valor_nulo();
        }

        case NODO_GENERADOR: {
            /* Registra el generador como función especial en el entorno */
            FuncionLince *f = malloc(sizeof(FuncionLince));
            f->nombre           = strdup(n->generador.nombre);
            f->parametros       = n->generador.parametros;
            f->num_parametros   = n->generador.num_parametros;
            f->tipo_retorno     = TIPO_LISTA; /* siempre devuelve lista */
            f->cuerpo           = n->generador.cuerpo;
            f->entorno_closure  = e;
            e->refs++;
            /* Marcamos que es generador con tipo_retorno especial */
            Valor *vf = valor_funcion(f);
            vf->es_modulo = 99; /* marca: es generador */
            entorno_definir(e, f->nombre, vf, 0);
            return valor_nulo();
        }

        case NODO_PRODUCIR: {
            /* Solo válido dentro de un generador en ejecución */
            if (!generador_actual) {
                valor_error = valor_crear_error("Error",
                    "'producir' solo puede usarse dentro de un generador.", n->linea);
                hay_error = 1;
                return valor_nulo();
            }
            if (generador_actual->generador.cantidad >= MAX_GENERADOR) {
                valor_error = valor_crear_error("Error",
                    "El generador ha superado el límite de 10000 valores.", n->linea);
                hay_error = 1;
                return valor_nulo();
            }
            Valor *v = ejecutar(n->producir, e);
            generador_agregar(generador_actual, v);
            hay_producir = 1;
            return valor_nulo();
        }

        case NODO_INTERFAZ: {
            /* Registra la interfaz en el entorno como un valor especial */
            InterfazLince *iface = malloc(sizeof(InterfazLince));
            iface->nombre      = strdup(n->interfaz.nombre);
            iface->num_metodos = n->interfaz.num_metodos;
            iface->metodos     = malloc(sizeof(char*) * n->interfaz.num_metodos);
            iface->tipos_retorno = malloc(sizeof(char*) * n->interfaz.num_metodos);
            for (int i = 0; i < n->interfaz.num_metodos; i++) {
                iface->metodos[i]      = strdup(n->interfaz.metodos[i]);
                iface->tipos_retorno[i] = strdup(n->interfaz.tipos_retorno[i]);
            }
            /* Guardamos la interfaz como un valor de texto especial en el entorno */
            /* El valor real es un puntero empaquetado — usamos diccionario de interfaz */
            Valor *viface = valor_diccionario_crear();
            viface->es_modulo = 2; /* 2 = interfaz */
            /* Guardamos el puntero en el primer valor */
            viface->diccionario.claves[0]  = strdup("__interfaz__");
            viface->diccionario.valores[0] = valor_numero((double)(uintptr_t)iface);
            viface->diccionario.cantidad   = 1;
            entorno_definir(e, iface->nombre, viface, 1);
            return valor_nulo();
        }

        case NODO_ENUMERACION: {
            /* Crea un diccionario con los valores de la enum */
            Valor *dic = valor_diccionario_crear();
            dic->es_modulo = 1; /* marcar como namespace — no iterable */

            for (int i = 0; i < n->enumeracion.num_valores; i++) {
                dic->diccionario.claves[i]  = strdup(n->enumeracion.valores[i]);
                /* El valor es un texto especial: "NombreEnum.Valor" */
                char buf[256];
                snprintf(buf, sizeof(buf), "%s.%s",
                    n->enumeracion.nombre,
                    n->enumeracion.valores[i]);
                dic->diccionario.valores[i] = valor_texto(buf);
                dic->diccionario.cantidad++;
            }
            entorno_definir(e, n->enumeracion.nombre, dic, 1); /* constante */
            return valor_nulo();
        }

        case NODO_IMPORTAR: {
            const char *nombre = n->importar;

            /* ¿Es un paquete instalado? paquete:nombre */
            if (strncmp(nombre, "paquete:", 8) == 0) {
                const char *nom_paquete = nombre + 8;
                char ruta[1024];
                if (!paquetes_resolver(nom_paquete, ruta, sizeof(ruta))) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                        "El paquete '%s' no está instalado.\n"
                        "   Instálalo con: lince instalar <url>",
                        nom_paquete);
                    valor_error = valor_crear_error("Error", msg, n->linea);
                    hay_error = 1;
                    return valor_nulo();
                }
                /* Ejecutar el archivo del paquete en el entorno actual */
                FILE *f = fopen(ruta, "r");
                if (!f) {
                    valor_error = valor_crear_error("Error",
                        "No se pudo leer el paquete.", n->linea);
                    hay_error = 1;
                    return valor_nulo();
                }
                fseek(f, 0, SEEK_END);
                long tam = ftell(f); rewind(f);
                char *codigo = malloc(tam + 1);
                size_t leido = fread(codigo, 1, tam, f);
                codigo[leido] = '\0';
                fclose(f);

                Lexer  *lx  = lexer_crear(codigo);
                int     cnt = 0;
                Token  *tok = lexer_tokenizar(lx, &cnt);
                Parser *pa  = parser_crear(tok, cnt);
                Nodo   *ast = parser_parsear(pa);
                ejecutar(ast, e);
                /* No destruimos el AST — las funciones definidas lo referencian */
                parser_destruir(pa);
                lexer_destruir(lx);
                free(codigo);
                return valor_nulo();
            }

            /* ¿Es un archivo local relativo? ./archivo o ../archivo */
            if (strncmp(nombre, "./", 2) == 0 || strncmp(nombre, "../", 3) == 0) {
                char ruta[1024];
                snprintf(ruta, sizeof(ruta), "%s.lince", nombre);
                FILE *f = fopen(ruta, "r");
                if (!f) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                        "No se encontró el archivo '%s'.", ruta);
                    valor_error = valor_crear_error("Error", msg, n->linea);
                    hay_error = 1;
                    return valor_nulo();
                }
                fseek(f, 0, SEEK_END);
                long tam = ftell(f); rewind(f);
                char *codigo = malloc(tam + 1);
                size_t leido = fread(codigo, 1, tam, f);
                codigo[leido] = '\0';
                fclose(f);

                Lexer  *lx  = lexer_crear(codigo);
                int     cnt = 0;
                Token  *tok = lexer_tokenizar(lx, &cnt);
                Parser *pa  = parser_crear(tok, cnt);
                Nodo   *ast = parser_parsear(pa);
                ejecutar(ast, e);
                /* No destruimos el AST — las funciones definidas lo referencian */
                parser_destruir(pa);
                lexer_destruir(lx);
                free(codigo);
                return valor_nulo();
            }

            /* Módulo estándar */
            modulo_cargar(nombre, e);
            return valor_nulo();
        }

        case NODO_LAMBDA: {
            /* Crea una función anónima y la retorna como valor */
            FuncionLince *f = malloc(sizeof(FuncionLince));
            f->nombre           = strdup("<lambda>");
            f->parametros       = n->lambda.parametros;
            f->num_parametros   = n->lambda.num_parametros;
            f->tipo_retorno     = n->lambda.tipo_retorno;
            f->cuerpo           = n->lambda.cuerpo;
            f->entorno_closure  = e;
            e->refs++;  /* el closure retiene el entorno */
            return valor_funcion(f);
        }

        case NODO_PADRE: {
            if (!esto_actual || !clase_actual) {
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   'padre()' solo se puede usar dentro del método 'crear' de una clase.\n\n");
                exit(1);
            }
            ClaseLince *cls_padre = clase_actual->padre;
            if (!cls_padre) {
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   Esta clase no extiende ninguna otra — no hay 'padre' al que llamar.\n\n");
                exit(1);
            }
            FuncionLince *f = buscar_metodo(cls_padre, "crear");
            if (!f) return valor_nulo(); /* padre sin crear — ok */

            if (n->padre.num_argumentos != f->num_parametros) {
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   'padre()' espera %d argumento(s), recibió %d.\n\n",
                    f->num_parametros, n->padre.num_argumentos);
                exit(1);
            }

            Entorno *fn_e = entorno_crear(f->entorno_closure);
            for (int i = 0; i < f->num_parametros; i++) {
                Valor *arg = ejecutar(n->padre.argumentos[i], e);
                validar_tipo(arg, f->parametros[i].tipo, "parámetro", f->parametros[i].nombre, n->linea);
                entorno_definir(fn_e, f->parametros[i].nombre, arg, 0);
            }

            ClaseLince *cls_ant = clase_actual;
            clase_actual = cls_padre;
            ejecutar(f->cuerpo, fn_e);
            clase_actual  = cls_ant;
            hay_retorno   = 0;
            valor_retorno = NULL;
            entorno_destruir(fn_e);
            return valor_nulo();
        }

        case NODO_LANZAR: {            Valor *vobj = ejecutar(n->lanzar, e);

            if (vobj->tipo == VAL_ERROR) {
                /* Ya es un error — lanzar directamente */
                valor_error = vobj;
            } else if (vobj->tipo == VAL_OBJETO) {
                /* Objeto de clase propia — envolver como error */
                char *msg  = NULL;
                char *tipo = strdup(vobj->objeto->clase->nombre);
                for (int i = 0; i < vobj->objeto->num_campos; i++) {
                    if (strcmp(vobj->objeto->campos_nombres[i], "mensaje") == 0) {
                        msg = valor_a_texto(vobj->objeto->campos_valores[i]);
                        break;
                    }
                }
                valor_error = valor_crear_error(tipo, msg ? msg : "sin mensaje", 0);
                free(tipo);
                if (msg) free(msg);
                valor_destruir(vobj);
            } else {
                char *s = valor_a_texto(vobj);
                valor_error = valor_crear_error("Error", s, 0);
                free(s);
                valor_destruir(vobj);
            }
            hay_error = 1;
            return valor_nulo();
        }

        case NODO_INTENTAR: {
            /* Ejecutar el bloque intentar */
            Entorno *e_try = entorno_crear(e);
            ejecutar(n->intentar.cuerpo, e_try);
            entorno_destruir(e_try);

            if (hay_error) {
                Valor *err = valor_error;
                hay_error   = 0;
                valor_error = NULL;

                int capturado = 0;
                for (int i = 0; i < n->intentar.num_capturas && !capturado; i++) {
                    const char *tipo_cap = n->intentar.tipos_captura[i];

                    /* "error" captura cualquier tipo */
                    int coincide_tipo = (strcmp(tipo_cap, "error") == 0) ||
                                        (err->tipo == VAL_ERROR &&
                                         strcmp(err->error->tipo, tipo_cap) == 0);

                    if (coincide_tipo) {
                        /* Crear entorno con la variable del error */
                        Entorno *e_cap = entorno_crear(e);

                        /* Exponer el error como objeto con campos */
                        ObjetoLince *obj = calloc(1, sizeof(ObjetoLince));
                        obj->clase      = NULL;
                        obj->num_campos = 3;
                        obj->campos_nombres[0] = strdup("mensaje");
                        obj->campos_valores[0] = valor_texto(err->error->mensaje);
                        obj->campos_nombres[1] = strdup("tipo");
                        obj->campos_valores[1] = valor_texto(err->error->tipo);
                        obj->campos_nombres[2] = strdup("linea");
                        obj->campos_valores[2] = valor_numero(err->error->linea);

                        entorno_definir(e_cap, n->intentar.vars_captura[i],
                                        valor_objeto(obj), 0);

                        ejecutar(n->intentar.cuerpos_captura[i], e_cap);
                        entorno_destruir(e_cap);
                        capturado = 1;
                    }
                }

                if (!capturado) {
                    /* Ningún capturar coincidió — propagar */
                    hay_error   = 1;
                    valor_error = err;
                } else {
                    valor_destruir(err);
                }
            }

            /* finalmente — siempre se ejecuta */
            if (n->intentar.finalmente) {
                int error_guardado   = hay_error;
                Valor *err_guardado  = valor_error;
                int retorno_guardado = hay_retorno;
                Valor *ret_guardado  = valor_retorno;

                hay_error   = 0;
                valor_error = NULL;

                Entorno *e_fin = entorno_crear(e);
                ejecutar(n->intentar.finalmente, e_fin);
                entorno_destruir(e_fin);

                /* Restaurar señales */
                if (!hay_error) {
                    hay_error   = error_guardado;
                    valor_error = err_guardado;
                }
                if (!hay_retorno) {
                    hay_retorno   = retorno_guardado;
                    valor_retorno = ret_guardado;
                }
            }

            return valor_nulo();
        }

        case NODO_ACCESO: {
            Valor *obj = ejecutar(n->acceso.objeto, e);
            /* Acceso a constante de módulo */
            if (obj->tipo == VAL_DICCIONARIO && obj->es_modulo) {
                const char *campo = n->acceso.campo;
                for (int i = 0; i < obj->diccionario.cantidad; i++) {
                    if (strcmp(obj->diccionario.claves[i], campo) == 0) {
                        Valor *r = valor_copiar(obj->diccionario.valores[i]);
                        valor_destruir(obj);
                        return r;
                    }
                }
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   El módulo no tiene el campo '%s'.\n\n", campo);
                exit(1);
            }
            if (obj->tipo == VAL_OBJETO) {
                const char *campo = n->acceso.campo;
                for (int i = 0; i < obj->objeto->num_campos; i++) {
                    if (strcmp(obj->objeto->campos_nombres[i], campo) == 0) {
                        Valor *r = valor_copiar(obj->objeto->campos_valores[i]);
                        valor_destruir(obj);
                        return r;
                    }
                }
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   El objeto no tiene el campo '%s'.\n\n", campo);
                exit(1);
            }
            fprintf(stderr, "\n❌ Error:\n   Acceso a campo en un tipo que no es objeto.\n\n");
            exit(1);
        }

        case NODO_ESTO: {
            if (!esto_actual) {
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   'esto' solo se puede usar dentro de un método de una clase.\n\n");
                exit(1);
            }
            return valor_retener(esto_actual);
        }

        case NODO_CLASE: {
            ClaseLince *c = malloc(sizeof(ClaseLince));
            c->nombre         = strdup(n->clase.nombre);
            c->num_metodos    = n->clase.num_metodos;
            c->metodos        = malloc(sizeof(FuncionLince*) * c->num_metodos);
            c->padre          = NULL;
            c->interfaces     = malloc(sizeof(InterfazLince*) * 16);
            c->num_interfaces = 0;

            /* Resolver clase padre */
            if (n->clase.padre) {
                Valor *vpadre = entorno_obtener(e, n->clase.padre, n->linea);
                if (vpadre->tipo != VAL_CLASE) {
                    fprintf(stderr,
                        "\n❌ Error:\n"
                        "   '%s' no es una clase — no se puede extender.\n\n",
                        n->clase.padre);
                    exit(1);
                }
                c->padre = vpadre->clase;
                valor_destruir(vpadre);
            }

            /* Registrar métodos */
            for (int i = 0; i < c->num_metodos; i++) {
                Nodo *mn = n->clase.metodos[i];
                FuncionLince *f = malloc(sizeof(FuncionLince));
                f->nombre           = strdup(mn->funcion.nombre);
                f->parametros       = mn->funcion.parametros;
                f->num_parametros   = mn->funcion.num_parametros;
                f->tipo_retorno     = mn->funcion.tipo_retorno;
                f->cuerpo           = mn->funcion.cuerpo;
                f->entorno_closure  = e;
                c->metodos[i] = f;
            }

            /* Verificar interfaces */
            for (int i = 0; i < n->clase.num_interfaces; i++) {
                Valor *viface = entorno_obtener(e, n->clase.interfaces[i], n->linea);
                if (!viface || viface->tipo != VAL_DICCIONARIO || viface->es_modulo != 2) {
                    fprintf(stderr,
                        "\n❌ Error en línea %d:\n"
                        "   '%s' no es una interfaz.\n\n",
                        n->linea, n->clase.interfaces[i]);
                    exit(1);
                }
                InterfazLince *iface = (InterfazLince*)(uintptr_t)
                    (long long)viface->diccionario.valores[0]->numero;
                c->interfaces[c->num_interfaces++] = iface;

                for (int j = 0; j < iface->num_metodos; j++) {
                    if (!buscar_metodo(c, iface->metodos[j])) {
                        fprintf(stderr,
                            "\n❌ Error en línea %d:\n"
                            "   La clase '%s' dice implementar '%s'\n"
                            "   pero le falta el método '%s'.\n\n",
                            n->linea, c->nombre, iface->nombre, iface->metodos[j]);
                        exit(1);
                    }
                }
                valor_destruir(viface);
            }

            entorno_definir(e, c->nombre, valor_clase(c), 0);
            return valor_nulo();
        }

        case NODO_INSTANCIA: {
            Valor *vcls = entorno_obtener(e, n->instancia.clase, n->linea);
            if (vcls->tipo != VAL_CLASE) {
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   '%s' no es una clase.\n\n", n->instancia.clase);
                exit(1);
            }
            ClaseLince *cls = vcls->clase;

            /* ¿Es un tipo de error predefinido o sin constructor? */
            int es_error_pred = (cls->num_metodos == 0 &&
                (strncmp(cls->nombre, "Error", 5) == 0));

            if (es_error_pred) {
                /* Error("mensaje") — el primer argumento es el mensaje */
                char *msg = strdup("sin mensaje");
                if (n->instancia.num_argumentos >= 1) {
                    Valor *varg = ejecutar(n->instancia.argumentos[0], e);
                    free(msg);
                    msg = valor_a_texto(varg);
                    valor_destruir(varg);
                }
                Valor *verr = valor_crear_error(cls->nombre, msg, 0);
                free(msg);
                valor_destruir(vcls);
                return verr;
            }

            ObjetoLince *obj = calloc(1, sizeof(ObjetoLince));
            obj->clase      = cls;
            obj->num_campos = 0;
            Valor *vobj = valor_objeto(obj);

            /* Buscar y llamar 'crear' en la jerarquía */
            FuncionLince *f = buscar_metodo(cls, "crear");
            if (f) {
                if (n->instancia.num_argumentos != f->num_parametros) {
                    fprintf(stderr,
                        "\n❌ Error:\n"
                        "   '%s' espera %d argumento(s) en 'crear', recibió %d.\n\n",
                        cls->nombre, f->num_parametros, n->instancia.num_argumentos);
                    exit(1);
                }
                Entorno *fn_e = entorno_crear(f->entorno_closure);
                for (int j = 0; j < f->num_parametros; j++) {
                    Valor *arg = ejecutar(n->instancia.argumentos[j], e);
                    validar_tipo(arg, f->parametros[j].tipo, "parámetro", f->parametros[j].nombre, n->linea);
                    entorno_definir(fn_e, f->parametros[j].nombre, arg, 0);
                }
                Valor *esto_anterior   = esto_actual;
                ClaseLince *cls_ant    = clase_actual;
                esto_actual  = vobj;
                clase_actual = cls;
                ejecutar(f->cuerpo, fn_e);
                esto_actual  = esto_anterior;
                clase_actual = cls_ant;
                hay_retorno   = 0;
                valor_retorno = NULL;
                entorno_destruir(fn_e);
            }
            valor_destruir(vcls);
            return vobj;
        }

        case NODO_ASIGNACION_CAMPO: {
            Valor *val = ejecutar(n->asignacion_campo.valor, e);
            Valor *obj;

            if (n->asignacion_campo.objeto->tipo == NODO_ESTO) {
                obj = esto_actual;
            } else {
                obj = ejecutar(n->asignacion_campo.objeto, e);
            }

            if (!obj || obj->tipo != VAL_OBJETO) {
                fprintf(stderr,
                    "\n❌ Error:\n"
                    "   Solo puedes asignar campos a objetos.\n\n");
                exit(1);
            }

            const char *campo = n->asignacion_campo.campo;
            /* Buscar si el campo ya existe */
            for (int i = 0; i < obj->objeto->num_campos; i++) {
                if (strcmp(obj->objeto->campos_nombres[i], campo) == 0) {
                    valor_destruir(obj->objeto->campos_valores[i]);
                    obj->objeto->campos_valores[i] = val;
                    if (n->asignacion_campo.objeto->tipo != NODO_ESTO)
                        valor_destruir(obj);
                    return valor_nulo();
                }
            }
            /* Campo nuevo */
            int idx = obj->objeto->num_campos;
            obj->objeto->campos_nombres[idx] = strdup(campo);
            obj->objeto->campos_valores[idx] = val;
            obj->objeto->num_campos++;
            if (n->asignacion_campo.objeto->tipo != NODO_ESTO)
                valor_destruir(obj);
            return valor_nulo();
        }

        default:
            fprintf(stderr, "\n❌ Error interno: tipo de nodo desconocido (%d).\n\n", n->tipo);
            exit(1);
    }
}

/* ─────────────────────────────────────────
   API PÚBLICA
───────────────────────────────────────── */
/* Crea una clase de error predefinida */
/* Función nativa global — leer() */
static Valor *fn_leer(Valor **a, int n) {
    if (n >= 1 && a[0]->tipo == VAL_TEXTO)
        fprintf(stdout, "%s", a[0]->texto);
    fflush(stdout);
    char buf[4096] = "";
    if (fgets(buf, sizeof(buf), stdin)) {
        /* Quitar salto de línea */
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    }
    return valor_texto(buf);
}

/* Argumentos de línea de comandos — se rellenan desde main */
static Valor *args_globales = NULL;

void interprete_set_args(int argc, char *argv[]) {
    if (args_globales) return;
    args_globales = valor_lista_crear();
    for (int i = 0; i < argc; i++)
        lista_agregar(args_globales, valor_texto(argv[i]));
}

static Valor *fn_argumentos(Valor **a, int n) {
    (void)a; (void)n;
    if (!args_globales) return valor_lista_crear();
    return valor_retener(args_globales);
}

/* Función nativa global — mapear() */
static Valor *fn_mapear(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_LISTA || a[1]->tipo != VAL_FUNCION) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
            "mapear() espera una lista y una función", 0);
        return valor_nulo();
    }
    Valor        *lista     = a[0];
    FuncionLince *f         = a[1]->funcion;
    Valor        *resultado = valor_lista_crear();

    for (int i = 0; i < lista->lista.cantidad && !hay_error; i++) {
        Valor *elem = valor_copiar(lista->lista.elementos[i]);

        if (f->entorno_closure == NULL) {
            /* Función nativa */
            typedef Valor *(*FnNativa)(Valor**, int);
            FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
            Valor *r = fn(&elem, 1);
            valor_destruir(elem);
            lista_agregar(resultado, r ? r : valor_nulo());
        } else {
            Entorno *fn_e = entorno_crear(f->entorno_closure);
            if (f->num_parametros >= 1)
                entorno_definir(fn_e, f->parametros[0].nombre, elem, 0);
            ejecutar(f->cuerpo, fn_e);
            Valor *r = valor_nulo();
            if (hay_retorno) {
                r = valor_retorno; valor_retorno = NULL; hay_retorno = 0;
            }
            entorno_destruir(fn_e);
            lista_agregar(resultado, r);
        }
    }
    return resultado;
}

/* Función nativa global — filtrar() */
static Valor *fn_filtrar(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_LISTA || a[1]->tipo != VAL_FUNCION) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
            "filtrar() espera una lista y una función", 0);
        return valor_nulo();
    }
    Valor        *lista     = a[0];
    FuncionLince *f         = a[1]->funcion;
    Valor        *resultado = valor_lista_crear();

    for (int i = 0; i < lista->lista.cantidad && !hay_error; i++) {
        Valor *elem = lista->lista.elementos[i];
        int    ok   = 0;

        if (f->entorno_closure == NULL) {
            typedef Valor *(*FnNativa)(Valor**, int);
            FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
            Valor *r = fn(&elem, 1);
            ok = es_verdadero(r);
            valor_destruir(r);
        } else {
            Entorno *fn_e = entorno_crear(f->entorno_closure);
            if (f->num_parametros >= 1)
                entorno_definir(fn_e, f->parametros[0].nombre,
                    valor_copiar(elem), 0);
            ejecutar(f->cuerpo, fn_e);
            Valor *r = valor_nulo();
            if (hay_retorno) {
                r = valor_retorno; valor_retorno = NULL; hay_retorno = 0;
            }
            ok = es_verdadero(r);
            valor_destruir(r);
            entorno_destruir(fn_e);
        }

        if (ok)
            lista_agregar(resultado, valor_copiar(elem));
    }
    return resultado;
}

/* Función nativa global — reducir() */
static Valor *fn_reducir(Valor **a, int n) {
    (void)n;
    if (a[0]->tipo != VAL_LISTA || a[2]->tipo != VAL_FUNCION) {
        hay_error = 1;
        valor_error = valor_crear_error("ErrorTipo",
            "reducir() espera una lista, valor inicial y una función", 0);
        return valor_nulo();
    }
    Valor        *lista = a[0];
    Valor        *acc   = valor_copiar(a[1]);
    FuncionLince *f     = a[2]->funcion;

    for (int i = 0; i < lista->lista.cantidad && !hay_error; i++) {
        Valor *elem = lista->lista.elementos[i]; /* no copiamos */
        Valor *nuevo_acc;

        if (f->entorno_closure == NULL) {
            typedef Valor *(*FnNativa)(Valor**, int);
            FnNativa fn = (FnNativa)(uintptr_t)f->cuerpo;
            Valor *args2[2] = { acc, elem };
            nuevo_acc = fn(args2, 2);
        } else {
            Entorno *fn_e = entorno_crear(f->entorno_closure);
            if (f->num_parametros >= 1)
                entorno_definir(fn_e, f->parametros[0].nombre,
                    valor_copiar(acc), 0);
            if (f->num_parametros >= 2)
                entorno_definir(fn_e, f->parametros[1].nombre,
                    valor_copiar(elem), 0);
            ejecutar(f->cuerpo, fn_e);
            nuevo_acc = valor_nulo();
            if (hay_retorno) {
                nuevo_acc = valor_retorno;
                valor_retorno = NULL;
                hay_retorno   = 0;
            }
            entorno_destruir(fn_e);
        }
        valor_destruir(acc);
        acc = nuevo_acc;
    }
    return acc;
}
/* rango() — genera una secuencia de números */
static Valor *fn_rango(Valor **a, int n) {
    double inicio = 0, fin = 0, paso = 1;
    if (n == 1) {
        fin = a[0]->numero;
    } else if (n == 2) {
        inicio = a[0]->numero;
        fin    = a[1]->numero;
    } else if (n >= 3) {
        inicio = a[0]->numero;
        fin    = a[1]->numero;
        paso   = a[2]->numero;
    }
    if (paso == 0) {
        valor_error = valor_crear_error("Error", "El paso de rango() no puede ser cero.", 0);
        hay_error = 1;
        return valor_nulo();
    }
    Valor *gen = valor_generador_crear();
    int count = 0;
    for (double i = inicio;
         (paso > 0 ? i < fin : i > fin) && count < MAX_GENERADOR;
         i += paso, count++) {
        generador_agregar(gen, valor_numero(i));
    }
    return gen;
}

static Valor *fn_conv_numero(Valor **a, int n) {
    (void)n;
    Valor *v = a[0];
    switch (v->tipo) {
        case VAL_NUMERO:   return valor_numero(v->numero);
        case VAL_BOOLEANO: return valor_numero(v->booleano ? 1.0 : 0.0);
        case VAL_TEXTO: {
            char *fin;
            double d = strtod(v->texto, &fin);
            if (fin == v->texto || *fin != '\0') {
                hay_error = 1;
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "No se puede convertir \"%s\" a numero.", v->texto);
                valor_error = valor_crear_error("ErrorTipo", msg, 0);
                return valor_nulo();
            }
            return valor_numero(d);
        }
        case VAL_NULO: return valor_numero(0);
        default: {
            hay_error = 1;
            valor_error = valor_crear_error("ErrorTipo",
                "numero() no puede convertir este tipo.", 0);
            return valor_nulo();
        }
    }
}

/* Función nativa global — conversión texto() */
static Valor *fn_conv_texto(Valor **a, int n) {
    (void)n;
    Valor *v = a[0];
    char buf[256];
    switch (v->tipo) {
        case VAL_TEXTO:    return valor_texto(v->texto);
        case VAL_NUMERO:
            if (v->numero == (long long)v->numero)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->numero);
            else
                snprintf(buf, sizeof(buf), "%g", v->numero);
            return valor_texto(buf);
        case VAL_BOOLEANO: return valor_texto(v->booleano ? "verdadero" : "falso");
        case VAL_NULO:     return valor_texto("nulo");
        default: {
            hay_error = 1;
            valor_error = valor_crear_error("ErrorTipo",
                "texto() no puede convertir este tipo.", 0);
            return valor_nulo();
        }
    }
}

/* Función nativa global — conversión logico() */
static Valor *fn_conv_logico(Valor **a, int n) {
    (void)n;
    Valor *v = a[0];
    switch (v->tipo) {
        case VAL_BOOLEANO: return valor_booleano(v->booleano);
        case VAL_NUMERO:   return valor_booleano(v->numero != 0);
        case VAL_TEXTO:
            return valor_booleano(
                strcmp(v->texto, "verdadero") == 0 ||
                strcmp(v->texto, "true") == 0 ||
                (strlen(v->texto) > 0 && strcmp(v->texto, "falso") != 0 &&
                 strcmp(v->texto, "false") != 0 && strcmp(v->texto, "0") != 0));
        case VAL_NULO:     return valor_booleano(0);
        default:           return valor_booleano(1);
    }
}

/* Registra una función nativa en el entorno global */
static void registrar_fn_nativa(Entorno *e, const char *nombre,
                                 void *fn_ptr, int num_params) {
    FuncionLince *f = calloc(1, sizeof(FuncionLince));
    f->nombre         = strdup(nombre);
    f->num_parametros = num_params;
    f->cuerpo         = (Nodo*)(uintptr_t)fn_ptr;
    f->entorno_closure = NULL; /* señal de función nativa */
    entorno_definir(e, nombre, valor_funcion(f), 0);
}

static void registrar_error_predefinido(Entorno *e, const char *nombre) {
    ClaseLince *c = malloc(sizeof(ClaseLince));
    c->nombre      = strdup(nombre);
    c->num_metodos = 0;
    c->metodos     = NULL;
    c->padre       = NULL;
    entorno_definir(e, nombre, valor_clase(c), 0);
}

Interprete *interprete_crear(void) {
    Interprete *interp = malloc(sizeof(Interprete));
    interp->global = entorno_crear(NULL);

    /* Registrar tipos de error predefinidos */
    registrar_error_predefinido(interp->global, "Error");
    registrar_error_predefinido(interp->global, "ErrorTipo");
    registrar_error_predefinido(interp->global, "ErrorRango");
    registrar_error_predefinido(interp->global, "ErrorNulo");
    registrar_error_predefinido(interp->global, "ErrorMatematico");
    registrar_error_predefinido(interp->global, "ErrorArgumento");

    /* Registrar funciones de conversión de tipos */
    registrar_fn_nativa(interp->global, "a_numero", fn_conv_numero, 1);
    registrar_fn_nativa(interp->global, "a_texto",  fn_conv_texto,  1);
    registrar_fn_nativa(interp->global, "a_logico", fn_conv_logico, 1);

    /* Registrar funciones de entrada/salida y utilidades */
    registrar_fn_nativa(interp->global, "leer",        fn_leer,        -1);
    registrar_fn_nativa(interp->global, "argumentos",  fn_argumentos,   0);
    registrar_fn_nativa(interp->global, "mapear",      fn_mapear,       2);
    registrar_fn_nativa(interp->global, "filtrar",     fn_filtrar,      2);
    registrar_fn_nativa(interp->global, "reducir",     fn_reducir,      3);
    registrar_fn_nativa(interp->global, "rango",       fn_rango,       -1);

    /* Entrada/salida */
    registrar_fn_nativa(interp->global, "leer",       fn_leer,       -1);
    registrar_fn_nativa(interp->global, "argumentos", fn_argumentos, 0);

    /* Funciones de orden superior */
    registrar_fn_nativa(interp->global, "mapear",  fn_mapear,  2);
    registrar_fn_nativa(interp->global, "filtrar", fn_filtrar, 2);
    registrar_fn_nativa(interp->global, "reducir", fn_reducir, 3);

    return interp;
}

void interprete_destruir(Interprete *interp) {
    entorno_destruir(interp->global);
    free(interp);
}

void interprete_ejecutar(Interprete *interp, Nodo *programa) {
    ejecutar(programa, interp->global);
    if (hay_error && valor_error) {
        fprintf(stderr,
            "\n❌ %s: %s\n\n",
            valor_error->error->tipo,
            valor_error->error->mensaje);
        exit(1);
    }
}

/* Expuesta para el REPL */
Valor *ejecutar_nodo(Nodo *n, Entorno *e) {
    return ejecutar(n, e);
}

/* Versión con comillas para textos — para el REPL */
char *valor_a_texto_repl(Valor *v) {
    if (!v) return strdup("nulo");
    if (v->tipo == VAL_TEXTO) {
        size_t len = strlen(v->texto) + 3;
        char *buf = malloc(len);
        snprintf(buf, len, "\"%s\"", v->texto);
        return buf;
    }
    return valor_a_texto(v);
}

/* ─────────────────────────────────────────
   API pública para llamar funciones desde módulos
───────────────────────────────────────── */
Valor *interprete_llamar_funcion(Valor *fn, Valor **args, int nargs, Entorno *ent) {
    if (!fn || fn->tipo != VAL_FUNCION) return valor_nulo();
    FuncionLince *f = fn->funcion;
    if (!f) return valor_nulo();

    if (!f->entorno_closure && !f->cuerpo) return valor_nulo();

    if (f->entorno_closure == NULL) {
        /* Función nativa */
        typedef Valor *(*FnNativa)(Valor**, int);
        FnNativa fnnat = (FnNativa)(uintptr_t)f->cuerpo;
        return fnnat ? fnnat(args, nargs) : valor_nulo();
    }

    Entorno *fn_e = entorno_crear(f->entorno_closure ? f->entorno_closure : ent);
    for (int i = 0; i < f->num_parametros && i < nargs; i++)
        entorno_definir(fn_e, f->parametros[i].nombre, args[i], 0);
    ejecutar(f->cuerpo, fn_e);
    Valor *r = valor_nulo();
    if (hay_retorno) {
        r = valor_retorno; valor_retorno = NULL; hay_retorno = 0;
    }
    entorno_destruir(fn_e);
    return r;
}
