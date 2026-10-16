/* ─────────────────────────────────────────
   MÓDULO: juego  (se incluye desde modulos.c)
   Ventana 2D, dibujo, imágenes, texto, teclado, ratón, mando y sonido. Usa SDL2,
   cargada en tiempo de ejecución (dlopen / LoadLibrary): Lince no la necesita
   para compilarse, y si falta solo falla juego.abrir(). Las imágenes (stb_image)
   y el OGG (stb_vorbis) los lee el propio Lince.
   Estructura típica:
       juego.abrir("Mi juego", 800, 600)
       mientras juego.abierto() { ...dibujar...; juego.presentar() }
───────────────────────────────────────── */
#ifdef _WIN32
#define JG_ABRIR_LIB()   ((void *)LoadLibraryA("SDL2.dll"))
#define JG_SIMBOLO(h, n) ((void *)GetProcAddress((HMODULE)(h), n))
#else
static void *jg_abrir_lib(void) {
    static const char *const nombres[] = {
        "libSDL2-2.0.so.0", "libSDL2.so", "libSDL2-2.0.0.dylib", "libSDL2.dylib", NULL };
    for (int i = 0; nombres[i]; i++) {
        void *h = dlopen(nombres[i], RTLD_NOW);
        if (h) return h;
    }
    /* Si no está instalada, la que lleva el juego al lado (lince repartir) */
    char ruta[4096];
    if (!empaquetado_ruta_ejecutable(ruta, sizeof ruta - 32)) return NULL;
    char *barra = strrchr(ruta, '/');
    if (!barra) return NULL;
    strcpy(barra + 1, "libSDL2-2.0.so.0");
    return dlopen(ruta, RTLD_NOW);
}
#define JG_ABRIR_LIB()   jg_abrir_lib()
#define JG_SIMBOLO(h, n) dlsym(h, n)
#endif

typedef struct { int x, y, w, h; } JgRect;

static struct {
    int  (*Init)(unsigned);
    void (*Quit)(void);
    const char *(*GetError)(void);
    void *(*CrearVentana)(const char *, int, int, int, int, unsigned);
    void (*DestroyWindow)(void *);
    void *(*CreateRenderer)(void *, int, unsigned);
    void (*DestroyRenderer)(void *);
    int  (*SetDrawColor)(void *, unsigned char, unsigned char, unsigned char, unsigned char);
    int  (*SetBlend)(void *, int);
    int  (*RenderClear)(void *);
    int  (*FillRect)(void *, const JgRect *);
    int  (*DrawRect)(void *, const JgRect *);
    int  (*DrawLine)(void *, int, int, int, int);
    void (*RenderPresent)(void *);
    int  (*ReadPixels)(void *, const JgRect *, unsigned, void *, int);
    void *(*SurfaceFrom)(void *, int, int, int, int, unsigned, unsigned, unsigned, unsigned);
    void (*FreeSurface)(void *);
    void *(*RWFromFile)(const char *, const char *);
    int  (*SaveBMP_RW)(void *, void *, int);
    int  (*PollEvent)(void *);
    const unsigned char *(*KeyboardState)(int *);
    unsigned (*MouseState)(int *, int *);
    int  (*ScancodeFromName)(const char *);
    void (*Delay)(unsigned);
    void (*SetTitle)(void *, const char *);
    unsigned (*OpenAudio)(const char *, int, const void *, void *, int);
    void (*CloseAudio)(unsigned);
    void (*PauseAudio)(unsigned, int);
    void (*LockAudio)(unsigned);
    void (*UnlockAudio)(unsigned);
    void *(*TextureFromSurface)(void *, void *);
    int  (*QueryTexture)(void *, unsigned *, int *, int *, int *);
    void (*DestroyTexture)(void *);
    int  (*RenderCopyEx)(void *, void *, const JgRect *, const JgRect *, double, const void *, int);
    int  (*TextureAlpha)(void *, unsigned char);
    int  (*TextureColor)(void *, unsigned char, unsigned char, unsigned char);
    int  (*TextureBlend)(void *, int);
    void *(*RWFromConstMem)(const void *, int);
    void *(*LoadBMP_RW)(void *, int);
    void *(*LoadWAV_RW)(void *, int, void *, unsigned char **, unsigned *);
    void (*FreeWAV)(unsigned char *);
    void *(*NewAudioStream)(unsigned short, unsigned char, int, unsigned short, unsigned char, int);
    int  (*StreamPut)(void *, const void *, int);
    int  (*StreamFlush)(void *);
    int  (*StreamAvailable)(void *);
    int  (*StreamGet)(void *, void *, int);
    void (*FreeStream)(void *);
    int  (*NumJoysticks)(void);
    int  (*IsGameController)(int);
    void *(*ControllerOpen)(int);
    void (*ControllerClose)(void *);
    unsigned char (*ControllerButton)(void *, int);
    short (*ControllerAxis)(void *, int);
    int  (*SetFullscreen)(void *, unsigned);
    int  (*LogicalSize)(void *, int, int);
    int  (*OutputSize)(void *, int *, int *);
    /* opcionales */
    void (*WindowToLogical)(void *, int, int, float *, float *);   /* SDL ≥ 2.0.18 */
    int  (*ShowCursor)(int);
    int  (*RelativeMouse)(int);
    void (*StartTextInput)(void);
    void (*SetWindowSize)(void *, int, int);
} _jg;

/* Formato de audio de Lince: 16 bits con signo, estéreo, 44100 Hz */
#define JG_FRECUENCIA 44100
#define JG_S16        0x8010
#define JG_CANALES    2

typedef struct { int freq; unsigned short formato; unsigned char canales, silencio;
                 unsigned short muestras, pad; unsigned tam; void (*callback)(void *, unsigned char *, int);
                 void *datos; } JgAudioSpec;

/* mtime/tam: huella del archivo al cargarlo, para recargarlo si cambia */
typedef struct { void *tex; int ancho, alto; char *ruta; long long mtime, tam; } JgImagen;
typedef struct { short *datos; int muestras; char *ruta; long long mtime, tam; } JgSonido;   /* muestras por canal */
typedef struct {
    const short *datos; int muestras, pos, bucle, activa, id;
    float vol;
    short *propio;        /* pitidos: la voz es dueña de sus datos */
} JgVoz;

#define JG_MAX_IMAGENES 1024
#define JG_MAX_SONIDOS  256
#define JG_VOCES        32
#define JG_MANDOS       4
#define JG_BOTONES      15
#define JG_EJES         6
#define JG_ESCRITO      512       /* texto tecleado en un fotograma ('\b' = borrar) */
#define JG_ACCIONES     64
#define JG_ENTRADAS     16
#define JG_PARTICULAS   8192

/* Una entrada de una acción: tecla, botón del mando, botón del ratón o eje del mando
   hacia un lado. mando = -1: cualquiera. */
typedef struct { char tipo; int codigo, mando, signo; } JgEntrada;
typedef struct { char *nombre; JgEntrada e[JG_ENTRADAS]; int n; } JgAccion;

/* Partícula: se mueve, cae, cambia de tamaño y de color, y se apaga al acabar su vida */
typedef struct {
    float x, y, vx, vy, grav, vida, total, tam, tam_fin;
    unsigned char c[4], c_fin[4];
} JgParticula;

static struct {
    int cargada, abierta, ancho, alto, cierre;
    void *ventana, *render;
    unsigned audio;
    unsigned char teclas[512], previas[512];   /* estado actual y del fotograma anterior */
    unsigned raton, raton_prev;
    int rx, ry, rueda;
    double t_ultimo, dt, fps, t_fps;
    int cuadros;
    /* Modo de prueba (sin pantalla ni espera): LINCE_JUEGO_FOTOGRAMAS=N cierra tras N
       fotogramas con dt fijo; LINCE_JUEGO_PULSAR=a,espacio,... pulsa esas teclas en los
       fotogramas impares; LINCE_JUEGO_CLIC=x,y hace clic ahí en los fotogramas impares;
       LINCE_JUEGO_ESCRIBIR=texto lo teclea (con '<' como «borrar») la primera vez que
       el juego llama a editar() o texto_escrito();
       LINCE_JUEGO_CAPTURA=ruta.bmp guarda el último fotograma, y
       LINCE_JUEGO_FOTOS=carpeta todos (00000.bmp, 00001.bmp...);
       LINCE_JUEGO_SEMILLA=n fija la semilla de matematica.aleatorio(). */
    long limite, total;
    int pulsar[16], npulsar, clic_x, clic_y, hay_clic;
    double cam_x, cam_y, t_inicio;
    int completa;
    JgImagen imagenes[JG_MAX_IMAGENES];
    int nimagenes;
    JgSonido sonidos[JG_MAX_SONIDOS];
    int nsonidos;
    JgVoz voces[JG_VOCES];
    int siguiente_voz, voz_musica;
    float volumen;
    void *mandos[JG_MANDOS];
    unsigned char botones[JG_MANDOS][JG_BOTONES], botones_prev[JG_MANDOS][JG_BOTONES];
    float ejes[JG_MANDOS][JG_EJES], ejes_prev[JG_MANDOS][JG_EJES];
    int njoy;
    char escrito[JG_ESCRITO];            /* lo tecleado en el último fotograma */
    int nescrito;
    int mov_x, mov_y;                    /* cuánto se ha movido el ratón en el último fotograma */
    char *escribir_prueba;               /* modo de prueba: LINCE_JUEGO_ESCRIBIR */
    int pide_texto;                      /* el juego ha llamado a editar() o texto_escrito() */
    JgAccion acciones[JG_ACCIONES];
    int nacciones;
    int reabrir;                         /* recarga en caliente: abrir() reutiliza la ventana */
    int recargado;                       /* el programa se recargó conservando sus variables */
    JgParticula part[JG_PARTICULAS];
    int npart;
    /* escenas: la actual, la que viene (se cambia en presentar()) y el fundido */
    char escena[64], siguiente[64];
    int hay_siguiente, escena_nueva, fase;   /* fase 1: oscurece; 2: aclara */
    double t_escena, t_fundido, fundido;
    double t_vigilar;                    /* última comprobación de archivos cambiados */
} _jgs;

static int jg_tecla_codigo(const char *nombre);
static int jg_guardar_bmp(const char *ruta);
static void jg_leer_mandos(void);
static void jg_recoger_voces(void);
static int jg_escribir(const char *t, int x, int y, int e, int ancho);
static void jg_glifo(int ch, int px, int py, int e, int ac);
static int jg_vigilar(void);
static double jg_reloj(void);
static void jg_escenas(void);
static void jg_aviso_error(const char *msg, int pantalla);
static int jg_cargar_wav(const char *ruta, short **datos, int *muestras, char *error, size_t max);
static int jg_leer_textura(const char *ruta, void **tex, char *msg, size_t max);

static Valor *jg_error(const char *tipo, const char *msg) {
    hay_error = 1;
    valor_error = valor_error_nuevo(tipo, msg, 0);
    return valor_nulo();
}

static int jg_cargar(void) {
    if (_jg.Init) return 1;
    void *h = JG_ABRIR_LIB();
    if (!h) return 0;
#define JG_FN(campo, nombre) \
    *(void **)(&_jg.campo) = JG_SIMBOLO(h, nombre); if (!_jg.campo) { _jg.Init = NULL; return 0; }
    JG_FN(Init, "SDL_Init")
    JG_FN(Quit, "SDL_Quit")
    JG_FN(GetError, "SDL_GetError")
    JG_FN(CrearVentana, "SDL_CreateWindow")
    JG_FN(DestroyWindow, "SDL_DestroyWindow")
    JG_FN(CreateRenderer, "SDL_CreateRenderer")
    JG_FN(DestroyRenderer, "SDL_DestroyRenderer")
    JG_FN(SetDrawColor, "SDL_SetRenderDrawColor")
    JG_FN(SetBlend, "SDL_SetRenderDrawBlendMode")
    JG_FN(RenderClear, "SDL_RenderClear")
    JG_FN(FillRect, "SDL_RenderFillRect")
    JG_FN(DrawRect, "SDL_RenderDrawRect")
    JG_FN(DrawLine, "SDL_RenderDrawLine")
    JG_FN(RenderPresent, "SDL_RenderPresent")
    JG_FN(ReadPixels, "SDL_RenderReadPixels")
    JG_FN(SurfaceFrom, "SDL_CreateRGBSurfaceFrom")
    JG_FN(FreeSurface, "SDL_FreeSurface")
    JG_FN(RWFromFile, "SDL_RWFromFile")
    JG_FN(SaveBMP_RW, "SDL_SaveBMP_RW")
    JG_FN(PollEvent, "SDL_PollEvent")
    JG_FN(KeyboardState, "SDL_GetKeyboardState")
    JG_FN(MouseState, "SDL_GetMouseState")
    JG_FN(ScancodeFromName, "SDL_GetScancodeFromName")
    JG_FN(Delay, "SDL_Delay")
    JG_FN(SetTitle, "SDL_SetWindowTitle")
    JG_FN(OpenAudio, "SDL_OpenAudioDevice")
    JG_FN(CloseAudio, "SDL_CloseAudioDevice")
    JG_FN(PauseAudio, "SDL_PauseAudioDevice")
    JG_FN(LockAudio, "SDL_LockAudioDevice")
    JG_FN(UnlockAudio, "SDL_UnlockAudioDevice")
    JG_FN(TextureFromSurface, "SDL_CreateTextureFromSurface")
    JG_FN(QueryTexture, "SDL_QueryTexture")
    JG_FN(DestroyTexture, "SDL_DestroyTexture")
    JG_FN(RenderCopyEx, "SDL_RenderCopyEx")
    JG_FN(TextureAlpha, "SDL_SetTextureAlphaMod")
    JG_FN(TextureColor, "SDL_SetTextureColorMod")
    JG_FN(TextureBlend, "SDL_SetTextureBlendMode")
    JG_FN(RWFromConstMem, "SDL_RWFromConstMem")
    JG_FN(LoadBMP_RW, "SDL_LoadBMP_RW")
    JG_FN(LoadWAV_RW, "SDL_LoadWAV_RW")
    JG_FN(FreeWAV, "SDL_FreeWAV")
    JG_FN(NewAudioStream, "SDL_NewAudioStream")
    JG_FN(StreamPut, "SDL_AudioStreamPut")
    JG_FN(StreamFlush, "SDL_AudioStreamFlush")
    JG_FN(StreamAvailable, "SDL_AudioStreamAvailable")
    JG_FN(StreamGet, "SDL_AudioStreamGet")
    JG_FN(FreeStream, "SDL_FreeAudioStream")
    JG_FN(NumJoysticks, "SDL_NumJoysticks")
    JG_FN(IsGameController, "SDL_IsGameController")
    JG_FN(ControllerOpen, "SDL_GameControllerOpen")
    JG_FN(ControllerClose, "SDL_GameControllerClose")
    JG_FN(ControllerButton, "SDL_GameControllerGetButton")
    JG_FN(ControllerAxis, "SDL_GameControllerGetAxis")
    JG_FN(SetFullscreen, "SDL_SetWindowFullscreen")
    JG_FN(LogicalSize, "SDL_RenderSetLogicalSize")
    JG_FN(OutputSize, "SDL_GetRendererOutputSize")
    *(void **)(&_jg.WindowToLogical) = JG_SIMBOLO(h, "SDL_RenderWindowToLogical");
    *(void **)(&_jg.ShowCursor) = JG_SIMBOLO(h, "SDL_ShowCursor");
    *(void **)(&_jg.RelativeMouse) = JG_SIMBOLO(h, "SDL_SetRelativeMouseMode");
    *(void **)(&_jg.StartTextInput) = JG_SIMBOLO(h, "SDL_StartTextInput");
    *(void **)(&_jg.SetWindowSize) = JG_SIMBOLO(h, "SDL_SetWindowSize");
    return 1;
}

