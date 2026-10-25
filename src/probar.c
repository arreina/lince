/*
 * 🐆 Lince — probar.c
 * 'lince probar [carpeta o archivo]…': ejecuta las pruebas y dice qué parte
 * del código han recorrido (cobertura).
 *
 * Una prueba es un archivo que se llama prueba_*.lince o *_prueba.lince
 * (normalmente usa el módulo 'pruebas'). Cada una corre en su propio proceso,
 * como si se lanzara con 'lince prueba_x.lince': pasa si termina bien.
 * Mientras corre, el intérprete apunta las líneas que ejecuta
 * (LINCE_COBERTURA, ver interprete.c), y al final se cuenta, para cada
 * archivo de código que han usado las pruebas, cuántas de sus sentencias se
 * han ejecutado alguna vez y cuáles no.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "lexer.h"
#include "parser.h"
#include "empaquetado.h"
#include "probar.h"
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

/* ── Las pruebas: los archivos que se llaman así ── */
static int es_prueba(const char *nombre) {
    size_t n = strlen(nombre);
    if (n < 7 || strcmp(nombre + n - 6, ".lince") != 0) return 0;
    if (strncmp(nombre, "prueba_", 7) == 0 || strcmp(nombre, "prueba.lince") == 0) return 1;
    return n >= 13 && strcmp(nombre + n - 13, "_prueba.lince") == 0;
}

static int es_carpeta(const char *ruta) {
    struct stat st;
    return stat(ruta, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

typedef struct { char **v; int n, cap; } Lista;
static void lista_poner(Lista *l, const char *s) {
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 16; l->v = realloc(l->v, sizeof(char *) * (size_t)l->cap); }
    l->v[l->n++] = strdup(s);
}
static int comparar(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* Busca pruebas en la carpeta y sus subcarpetas (no en las ocultas) */
static void buscar(const char *carpeta, Lista *out, int prof) {
    if (prof > 12) return;
#ifdef _WIN32
    char patron[1100];
    snprintf(patron, sizeof patron, "%s\\*", carpeta);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(patron, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const char *nom = fd.cFileName;
        if (nom[0] == '.') continue;
        char r[2048];
        snprintf(r, sizeof r, "%s/%s", carpeta, nom);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) buscar(r, out, prof + 1);
        else if (es_prueba(nom)) lista_poner(out, r);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(carpeta);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char r[2048];
        snprintf(r, sizeof r, "%s/%s", strcmp(carpeta, ".") == 0 ? "." : carpeta, e->d_name);
        if (es_carpeta(r)) buscar(r, out, prof + 1);
        else if (es_prueba(e->d_name)) lista_poner(out, r);
    }
    closedir(d);
#endif
}

/* La ruta completa, para reconocer el mismo archivo escrito de dos maneras */
static void absoluta(const char *ruta, char *out, size_t max) {
#ifdef _WIN32
    if (!_fullpath(out, ruta, max)) snprintf(out, max, "%s", ruta);
    for (char *p = out; *p; p++) if (*p == '\\') *p = '/';
#else
    char tmp[4096];
    if (realpath(ruta, tmp)) snprintf(out, max, "%s", tmp);
    else snprintf(out, max, "%s", ruta);
#endif
}

/* Lo que se enseña de una ruta: relativa a la carpeta de trabajo si está dentro */
static const char *bonita(const char *abs, const char *cwd) {
    size_t n = strlen(cwd);
    if (n && strncmp(abs, cwd, n) == 0 && abs[n] == '/') return abs + n + 1;
    return abs;
}

static char *leer(const char *ruta) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    rewind(f);
    char *b = malloc((size_t)t + 1);
    size_t r = fread(b, 1, (size_t)t, f);
    b[r] = '\0';
    fclose(f);
    return b;
}

/* ── Ejecutar una prueba: su salida va a un archivo, y se enseña si falla ── */
static int ejecutar(const char *exe, const char *prueba, const char *salida) {
    char cmd[8192];
#ifdef _WIN32
    /* cmd /c quita las comillas de fuera: el comando entero va entre otras */
    snprintf(cmd, sizeof cmd, "\"\"%s\" \"%s\" > \"%s\" 2>&1\"", exe, prueba, salida);
    return system(cmd);
#else
    snprintf(cmd, sizeof cmd, "'%s' '%s' > '%s' 2>&1 < /dev/null", exe, prueba, salida);
    int r = system(cmd);
    return r == -1 ? 1 : WIFEXITED(r) ? WEXITSTATUS(r) : 1;
#endif
}

