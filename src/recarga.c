/*
 * 🐆 Lince — recarga.c
 * Vigila los archivos del programa para la recarga en caliente (ver recarga.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "recarga.h"
#include "lsp.h"
#include "interprete.h"

#define MAX_VIGILADOS 128

typedef struct {
    char      *ruta;
    long long  mtime, tam;        /* lo que había en el código que corre */
    long long  visto_mtime, visto_tam;   /* lo visto en la última comprobación */
} Vigilado;

static Vigilado _vig[MAX_VIGILADOS];
static int      _nvig = 0;
static int      _activa = 0;
static int      _completa = 0;
static char     _error[600] = "";

void recarga_huella(const char *ruta, long long *mtime, long long *tam) {
#ifdef _WIN32
    /* stat() solo da segundos: dos guardados seguidos no se distinguirían */
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(ruta, GetFileExInfoStandard, &d)) { *mtime = -1; *tam = -1; return; }
    *mtime = (long long)d.ftLastWriteTime.dwHighDateTime << 32 | d.ftLastWriteTime.dwLowDateTime;
    *tam = (long long)d.nFileSizeHigh << 32 | d.nFileSizeLow;
    return;
#endif
    struct stat st;
    if (stat(ruta, &st) != 0) { *mtime = -1; *tam = -1; return; }
#if defined(__linux__)
    *mtime = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
#else
    *mtime = (long long)st.st_mtime;
#endif
    *tam = (long long)st.st_size;
}

void recarga_iniciar(const char *archivo) {
    _activa = 1;
    recarga_vigilar(archivo);
}

int recarga_activa(void) { return _activa; }

void recarga_vigilar(const char *ruta) {
    if (!_activa || !ruta) return;
    for (int i = 0; i < _nvig; i++) if (strcmp(_vig[i].ruta, ruta) == 0) return;
    if (_nvig == MAX_VIGILADOS) return;
    Vigilado *v = &_vig[_nvig++];
    v->ruta = strdup(ruta);
    recarga_huella(ruta, &v->mtime, &v->tam);
    v->visto_mtime = v->mtime;
    v->visto_tam = v->tam;
}

const char *recarga_error(void) { return _error[0] ? _error : NULL; }
int  recarga_completa(void) { return _completa; }
void recarga_pedir_completa(int si) { _completa = si; }

/* Un cambio cuenta cuando el archivo ya no se mueve: lo visto ahora es igual
   a lo de la comprobación anterior. Así no se lee a medio guardar. Vacío
   tampoco cuenta: muchos programas (cp, algunos editores) primero lo vacían y
   luego lo escriben, y sin pantalla el juego comprueba en cada fotograma, tan
   seguido que puede ver dos veces el archivo vacío y ejecutar un programa sin
   nada. */
int recarga_comprobar(int forzar) {
    if (!_activa) return 0;
    int cambiado = 0, quieto = 1;
    for (int i = 0; i < _nvig; i++) {
        long long m, t;
        recarga_huella(_vig[i].ruta, &m, &t);
        if (m != _vig[i].visto_mtime || t != _vig[i].visto_tam || t == 0) quieto = 0;
        _vig[i].visto_mtime = m;
        _vig[i].visto_tam = t;
        if (m != _vig[i].mtime || t != _vig[i].tam) cambiado = 1;
    }
    if (!forzar && (!cambiado || !quieto)) return 0;

    /* Comprobar la sintaxis de todos antes de tocar el programa que corre */
    _error[0] = '\0';
    for (int i = 0; i < _nvig; i++) {
        char *codigo = interprete_leer_fuente(_vig[i].ruta);
        if (!codigo) continue;          /* borrado: ya lo dirá el importar */
        int linea = 0;
        char msg[400];
        int malo = lsp_comprobar_texto(codigo, &linea, msg, sizeof msg);
        free(codigo);
        if (malo) {
            const char *nombre = _vig[i].ruta;
            for (const char *c = nombre; *c; c++) if (*c == '/' || *c == '\\') nombre = c + 1;
            snprintf(_error, sizeof _error, "%s, línea %d: %s", nombre, linea, msg);
            break;
        }
    }
    /* Lo que se ha visto ya no es «nuevo»: con error, se espera al siguiente
       guardado; sin él, es el código que va a correr. */
    for (int i = 0; i < _nvig; i++) { _vig[i].mtime = _vig[i].visto_mtime; _vig[i].tam = _vig[i].visto_tam; }
    if (_error[0]) {
        fprintf(stderr, "\n❌ Error de sintaxis en %s\n   (el juego sigue con el código anterior)\n\n", _error);
        return 0;
    }
    return 1;
}