static double jg_ahora(void) {
#ifdef _WIN32
    static LARGE_INTEGER frecuencia;
    LARGE_INTEGER t;
    if (!frecuencia.QuadPart) QueryPerformanceFrequency(&frecuencia);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)frecuencia.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

static int jg_num(Valor **a, int n, int i, double *out, const char *fn) {
    if (i < n && a[i]->tipo == VAL_NUMERO) { *out = a[i]->numero; return 1; }
    char msg[96];
    snprintf(msg, sizeof(msg), "%s(): el argumento %d debe ser un número", fn, i + 1);
    jg_error("ErrorTipo", msg);
    return 0;
}

static int jg_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Color en a[i]: lista [r, g, b] o [r, g, b, a], o texto "#rrggbb" / "#rrggbbaa".
   Si falta, usa blanco. Devuelve 0 (con error puesto) si es inválido. */
static int jg_color(Valor **a, int n, int i, unsigned char c[4]) {
    c[0] = c[1] = c[2] = c[3] = 255;
    if (i >= n || a[i]->tipo == VAL_NULO) return 1;
    Valor *v = a[i];
    if (v->tipo == VAL_LISTA && (v->lista.cantidad == 3 || v->lista.cantidad == 4)) {
        for (int k = 0; k < v->lista.cantidad; k++) {
            Valor *e = v->lista.elementos[k];
            if (e->tipo != VAL_NUMERO) goto malo;
            double d = e->numero;
            c[k] = d < 0 ? 0 : d > 255 ? 255 : (unsigned char)d;
        }
        return 1;
    }
    if (v->tipo == VAL_TEXTO && v->texto[0] == '#') {
        size_t len = strlen(v->texto) - 1;
        if (len == 6 || len == 8) {
            for (size_t k = 0; k < len / 2; k++) {
                int h = jg_hex(v->texto[1 + 2 * k]), l = jg_hex(v->texto[2 + 2 * k]);
                if (h < 0 || l < 0) goto malo;
                c[k] = (unsigned char)(h * 16 + l);
            }
            return 1;
        }
    }
malo:
    jg_error("ErrorTipo", "el color debe ser [r, g, b], [r, g, b, a] o texto '#rrggbb'");
    return 0;
}

#define JG_ABIERTA() do { if (!_jgs.abierta) return jg_error("Error", "la ventana no está abierta: llama a juego.abrir() primero"); } while (0)

static void jg_pintar(const unsigned char c[4]) {
    _jg.SetBlend(_jgs.render, c[3] < 255 ? 1 : 0);
    _jg.SetDrawColor(_jgs.render, c[0], c[1], c[2], c[3]);
}

static void jg_cerrar_todo(void) {
    if (_jgs.audio) { _jg.CloseAudio(_jgs.audio); _jgs.audio = 0; }
    for (int i = 0; i < JG_VOCES; i++) free(_jgs.voces[i].propio);
    memset(_jgs.voces, 0, sizeof _jgs.voces);
    for (int i = 0; i < _jgs.nsonidos; i++) { free(_jgs.sonidos[i].datos); free(_jgs.sonidos[i].ruta); }
    _jgs.nsonidos = 0;
    for (int i = 0; i < _jgs.nimagenes; i++) { _jg.DestroyTexture(_jgs.imagenes[i].tex); free(_jgs.imagenes[i].ruta); }
    _jgs.nimagenes = 0;
    for (int i = 0; i < JG_MANDOS; i++) if (_jgs.mandos[i]) { _jg.ControllerClose(_jgs.mandos[i]); _jgs.mandos[i] = NULL; }
    if (_jgs.render) _jg.DestroyRenderer(_jgs.render);
    if (_jgs.ventana) _jg.DestroyWindow(_jgs.ventana);
    _jgs.render = _jgs.ventana = NULL;
    if (_jgs.abierta) _jg.Quit();
    _jgs.abierta = 0;
}

/* Variable de entorno en UTF-8 (en Windows getenv usa la página de códigos local) */
static char *jg_entorno_utf8(const char *nombre) {
#ifdef _WIN32
    wchar_t wnombre[64];
    if (!MultiByteToWideChar(CP_UTF8, 0, nombre, -1, wnombre, 64)) return NULL;
    const wchar_t *w = _wgetenv(wnombre);
    if (!w) return NULL;
    int largo = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *t = largo > 0 ? malloc((size_t)largo) : NULL;
    if (t) WideCharToMultiByte(CP_UTF8, 0, w, -1, t, largo, NULL, NULL);
    return t;
#else
    const char *e = getenv(nombre);
    return e ? strdup(e) : NULL;
#endif
}

/* abrir(titulo, ancho, alto) */
static Valor *fn_jg_abrir(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "abrir() espera (título, ancho, alto)");
    double w = 800, h = 600;
    if (n > 1 && !jg_num(a, n, 1, &w, "abrir")) return valor_nulo();
    if (n > 2 && !jg_num(a, n, 2, &h, "abrir")) return valor_nulo();
    if (w < 1 || h < 1 || w > 16384 || h > 16384) return jg_error("Error", "abrir(): tamaño de ventana no válido");
    if (_jgs.abierta && _jgs.reabrir) {          /* programa recargado: la ventana sigue */
        _jgs.reabrir = 0;
        _jg.SetTitle(_jgs.ventana, a[0]->texto);
        if ((int)w != _jgs.ancho || (int)h != _jgs.alto) {
            if (_jg.SetWindowSize && !_jgs.completa) _jg.SetWindowSize(_jgs.ventana, (int)w, (int)h);
            _jg.LogicalSize(_jgs.render, (int)w, (int)h);
            _jgs.ancho = (int)w; _jgs.alto = (int)h;
        }
        _jgs.cam_x = _jgs.cam_y = 0;
        _jgs.cierre = 0;
        return valor_nulo();
    }
    if (_jgs.abierta) return jg_error("Error", "la ventana ya está abierta");
    if (!jg_cargar())
        return jg_error("ErrorJuego", "no se encuentra SDL2: instálala (por ejemplo 'sudo apt install libsdl2-2.0-0')");
    /* vídeo, y si se puede audio y mandos (0x10 audio, 0x200 joystick, 0x2000 mando) */
    if (_jg.Init(0x20 | 0x10 | 0x200 | 0x2000) != 0 && _jg.Init(0x20 | 0x10) != 0 && _jg.Init(0x20) != 0) {
        char msg[400];
        snprintf(msg, sizeof(msg), "no se pudo iniciar SDL2: %s", _jg.GetError());
        return jg_error("ErrorJuego", msg);
    }
    _jgs.abierta = 1;
    _jgs.ventana = _jg.CrearVentana(a[0]->texto, 0x2FFF0000, 0x2FFF0000, (int)w, (int)h, 0);
    if (_jgs.ventana) {
        _jgs.render = _jg.CreateRenderer(_jgs.ventana, -1, 2 | 4);       /* acelerado + vsync */
        if (!_jgs.render) _jgs.render = _jg.CreateRenderer(_jgs.ventana, -1, 1);   /* por software */
    }
    if (!_jgs.ventana || !_jgs.render) {
        char msg[400];
        snprintf(msg, sizeof(msg), "no se pudo crear la ventana: %s", _jg.GetError());
        jg_cerrar_todo();
        return jg_error("ErrorJuego", msg);
    }
    _jg.LogicalSize(_jgs.render, (int)w, (int)h);    /* en pantalla completa se escala */
    memset(_jgs.teclas, 0, sizeof(_jgs.teclas));
    memset(_jgs.previas, 0, sizeof(_jgs.previas));
    memset(_jgs.botones, 0, sizeof(_jgs.botones));
    memset(_jgs.botones_prev, 0, sizeof(_jgs.botones_prev));
    memset(_jgs.ejes, 0, sizeof(_jgs.ejes));
    memset(_jgs.ejes_prev, 0, sizeof(_jgs.ejes_prev));
    _jgs.nescrito = 0; _jgs.mov_x = _jgs.mov_y = 0;
    if (_jg.StartTextInput) _jg.StartTextInput();          /* recibir texto tecleado (tildes, ñ…) */
    _jgs.cam_x = _jgs.cam_y = 0;
    _jgs.completa = 0;
    _jgs.njoy = -1;
    _jgs.volumen = 1;
    _jgs.voz_musica = 0;
    _jgs.t_inicio = jg_ahora();
    _jgs.ancho = (int)w; _jgs.alto = (int)h; _jgs.cierre = 0;
    _jgs.limite = 0; _jgs.total = 0; _jgs.npulsar = 0; _jgs.hay_clic = 0;
    const char *e = getenv("LINCE_JUEGO_FOTOGRAMAS");
    if (e && atol(e) > 0) _jgs.limite = atol(e);
    e = getenv("LINCE_JUEGO_PULSAR");
    if (e) {
        char buf[200];
        snprintf(buf, sizeof buf, "%s", e);
        for (char *t = strtok(buf, ","); t && _jgs.npulsar < 16; t = strtok(NULL, ",")) {
            int c = jg_tecla_codigo(t);
            if (c >= 0) _jgs.pulsar[_jgs.npulsar++] = c;
        }
    }
    free(_jgs.escribir_prueba);
    _jgs.escribir_prueba = jg_entorno_utf8("LINCE_JUEGO_ESCRIBIR");
    _jgs.pide_texto = 0;
    e = getenv("LINCE_JUEGO_CLIC");
    if (e && sscanf(e, "%d,%d", &_jgs.clic_x, &_jgs.clic_y) == 2) _jgs.hay_clic = 1;
    e = getenv("LINCE_JUEGO_SEMILLA");
    if (e && *e) {                                   /* prueba: partidas repetibles */
        Valor *s = valor_numero(atof(e));
        valor_destruir(fn_mat_semilla(&s, 1));
        valor_destruir(s);
    }
    _jgs.dt = 1.0 / 60; _jgs.t_ultimo = jg_ahora(); _jgs.t_fps = _jgs.t_ultimo; _jgs.cuadros = 0; _jgs.fps = 0;
    return valor_nulo();
}

static Valor *fn_jg_abierto(Valor **a, int n) {
    (void)a; (void)n;
    return valor_booleano(_jgs.abierta && !_jgs.cierre);
}

/* salir(): pide terminar el bucle (abierto() pasa a falso) sin destruir la ventana aún */
static Valor *fn_jg_salir(Valor **a, int n) {
    (void)a; (void)n;
    _jgs.cierre = 1;
    return valor_nulo();
}

static Valor *fn_jg_cerrar(Valor **a, int n) {
    (void)a; (void)n;
    if (_jgs.abierta) jg_cerrar_todo();
    return valor_nulo();
}

static Valor *fn_jg_titulo(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "titulo() espera un texto");
    _jg.SetTitle(_jgs.ventana, a[0]->texto);
    return valor_nulo();
}

static Valor *fn_jg_ancho(Valor **a, int n) { (void)a; (void)n; return valor_numero(_jgs.ancho); }
static Valor *fn_jg_alto(Valor **a, int n)  { (void)a; (void)n; return valor_numero(_jgs.alto); }
static Valor *fn_jg_dt(Valor **a, int n)    { (void)a; (void)n; return valor_numero(_jgs.dt); }
static Valor *fn_jg_fps(Valor **a, int n)   { (void)a; (void)n; return valor_numero(_jgs.fps); }

/* limpiar([color]) */
static Valor *fn_jg_limpiar(Valor **a, int n) {
    JG_ABIERTA();
    unsigned char c[4] = {0, 0, 0, 255};
    if (n > 0 && !jg_color(a, n, 0, c)) return valor_nulo();
    jg_pintar(c);
    _jg.RenderClear(_jgs.render);
    return valor_nulo();
}

/* rectangulo(x, y, ancho, alto[, color]) y contorno(...) */
static Valor *jg_rect(Valor **a, int n, int relleno, const char *fn) {
    JG_ABIERTA();
    double v[4];
    for (int i = 0; i < 4; i++) if (!jg_num(a, n, i, &v[i], fn)) return valor_nulo();
    unsigned char c[4];
    if (!jg_color(a, n, 4, c)) return valor_nulo();
    JgRect r = {(int)floor(v[0] - _jgs.cam_x), (int)floor(v[1] - _jgs.cam_y), (int)ceil(v[2]), (int)ceil(v[3])};
    jg_pintar(c);
    if (relleno) _jg.FillRect(_jgs.render, &r); else _jg.DrawRect(_jgs.render, &r);
    return valor_nulo();
}
static Valor *fn_jg_rectangulo(Valor **a, int n) { return jg_rect(a, n, 1, "rectangulo"); }
static Valor *fn_jg_contorno(Valor **a, int n)   { return jg_rect(a, n, 0, "contorno"); }