/* ── Las líneas con sentencias de un archivo (las que se pueden ejecutar) ── */
static unsigned char *_ejecutables;
static int _cap_ejec;
static void apuntar(Nodo *s) {
    int l = s->linea_ini ? s->linea_ini : s->linea;
    if (l > 0 && l < _cap_ejec) _ejecutables[l] = 1;
}

typedef struct {
    char  ruta[2048];
    unsigned char *vista;     /* las líneas que se ejecutaron */
    int   cap;
} Cubierto;

static void rangos(char *out, size_t max, const unsigned char *ejec, const unsigned char *vista, int cap) {
    out[0] = '\0';
    size_t n = 0;
    int mostrados = 0;
    for (int l = 1; l < cap; l++) {
        if (!ejec[l] || vista[l]) continue;
        int fin = l;
        /* un tramo seguido: las líneas entre medias sin sentencias no lo cortan */
        for (int k = l + 1; k < cap; k++) {
            if (ejec[k] && vista[k]) break;
            if (ejec[k]) fin = k;
        }
        if (mostrados == 8) { n += (size_t)snprintf(out + n, max - n, ", …"); break; }
        if (fin > l) n += (size_t)snprintf(out + n, max - n, "%s%d-%d", mostrados ? ", " : "", l, fin);
        else n += (size_t)snprintf(out + n, max - n, "%s%d", mostrados ? ", " : "", l);
        if (n >= max) break;
        mostrados++;
        l = fin;
    }
}

int probar_ejecutar(int argc, char *argv[]) {
    Lista pruebas = {0};
    int sin_cobertura = 0, rutas = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--sin-cobertura") == 0) { sin_cobertura = 1; continue; }
        rutas++;
        if (es_carpeta(argv[i])) buscar(argv[i], &pruebas, 0);
        else lista_poner(&pruebas, argv[i]);
    }
    if (!rutas) buscar(".", &pruebas, 0);
    if (pruebas.n == 0) {
        fprintf(stderr, "\n❌ No hay pruebas. Una prueba es un archivo que se llama prueba_algo.lince\n"
                        "   o algo_prueba.lince (por ejemplo, con el módulo 'pruebas').\n\n");
        return 1;
    }
    qsort(pruebas.v, (size_t)pruebas.n, sizeof(char *), comparar);

    char exe[2048];
    if (!empaquetado_ruta_ejecutable(exe, sizeof exe)) { fprintf(stderr, "\n❌ No sé dónde está lince.\n\n"); return 1; }
    char tmpdir[1024];
#ifdef _WIN32
    if (!GetTempPathA(sizeof tmpdir, tmpdir)) snprintf(tmpdir, sizeof tmpdir, ".\\");
