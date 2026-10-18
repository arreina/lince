/*
 * 🐆 Lince — corrutina.c
 * Ver corrutina.h. Va aparte para que windows.h no se mezcle con el resto.
 */
#include "corrutina.h"
#include <time.h>


/* Cuánta pila puede llegar a usar: la misma que el programa principal, para
   que quepan las mismas llamadas anidadas. Solo se reserva el espacio de
   direcciones; la memoria se usa según se va necesitando. */
#if defined(__SANITIZE_ADDRESS__)
#  define LINCE_ASAN 1
#  define CORRUTINA_PILA ((size_t)64 << 20)   /* make test-asan usa ulimit -s 65536 */
#else
#  define CORRUTINA_PILA ((size_t)8 << 20)
#endif

#ifdef LINCE_ASAN
/* AddressSanitizer tiene que saber que se cambia de pila */
void __sanitizer_start_switch_fiber(void **falsa, const void *base, size_t tam);
void __sanitizer_finish_switch_fiber(void *falsa, const void **base_ant, size_t *tam_ant);
#  define ASAN_EMPIEZA(f, b, t) __sanitizer_start_switch_fiber((f), (b), (t))
#  define ASAN_ACABA(f, b, t)   __sanitizer_finish_switch_fiber((f), (b), (t))
#else
#  define ASAN_EMPIEZA(f, b, t) ((void)0)
#  define ASAN_ACABA(f, b, t)   ((void)0)
#endif

#ifdef _WIN32
/* ── Windows: fibras ── */
#pragma GCC diagnostic ignored "-Warray-bounds"   /* GetCurrentFiber() de MinGW lo da en falso */
#include <windows.h>

struct Corrutina {
    void  *fibra;
    void  *volver;          /* la fibra de quien la hizo entrar */
    void (*fn)(void *);
    void  *arg;
    int    terminada;
};

static void WINAPI corrutina_inicio(void *p) {
    Corrutina *c = (Corrutina *)p;
    c->fn(c->arg);
    c->terminada = 1;
    SwitchToFiber(c->volver);           /* una fibra no puede acabar con return */
}

Corrutina *corrutina_crear(void (*fn)(void *), void *arg) {
    Corrutina *c = (Corrutina *)calloc(1, sizeof(Corrutina));
    if (!c) return NULL;
    c->fn = fn; c->arg = arg;
    c->fibra = CreateFiberEx(64 * 1024, CORRUTINA_PILA, FIBER_FLAG_FLOAT_SWITCH, corrutina_inicio, c);
    if (!c->fibra) { free(c); return NULL; }
    return c;
}

void corrutina_entrar(Corrutina *c) {
    if (!IsThreadAFiber()) ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
    c->volver = GetCurrentFiber();
    SwitchToFiber(c->fibra);
}

void corrutina_salir(Corrutina *c) { SwitchToFiber(c->volver); }

void corrutina_destruir(Corrutina *c) {
    if (!c) return;
    DeleteFiber(c->fibra);
    free(c);
}

#else
/* ── Linux y demás: ucontext ── */
#include <ucontext.h>
#include <sys/mman.h>
#include <unistd.h>

struct Corrutina {
    ucontext_t  ctx, volver;
    char       *base;           /* lo reservado, con la página de guarda al principio */
    char       *pila;           /* lo usable */
    size_t      tam;
    void      (*fn)(void *);
    void       *arg;
    int         empezada, terminada;
#ifdef LINCE_ASAN
    void       *falsa;
    const void *otra_base;      /* la pila de quien la hizo entrar */
    size_t      otra_tam;
#endif
};

#ifdef LINCE_ASAN
/* AddressSanitizer intercepta makecontext y swapcontext para limpiar la pila de destino y
   avisa por stderr de que no lo entiende del todo. Aquí ya se le cuenta cada
   cambio con ASAN_EMPIEZA / ASAN_ACABA y las pilas son nuevas, así que se usa
   el de la biblioteca de C directamente (y las pruebas no ven el aviso). */