/* linea(x1, y1, x2, y2[, color]) */
static Valor *fn_jg_linea(Valor **a, int n) {
    JG_ABIERTA();
    double v[4];
    for (int i = 0; i < 4; i++) if (!jg_num(a, n, i, &v[i], "linea")) return valor_nulo();
    unsigned char c[4];
    if (!jg_color(a, n, 4, c)) return valor_nulo();
    jg_pintar(c);
    _jg.DrawLine(_jgs.render, (int)(v[0] - _jgs.cam_x), (int)(v[1] - _jgs.cam_y),
                 (int)(v[2] - _jgs.cam_x), (int)(v[3] - _jgs.cam_y));
    return valor_nulo();
}

/* circulo(x, y, radio[, color]): relleno, con una línea horizontal por fila */
static Valor *fn_jg_circulo(Valor **a, int n) {
    JG_ABIERTA();
    double v[3];
    for (int i = 0; i < 3; i++) if (!jg_num(a, n, i, &v[i], "circulo")) return valor_nulo();
    unsigned char c[4];
    if (!jg_color(a, n, 3, c)) return valor_nulo();
    jg_pintar(c);
    int cx = (int)(v[0] - _jgs.cam_x), cy = (int)(v[1] - _jgs.cam_y), r = (int)v[2];
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)floor(sqrt((double)r * r - (double)dy * dy) + 0.5);
        JgRect fila = {cx - dx, cy + dy, 2 * dx + 1, 1};
        _jg.FillRect(_jgs.render, &fila);
    }
    return valor_nulo();
}

/* ── Texto: fuente de mapa de bits 5×7 (ASCII 32..95; minúsculas como mayúsculas) ── */
static const unsigned char JG_FUENTE[64][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
};

/* Decodifica el siguiente carácter UTF-8 de *p; devuelve el carácter ASCII base
   (vocales acentuadas, ü, ñ, ¿, ¡ incluidos) y pone *acento a 1 si lleva marca. */
static int jg_siguiente(const unsigned char **p, int *acento) {
    const unsigned char *s = *p;
    *acento = 0;
    if (s[0] < 0x80) { *p = s + 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        int cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p = s + 2;
        switch (cp) {
            case 0xE1: case 0xC1: *acento = 1; return 'A';
            case 0xE9: case 0xC9: *acento = 1; return 'E';
            case 0xED: case 0xCD: *acento = 1; return 'I';
            case 0xF3: case 0xD3: *acento = 1; return 'O';
            case 0xFA: case 0xDA: case 0xFC: case 0xDC: *acento = 1; return 'U';
            case 0xF1: case 0xD1: *acento = 1; return 'N';
            case 0xBF: return '?';
            case 0xA1: return '!';
            default: return '?';
        }
    }
    int k = 1;                                    /* secuencia más larga: se salta entera */
    while ((s[k] & 0xC0) == 0x80) k++;
    *p = s + k;
    return '?';
}

static int jg_ancho_texto(const char *t, int escala) {
    int cuenta = 0;
    const unsigned char *p = (const unsigned char *)t;
    int ac;
    while (*p) { jg_siguiente(&p, &ac); cuenta++; }
    return cuenta ? (cuenta * 6 - 1) * escala : 0;
}

/* texto(contenido, x, y[, escala[, color]]) */
static Valor *fn_jg_texto(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 3 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "texto() espera (contenido, x, y[, escala[, color]])");
    double x, y, esc = 2;
    if (!jg_num(a, n, 1, &x, "texto") || !jg_num(a, n, 2, &y, "texto")) return valor_nulo();
    if (n > 3 && !jg_num(a, n, 3, &esc, "texto")) return valor_nulo();
    unsigned char c[4];
    if (!jg_color(a, n, 4, c)) return valor_nulo();
    int e = esc < 1 ? 1 : esc > 64 ? 64 : (int)esc;
    jg_pintar(c);
    jg_escribir(a[0]->texto, (int)(x - _jgs.cam_x), (int)(y - _jgs.cam_y), e, -1);
    return valor_nulo();
}

/* Dibuja 't' con la fuente de Lince en (x, y) a escala e, en el color actual.
   Con 'ancho' > 0 parte las líneas (por los espacios) para no pasar de ese
   ancho. Devuelve la altura ocupada. */
static int jg_escribir(const char *t, int x, int y, int e, int ancho) {
    int px = x, py = y, ac;
    int caben = ancho > 0 ? ancho / (6 * e) : 0;
    const unsigned char *p = (const unsigned char *)t;
    while (*p) {
        if (caben && px == x) {             /* principio de línea: ¿cabe la palabra? */
            const unsigned char *q = p, *corte = NULL;
            int nc = 0;
            while (*q && *q != '\n' && nc < caben) {
                if (*q == ' ') corte = q;
                q += utf8_tam((const char *)q);
                nc++;
            }
            if (*q && *q != '\n' && nc == caben) {
                const unsigned char *fin = corte && corte > p ? corte : q;
                while (p < fin) {
                    int ch = jg_siguiente(&p, &ac);
                    jg_glifo(ch, px, py, e, ac);
                    px += 6 * e;
                }
                if (*p == ' ') p++;
                px = x; py += 9 * e;
                continue;
            }
        }
        int ch = jg_siguiente(&p, &ac);
        if (ch == '\n') { px = x; py += 9 * e; continue; }
        jg_glifo(ch, px, py, e, ac);
        px += 6 * e;
    }
    return py - y + 9 * e;
}

static void jg_glifo(int ch, int px, int py, int e, int ac) {
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch == '{') ch = '(';                  /* la fuente no tiene llaves */
    if (ch == '}') ch = ')';
    if (ch < 32 || ch > 95) ch = '?';
    const unsigned char *g = JG_FUENTE[ch - 32];
    for (int col = 0; col < 5; col++) {
        int fila = 0;
        while (fila < 7) {
            if (!(g[col] >> fila & 1)) { fila++; continue; }
            int ini = fila;
            while (fila < 7 && (g[col] >> fila & 1)) fila++;
            JgRect r = {px + col * e, py + ini * e, e, (fila - ini) * e};
            _jg.FillRect(_jgs.render, &r);
        }
    }
    if (ac) { JgRect r = {px + 2 * e, py - 2 * e, 2 * e, e}; _jg.FillRect(_jgs.render, &r); }
}

/* ancho_texto(contenido[, escala]): para centrar o alinear */
static Valor *fn_jg_ancho_texto(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "ancho_texto() espera un texto");
    double esc = 2;
    if (n > 1 && !jg_num(a, n, 1, &esc, "ancho_texto")) return valor_nulo();
    return valor_numero(jg_ancho_texto(a[0]->texto, esc < 1 ? 1 : (int)esc));
}

/* ── Entrada ── */
static int jg_tecla_codigo(const char *nombre) {
    static const struct { const char *n; int c; } T[] = {
        {"arriba", 82}, {"abajo", 81}, {"izquierda", 80}, {"derecha", 79},
        {"espacio", 44}, {"entrar", 40}, {"escape", 41}, {"borrar", 42}, {"tab", 43},
        {"mayus", 225}, {"ctrl", 224}, {"alt", 226},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(nombre, T[i].n) == 0) return T[i].c;
    if (nombre[0] && !nombre[1]) {
        char c = nombre[0];
        if (c >= 'a' && c <= 'z') return 4 + (c - 'a');
        if (c >= 'A' && c <= 'Z') return 4 + (c - 'A');
        if (c >= '1' && c <= '9') return 30 + (c - '1');
        if (c == '0') return 39;
    }
    if ((nombre[0] == 'f' || nombre[0] == 'F') && nombre[1]) {
        int k = atoi(nombre + 1);
        if (k >= 1 && k <= 12) return 57 + k;
    }
    int c = _jg.ScancodeFromName(nombre);
    return c > 0 && c < 512 ? c : -1;
}

/* modo 0: mantenida; 1: pulsada en este fotograma; 2: soltada en este fotograma */
static Valor *jg_tecla(Valor **a, int n, int modo, const char *fn) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s() espera el nombre de una tecla", fn);
        return jg_error("ErrorTipo", msg);
    }
    int c = jg_tecla_codigo(a[0]->texto);
    if (c < 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s(): tecla desconocida '%s'", fn, a[0]->texto);
        return jg_error("Error", msg);
    }
    int ahora = _jgs.teclas[c], antes = _jgs.previas[c];
    return valor_booleano(modo == 0 ? ahora : modo == 1 ? (ahora && !antes) : (!ahora && antes));
}
static Valor *fn_jg_tecla(Valor **a, int n)          { return jg_tecla(a, n, 0, "tecla"); }
static Valor *fn_jg_tecla_pulsada(Valor **a, int n)  { return jg_tecla(a, n, 1, "tecla_pulsada"); }
static Valor *fn_jg_tecla_soltada(Valor **a, int n)  { return jg_tecla(a, n, 2, "tecla_soltada"); }

/* raton() -> [x, y] */
static Valor *fn_jg_raton(Valor **a, int n) {
    (void)a; (void)n;
    Valor *l = valor_lista_crear();
    lista_agregar(l, valor_numero(_jgs.rx));
    lista_agregar(l, valor_numero(_jgs.ry));
    return l;
}

/* boton_raton(n), n = 1 izquierdo, 2 central, 3 derecho; con 'pulsado' solo el fotograma del clic */
static Valor *jg_boton(Valor **a, int n, int solo_pulso) {
    double b = 1;
    if (n > 0 && !jg_num(a, n, 0, &b, "boton_raton")) return valor_nulo();
    if (b < 1 || b > 5) return jg_error("Error", "boton_raton(): el botón debe ser 1 a 5");
    unsigned m = 1u << ((int)b - 1);
    return valor_booleano(solo_pulso ? ((_jgs.raton & m) && !(_jgs.raton_prev & m)) : (_jgs.raton & m) != 0);
}
static Valor *fn_jg_boton_raton(Valor **a, int n)          { return jg_boton(a, n, 0); }
static Valor *fn_jg_boton_raton_pulsado(Valor **a, int n)  { return jg_boton(a, n, 1); }

static Valor *fn_jg_rueda(Valor **a, int n) { (void)a; (void)n; return valor_numero(_jgs.rueda); }

/* presentar(): muestra el dibujo, espera a 60 fps, lee eventos y calcula dt */
static Valor *fn_jg_presentar(Valor **a, int n) {
    (void)a; (void)n;
    JG_ABIERTA();
    int recargar = recarga_activa() && jg_vigilar();
    jg_escenas();
    if (recarga_error()) jg_aviso_error(recarga_error(), 0);
    if (_jgs.limite && _jgs.total + 1 >= _jgs.limite && getenv("LINCE_JUEGO_CAPTURA"))
        jg_guardar_bmp(getenv("LINCE_JUEGO_CAPTURA"));      /* prueba: guarda el último fotograma */
    if (_jgs.limite && getenv("LINCE_JUEGO_FOTOS")) {          /* prueba: todos, para hacer un vídeo */
        char ruta[600];
        snprintf(ruta, sizeof ruta, "%s/%05ld.bmp", getenv("LINCE_JUEGO_FOTOS"), _jgs.total);
        jg_guardar_bmp(ruta);
    }
    _jg.RenderPresent(_jgs.render);
    double t = jg_ahora();
    double espera = 1.0 / 60 - (t - _jgs.t_ultimo);
    if (!_jgs.limite && espera > 0.0015) { _jg.Delay((unsigned)((espera - 0.001) * 1000)); t = jg_ahora(); }
    _jgs.dt = t - _jgs.t_ultimo;
    if (_jgs.dt > 0.1) _jgs.dt = 0.1;
    if (_jgs.limite) _jgs.dt = 1.0 / 60;
    _jgs.total++;
    if (_jgs.limite && _jgs.total >= _jgs.limite) _jgs.cierre = 1;
    _jgs.t_ultimo = t;
    _jgs.cuadros++;
    if (t - _jgs.t_fps >= 0.5) { _jgs.fps = _jgs.cuadros / (t - _jgs.t_fps); _jgs.cuadros = 0; _jgs.t_fps = t; }

    memcpy(_jgs.previas, _jgs.teclas, sizeof(_jgs.teclas));
    _jgs.raton_prev = _jgs.raton;
    _jgs.rueda = 0;
    _jgs.nescrito = 0;
    _jgs.mov_x = _jgs.mov_y = 0;
    unsigned long long ev_buf[16];                      /* SDL_Event (56 bytes), alineado */
    unsigned char *ev = (unsigned char *)ev_buf;
    while (_jg.PollEvent(ev)) {
        unsigned tipo = *(unsigned *)ev;
        if (tipo == 0x100) _jgs.cierre = 1;                         /* SDL_QUIT */
        else if (tipo == 0x403) _jgs.rueda += *(int *)(ev + 20);    /* rueda: y */
        else if (tipo == 0x400) {                                   /* movimiento: xrel, yrel */
            _jgs.mov_x += *(int *)(ev + 28);
            _jgs.mov_y += *(int *)(ev + 32);
        } else if (tipo == 0x303) {                                 /* texto tecleado (UTF-8) */
            const char *t = (const char *)ev + 12;
            size_t l = strnlen(t, 32);
            if (_jgs.nescrito + (int)l < JG_ESCRITO) { memcpy(_jgs.escrito + _jgs.nescrito, t, l); _jgs.nescrito += (int)l; }
        } else if (tipo == 0x300 && *(int *)(ev + 16) == 42) {      /* «borrar», también al repetir */
            if (_jgs.nescrito + 1 < JG_ESCRITO) _jgs.escrito[_jgs.nescrito++] = '\b';
        }
    }
    _jgs.escrito[_jgs.nescrito] = '\0';
    int nt = 0;
    const unsigned char *k = _jg.KeyboardState(&nt);
    if (nt > 512) nt = 512;
    memcpy(_jgs.teclas, k, (size_t)nt);
    _jgs.raton = _jg.MouseState(&_jgs.rx, &_jgs.ry);
    if (_jg.WindowToLogical) {                 /* con pantalla completa la ventana se escala */
        float lx, ly;
        _jg.WindowToLogical(_jgs.render, _jgs.rx, _jgs.ry, &lx, &ly);
        _jgs.rx = (int)lx; _jgs.ry = (int)ly;
    }
    jg_leer_mandos();
    jg_recoger_voces();
    if (_jgs.pide_texto && _jgs.escribir_prueba) {     /* prueba: se teclea cuando el juego lo espera */
        snprintf(_jgs.escrito, JG_ESCRITO, "%s", _jgs.escribir_prueba);
        for (char *c = _jgs.escrito; *c; c++) if (*c == '<') *c = '\b';   /* '<' = borrar */
        _jgs.nescrito = (int)strlen(_jgs.escrito);
        free(_jgs.escribir_prueba);
        _jgs.escribir_prueba = NULL;
    }
    if (_jgs.total & 1) {
        for (int i = 0; i < _jgs.npulsar; i++) _jgs.teclas[_jgs.pulsar[i]] = 1;
        if (_jgs.hay_clic) { _jgs.rx = _jgs.clic_x; _jgs.ry = _jgs.clic_y; _jgs.raton |= 1; }
    } else if (_jgs.hay_clic) { _jgs.rx = _jgs.clic_x; _jgs.ry = _jgs.clic_y; }
    /* Código nuevo listo: se corta el programa que corre y main lo relanza */
    if (recargar) return jg_error(ERROR_RECARGA, "recarga en caliente");
    return valor_nulo();
}

