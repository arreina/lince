/* ─────────────────────────────────────────
   MÓDULO: juego  (se incluye desde modulos.c)
   Ventana 2D, dibujo, imágenes, texto, teclado, ratón, mando y sonido. Usa SDL2
   (y SDL2_image para PNG/JPG, si está) cargadas en tiempo de ejecución
   (dlopen / LoadLibrary): Lince no las necesita para compilarse, y si faltan
   solo falla juego.abrir().
   Estructura típica:
       juego.abrir("Mi juego", 800, 600)
       mientras juego.abierto() { ...dibujar...; juego.presentar() }
───────────────────────────────────────── */
#ifdef _WIN32
#define JG_ABRIR_LIB()   ((void *)LoadLibraryA("SDL2.dll"))
#define JG_ABRIR_IMG()   ((void *)LoadLibraryA("SDL2_image.dll"))
#define JG_SIMBOLO(h, n) ((void *)GetProcAddress((HMODULE)(h), n))
#else
static void *jg_abrir_lib(void) {
    static const char *const nombres[] = {
        "libSDL2-2.0.so.0", "libSDL2.so", "libSDL2-2.0.0.dylib", "libSDL2.dylib", NULL };
    for (int i = 0; nombres[i]; i++) {
        void *h = dlopen(nombres[i], RTLD_NOW);
        if (h) return h;
    }
    return NULL;
}
static void *jg_abrir_img(void) {
    static const char *const nombres[] = {
        "libSDL2_image-2.0.so.0", "libSDL2_image.so", "libSDL2_image-2.0.0.dylib", "libSDL2_image.dylib", NULL };
    for (int i = 0; nombres[i]; i++) {
        void *h = dlopen(nombres[i], RTLD_NOW);
        if (h) return h;
    }
    return NULL;
}
#define JG_ABRIR_LIB()   jg_abrir_lib()
#define JG_ABRIR_IMG()   jg_abrir_img()
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
    void *(*IMG_Load_RW)(void *, int);                               /* SDL2_image */
} _jg;

/* Formato de audio de Lince: 16 bits con signo, estéreo, 44100 Hz */
#define JG_FRECUENCIA 44100
#define JG_S16        0x8010
#define JG_CANALES    2

typedef struct { int freq; unsigned short formato; unsigned char canales, silencio;
                 unsigned short muestras, pad; unsigned tam; void (*callback)(void *, unsigned char *, int);
                 void *datos; } JgAudioSpec;

typedef struct { void *tex; int ancho, alto; char *ruta; } JgImagen;
typedef struct { short *datos; int muestras; char *ruta; } JgSonido;   /* muestras por canal */
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
       LINCE_JUEGO_CAPTURA=ruta.bmp guarda el último fotograma. */
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
    int njoy;
} _jgs;

static int jg_tecla_codigo(const char *nombre);
static int jg_guardar_bmp(const char *ruta);
static void jg_leer_mandos(void);
static void jg_recoger_voces(void);

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
    void *hi = JG_ABRIR_IMG();
    if (hi) *(void **)(&_jg.IMG_Load_RW) = JG_SIMBOLO(hi, "IMG_Load_RW");
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

/* abrir(titulo, ancho, alto) */
static Valor *fn_jg_abrir(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "abrir() espera (título, ancho, alto)");
    double w = 800, h = 600;
    if (n > 1 && !jg_num(a, n, 1, &w, "abrir")) return valor_nulo();
    if (n > 2 && !jg_num(a, n, 2, &h, "abrir")) return valor_nulo();
    if (w < 1 || h < 1 || w > 16384 || h > 16384) return jg_error("Error", "abrir(): tamaño de ventana no válido");
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
    e = getenv("LINCE_JUEGO_CLIC");
    if (e && sscanf(e, "%d,%d", &_jgs.clic_x, &_jgs.clic_y) == 2) _jgs.hay_clic = 1;
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
    x -= _jgs.cam_x;
    y -= _jgs.cam_y;
    int px = (int)x, py = (int)y, ac;
    const unsigned char *p = (const unsigned char *)a[0]->texto;
    while (*p) {
        int ch = jg_siguiente(&p, &ac);
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch == '\n') { px = (int)x; py += 9 * e; continue; }
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
        px += 6 * e;
    }
    return valor_nulo();
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
    if (_jgs.limite && _jgs.total + 1 >= _jgs.limite && getenv("LINCE_JUEGO_CAPTURA"))
        jg_guardar_bmp(getenv("LINCE_JUEGO_CAPTURA"));      /* prueba: guarda el último fotograma */
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
    unsigned long long ev_buf[16];                      /* SDL_Event (56 bytes), alineado */
    unsigned char *ev = (unsigned char *)ev_buf;
    while (_jg.PollEvent(ev)) {
        unsigned tipo = *(unsigned *)ev;
        if (tipo == 0x100) _jgs.cierre = 1;                         /* SDL_QUIT */
        else if (tipo == 0x403) _jgs.rueda += *(int *)(ev + 20);    /* rueda: y */
    }
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
    if (_jgs.total & 1) {
        for (int i = 0; i < _jgs.npulsar; i++) _jgs.teclas[_jgs.pulsar[i]] = 1;
        if (_jgs.hay_clic) { _jgs.rx = _jgs.clic_x; _jgs.ry = _jgs.clic_y; _jgs.raton |= 1; }
    } else if (_jgs.hay_clic) { _jgs.rx = _jgs.clic_x; _jgs.ry = _jgs.clic_y; }
    return valor_nulo();
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

