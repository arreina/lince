/*
 * 🐆 Lince — corrutina.c
 * Ver corrutina.h. Va aparte para que windows.h no se mezcle con el resto.
 */
#include "corrutina.h"
#include <time.h>
#include <string.h>


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
/* ── Linux y demás: ucontext; en Android (que no lo tiene), el cambio de
   pila escrito a mano para arm64 y x86_64 (LINCE_CORRUTINA_ASM) ── */
#if defined(__ANDROID__) && !defined(LINCE_CORRUTINA_ASM)
#  define LINCE_CORRUTINA_ASM 1
#endif
#ifndef LINCE_CORRUTINA_ASM
#include <ucontext.h>
#endif
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#ifdef LINCE_CORRUTINA_ASM
/* lince_cambiar_pila(&guardar, saltar): guarda los registros que una función
   tiene que conservar en la pila actual, deja en 'guardar' dónde quedó y
   sigue en la pila 'saltar' (preparada igual), volviendo de su
   lince_cambiar_pila. Una pila nueva se prepara para que esa vuelta sea la
   entrada a corrutina_inicio. */
void lince_cambiar_pila(void **guardar, void *saltar);
#if defined(__x86_64__)
__asm__(
    ".text\n"
    ".globl lince_cambiar_pila\n"
    ".hidden lince_cambiar_pila\n"
    ".type lince_cambiar_pila,@function\n"
    "lince_cambiar_pila:\n"
    "    pushq %rbp\n    pushq %rbx\n    pushq %r12\n    pushq %r13\n    pushq %r14\n    pushq %r15\n"
    "    subq $8, %rsp\n"
    "    stmxcsr (%rsp)\n"
    "    fnstcw 4(%rsp)\n"
    "    movq %rsp, (%rdi)\n"
    "    movq %rsi, %rsp\n"
    "    ldmxcsr (%rsp)\n"
    "    fldcw 4(%rsp)\n"
    "    addq $8, %rsp\n"
    "    popq %r15\n    popq %r14\n    popq %r13\n    popq %r12\n    popq %rbx\n    popq %rbp\n"
    "    ret\n"
    ".size lince_cambiar_pila, .-lince_cambiar_pila\n");

/* [control de coma flotante][r15 r14 r13 r12 rbx rbp][vuelta] */
static void *preparar_pila(char *tope, void (*fn)(void)) {
    uintptr_t a = ((uintptr_t)tope & ~(uintptr_t)15) - 16;    /* la vuelta, alineada a 16 */
    void **p = (void **)a;
    p[0] = (void *)fn;
    for (int i = 1; i <= 6; i++) p[-i] = 0;
    uint32_t *control = (uint32_t *)(a - 56);
    control[0] = 0x1F80;                                       /* MXCSR por defecto */
    control[1] = 0x037F;                                       /* x87 por defecto */
    return (void *)(a - 56);
}
#elif defined(__aarch64__)
__asm__(
    ".text\n"
    ".globl lince_cambiar_pila\n"
    ".hidden lince_cambiar_pila\n"
    ".type lince_cambiar_pila,%function\n"
    "lince_cambiar_pila:\n"
    "    sub sp, sp, #176\n"
    "    stp x19, x20, [sp, #0]\n    stp x21, x22, [sp, #16]\n    stp x23, x24, [sp, #32]\n"
    "    stp x25, x26, [sp, #48]\n    stp x27, x28, [sp, #64]\n    stp x29, x30, [sp, #80]\n"
    "    stp d8, d9, [sp, #96]\n    stp d10, d11, [sp, #112]\n    stp d12, d13, [sp, #128]\n"
    "    stp d14, d15, [sp, #144]\n"
    "    mov x2, sp\n"
    "    str x2, [x0]\n"
    "    mov sp, x1\n"
    "    ldp x19, x20, [sp, #0]\n    ldp x21, x22, [sp, #16]\n    ldp x23, x24, [sp, #32]\n"
    "    ldp x25, x26, [sp, #48]\n    ldp x27, x28, [sp, #64]\n    ldp x29, x30, [sp, #80]\n"
    "    ldp d8, d9, [sp, #96]\n    ldp d10, d11, [sp, #112]\n    ldp d12, d13, [sp, #128]\n"
    "    ldp d14, d15, [sp, #144]\n"
    "    add sp, sp, #176\n"
    "    ret\n"
    ".size lince_cambiar_pila, .-lince_cambiar_pila\n");

/* 176 bytes: x19..x28, x29 (marco), x30 (vuelta), d8..d15 y relleno */
static void *preparar_pila(char *tope, void (*fn)(void)) {
    uintptr_t a = ((uintptr_t)tope & ~(uintptr_t)15) - 176;
    memset((void *)a, 0, 176);
    ((void **)a)[11] = (void *)fn;                             /* x30, en el byte 88 */
    return (void *)a;
}
#else
#  error "LINCE_CORRUTINA_ASM solo está escrito para x86_64 y arm64"
#endif
#endif /* LINCE_CORRUTINA_ASM */

struct Corrutina {
#ifdef LINCE_CORRUTINA_ASM
    void       *sp, *sp_volver;     /* dónde quedó su pila y la de quien la hizo entrar */
#else
    ucontext_t  ctx, volver;
#endif
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
#ifdef LINCE_CORRUTINA_ASM
    lince_cambiar_pila(&c->sp, c->sp_volver);        /* ya no vuelve aquí */
#else
    ASAN_EMPIEZA(NULL, c->otra_base, c->otra_tam);   /* NULL: esta pila ya no vuelve */
    setcontext(&c->volver);
#endif
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

#ifdef LINCE_CORRUTINA_ASM
void corrutina_entrar(Corrutina *c) {
    if (!c->empezada) {
        c->empezada = 1;
        c->sp = preparar_pila(c->pila + c->tam, corrutina_inicio);
        _co_naciendo = c;
    }
    lince_cambiar_pila(&c->sp_volver, c->sp);
}

void corrutina_salir(Corrutina *c) { lince_cambiar_pila(&c->sp, c->sp_volver); }
#else
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
#endif /* LINCE_CORRUTINA_ASM */

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