/* ── Recarga en caliente ──
   Con el juego en marcha, cada cuarto de segundo (cada fotograma en el modo
   de prueba) se mira si han cambiado el programa, lo que importa, o las
   imágenes y sonidos cargados. Las imágenes y sonidos se cambian en el
   momento; el código, si su sintaxis es correcta, hace que presentar()
   corte el programa para que main lo vuelva a lanzar. F5 lo relanza desde
   cero, sin conservar las variables. */
static void jg_recargar_recursos(void) {
    long long m, t;
    for (int i = 0; i < _jgs.nimagenes; i++) {
        JgImagen *im = &_jgs.imagenes[i];
        recarga_huella(im->ruta, &m, &t);
        if (m < 0 || (m == im->mtime && t == im->tam)) continue;
        char msg[600];
        void *tex;
        if (!jg_leer_textura(im->ruta, &tex, msg, sizeof msg)) continue;   /* a medio guardar: luego */
        _jg.DestroyTexture(im->tex);
        im->tex = tex;
        _jg.QueryTexture(tex, NULL, NULL, &im->ancho, &im->alto);
        im->mtime = m; im->tam = t;
    }
    for (int i = 0; i < _jgs.nsonidos; i++) {
        JgSonido *so = &_jgs.sonidos[i];
        recarga_huella(so->ruta, &m, &t);
        if (m < 0 || (m == so->mtime && t == so->tam)) continue;
        char msg[600];
        short *datos;
        int muestras;
        if (!jg_cargar_wav(so->ruta, &datos, &muestras, msg, sizeof msg)) continue;
        if (_jgs.audio) _jg.LockAudio(_jgs.audio);
        for (int v = 0; v < JG_VOCES; v++) {           /* lo que suena sigue con el sonido nuevo */
            JgVoz *vz = &_jgs.voces[v];
            if (vz->datos != so->datos) continue;
            vz->datos = datos;
            vz->muestras = muestras;
            if (vz->pos >= muestras) vz->pos = 0;
        }
        short *viejo = so->datos;
        so->datos = datos;
        so->muestras = muestras;
        if (_jgs.audio) _jg.UnlockAudio(_jgs.audio);
        free(viejo);
        so->mtime = m; so->tam = t;
    }
}

static int jg_vigilar(void) {
    int f5 = _jgs.teclas[62] && !_jgs.previas[62];
    double t = jg_ahora();
    if (!f5 && !_jgs.limite && t - _jgs.t_vigilar < 0.25) return 0;
    _jgs.t_vigilar = t;
    jg_recargar_recursos();
    int r = recarga_comprobar(f5);
    recarga_pedir_completa(r && f5);
    return r;
}

/* Muestra un error en la ventana: abajo, sobre el juego, o (pantalla = 1)
   ocupándola entera */
static void jg_aviso_error(const char *msg, int pantalla) {
    int w = _jgs.ancho, e = w >= 480 ? 2 : 1, margen = 6 * e;
    char texto[800];
    snprintf(texto, sizeof texto, "%s", msg);
    /* altura que ocupa el texto: se dibuja una vez fuera de la ventana para medirlo */
    _jg.SetDrawColor(_jgs.render, 0, 0, 0, 0);
    int alto = jg_escribir(texto, -100000, -100000, e, w - 2 * margen);
    int y0 = pantalla ? 0 : _jgs.alto - alto - 2 * margen - 9 * e;
    unsigned char fondo[4] = {90, 20, 25, pantalla ? 255 : 235};
    jg_pintar(fondo);
    JgRect r = {0, y0, w, _jgs.alto - y0};
    _jg.FillRect(_jgs.render, &r);
    unsigned char amarillo[4] = {255, 210, 90, 255}, blanco[4] = {255, 255, 255, 255}, gris[4] = {230, 170, 170, 255};
    jg_pintar(amarillo);
    jg_escribir(pantalla ? "ERROR: EL JUEGO SE HA PARADO" : "ERROR: SIGUE EL CÓDIGO ANTERIOR", margen, y0 + margen, e, 0);
    jg_pintar(blanco);
    int y = y0 + margen + 9 * e + (pantalla ? 6 * e : 0);
    y += jg_escribir(texto, margen, y, e, w - 2 * margen);
    if (pantalla) {
        jg_pintar(gris);
        jg_escribir("Corrígelo y guarda: el juego se recarga solo. F5 lo empieza de cero.",
                    margen, y + 6 * e, e, w - 2 * margen);
    }
}

/* ¿Hay una ventana abierta? (main decide así si un error se muestra en ella) */
int juego_ventana_abierta(void) { return _jgs.abierta && !_jgs.cierre; }

/* Antes de relanzar el programa recargado: su abrir() reutilizará la ventana */
void juego_preparar_recarga(int conserva) { _jgs.reabrir = _jgs.abierta; _jgs.recargado = conserva; }

/* recargado(): verdadero si el programa se ha recargado en caliente y conserva
   sus variables. Sirve para no reiniciar la partida al guardar:
       si no juego.recargado() { empezar() } */
static Valor *fn_jg_recargado(Valor **a, int n) { (void)a; (void)n; return valor_booleano(_jgs.recargado); }

/* El programa se paró por un error: lo muestra en la ventana hasta que se
   guarde un arreglo (devuelve 1) o se cierre la ventana (devuelve 0). */
int juego_esperar_arreglo(const char *msg) {
    if (!_jgs.abierta) return 0;
    if (_jgs.audio) _jg.PauseAudio(_jgs.audio, 1);
    int r = 0;
    while (!_jgs.cierre) {
        _jg.SetDrawColor(_jgs.render, 0, 0, 0, 255);
        _jg.RenderClear(_jgs.render);
        jg_aviso_error(recarga_error() ? recarga_error() : msg, 1);
        if (_jgs.limite && getenv("LINCE_JUEGO_CAPTURA")) jg_guardar_bmp(getenv("LINCE_JUEGO_CAPTURA"));
        _jg.RenderPresent(_jgs.render);
        memcpy(_jgs.previas, _jgs.teclas, sizeof(_jgs.teclas));
        unsigned long long ev_buf[16];
        unsigned char *ev = (unsigned char *)ev_buf;
        while (_jg.PollEvent(ev)) if (*(unsigned *)ev == 0x100) _jgs.cierre = 1;
        int nt = 0;
        const unsigned char *k = _jg.KeyboardState(&nt);
        memcpy(_jgs.teclas, k, (size_t)(nt > 512 ? 512 : nt));
        if (_jgs.teclas[41]) _jgs.cierre = 1;                      /* Esc */
        if (jg_vigilar()) { r = 1; break; }
        if (_jgs.limite) { if (++_jgs.total >= _jgs.limite) _jgs.cierre = 1; }
        else _jg.Delay(30);
    }
    if (_jgs.audio) _jg.PauseAudio(_jgs.audio, 0);
    return r;
}

/* Guarda la imagen actual de la ventana como BMP; 1 si va bien */
static int jg_guardar_bmp(const char *ruta) {
    int w = _jgs.ancho, h = _jgs.alto;
    _jg.OutputSize(_jgs.render, &w, &h);
    size_t tam = (size_t)w * h * 4;
    unsigned char *px = malloc(tam);
    if (!px) return 0;
    int ok = 0;
    if (_jg.ReadPixels(_jgs.render, NULL, 0x16362004, px, w * 4) == 0) {
        void *sup = _jg.SurfaceFrom(px, w, h, 32, w * 4,
                                    0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        void *rw = _jg.RWFromFile(ruta, "wb");
        if (sup && rw) ok = _jg.SaveBMP_RW(sup, rw, 1) == 0;
        if (sup) _jg.FreeSurface(sup);
    }
    free(px);
    return ok;
}

/* captura(ruta): guarda la ventana como BMP */
static Valor *fn_jg_captura(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "captura() espera la ruta del archivo .bmp");
    if (!jg_guardar_bmp(a[0]->texto)) return jg_error("ErrorJuego", "no se pudo guardar la captura");
    return valor_nulo();
}

/* ── Sonido: mezclador propio ──
   Un dispositivo de audio (estéreo, 16 bits, 44100 Hz) con callback que suma
   hasta JG_VOCES voces. Los sonidos se convierten a ese formato al cargarlos. */
static void jg_mezclar(void *datos, unsigned char *flujo, int largo) {
    (void)datos;
    short *salida = (short *)flujo;
    int muestras = largo / (int)sizeof(short);           /* L y R intercaladas */
    for (int i = 0; i < muestras; i++) {
        int acc = 0;
        for (int v = 0; v < JG_VOCES; v++) {
            JgVoz *voz = &_jgs.voces[v];
            if (!voz->activa) continue;
            int total = voz->muestras * JG_CANALES;
            if (voz->pos >= total) {
                if (voz->bucle && total > 0) voz->pos = 0;
                else { voz->activa = 0; continue; }
            }
            acc += (int)(voz->datos[voz->pos++] * voz->vol);
        }
        acc = (int)(acc * _jgs.volumen);
        salida[i] = (short)(acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc);
    }
}

/* Abre el dispositivo la primera vez; 0 si no hay audio (se juega en silencio) */
static int jg_audio(void) {
    if (_jgs.audio) return 1;
    JgAudioSpec want, have;
    memset(&want, 0, sizeof want);
    want.freq = JG_FRECUENCIA; want.formato = JG_S16; want.canales = JG_CANALES; want.muestras = 1024;
    want.callback = jg_mezclar;
    _jgs.audio = _jg.OpenAudio(NULL, 0, &want, &have, 0);      /* sin cambios permitidos: SDL convierte */
    if (!_jgs.audio) return 0;
    _jg.PauseAudio(_jgs.audio, 0);
    return 1;
}

/* Libera los datos de los pitidos que ya sonaron (desde el hilo principal) */
static void jg_recoger_voces(void) {
    if (!_jgs.audio) return;
    _jg.LockAudio(_jgs.audio);
    for (int v = 0; v < JG_VOCES; v++)
        if (!_jgs.voces[v].activa && _jgs.voces[v].propio) {
            free(_jgs.voces[v].propio);
            _jgs.voces[v].propio = NULL;
        }
    _jg.UnlockAudio(_jgs.audio);
}

/* Pone a sonar unos datos; devuelve el id de la voz (0 si no hay audio o hueco) */
static int jg_sonar(const short *datos, int muestras, float vol, int bucle, short *propio) {
    if (!jg_audio()) { free(propio); return 0; }
    jg_recoger_voces();
    _jg.LockAudio(_jgs.audio);
    int libre = -1;
    for (int v = 0; v < JG_VOCES; v++)
        if (!_jgs.voces[v].activa && !_jgs.voces[v].propio) { libre = v; break; }
    if (libre < 0) {                                       /* todas ocupadas: se roba la más avanzada */
        int mejor = -1;
        for (int v = 0; v < JG_VOCES; v++)
            if (_jgs.voces[v].id != _jgs.voz_musica && !_jgs.voces[v].bucle &&
                (mejor < 0 || _jgs.voces[v].pos > _jgs.voces[mejor].pos)) mejor = v;
        if (mejor < 0) { _jg.UnlockAudio(_jgs.audio); free(propio); return 0; }
        free(_jgs.voces[mejor].propio);
        libre = mejor;
    }
    JgVoz *voz = &_jgs.voces[libre];
    voz->datos = datos; voz->muestras = muestras; voz->pos = 0;
    voz->vol = vol; voz->bucle = bucle; voz->propio = propio;
    voz->id = ++_jgs.siguiente_voz;
    voz->activa = 1;
    _jg.UnlockAudio(_jgs.audio);
    return voz->id;
}

static void jg_parar_voz(int id) {
    if (!_jgs.audio || id <= 0) return;
    _jg.LockAudio(_jgs.audio);
    for (int v = 0; v < JG_VOCES; v++)
        if (_jgs.voces[v].id == id) _jgs.voces[v].activa = 0;
    _jg.UnlockAudio(_jgs.audio);
}

static float jg_volumen_arg(Valor **a, int n, int i, float defecto) {
    if (i < n && a[i]->tipo == VAL_NUMERO) {
        double v = a[i]->numero;
        return (float)(v < 0 ? 0 : v > 1 ? 1 : v);
    }
    return defecto;
}

/* pitido(frecuencia, milisegundos[, volumen]): onda cuadrada suavizada, no bloquea */
static Valor *fn_jg_pitido(Valor **a, int n) {
    JG_ABIERTA();
    double f, ms;
    if (!jg_num(a, n, 0, &f, "pitido") || !jg_num(a, n, 1, &ms, "pitido")) return valor_nulo();
    if (f < 20 || f > 20000 || ms <= 0 || ms > 5000) return jg_error("Error", "pitido(): frecuencia 20-20000 Hz y duración hasta 5000 ms");
    float vol = jg_volumen_arg(a, n, 2, 0.25f);
    int total = (int)(JG_FRECUENCIA * ms / 1000);
    if (total < 2) return valor_nulo();
    short *buf = malloc((size_t)total * JG_CANALES * sizeof(short));
    if (!buf) return valor_nulo();
    int fade = total < 441 ? total / 2 : 441;                           /* 10 ms de rampa: sin chasquidos */
    for (int i = 0; i < total; i++) {
        double fase = fmod(i * f / JG_FRECUENCIA, 1.0);
        double m = fase < 0.5 ? 1.0 : -1.0;
        double env = i < fade ? (double)i / fade : (total - i) < fade ? (double)(total - i) / fade : 1.0;
        short muestra = (short)(m * env * 12000);
        buf[2 * i] = buf[2 * i + 1] = muestra;
    }
    jg_sonar(buf, total, vol, 0, buf);
    return valor_nulo();
}