/* Carga un WAV (del disco o del ejecutable empaquetado) en el formato de Lince */
static int jg_cargar_wav(const char *ruta, short **datos, int *muestras, char *error, size_t max) {
    size_t largo = 0;
    char *bytes = interprete_leer_bytes(ruta, &largo);
    if (!bytes) { snprintf(error, max, "no se encuentra el sonido '%s'", ruta); return 0; }
    JgAudioSpec spec;
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
    size_t cap = (size_t)wav_largo * 4 + 4096, leido = 0;
    char *buf = malloc(cap);
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

/* sonido(ruta): carga un WAV; devuelve {"sonido": n, "duracion": segundos} */
static Valor *fn_jg_sonido(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "sonido() espera la ruta de un archivo .wav");
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

/* imagen(ruta): PNG/JPG (con SDL2_image) o BMP; devuelve {"imagen", "ancho", "alto"} */
static Valor *fn_jg_imagen(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "imagen() espera la ruta de una imagen");
    for (int i = 0; i < _jgs.nimagenes; i++)
        if (strcmp(_jgs.imagenes[i].ruta, a[0]->texto) == 0) return jg_imagen_valor(i);
    if (_jgs.nimagenes == JG_MAX_IMAGENES) return jg_error("ErrorJuego", "demasiadas imágenes cargadas");
    size_t largo = 0;
    char *bytes = interprete_leer_bytes(a[0]->texto, &largo);
    char msg[600];
    if (!bytes) {
        snprintf(msg, sizeof msg, "no se encuentra la imagen '%s'", a[0]->texto);
        return jg_error("ErrorJuego", msg);
    }
    int es_bmp = largo > 2 && bytes[0] == 'B' && bytes[1] == 'M';
    void *sup = NULL;
    void *rw = _jg.RWFromConstMem(bytes, (int)largo);
    if (rw) sup = es_bmp || !_jg.IMG_Load_RW ? _jg.LoadBMP_RW(rw, 1) : _jg.IMG_Load_RW(rw, 1);
    free(bytes);
    if (!sup) {
        if (!es_bmp && !_jg.IMG_Load_RW)
            snprintf(msg, sizeof msg, "para cargar '%s' hace falta SDL2_image (por ejemplo 'sudo apt install libsdl2-image-2.0-0'); sin ella solo se leen .bmp", a[0]->texto);
        else
            snprintf(msg, sizeof msg, "no se pudo leer la imagen '%s': %s", a[0]->texto, _jg.GetError());
        return jg_error("ErrorJuego", msg);
    }
    void *tex = _jg.TextureFromSurface(_jgs.render, sup);
    _jg.FreeSurface(sup);
    if (!tex) {
        snprintf(msg, sizeof msg, "no se pudo preparar la imagen '%s': %s", a[0]->texto, _jg.GetError());
        return jg_error("ErrorJuego", msg);
    }
    int k = _jgs.nimagenes++;
    _jgs.imagenes[k].tex = tex;
    _jgs.imagenes[k].ruta = strdup(a[0]->texto);
    _jg.QueryTexture(tex, NULL, NULL, &_jgs.imagenes[k].ancho, &_jgs.imagenes[k].alto);
    _jg.TextureBlend(tex, 1);
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
    char nombre[32];
    snprintf(nombre, sizeof nombre, "<sprite %d>", k);
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
static Valor *fn_jg_tiempo(Valor **a, int n) {
    (void)a; (void)n;
    if (!_jgs.abierta) return valor_numero(0);
    if (_jgs.limite) return valor_numero(_jgs.total / 60.0);      /* modo de prueba: tiempo fijo */
    return valor_numero(jg_ahora() - _jgs.t_inicio);
}

/* ── Mandos ── */
static const char *const JG_NOMBRES_BOTON[JG_BOTONES] = {
    "a", "b", "x", "y", "atras", "guia", "inicio", "stick_izq", "stick_der",
    "lb", "rb", "arriba", "abajo", "izquierda", "derecha" };

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

static void jg_leer_mandos(void) {
    memcpy(_jgs.botones_prev, _jgs.botones, sizeof _jgs.botones);
    if (_jg.NumJoysticks() != _jgs.njoy) jg_abrir_mandos();
    for (int i = 0; i < JG_MANDOS; i++)
        for (int b = 0; b < JG_BOTONES; b++)
            _jgs.botones[i][b] = _jgs.mandos[i] ? _jg.ControllerButton(_jgs.mandos[i], b) : 0;
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
    static const char *const ejes[] = {"izq_x", "izq_y", "der_x", "der_y", "lt", "rt"};
    int e = -1;
    if (n > 0 && a[0]->tipo == VAL_TEXTO)
        for (int i = 0; i < 6; i++) if (strcmp(a[0]->texto, ejes[i]) == 0) { e = i; break; }
    if (e < 0) return jg_error("Error", "eje() espera \"izq_x\", \"izq_y\", \"der_x\", \"der_y\", \"lt\" o \"rt\"");
    int m = jg_mando_arg(a, n, 1);
    if (m < 0 || !_jgs.mandos[m]) return valor_numero(0);
    double v = _jg.ControllerAxis(_jgs.mandos[m], e) / 32767.0;
    if (v < -1) v = -1;
    if (fabs(v) < 0.15) v = 0;
    return valor_numero(v);
}

/* colision(x1, y1, w1, h1, x2, y2, w2, h2): ¿se solapan los dos rectángulos? */
static Valor *fn_jg_colision(Valor **a, int n) {
    double v[8];
    for (int i = 0; i < 8; i++) if (!jg_num(a, n, i, &v[i], "colision")) return valor_nulo();
    return valor_booleano(v[0] < v[4] + v[6] && v[0] + v[2] > v[4] &&
                          v[1] < v[5] + v[7] && v[1] + v[3] > v[5]);
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
    };
    registrar_modulo_diccionario(entorno, "juego",
        fns, (int)(sizeof(fns) / sizeof(fns[0])), NULL, NULL, 0);
}