#include <dlfcn.h>
static void *de_libc(const char *nombre) {
    void *libc = dlopen("libc.so.6", RTLD_LAZY | RTLD_NOLOAD);
    void *f = libc ? dlsym(libc, nombre) : NULL;
    return f ? f : dlsym(RTLD_NEXT, nombre);
}
static int cambiar_contexto(ucontext_t *desde, const ucontext_t *a) {
    static int (*real)(ucontext_t *, const ucontext_t *) = NULL;
    if (!real) real = (int (*)(ucontext_t *, const ucontext_t *))de_libc("swapcontext");
    return real(desde, a);
}
static void preparar_contexto(ucontext_t *c, void (*fn)(void)) {
    static void (*real)(ucontext_t *, void (*)(void), int, ...) = NULL;
    if (!real) real = (void (*)(ucontext_t *, void (*)(void), int, ...))de_libc("makecontext");
    real(c, fn, 0);
}
#  define swapcontext cambiar_contexto
#  define makecontext(c, fn, n) preparar_contexto((c), (fn))
#endif

/* Las pilas de las corrutinas que acaban se guardan para la siguiente: un
   juego crea y termina tareas sin parar */
#define CORRUTINA_PILAS_LIBRES 16     /* con AddressSanitizer no se reutilizan */
static char  *_co_pilas[CORRUTINA_PILAS_LIBRES];
static int    _co_npilas = 0;
static Corrutina *_co_naciendo = NULL;

static void corrutina_inicio(void) {
    Corrutina *c = _co_naciendo;
#ifdef LINCE_ASAN
    ASAN_ACABA(NULL, &c->otra_base, &c->otra_tam);
#endif
    c->fn(c->arg);
    c->terminada = 1;
    ASAN_EMPIEZA(NULL, c->otra_base, c->otra_tam);   /* NULL: esta pila ya no vuelve */
    setcontext(&c->volver);
}

Corrutina *corrutina_crear(void (*fn)(void *), void *arg) {
    Corrutina *c = (Corrutina *)calloc(1, sizeof(Corrutina));
    if (!c) return NULL;
    size_t pagina = (size_t)sysconf(_SC_PAGESIZE);
    if (_co_npilas > 0) {
        c->base = _co_pilas[--_co_npilas];
    } else {
        void *m = mmap(NULL, CORRUTINA_PILA, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (m == MAP_FAILED) { free(c); return NULL; }
        mprotect(m, pagina, PROT_NONE);       /* guarda: pasarse la cuelga, no pisa nada */
        c->base = (char *)m;
    }
    c->pila = c->base + pagina;
    c->tam  = CORRUTINA_PILA - pagina;
    c->fn = fn; c->arg = arg;
    return c;
}

void corrutina_entrar(Corrutina *c) {
    if (!c->empezada) {
        c->empezada = 1;
        getcontext(&c->ctx);
        c->ctx.uc_stack.ss_sp   = c->pila;
        c->ctx.uc_stack.ss_size = c->tam;
        c->ctx.uc_link          = NULL;
        makecontext(&c->ctx, corrutina_inicio, 0);
        _co_naciendo = c;
    }
    void *falsa = NULL;
    ASAN_EMPIEZA(&falsa, c->pila, c->tam);
    swapcontext(&c->volver, &c->ctx);
    ASAN_ACABA(falsa, NULL, NULL);
    (void)falsa;
}

void corrutina_salir(Corrutina *c) {
#ifdef LINCE_ASAN
    ASAN_EMPIEZA(&c->falsa, c->otra_base, c->otra_tam);
    swapcontext(&c->ctx, &c->volver);
    ASAN_ACABA(c->falsa, &c->otra_base, &c->otra_tam);
#else
    swapcontext(&c->ctx, &c->volver);
#endif
}

void corrutina_destruir(Corrutina *c) {
    if (!c) return;
#ifndef LINCE_ASAN
    if (_co_npilas < CORRUTINA_PILAS_LIBRES) _co_pilas[_co_npilas++] = c->base;
    else
#endif
    munmap(c->base, CORRUTINA_PILA);
    free(c);
}
#endif

int corrutina_terminada(Corrutina *c) { return c->terminada; }


double corrutina_reloj(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

void corrutina_dormir(double s) {
    if (s <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)(s * 1000));
#else
    struct timespec ts = { (time_t)s, (long)((s - (double)(time_t)s) * 1e9) };
    nanosleep(&ts, NULL);
#endif
}