/* Un OGG decodificado a estéreo de 16 bits (aún a su frecuencia original) */
static char *jg_ogg_estereo(const char *bytes, size_t largo, int *frecuencia, size_t *leido) {
    int canales, muestras;
    short *ogg = ogg_decodificar((const unsigned char *)bytes, largo, &canales, frecuencia, &muestras);
    if (!ogg) return NULL;
    short *est = malloc((size_t)muestras * JG_CANALES * sizeof(short) + 4);
    if (est)
        for (int i = 0; i < muestras; i++) {                 /* mono: igual en los dos lados */
            est[2 * i]     = ogg[(size_t)i * canales];
            est[2 * i + 1] = ogg[(size_t)i * canales + (canales > 1)];
        }
    free(ogg);
    *leido = (size_t)muestras * JG_CANALES * sizeof(short);
    return (char *)est;
}

/* Carga un WAV u OGG (del disco o del ejecutable empaquetado) en el formato de Lince */
static int jg_cargar_wav(const char *ruta, short **datos, int *muestras, char *error, size_t max) {
    size_t largo = 0;
    char *bytes = interprete_leer_bytes(ruta, &largo);
    if (!bytes) { snprintf(error, max, "no se encuentra el sonido '%s'", ruta); return 0; }
    JgAudioSpec spec;
    char *buf;
    size_t leido = 0;
    if (largo >= 4 && memcmp(bytes, "OggS", 4) == 0) {
        int freq = 0;
        buf = jg_ogg_estereo(bytes, largo, &freq, &leido);
        free(bytes);
        if (!buf) { snprintf(error, max, "'%s' no es un OGG Vorbis válido", ruta); return 0; }
        spec.freq = freq;
        goto remuestrear;
    }
    unsigned char *wav = NULL;
    unsigned wav_largo = 0;
    void *rw = _jg.RWFromConstMem(bytes, (int)largo);
    if (!rw || !_jg.LoadWAV_RW(rw, 1, &spec, &wav, &wav_largo)) {
        free(bytes);
        snprintf(error, max, "'%s' no es un WAV válido: %s", ruta, _jg.GetError());
        return 0;
    }
    free(bytes);
    /* SDL convierte formato y canales; la frecuencia se cambia aquí (su remuestreador
       se guarda el final del sonido) */
    void *flujo = _jg.NewAudioStream(spec.formato, spec.canales, spec.freq, JG_S16, JG_CANALES, spec.freq);
    if (!flujo) { _jg.FreeWAV(wav); snprintf(error, max, "formato de audio no soportado en '%s'", ruta); return 0; }
    _jg.StreamPut(flujo, wav, (int)wav_largo);
    _jg.StreamFlush(flujo);
    _jg.FreeWAV(wav);
    /* el conversor puede entregar los datos en varios trozos */
    size_t cap = (size_t)wav_largo * 4 + 4096;
    buf = malloc(cap);
    for (;;) {
        if (!buf) break;
        int disp = _jg.StreamAvailable(flujo);
        if (disp <= 0) break;
        if (leido + (size_t)disp > cap) {
            cap = (leido + (size_t)disp) * 2;
            char *nuevo = realloc(buf, cap);
            if (!nuevo) { free(buf); buf = NULL; break; }
            buf = nuevo;
        }
        int r = _jg.StreamGet(flujo, buf + leido, disp);
        if (r <= 0) break;
        leido += (size_t)r;
    }
    _jg.FreeStream(flujo);
remuestrear:
    if (!buf) { snprintf(error, max, "sin memoria"); return 0; }
    int origen = (int)(leido / (sizeof(short) * JG_CANALES));
    if (spec.freq != JG_FRECUENCIA && origen > 1 && spec.freq > 0) {
        const short *ent = (const short *)buf;
        int destino = (int)((long long)origen * JG_FRECUENCIA / spec.freq);
        short *sal = malloc((size_t)destino * JG_CANALES * sizeof(short) + 4);
        if (!sal) { free(buf); snprintf(error, max, "sin memoria"); return 0; }
        double paso = (double)spec.freq / JG_FRECUENCIA;
        for (int i = 0; i < destino; i++) {
            double t = i * paso;
            int j = (int)t;
            double f = t - j;
            if (j >= origen - 1) { j = origen - 2; f = 1; }
            for (int c = 0; c < JG_CANALES; c++)
                sal[i * JG_CANALES + c] = (short)(ent[j * JG_CANALES + c] * (1 - f) + ent[(j + 1) * JG_CANALES + c] * f);
        }
        free(buf);
        buf = (char *)sal;
        origen = destino;
    }
    *datos = (short *)buf;
    *muestras = origen;
    return 1;
}

static int jg_id_de(Valor *v, const char *clave_tipo) {
    if (v->tipo == VAL_DICCIONARIO) {
        int k = valor_diccionario_buscar(v, clave_tipo);
        if (k >= 0 && v->diccionario.valores[k]->tipo == VAL_NUMERO) return (int)v->diccionario.valores[k]->numero;
    }
    return -1;
}

/* sonido(ruta): carga un WAV u OGG; devuelve {"sonido": n, "duracion": segundos} */
static Valor *fn_jg_sonido(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "sonido() espera la ruta de un archivo .wav u .ogg");
    int k = -1;
    for (int i = 0; i < _jgs.nsonidos; i++)
        if (strcmp(_jgs.sonidos[i].ruta, a[0]->texto) == 0) { k = i; break; }
    if (k < 0) {
        if (_jgs.nsonidos == JG_MAX_SONIDOS) return jg_error("ErrorJuego", "demasiados sonidos cargados");
        char error[600];
        short *datos;
        int muestras;
        if (!jg_cargar_wav(a[0]->texto, &datos, &muestras, error, sizeof error)) return jg_error("ErrorJuego", error);
        k = _jgs.nsonidos++;
        _jgs.sonidos[k].datos = datos;
        _jgs.sonidos[k].muestras = muestras;
        _jgs.sonidos[k].ruta = strdup(a[0]->texto);
        recarga_huella(a[0]->texto, &_jgs.sonidos[k].mtime, &_jgs.sonidos[k].tam);
    }
    Valor *d = valor_diccionario_crear();
    valor_diccionario_agregar(d, "sonido", valor_numero(k));
    valor_diccionario_agregar(d, "duracion", valor_numero((double)_jgs.sonidos[k].muestras / JG_FRECUENCIA));
    valor_diccionario_agregar(d, "ruta", valor_texto(a[0]->texto));
    return d;
}

static JgSonido *jg_sonido_arg(Valor **a, int n, const char *fn) {
    int k = n > 0 ? jg_id_de(a[0], "sonido") : -1;
    if (k < 0 || k >= _jgs.nsonidos) {
        char msg[120];
        snprintf(msg, sizeof msg, "%s() espera un sonido cargado con juego.sonido()", fn);
        jg_error("ErrorTipo", msg);
        return NULL;
    }
    return &_jgs.sonidos[k];
}

/* reproducir(sonido[, volumen[, bucle]]): devuelve el número de la voz */
static Valor *fn_jg_reproducir(Valor **a, int n) {
    JG_ABIERTA();
    JgSonido *s = jg_sonido_arg(a, n, "reproducir");
    if (!s) return valor_nulo();
    int bucle = n > 2 && a[2]->tipo == VAL_BOOLEANO && a[2]->booleano;
    return valor_numero(jg_sonar(s->datos, s->muestras, jg_volumen_arg(a, n, 1, 1), bucle, NULL));
}

/* detener([voz]): para una voz o, sin argumento, todo (también la música) */
static Valor *fn_jg_detener(Valor **a, int n) {
    JG_ABIERTA();
    if (n > 0 && a[0]->tipo == VAL_NUMERO) { jg_parar_voz((int)a[0]->numero); return valor_nulo(); }
    if (_jgs.audio) {
        _jg.LockAudio(_jgs.audio);
        for (int v = 0; v < JG_VOCES; v++) _jgs.voces[v].activa = 0;
        _jg.UnlockAudio(_jgs.audio);
    }
    _jgs.voz_musica = 0;
    return valor_nulo();
}

/* musica(sonido[, volumen]): suena en bucle y sustituye a la anterior; musica(nulo) la para */
static Valor *fn_jg_musica(Valor **a, int n) {
    JG_ABIERTA();
    jg_parar_voz(_jgs.voz_musica);
    _jgs.voz_musica = 0;
    if (n < 1 || a[0]->tipo == VAL_NULO) return valor_nulo();
    JgSonido *s = jg_sonido_arg(a, n, "musica");
    if (!s) return valor_nulo();
    _jgs.voz_musica = jg_sonar(s->datos, s->muestras, jg_volumen_arg(a, n, 1, 0.6f), 1, NULL);
    return valor_nulo();
}

/* volumen(v): volumen general de 0 a 1 */
static Valor *fn_jg_volumen(Valor **a, int n) {
    if (n > 0) {
        double v;
        if (!jg_num(a, n, 0, &v, "volumen")) return valor_nulo();
        _jgs.volumen = (float)(v < 0 ? 0 : v > 1 ? 1 : v);
    }
    return valor_numero(_jgs.volumen);
}

/* ── Imágenes ── */
static Valor *jg_imagen_valor(int k) {
    Valor *d = valor_diccionario_crear();
    valor_diccionario_agregar(d, "imagen", valor_numero(k));
    valor_diccionario_agregar(d, "ancho", valor_numero(_jgs.imagenes[k].ancho));
    valor_diccionario_agregar(d, "alto", valor_numero(_jgs.imagenes[k].alto));
    valor_diccionario_agregar(d, "ruta", valor_texto(_jgs.imagenes[k].ruta));
    return d;
}

/* Lee la imagen 'ruta' como textura; 1 si va bien, 0 con el motivo en msg.
   Lee PNG, JPG, BMP, GIF y TGA con stb_image. */
static int jg_leer_textura(const char *ruta, void **tex, char *msg, size_t max) {
    size_t largo = 0;
    char *bytes = interprete_leer_bytes(ruta, &largo);
    if (!bytes) {
        snprintf(msg, max, "no se encuentra la imagen '%s'", ruta);
        return 0;
    }
    int w = 0, h = 0;
    const char *motivo = NULL;
    unsigned char *px = imagen_decodificar((const unsigned char *)bytes, largo, &w, &h, &motivo);
    void *sup = NULL;
    if (px)       /* RGBA byte a byte: en little endian, R es el byte bajo */
        sup = _jg.SurfaceFrom(px, w, h, 32, w * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    free(bytes);
    if (!sup) {
        free(px);
        snprintf(msg, max, "no se pudo leer la imagen '%s' (%s); Lince lee PNG, JPG, BMP, GIF y TGA",
                 ruta, px ? _jg.GetError() : motivo);
        return 0;
    }
    *tex = _jg.TextureFromSurface(_jgs.render, sup);
    _jg.FreeSurface(sup);
    free(px);
    if (!*tex) {
        snprintf(msg, max, "no se pudo preparar la imagen '%s': %s", ruta, _jg.GetError());
        return 0;
    }
    _jg.TextureBlend(*tex, 1);
    return 1;
}

/* imagen(ruta): PNG, JPG, BMP, GIF o TGA; devuelve {"imagen", "ancho", "alto"} */
static Valor *fn_jg_imagen(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "imagen() espera la ruta de una imagen");
    for (int i = 0; i < _jgs.nimagenes; i++)
        if (strcmp(_jgs.imagenes[i].ruta, a[0]->texto) == 0) return jg_imagen_valor(i);
    if (_jgs.nimagenes == JG_MAX_IMAGENES) return jg_error("ErrorJuego", "demasiadas imágenes cargadas");
    char msg[600];
    void *tex;
    if (!jg_leer_textura(a[0]->texto, &tex, msg, sizeof msg)) return jg_error("ErrorJuego", msg);
    int k = _jgs.nimagenes++;
    _jgs.imagenes[k].tex = tex;
    _jgs.imagenes[k].ruta = strdup(a[0]->texto);
    recarga_huella(a[0]->texto, &_jgs.imagenes[k].mtime, &_jgs.imagenes[k].tam);
    _jg.QueryTexture(tex, NULL, NULL, &_jgs.imagenes[k].ancho, &_jgs.imagenes[k].alto);
    return jg_imagen_valor(k);
}

/* sprite(filas, colores): imagen hecha con texto, un carácter por píxel.
     sea nave = juego.sprite([".X.", "XXX"], {"X": [120, 200, 240]})
   Los caracteres que no están en 'colores' son transparentes. */
static Valor *fn_jg_sprite(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 2 || a[0]->tipo != VAL_LISTA || a[1]->tipo != VAL_DICCIONARIO || a[0]->lista.cantidad == 0)
        return jg_error("ErrorTipo", "sprite() espera (lista de filas de texto, diccionario de colores)");
    if (_jgs.nimagenes == JG_MAX_IMAGENES) return jg_error("ErrorJuego", "demasiadas imágenes cargadas");
    int alto = a[0]->lista.cantidad, ancho = 0;
    for (int y = 0; y < alto; y++) {
        Valor *f = a[0]->lista.elementos[y];
        if (f->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "sprite(): cada fila es un texto");
        int l = (int)strlen(f->texto);
        if (l > ancho) ancho = l;
    }
    if (ancho == 0 || ancho > 4096 || alto > 4096) return jg_error("Error", "sprite(): tamaño no válido");
    /* paleta: un color por carácter ASCII */
    unsigned char pal[128][4];
    memset(pal, 0, sizeof pal);
    Valor *d = a[1];
    for (int i = 0; i < d->diccionario.cantidad; i++) {
        const char *k = d->diccionario.claves[i];
        if (!k[0] || k[1] || (unsigned char)k[0] >= 128)
            return jg_error("ErrorTipo", "sprite(): cada clave de colores es un solo carácter, por ejemplo \"X\"");
        Valor *arg[1] = {d->diccionario.valores[i]};
        if (!jg_color(arg, 1, 0, pal[(unsigned char)k[0]])) return valor_nulo();
    }
    /* El mismo dibujo con los mismos colores es la misma imagen: así el programa
       recargado en caliente no crea texturas nuevas en cada recarga. */
    unsigned long long huella = 1469598103934665603ULL;
    for (int y = 0; y < alto; y++) {
        for (const unsigned char *t = (const unsigned char *)a[0]->lista.elementos[y]->texto; *t; t++)
            huella = (huella ^ *t) * 1099511628211ULL;
        huella = (huella ^ '\n') * 1099511628211ULL;
    }
    for (int i = 0; i < 128 * 4; i++) huella = (huella ^ ((unsigned char *)pal)[i]) * 1099511628211ULL;
    char nombre[48];
    snprintf(nombre, sizeof nombre, "<sprite %016llx>", huella);
    for (int i = 0; i < _jgs.nimagenes; i++)
        if (strcmp(_jgs.imagenes[i].ruta, nombre) == 0) return jg_imagen_valor(i);
    unsigned *px = calloc((size_t)ancho * alto, 4);
    if (!px) return jg_error("Error", "sin memoria");
    for (int y = 0; y < alto; y++) {
        const unsigned char *t = (const unsigned char *)a[0]->lista.elementos[y]->texto;
        for (int x = 0; t[x]; x++) {
            if (t[x] >= 128) continue;
            const unsigned char *c = pal[t[x]];
            px[y * ancho + x] = (unsigned)c[3] << 24 | (unsigned)c[0] << 16 | (unsigned)c[1] << 8 | c[2];
        }
    }
    void *sup = _jg.SurfaceFrom(px, ancho, alto, 32, ancho * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    void *tex = sup ? _jg.TextureFromSurface(_jgs.render, sup) : NULL;
    if (sup) _jg.FreeSurface(sup);
    free(px);
    if (!tex) return jg_error("ErrorJuego", "no se pudo crear el sprite");
    int k = _jgs.nimagenes++;
    _jgs.imagenes[k].tex = tex;
    _jgs.imagenes[k].ruta = strdup(nombre);
    _jgs.imagenes[k].ancho = ancho;
    _jgs.imagenes[k].alto = alto;
    _jg.TextureBlend(tex, 1);
    return jg_imagen_valor(k);
}