#else
    snprintf(tmpdir, sizeof tmpdir, "%s/", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
#endif
    char cob[1200], salida[1200];
    snprintf(cob, sizeof cob, "%slince_cobertura_%d.txt", tmpdir, (int)getpid());
    snprintf(salida, sizeof salida, "%slince_salida_%d.txt", tmpdir, (int)getpid());
    remove(cob);
#ifdef _WIN32
    if (!sin_cobertura) _putenv_s("LINCE_COBERTURA", cob);
#else
    if (!sin_cobertura) setenv("LINCE_COBERTURA", cob, 1);
#endif

    printf("\n🐆 Lince — Pruebas\n\n");
    fflush(stdout);
    int fallos = 0;
    for (int i = 0; i < pruebas.n; i++) {
        int r = ejecutar(exe, pruebas.v[i], salida);
        if (r == 0) printf("   ✅ %s\n", pruebas.v[i]);
        else {
            fallos++;
            printf("   ❌ %s\n", pruebas.v[i]);
            char *txt = leer(salida);
            if (txt) {
                /* lo que dijo, con sangría */
                for (char *l = txt; *l; ) {
                    char *fin = strchr(l, '\n');
                    if (fin) *fin = '\0';
                    if (*l) printf("      │ %s\n", l);
                    if (!fin) break;
                    l = fin + 1;
                }
                free(txt);
            }
        }
        fflush(stdout);
    }
    remove(salida);
    printf("\n   %d de %d pruebas bien%s\n", pruebas.n - fallos, pruebas.n, fallos ? "" : " 🎉");

    if (!sin_cobertura) {
        /* ── Cobertura: junta lo apuntado por todas las pruebas ── */
        char cwd[2048];
        absoluta(".", cwd, sizeof cwd);
        char *datos = leer(cob);
        remove(cob);
        Cubierto *c = NULL;
        int nc = 0;
        Lista excluir = {0};
        for (int i = 0; i < pruebas.n; i++) {
            char a[2048];
            absoluta(pruebas.v[i], a, sizeof a);
            lista_poner(&excluir, a);
        }
        for (char *l = datos; l && *l; ) {
            char *fin = strchr(l, '\n');
            if (fin) *fin = '\0';
            char *tab = strchr(l, '\t');
            if (tab) {
                *tab = '\0';
                char a[2048];
                absoluta(l, a, sizeof a);
                int es_prueba_ = 0;
                for (int k = 0; k < excluir.n && !es_prueba_; k++) es_prueba_ = strcmp(excluir.v[k], a) == 0;
                if (!es_prueba_) {
                    int k = 0;
                    while (k < nc && strcmp(c[k].ruta, a) != 0) k++;
                    if (k == nc) {
                        c = realloc(c, sizeof(Cubierto) * (size_t)(nc + 1));
                        memset(&c[k], 0, sizeof c[k]);
                        snprintf(c[k].ruta, sizeof c[k].ruta, "%s", a);
                        nc++;
                    }
                    for (char *p = tab + 1; *p; ) {
                        int ln = atoi(p);
                        if (ln > 0) {
                            if (ln >= c[k].cap) {
                                int cap = ln * 2 + 64;
                                c[k].vista = realloc(c[k].vista, (size_t)cap);
                                memset(c[k].vista + c[k].cap, 0, (size_t)(cap - c[k].cap));
                                c[k].cap = cap;
                            }
                            c[k].vista[ln] = 1;
                        }
                        char *coma = strchr(p, ',');
                        if (!coma) break;
                        p = coma + 1;
                    }
                }
            }
            if (!fin) break;
            l = fin + 1;
        }
        free(datos);

        if (nc > 0) {
            Cubierto **orden = malloc(sizeof(Cubierto *) * (size_t)nc);
            for (int i = 0; i < nc; i++) orden[i] = &c[i];
            for (int i = 1; i < nc; i++)                     /* por nombre */
                for (int j = i; j > 0 && strcmp(orden[j - 1]->ruta, orden[j]->ruta) > 0; j--) {
                    Cubierto *t = orden[j]; orden[j] = orden[j - 1]; orden[j - 1] = t;
                }
            printf("\n   Cobertura (las sentencias que han ejecutado las pruebas):\n\n");
            long total = 0, hechas = 0;
            for (int i = 0; i < nc; i++) {
                Cubierto *k = orden[i];
                char *codigo = leer(k->ruta);
                if (!codigo) continue;
                int lineas = 2;
                for (char *p = codigo; *p; p++) if (*p == '\n') lineas++;
                _cap_ejec = lineas;
                _ejecutables = calloc((size_t)lineas, 1);
                /* ya se ejecutó: está bien escrito y el parser no se para */
                Lexer *lx = lexer_crear(codigo);
                int nt = 0;
                Token *tk = lexer_tokenizar(lx, &nt);
                Parser *ps = parser_crear(tk, nt);
                parser_al_leer_sentencia = apuntar;
                nodo_destruir(parser_parsear(ps));
                parser_al_leer_sentencia = NULL;
                parser_destruir(ps);
                lexer_destruir(lx);
                if (k->cap < lineas) {
                    k->vista = realloc(k->vista, (size_t)lineas);
                    memset(k->vista + k->cap, 0, (size_t)(lineas - k->cap));
                    k->cap = lineas;
                }
                int ej = 0, vi = 0;
                for (int l = 1; l < lineas; l++) if (_ejecutables[l]) { ej++; if (k->vista[l]) vi++; }
                total += ej; hechas += vi;
                char faltan[600];
                rangos(faltan, sizeof faltan, _ejecutables, k->vista, lineas);
                int pc = ej ? (int)(100.0 * vi / ej) : 100;
                printf("   %3d%%  %s", pc, bonita(k->ruta, cwd));
                if (faltan[0]) printf("   sin probar: %s", faltan);
                printf("\n");
                free(_ejecutables);
                free(codigo);
            }
            printf("\n   Total: %d%% (%ld de %ld sentencias)\n", total ? (int)(100.0 * hechas / total) : 100, hechas, total);
            free(orden);
        }
        for (int i = 0; i < nc; i++) free(c[i].vista);
        free(c);
        for (int i = 0; i < excluir.n; i++) free(excluir.v[i]);
        free(excluir.v);
    }
    printf("\n");
    for (int i = 0; i < pruebas.n; i++) free(pruebas.v[i]);
    free(pruebas.v);
    return fallos ? 1 : 0;
}