/* Lee la opción numérica 'clave' del diccionario; 1 si estaba */
static int jg_opcion(Valor *op, const char *clave, double *out) {
    if (!op || op->tipo != VAL_DICCIONARIO) return 0;
    int k = valor_diccionario_buscar(op, clave);
    if (k < 0) return 0;
    Valor *v = op->diccionario.valores[k];
    if (v->tipo == VAL_NUMERO) { *out = v->numero; return 1; }
    if (v->tipo == VAL_BOOLEANO) { *out = v->booleano; return 1; }
    return 0;
}
static Valor *jg_opcion_valor(Valor *op, const char *clave) {
    if (!op || op->tipo != VAL_DICCIONARIO) return NULL;
    int k = valor_diccionario_buscar(op, clave);
    return k < 0 ? NULL : op->diccionario.valores[k];
}

/* dibujar(imagen, x, y[, opciones]). Opciones (todas opcionales):
     escala, ancho, alto, angulo (grados), espejo, voltear, centrado, alfa (0-255),
     color ([r, g, b] para teñir), recorte ([x, y, ancho, alto]),
     cuadro + celda (hoja de sprites: número de cuadro y [ancho, alto] de cada uno) */
static Valor *fn_jg_dibujar(Valor **a, int n) {
    JG_ABIERTA();
    int k = n > 0 ? jg_id_de(a[0], "imagen") : -1;
    if (k < 0 || k >= _jgs.nimagenes) return jg_error("ErrorTipo", "dibujar() espera una imagen cargada con juego.imagen()");
    double x, y;
    if (!jg_num(a, n, 1, &x, "dibujar") || !jg_num(a, n, 2, &y, "dibujar")) return valor_nulo();
    Valor *op = n > 3 ? a[3] : NULL;
    if (op && op->tipo != VAL_DICCIONARIO && op->tipo != VAL_NULO)
        return jg_error("ErrorTipo", "dibujar(): las opciones van en un diccionario, por ejemplo {\"escala\": 2}");
    JgImagen *img = &_jgs.imagenes[k];
    JgRect src = {0, 0, img->ancho, img->alto};
    Valor *rec = jg_opcion_valor(op, "recorte");
    Valor *celda = jg_opcion_valor(op, "celda");
    double cuadro;
    if (rec) {
        if (rec->tipo != VAL_LISTA || rec->lista.cantidad != 4) return jg_error("ErrorTipo", "dibujar(): 'recorte' es [x, y, ancho, alto]");
        int *dst[4] = {&src.x, &src.y, &src.w, &src.h};
        for (int i = 0; i < 4; i++) {
            if (rec->lista.elementos[i]->tipo != VAL_NUMERO) return jg_error("ErrorTipo", "dibujar(): 'recorte' es [x, y, ancho, alto]");
            *dst[i] = (int)rec->lista.elementos[i]->numero;
        }
    } else if (celda) {
        if (celda->tipo != VAL_LISTA || celda->lista.cantidad != 2 ||
            celda->lista.elementos[0]->tipo != VAL_NUMERO || celda->lista.elementos[1]->tipo != VAL_NUMERO)
            return jg_error("ErrorTipo", "dibujar(): 'celda' es [ancho, alto] de cada cuadro");
        int cw = (int)celda->lista.elementos[0]->numero, ch = (int)celda->lista.elementos[1]->numero;
        if (cw < 1 || ch < 1) return jg_error("Error", "dibujar(): la celda debe medir al menos 1×1");
        int columnas = img->ancho / cw;
        if (columnas < 1) columnas = 1;
        int filas = img->alto / ch;
        if (filas < 1) filas = 1;
        int c = jg_opcion(op, "cuadro", &cuadro) ? (int)cuadro : 0;
        if (c < 0) c = 0;
        c %= columnas * filas;
        src.x = (c % columnas) * cw; src.y = (c / columnas) * ch; src.w = cw; src.h = ch;
    }
    double escala = 1, w, h, angulo = 0, v;
    jg_opcion(op, "escala", &escala);
    w = src.w * escala; h = src.h * escala;
    if (jg_opcion(op, "ancho", &v)) w = v;
    if (jg_opcion(op, "alto", &v)) h = v;
    jg_opcion(op, "angulo", &angulo);
    if (jg_opcion(op, "centrado", &v) && v) { x -= w / 2; y -= h / 2; }
    int volteo = 0;
    if (jg_opcion(op, "espejo", &v) && v) volteo |= 1;
    if (jg_opcion(op, "voltear", &v) && v) volteo |= 2;
    double alfa = 255;
    jg_opcion(op, "alfa", &alfa);
    unsigned char tinte[4] = {255, 255, 255, 255};
    Valor *col = jg_opcion_valor(op, "color");
    if (col) { Valor *arg[1] = {col}; if (!jg_color(arg, 1, 0, tinte)) return valor_nulo(); }
    _jg.TextureAlpha(img->tex, (unsigned char)(alfa < 0 ? 0 : alfa > 255 ? 255 : alfa));
    _jg.TextureColor(img->tex, tinte[0], tinte[1], tinte[2]);
    JgRect dst = {(int)floor(x - _jgs.cam_x), (int)floor(y - _jgs.cam_y), (int)ceil(w), (int)ceil(h)};
    _jg.RenderCopyEx(_jgs.render, img->tex, &src, &dst, angulo, NULL, volteo);
    return valor_nulo();
}

/* ── Cámara y pantalla ── */
/* camara(x, y): lo que se dibuja se desplaza; camara() vuelve a (0, 0). Devuelve [x, y] */
static Valor *fn_jg_camara(Valor **a, int n) {
    if (n >= 2) {
        double x, y;
        if (!jg_num(a, n, 0, &x, "camara") || !jg_num(a, n, 1, &y, "camara")) return valor_nulo();
        _jgs.cam_x = x; _jgs.cam_y = y;
    } else if (n == 0) {
        _jgs.cam_x = _jgs.cam_y = 0;
    } else {
        return jg_error("ErrorTipo", "camara() espera (x, y) o nada");
    }
    Valor *l = valor_lista_crear();
    lista_agregar(l, valor_numero(_jgs.cam_x));
    lista_agregar(l, valor_numero(_jgs.cam_y));
    return l;
}

/* pantalla_completa([sí/no]): sin argumento alterna. El juego se escala sin deformarse. */
static Valor *fn_jg_pantalla_completa(Valor **a, int n) {
    JG_ABIERTA();
    int quiere = !_jgs.completa;
    if (n > 0 && a[0]->tipo == VAL_BOOLEANO) quiere = a[0]->booleano;
    if (_jg.SetFullscreen(_jgs.ventana, quiere ? 0x1001u : 0) == 0) _jgs.completa = quiere;
    return valor_booleano(_jgs.completa);
}

/* tiempo(): segundos desde que se abrió la ventana (para animaciones) */
/* Segundos desde que se abrió la ventana (en el modo de prueba, fotogramas / 60) */
static double jg_reloj(void) {
    if (!_jgs.abierta) return 0;
    if (_jgs.limite) return _jgs.total / 60.0;
    return jg_ahora() - _jgs.t_inicio;
}

static Valor *fn_jg_tiempo(Valor **a, int n) {
    (void)a; (void)n;
    return valor_numero(jg_reloj());
}

/* ── Mandos ── */
static const char *const JG_NOMBRES_BOTON[JG_BOTONES] = {
    "a", "b", "x", "y", "atras", "guia", "inicio", "stick_izq", "stick_der",
    "lb", "rb", "arriba", "abajo", "izquierda", "derecha" };
static const char *const JG_NOMBRES_EJE[JG_EJES] = {"izq_x", "izq_y", "der_x", "der_y", "lt", "rt"};

static void jg_abrir_mandos(void) {
    for (int i = 0; i < JG_MANDOS; i++) if (_jgs.mandos[i]) { _jg.ControllerClose(_jgs.mandos[i]); _jgs.mandos[i] = NULL; }
    int k = 0, total = _jg.NumJoysticks();
    for (int j = 0; j < total && k < JG_MANDOS; j++)
        if (_jg.IsGameController(j)) {
            void *m = _jg.ControllerOpen(j);
            if (m) _jgs.mandos[k++] = m;
        }
    _jgs.njoy = total;
}

/* Valor de un eje de -1 a 1 (gatillos de 0 a 1), con zona muerta */
static float jg_leer_eje(void *mando, int e) {
    float v = _jg.ControllerAxis(mando, e) / 32767.0f;
    if (v < -1) v = -1;
    if (fabsf(v) < 0.15f) v = 0;
    return v;
}

static void jg_leer_mandos(void) {
    memcpy(_jgs.botones_prev, _jgs.botones, sizeof _jgs.botones);
    memcpy(_jgs.ejes_prev, _jgs.ejes, sizeof _jgs.ejes);
    if (_jg.NumJoysticks() != _jgs.njoy) jg_abrir_mandos();
    for (int i = 0; i < JG_MANDOS; i++) {
        for (int b = 0; b < JG_BOTONES; b++)
            _jgs.botones[i][b] = _jgs.mandos[i] ? _jg.ControllerButton(_jgs.mandos[i], b) : 0;
        for (int e = 0; e < JG_EJES; e++)
            _jgs.ejes[i][e] = _jgs.mandos[i] ? jg_leer_eje(_jgs.mandos[i], e) : 0;
    }
}

static int jg_mando_arg(Valor **a, int n, int i) {
    if (i < n && a[i]->tipo == VAL_NUMERO) {
        int m = (int)a[i]->numero - 1;
        return m >= 0 && m < JG_MANDOS ? m : -1;
    }
    return 0;
}

static Valor *fn_jg_mandos(Valor **a, int n) {
    (void)a; (void)n;
    int c = 0;
    for (int i = 0; i < JG_MANDOS; i++) if (_jgs.mandos[i]) c++;
    return valor_numero(c);
}

/* boton(nombre[, mando]) / boton_pulsado(nombre[, mando]); mando = 1..4 */
static Valor *jg_boton_mando(Valor **a, int n, int pulso, const char *fn) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) {
        char msg[120];
        snprintf(msg, sizeof msg, "%s() espera el nombre de un botón, por ejemplo \"a\"", fn);
        return jg_error("ErrorTipo", msg);
    }
    int b = -1;
    for (int i = 0; i < JG_BOTONES; i++) if (strcmp(a[0]->texto, JG_NOMBRES_BOTON[i]) == 0) { b = i; break; }
    if (b < 0) {
        char msg[300];
        snprintf(msg, sizeof msg, "%s(): botón desconocido '%s' (a, b, x, y, lb, rb, inicio, atras, guia, arriba, abajo, izquierda, derecha, stick_izq, stick_der)", fn, a[0]->texto);
        return jg_error("Error", msg);
    }
    int m = jg_mando_arg(a, n, 1);
    if (m < 0) return valor_booleano(0);
    int ahora = _jgs.botones[m][b], antes = _jgs.botones_prev[m][b];
    return valor_booleano(pulso ? (ahora && !antes) : ahora);
}
static Valor *fn_jg_boton(Valor **a, int n)         { return jg_boton_mando(a, n, 0, "boton"); }
static Valor *fn_jg_boton_pulsado(Valor **a, int n) { return jg_boton_mando(a, n, 1, "boton_pulsado"); }

/* eje(nombre[, mando]): izq_x, izq_y, der_x, der_y (-1..1), lt, rt (0..1); zona muerta 0,15 */
static Valor *fn_jg_eje(Valor **a, int n) {
    JG_ABIERTA();
    int e = -1;
    if (n > 0 && a[0]->tipo == VAL_TEXTO)
        for (int i = 0; i < JG_EJES; i++) if (strcmp(a[0]->texto, JG_NOMBRES_EJE[i]) == 0) { e = i; break; }
    if (e < 0) return jg_error("Error", "eje() espera \"izq_x\", \"izq_y\", \"der_x\", \"der_y\", \"lt\" o \"rt\"");
    int m = jg_mando_arg(a, n, 1);
    if (m < 0) return valor_numero(0);
    return valor_numero(_jgs.ejes[m][e]);
}

/* ── Texto tecleado ── */

/* texto_escrito(): lo que se ha tecleado desde el fotograma anterior (con tildes y ñ) */
static Valor *fn_jg_texto_escrito(Valor **a, int n) {
    (void)a; (void)n;
    JG_ABIERTA();
    _jgs.pide_texto = 1;
    char buf[JG_ESCRITO];
    int k = 0;
    for (int i = 0; i < _jgs.nescrito; i++) if (_jgs.escrito[i] != '\b') buf[k++] = _jgs.escrito[i];
    buf[k] = '\0';
    return valor_texto(buf);
}

static int jg_caracteres(const char *t) {
    int c = 0;
    for (; *t; t++) if (((unsigned char)*t & 0xC0) != 0x80) c++;
    return c;
}

/* editar(texto[, maximo]): el texto con lo tecleado añadido y «borrar» aplicado.
   Uso: nombre = juego.editar(nombre, 12) en cada fotograma. */
static Valor *fn_jg_editar(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "editar() espera (texto[, máximo de caracteres])");
    _jgs.pide_texto = 1;
    double max = 0;
    if (n > 1 && !jg_num(a, n, 1, &max, "editar")) return valor_nulo();
    size_t largo = strlen(a[0]->texto);
    char *t = malloc(largo + JG_ESCRITO + 1);
    if (!t) return valor_nulo();
    memcpy(t, a[0]->texto, largo + 1);
    int cuenta = jg_caracteres(t);
    for (int i = 0; i < _jgs.nescrito; ) {
        unsigned char c = (unsigned char)_jgs.escrito[i];
        if (c == '\b') {                                   /* quita el último carácter entero */
            if (largo > 0) {
                do largo--; while (largo > 0 && ((unsigned char)t[largo] & 0xC0) == 0x80);
                t[largo] = '\0';
                cuenta--;
            }
            i++;
            continue;
        }
        int l = 1;                                         /* un carácter UTF-8 completo */
        while (i + l < _jgs.nescrito && ((unsigned char)_jgs.escrito[i + l] & 0xC0) == 0x80) l++;
        if (c >= 32 && (max <= 0 || cuenta < (int)max)) {
            memcpy(t + largo, _jgs.escrito + i, (size_t)l);
            largo += (size_t)l;
            t[largo] = '\0';
            cuenta++;
        }
        i += l;
    }
    Valor *v = valor_texto(t);
    free(t);
    return v;
}

/* ── Puntero del ratón ── */

/* cursor(visible): muestra u oculta la flecha del ratón sobre la ventana */
static Valor *fn_jg_cursor(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_BOOLEANO) return jg_error("ErrorTipo", "cursor() espera verdadero o falso");
    if (_jg.ShowCursor) _jg.ShowCursor(a[0]->booleano ? 1 : 0);
    return valor_nulo();
}

/* capturar_raton(si): encierra y oculta el ratón; solo cuenta movimiento_raton().
   Devuelve si se pudo (algunos sistemas no lo permiten). */
static Valor *fn_jg_capturar_raton(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_BOOLEANO) return jg_error("ErrorTipo", "capturar_raton() espera verdadero o falso");
    int ok = _jg.RelativeMouse && _jg.RelativeMouse(a[0]->booleano ? 1 : 0) == 0;
    return valor_booleano(ok);
}

/* movimiento_raton() -> [dx, dy]: cuánto se movió desde el fotograma anterior */
static Valor *fn_jg_movimiento_raton(Valor **a, int n) {
    (void)a; (void)n;
    Valor *l = valor_lista_crear();
    lista_agregar(l, valor_numero(_jgs.mov_x));
    lista_agregar(l, valor_numero(_jgs.mov_y));
    return l;
}

/* ── Acciones: un nombre para varias teclas, botones y ejes ──
   Entradas: "espacio" (tecla), "mando:a" (botón de cualquier mando), "mando2:a"
   (del mando 2), "mando:izq_x-" / "mando:izq_x+" (eje hacia un lado), "raton:izquierdo". */

static int jg_entrada(const char *t, JgEntrada *e, char *msg, size_t max) {
    e->mando = -1; e->signo = 0;
    if (strncmp(t, "raton:", 6) == 0) {
        static const char *const B[] = {"izquierdo", "central", "derecho"};
        for (int i = 0; i < 3; i++) if (strcmp(t + 6, B[i]) == 0) { e->tipo = 'r'; e->codigo = i + 1; return 1; }
        snprintf(msg, max, "botón de ratón desconocido '%s' (raton:izquierdo, raton:central, raton:derecho)", t);
        return 0;
    }
    if (strncmp(t, "mando", 5) == 0 && (t[5] == ':' || (t[5] >= '1' && t[5] <= '4' && t[6] == ':'))) {
        if (t[5] != ':') e->mando = t[5] - '1';
        const char *b = t + (t[5] == ':' ? 6 : 7);
        for (int i = 0; i < JG_BOTONES; i++)
            if (strcmp(b, JG_NOMBRES_BOTON[i]) == 0) { e->tipo = 'b'; e->codigo = i; return 1; }
        size_t lb = strlen(b);
        if (lb > 1 && (b[lb - 1] == '+' || b[lb - 1] == '-'))
            for (int i = 0; i < JG_EJES; i++)
                if (strlen(JG_NOMBRES_EJE[i]) == lb - 1 && strncmp(b, JG_NOMBRES_EJE[i], lb - 1) == 0) {
                    e->tipo = 'e'; e->codigo = i; e->signo = b[lb - 1] == '+' ? 1 : -1;
                    return 1;
                }
        snprintf(msg, max, "entrada de mando desconocida '%s' (por ejemplo mando:a, mando:izquierda o mando:izq_x-)", t);
        return 0;
    }
    int c = jg_tecla_codigo(t);
    if (c < 0) { snprintf(msg, max, "tecla desconocida '%s'", t); return 0; }
    e->tipo = 't'; e->codigo = c;
    return 1;
}

/* Fuerza de una entrada (0 a 1) ahora o en el fotograma anterior */
static float jg_fuerza(const JgEntrada *e, int antes) {
    switch (e->tipo) {
        case 't': return (antes ? _jgs.previas : _jgs.teclas)[e->codigo] ? 1 : 0;
        case 'r': return ((antes ? _jgs.raton_prev : _jgs.raton) & (1u << (e->codigo - 1))) ? 1 : 0;
        case 'b': case 'e': {
            float mejor = 0;
            for (int m = 0; m < JG_MANDOS; m++) {
                if (e->mando >= 0 && m != e->mando) continue;
                float v = e->tipo == 'b' ? (antes ? _jgs.botones_prev : _jgs.botones)[m][e->codigo]
                                         : (antes ? _jgs.ejes_prev : _jgs.ejes)[m][e->codigo] * e->signo;
                if (v > mejor) mejor = v;
            }
            return mejor > 1 ? 1 : mejor;
        }
    }
    return 0;
}

static JgAccion *jg_buscar_accion(const char *nombre) {
    for (int i = 0; i < _jgs.nacciones; i++)
        if (strcmp(_jgs.acciones[i].nombre, nombre) == 0) return &_jgs.acciones[i];
    return NULL;
}

/* definir_accion(nombre, [entradas]): crea o sustituye la acción */
static Valor *fn_jg_definir_accion(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 2 || a[0]->tipo != VAL_TEXTO || a[1]->tipo != VAL_LISTA)
        return jg_error("ErrorTipo", "definir_accion() espera (nombre, lista de entradas), por ejemplo (\"saltar\", [\"espacio\", \"mando:a\"])");
    if (a[1]->lista.cantidad > JG_ENTRADAS) return jg_error("Error", "definir_accion(): como mucho 16 entradas por acción");
    JgAccion nueva = {0};
    for (int i = 0; i < a[1]->lista.cantidad; i++) {
        Valor *v = a[1]->lista.elementos[i];
        char msg[300];
        if (v->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "definir_accion(): cada entrada es un texto, por ejemplo \"espacio\" o \"mando:a\"");
        if (!jg_entrada(v->texto, &nueva.e[nueva.n], msg, sizeof msg)) {
            char m2[400];
            snprintf(m2, sizeof m2, "definir_accion(\"%s\"): %s", a[0]->texto, msg);
            return jg_error("Error", m2);
        }
        nueva.n++;
    }
    JgAccion *acc = jg_buscar_accion(a[0]->texto);
    if (!acc) {
        if (_jgs.nacciones == JG_ACCIONES) return jg_error("Error", "definir_accion(): como mucho 64 acciones");
        acc = &_jgs.acciones[_jgs.nacciones++];
        acc->nombre = strdup(a[0]->texto);
    }
    nueva.nombre = acc->nombre;
    *acc = nueva;
    return valor_nulo();
}

static JgAccion *jg_accion_arg(Valor **a, int n, int i, const char *fn) {
    char msg[200];
    if (i >= n || a[i]->tipo != VAL_TEXTO) {
        snprintf(msg, sizeof msg, "%s() espera el nombre de una acción", fn);
        jg_error("ErrorTipo", msg);
        return NULL;
    }
    JgAccion *acc = jg_buscar_accion(a[i]->texto);
    if (!acc) {
        snprintf(msg, sizeof msg, "%s(): la acción '%s' no está definida (usa juego.definir_accion)", fn, a[i]->texto);
        jg_error("Error", msg);
    }
    return acc;
}

/* La fuerza de una acción es la mayor de sus entradas; activa a partir de 0,5 */
static float jg_accion_fuerza(const JgAccion *acc, int antes) {
    float f = 0;
    for (int i = 0; i < acc->n; i++) { float v = jg_fuerza(&acc->e[i], antes); if (v > f) f = v; }
    return f;
}

/* modo 0: mantenida; 1: empieza en este fotograma; 2: termina en este fotograma */
static Valor *jg_accion(Valor **a, int n, int modo, const char *fn) {
    JG_ABIERTA();
    JgAccion *acc = jg_accion_arg(a, n, 0, fn);
    if (!acc) return valor_nulo();
    int ahora = jg_accion_fuerza(acc, 0) >= 0.5f, antes = jg_accion_fuerza(acc, 1) >= 0.5f;
    return valor_booleano(modo == 0 ? ahora : modo == 1 ? (ahora && !antes) : (!ahora && antes));
}
static Valor *fn_jg_accion(Valor **a, int n)          { return jg_accion(a, n, 0, "accion"); }
static Valor *fn_jg_accion_pulsada(Valor **a, int n)  { return jg_accion(a, n, 1, "accion_pulsada"); }
static Valor *fn_jg_accion_soltada(Valor **a, int n)  { return jg_accion(a, n, 2, "accion_soltada"); }

/* direccion(negativa, positiva): de -1 a 1, gradual con el stick y entera con teclas.
   Ejemplo: x = x + juego.direccion("izquierda", "derecha") * velocidad * juego.dt() */
static Valor *fn_jg_direccion(Valor **a, int n) {
    JG_ABIERTA();
    JgAccion *neg = jg_accion_arg(a, n, 0, "direccion");
    if (!neg) return valor_nulo();
    JgAccion *pos = jg_accion_arg(a, n, 1, "direccion");
    if (!pos) return valor_nulo();
    return valor_numero(jg_accion_fuerza(pos, 0) - jg_accion_fuerza(neg, 0));
}

/* colision(x1, y1, w1, h1, x2, y2, w2, h2): ¿se solapan los dos rectángulos? */
static Valor *fn_jg_colision(Valor **a, int n) {
    double v[8];
    for (int i = 0; i < 8; i++) if (!jg_num(a, n, i, &v[i], "colision")) return valor_nulo();
    return valor_booleano(v[0] < v[4] + v[6] && v[0] + v[2] > v[4] &&
                          v[1] < v[5] + v[7] && v[1] + v[3] > v[5]);
}

/* ── Colisiones ── */

/* colision_circulos(x1, y1, r1, x2, y2, r2): ¿se tocan los dos círculos? */
static Valor *fn_jg_colision_circulos(Valor **a, int n) {
    double v[6];
    for (int i = 0; i < 6; i++) if (!jg_num(a, n, i, &v[i], "colision_circulos")) return valor_nulo();
    double dx = v[3] - v[0], dy = v[4] - v[1], r = v[2] + v[5];
    return valor_booleano(dx * dx + dy * dy < r * r);
}

/* colision_circulo_rect(cx, cy, radio, x, y, ancho, alto): ¿toca el círculo al rectángulo? */
static Valor *fn_jg_colision_circulo_rect(Valor **a, int n) {
    double v[7];
    for (int i = 0; i < 7; i++) if (!jg_num(a, n, i, &v[i], "colision_circulo_rect")) return valor_nulo();
    double px = v[0] < v[3] ? v[3] : v[0] > v[3] + v[5] ? v[3] + v[5] : v[0];   /* punto del rectángulo más cercano */
    double py = v[1] < v[4] ? v[4] : v[1] > v[4] + v[6] ? v[4] + v[6] : v[1];
    double dx = v[0] - px, dy = v[1] - py;
    return valor_booleano(dx * dx + dy * dy < v[2] * v[2]);
}

/* dentro(px, py, x, y, ancho, alto): ¿está el punto dentro del rectángulo? (botones, clics) */
static Valor *fn_jg_dentro(Valor **a, int n) {
    double v[6];
    for (int i = 0; i < 6; i++) if (!jg_num(a, n, i, &v[i], "dentro")) return valor_nulo();
    return valor_booleano(v[0] >= v[2] && v[0] < v[2] + v[4] && v[1] >= v[3] && v[1] < v[3] + v[5]);
}

/* ── Animación ──
   cuadro(cuadros, por_segundo[, desde[, repetir]]): el cuadro que toca ahora.
   'cuadros' es cuántos hay (devuelve de 0 a cuadros - 1) o una lista de
   cuadros ([0, 1, 2, 1]: devuelve el elemento que toca). 'desde' es el
   juego.tiempo() en que empezó la animación (0 por defecto); con repetir =
   falso se queda en el último cuadro (una explosión, por ejemplo). */
static Valor *fn_jg_cuadro(Valor **a, int n) {
    if (n < 2 || (a[0]->tipo != VAL_NUMERO && a[0]->tipo != VAL_LISTA))
        return jg_error("ErrorTipo", "cuadro() espera (número de cuadros o lista de cuadros, cuadros por segundo[, desde[, repetir]])");
    double fps, desde = 0;
    if (!jg_num(a, n, 1, &fps, "cuadro")) return valor_nulo();
    if (n > 2 && !jg_num(a, n, 2, &desde, "cuadro")) return valor_nulo();
    int repetir = !(n > 3 && a[3]->tipo == VAL_BOOLEANO && !a[3]->booleano);
    int total = a[0]->tipo == VAL_LISTA ? a[0]->lista.cantidad : (int)a[0]->numero;
    if (total < 1) return jg_error("Error", "cuadro(): hace falta al menos un cuadro");
    double t = jg_reloj() - desde;
    long k = t > 0 ? (long)floor(t * fps + 1e-9) : 0;
    if (repetir) k %= total;
    else if (k >= total) k = total - 1;
    if (a[0]->tipo == VAL_LISTA) { Valor *e = a[0]->lista.elementos[k]; e->refs++; return e; }
    return valor_numero((double)k);
}

/* ── Partículas ──
   particulas(x, y[, opciones]) lanza un puñado desde (x, y); dibujar_particulas()
   las mueve y las dibuja (llámala cada fotograma, donde quieras que salgan). */
static const char *const JG_OPC_PART[] = {
    "cantidad", "color", "color_final", "velocidad", "direccion", "apertura", "vida",
    "tamaño", "tamano", "tamaño_final", "tamano_final", "gravedad", NULL };

/* Número u [mínimo, máximo] (al azar entre los dos) */
static int jg_rango(Valor *op, const char *clave, double *min, double *max) {
    Valor *v = jg_opcion_valor(op, clave);
    if (!v) return 1;
    if (v->tipo == VAL_NUMERO) { *min = *max = v->numero; return 1; }
    if (v->tipo == VAL_LISTA && v->lista.cantidad == 2 && v->lista.elementos[0]->tipo == VAL_NUMERO &&
        v->lista.elementos[1]->tipo == VAL_NUMERO) {
        *min = v->lista.elementos[0]->numero; *max = v->lista.elementos[1]->numero;
        return 1;
    }
    char msg[160];
    snprintf(msg, sizeof msg, "particulas(): '%s' es un número o [mínimo, máximo]", clave);
    jg_error("ErrorTipo", msg);
    return 0;
}

static double jg_azar_entre(double a, double b) { return a + (b - a) * mat_azar(); }

static Valor *fn_jg_particulas(Valor **a, int n) {
    JG_ABIERTA();
    double x, y;
    if (!jg_num(a, n, 0, &x, "particulas") || !jg_num(a, n, 1, &y, "particulas")) return valor_nulo();
    Valor *op = n > 2 ? a[2] : NULL;
    if (op && op->tipo != VAL_DICCIONARIO && op->tipo != VAL_NULO)
        return jg_error("ErrorTipo", "particulas() espera (x, y[, opciones]); las opciones van en un diccionario");
    if (op && op->tipo == VAL_DICCIONARIO) {
        for (int i = 0; i < op->diccionario.cantidad; i++) {
            const char *k = op->diccionario.claves[i];
            int bien = 0;
            for (int j = 0; JG_OPC_PART[j]; j++) if (strcmp(k, JG_OPC_PART[j]) == 0) bien = 1;
            if (!bien) {
                char msg[400];
                snprintf(msg, sizeof msg, "particulas(): opción desconocida '%s'. Las opciones son: cantidad, color, "
                         "color_final, velocidad, direccion, apertura, vida, tamaño, tamaño_final y gravedad", k);
                return jg_error("Error", msg);
            }
        }
    }
    double cantidad = 20, v0 = 60, v1 = 180, dir = 0, apertura = 360, vida0 = 0.4, vida1 = 0.8;
    double tam0 = 4, tam1 = 4, grav0 = 0, grav1 = 0;
    jg_opcion(op, "cantidad", &cantidad);
    jg_opcion(op, "direccion", &dir);
    jg_opcion(op, "apertura", &apertura);
    if (!jg_rango(op, "velocidad", &v0, &v1) || !jg_rango(op, "vida", &vida0, &vida1) ||
        !jg_rango(op, "tamano", &tam0, &tam1) || !jg_rango(op, "tamaño", &tam0, &tam1) ||
        !jg_rango(op, "gravedad", &grav0, &grav1)) return valor_nulo();
    double fin0 = -1, fin1 = -1;
    if (!jg_rango(op, "tamano_final", &fin0, &fin1) || !jg_rango(op, "tamaño_final", &fin0, &fin1)) return valor_nulo();
    unsigned char c[4], c_fin[4];
    Valor *vc = jg_opcion_valor(op, "color"), *vf = jg_opcion_valor(op, "color_final");
    Valor *arg[1];
    arg[0] = vc;
    if (!jg_color(arg, vc ? 1 : 0, 0, c)) return valor_nulo();
    memcpy(c_fin, c, 4);
    c_fin[3] = 0;                                           /* por defecto se desvanece */
    if (vf) { arg[0] = vf; if (!jg_color(arg, 1, 0, c_fin)) return valor_nulo(); }
    int cuantas = cantidad < 0 ? 0 : cantidad > JG_PARTICULAS ? JG_PARTICULAS : (int)cantidad;
    for (int i = 0; i < cuantas; i++) {
        JgParticula *p;
        if (_jgs.npart < JG_PARTICULAS) p = &_jgs.part[_jgs.npart++];
        else p = &_jgs.part[(int)(mat_azar() * JG_PARTICULAS)];   /* llenas: sustituye una cualquiera */
        double ang = (dir + (mat_azar() - 0.5) * apertura) * M_PI / 180.0;
        double vel = jg_azar_entre(v0, v1);
        p->x = (float)x; p->y = (float)y;
        p->vx = (float)(cos(ang) * vel); p->vy = (float)(sin(ang) * vel);
        p->grav = (float)jg_azar_entre(grav0, grav1);
        p->total = p->vida = (float)jg_azar_entre(vida0, vida1);
        p->tam = (float)jg_azar_entre(tam0, tam1);
        p->tam_fin = fin0 < 0 ? p->tam : (float)jg_azar_entre(fin0, fin1);
        memcpy(p->c, c, 4);
        memcpy(p->c_fin, c_fin, 4);
    }
    return valor_nulo();
}

/* dibujar_particulas(): mueve y dibuja las partículas vivas; devuelve cuántas quedan */
static Valor *fn_jg_dibujar_particulas(Valor **a, int n) {
    (void)a; (void)n;
    JG_ABIERTA();
    float dt = (float)_jgs.dt;
    int vivas = 0;
    _jg.SetBlend(_jgs.render, 1);
    for (int i = 0; i < _jgs.npart; i++) {
        JgParticula p = _jgs.part[i];
        p.vida -= dt;
        if (p.vida <= 0 || p.total <= 0) continue;
        p.vy += p.grav * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        _jgs.part[vivas++] = p;
        float t = 1 - p.vida / p.total;                     /* 0 al nacer, 1 al morir */
        unsigned char c[4];
        for (int k = 0; k < 4; k++) c[k] = (unsigned char)(p.c[k] + (p.c_fin[k] - p.c[k]) * t);
        float tam = p.tam + (p.tam_fin - p.tam) * t;
        if (tam < 0.5f || c[3] == 0) continue;
        _jg.SetDrawColor(_jgs.render, c[0], c[1], c[2], c[3]);
        JgRect r = {(int)floorf(p.x - tam / 2 - (float)_jgs.cam_x), (int)floorf(p.y - tam / 2 - (float)_jgs.cam_y),
                    (int)ceilf(tam), (int)ceilf(tam)};
        _jg.FillRect(_jgs.render, &r);
    }
    _jgs.npart = vivas;
    return valor_numero(vivas);
}

/* borrar_particulas(): quita todas (al cambiar de nivel, por ejemplo) */
static Valor *fn_jg_borrar_particulas(Valor **a, int n) {
    (void)a; (void)n;
    _jgs.npart = 0;
    return valor_nulo();
}

/* ── Escenas ──
   Un nombre para cada parte del juego ("menu", "partida", "fin"...). El cambio
   se hace en el siguiente presentar(), así un fotograma no mezcla dos escenas;
   con fundido, la pantalla se oscurece, cambia y se aclara. */
static void jg_poner_escena(const char *nombre) {
    snprintf(_jgs.escena, sizeof _jgs.escena, "%s", nombre);
    _jgs.escena_nueva = 1;
    _jgs.t_escena = jg_reloj();
}

/* cambiar_escena(nombre[, fundido]): fundido en segundos (0 = de golpe) */
static Valor *fn_jg_cambiar_escena(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "cambiar_escena() espera (nombre[, segundos de fundido])");
    double f = 0;
    if (n > 1 && !jg_num(a, n, 1, &f, "cambiar_escena")) return valor_nulo();
    if (!_jgs.escena[0] && f <= 0) {                       /* la primera escena empieza ya */
        jg_poner_escena(a[0]->texto);
        return valor_nulo();
    }
    snprintf(_jgs.siguiente, sizeof _jgs.siguiente, "%s", a[0]->texto);
    _jgs.hay_siguiente = 1;
    _jgs.fundido = f > 0 ? f : 0;
    if (f > 0 && _jgs.fase != 1) { _jgs.fase = 1; _jgs.t_fundido = jg_reloj(); }
    return valor_nulo();
}

static Valor *fn_jg_escena(Valor **a, int n)        { (void)a; (void)n; return valor_texto(_jgs.escena); }
static Valor *fn_jg_escena_nueva(Valor **a, int n)  { (void)a; (void)n; return valor_booleano(_jgs.escena_nueva); }
static Valor *fn_jg_tiempo_escena(Valor **a, int n) { (void)a; (void)n; return valor_numero(jg_reloj() - _jgs.t_escena); }

/* En presentar(): hace el cambio pendiente y dibuja el fundido */
static void jg_escenas(void) {
    _jgs.escena_nueva = 0;
    double ahora = jg_reloj();
    if (_jgs.hay_siguiente && _jgs.fase != 1) {
        _jgs.hay_siguiente = 0;
        jg_poner_escena(_jgs.siguiente);
    }
    if (!_jgs.fase) return;
    double mitad = _jgs.fundido / 2, p = mitad > 0 ? (ahora - _jgs.t_fundido) / mitad : 1;
    if (_jgs.fase == 1 && p >= 1) {                        /* a oscuras: cambia y empieza a aclarar */
        if (_jgs.hay_siguiente) { _jgs.hay_siguiente = 0; jg_poner_escena(_jgs.siguiente); }
        _jgs.fase = 2;
        _jgs.t_fundido = ahora;
        p = 0;
    } else if (_jgs.fase == 2 && p >= 1) {
        _jgs.fase = 0;
        return;
    }
    double alfa = _jgs.fase == 1 ? p : 1 - p;
    unsigned char negro[4] = {0, 0, 0, (unsigned char)(alfa < 0 ? 0 : alfa > 1 ? 255 : alfa * 255)};
    jg_pintar(negro);
    JgRect r = {0, 0, _jgs.ancho, _jgs.alto};
    _jg.FillRect(_jgs.render, &r);
}

static void cargar_juego(Entorno *entorno) {
    FuncionNativa fns[] = {
        {"abrir",           fn_jg_abrir,           -1},
        {"abierto",         fn_jg_abierto,          0},
        {"salir",           fn_jg_salir,            0},
        {"cerrar",          fn_jg_cerrar,           0},
        {"titulo",          fn_jg_titulo,           1},
        {"ancho",           fn_jg_ancho,            0},
        {"alto",            fn_jg_alto,             0},
        {"dt",              fn_jg_dt,               0},
        {"fps",             fn_jg_fps,              0},
        {"limpiar",         fn_jg_limpiar,         -1},
        {"rectangulo",      fn_jg_rectangulo,      -1},
        {"contorno",        fn_jg_contorno,        -1},
        {"linea",           fn_jg_linea,           -1},
        {"circulo",         fn_jg_circulo,         -1},
        {"texto",           fn_jg_texto,           -1},
        {"ancho_texto",     fn_jg_ancho_texto,     -1},
        {"tecla",           fn_jg_tecla,            1},
        {"tecla_pulsada",   fn_jg_tecla_pulsada,    1},
        {"tecla_soltada",   fn_jg_tecla_soltada,    1},
        {"raton",           fn_jg_raton,            0},
        {"boton_raton",     fn_jg_boton_raton,     -1},
        {"clic",            fn_jg_boton_raton_pulsado, -1},
        {"rueda",           fn_jg_rueda,            0},
        {"presentar",       fn_jg_presentar,        0},
        {"captura",         fn_jg_captura,          1},
        {"pitido",          fn_jg_pitido,          -1},
        {"colision",        fn_jg_colision,         8},
        {"imagen",          fn_jg_imagen,           1},
        {"dibujar",         fn_jg_dibujar,         -1},
        {"sprite",          fn_jg_sprite,           2},
        {"sonido",          fn_jg_sonido,           1},
        {"reproducir",      fn_jg_reproducir,      -1},
        {"detener",         fn_jg_detener,         -1},
        {"musica",          fn_jg_musica,          -1},
        {"volumen",         fn_jg_volumen,         -1},
        {"camara",          fn_jg_camara,          -1},
        {"pantalla_completa", fn_jg_pantalla_completa, -1},
        {"tiempo",          fn_jg_tiempo,           0},
        {"mandos",          fn_jg_mandos,           0},
        {"boton",           fn_jg_boton,           -1},
        {"boton_pulsado",   fn_jg_boton_pulsado,   -1},
        {"eje",             fn_jg_eje,             -1},
        {"texto_escrito",   fn_jg_texto_escrito,    0},
        {"editar",          fn_jg_editar,          -1},
        {"cursor",          fn_jg_cursor,           1},
        {"capturar_raton",  fn_jg_capturar_raton,   1},
        {"movimiento_raton", fn_jg_movimiento_raton, 0},
        {"definir_accion",  fn_jg_definir_accion,   2},
        {"accion",          fn_jg_accion,           1},
        {"accion_pulsada",  fn_jg_accion_pulsada,   1},
        {"accion_soltada",  fn_jg_accion_soltada,   1},
        {"direccion",       fn_jg_direccion,        2},
        {"recargado",       fn_jg_recargado,        0},
        {"colision_circulos", fn_jg_colision_circulos, 6},
        {"colision_circulo_rect", fn_jg_colision_circulo_rect, 7},
        {"dentro",          fn_jg_dentro,           6},
        {"cuadro",          fn_jg_cuadro,          -1},
        {"particulas",      fn_jg_particulas,      -1},
        {"dibujar_particulas", fn_jg_dibujar_particulas, 0},
        {"borrar_particulas", fn_jg_borrar_particulas, 0},
        {"cambiar_escena",  fn_jg_cambiar_escena,  -1},
        {"escena",          fn_jg_escena,           0},
        {"escena_nueva",    fn_jg_escena_nueva,     0},
        {"tiempo_escena",   fn_jg_tiempo_escena,    0},
    };
    registrar_modulo_diccionario(entorno, "juego",
        fns, (int)(sizeof(fns) / sizeof(fns[0])), NULL, NULL, 0);
}
