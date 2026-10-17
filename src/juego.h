/* ─────────────────────────────────────────
   MÓDULO: juego  (se incluye desde modulos.c)
   Ventana 2D, dibujo, imágenes, texto, teclado, ratón, mando y sonido. Usa SDL2,
   cargada en tiempo de ejecución (dlopen / LoadLibrary): Lince no la necesita
   para compilarse, y si falta solo falla juego.abrir(). Las imágenes (stb_image),
   el OGG (stb_vorbis) y las fuentes .ttf (stb_truetype) los lee el propio Lince.
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
/* Cómo se generó una animación con animar(): para rehacerla si cambia la imagen */
typedef struct { char tipo[16]; int cuadros, piernas, centro, frente; double intensidad; } JgReceta;
/* px: copia de los píxeles (RGBA) de las imágenes que no salen de un archivo
   (sprites y animaciones generadas); las de archivo se vuelven a leer si hace
   falta. origen: la imagen de la que se generó la animación, o -1. */
/* Cómo se prepara una imagen descargada: quitar el fondo (modo 0 nada, 1 auto,
   2 color) y reducirla (ver jg_preparar) */
#define JG_COLORES 16
typedef struct { int modo; unsigned char color[3]; double escala;
                 int ncolores; unsigned char colores[JG_COLORES][6]; } JgPreparar;   /* "colores": [de, a] */
/* version: cambia cada vez que cambia la textura (para las cachés que salen de ella) */
typedef struct { void *tex; unsigned version; int ancho, alto; char *ruta; long long mtime, tam;
                 unsigned char *px; int origen; JgReceta receta; JgPreparar prep; } JgImagen;
typedef struct { short *datos; int muestras; char *ruta; long long mtime, tam; } JgSonido;   /* muestras por canal */
typedef struct {
    const short *datos; int muestras, bucle, activa, id;
    double pos;           /* en muestras por canal; avanza 'paso' cada vez (el "tono") */
    float vol, izq, der, paso;
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
#define JG_HISTORIAL    32
#define JG_GUION        256
#define JG_ENTRADAS     16
#define JG_PARTICULAS   8192
#define JG_MAPAS        256

/* Una entrada de una acción: tecla, botón del mando, botón del ratón o eje del mando
   hacia un lado. mando = -1: cualquiera. */
typedef struct { char tipo; int codigo, mando, signo; } JgEntrada;
/* desde: cuándo se empezó a pulsar (-1: no está pulsada); soltada: cuándo se
   soltó la última vez, tras 'duro' segundos pulsada (para mantenida()) */
typedef struct { char *nombre; JgEntrada e[JG_ENTRADAS]; int n; double desde, soltada, duro; } JgAccion;

/* Mapa de casillas: un carácter ASCII por casilla, fila a fila */
typedef struct {
    char *celdas;
    int cols, filas;
    double tam;
    unsigned char solido[128], plataforma[128];
    int bordes;                 /* 0 ninguno, 1 los lados (por defecto), 2 todos */
} JgMapa;

/* Partícula: se mueve, cae, cambia de tamaño y de color, y se apaga al acabar su vida */
typedef struct {
    float x, y, vx, vy, grav, vida, total, tam, tam_fin;
    unsigned char c[4], c_fin[4];
    unsigned char mezcla;                /* como en SDL: 1 normal, 2 "sumar" (brilla), 4 "multiplicar" (oscurece) */
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
       LINCE_JUEGO_GUION=30:derecha:20,50:z mantiene derecha desde el
       fotograma 30 durante 20 y pulsa z en el 50 (para probar combos);
       LINCE_JUEGO_SEMILLA=n fija la semilla de matematica.aleatorio() y
       LINCE_JUEGO_AUDIO=ruta.wav guarda el sonido (1/60 s por fotograma, a la par
       que LINCE_JUEGO_FOTOS: sirve para hacer un vídeo con sonido). */
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
    FILE *audio_prueba;                  /* LINCE_JUEGO_AUDIO: el sonido de cada fotograma, a un .wav */
    long audio_bytes;
    JgParticula part[JG_PARTICULAS];
    int npart;
    JgMapa mapas[JG_MAPAS];
    int nmapas;
    /* música en streaming: el OGG se decodifica poco a poco en un anillo que
       el mezclador va vaciando (juego.musica("tema.ogg")) */
    struct {
        char *bytes, *ruta;
        size_t largo;
        OggFlujo *ogg;
        void *conv;                      /* SDL_AudioStream: a estéreo y 44100 Hz */
        short *anillo;
        int cap, lee, escribe, lleno;    /* en muestras (L y R cuentan por separado) */
        int activa, canales;
        float vol;
    } flujo;
    /* escenas: la actual, la que viene (se cambia en presentar()) y el fundido */
    char escena[64], siguiente[64];
    int hay_siguiente, escena_nueva, fase;   /* fase 1: oscurece; 2: aclara */
    double t_escena, t_fundido, fundido;
    double t_vigilar;                    /* última comprobación de archivos cambiados */
    /* el reloj del juego: avanza con dt, que se para con congelar() y va más
       lento o más rápido con velocidad() (las escenas van con el reloj de verdad) */
    double reloj, velocidad, congelar_hasta;
    int reloj_propio;                    /* se ha usado congelar() o velocidad(): ya no es el de verdad */
    /* temblor(): la cámara que pidió el juego (cu_x, cu_y) más el temblor de
       este fotograma, salvo tras camara() sin nada (el marcador no tiembla) */
    double cu_x, cu_y, tem_x, tem_y, tem_fuerza, tem_desde, tem_dur;
    int cam_quieta;
    unsigned tem_semilla;
    /* las últimas acciones pulsadas, para combo() */
    struct { int accion; double t; int usada; } hist[JG_HISTORIAL];
    int nhist;
    /* LINCE_JUEGO_GUION: teclas mantenidas desde un fotograma durante otros tantos */
    struct { long desde, cuantos; int tecla; } guion[JG_GUION];
    int nguion;
    int zoom;                   /* la "escala" de abrir() */
} _jgs;

static unsigned _jg_version_tex;
static void jg_poner_textura(JgImagen *im, void *tex) { im->tex = tex; im->version = ++_jg_version_tex; }

static int jg_tecla_codigo(const char *nombre);
static int jg_guardar_bmp(const char *ruta);
static void jg_leer_mandos(void);
static void jg_recoger_voces(void);
static void jg_parar_flujo(void);
static int jg_audio(void);
static void jg_mezclar(void *datos, unsigned char *flujo, int largo);
static void jg_alimentar_flujo(void);
static Valor *fn_jg_sonido(Valor **a, int n);
static int jg_escribir(const char *t, int x, int y, int e, int ancho);
static void jg_glifo(int ch, int px, int py, int e, int ac);
static int jg_vigilar(void);
static double jg_reloj(void);
static double jg_reloj_real(void);
static void jg_historial(void);
static void jg_escenas(void);
static void jg_aviso_error(const char *msg, int pantalla);
static int jg_cargar_wav(const char *ruta, short **datos, int *muestras, char *error, size_t max);
static int jg_leer_textura(const char *ruta, void **tex, char *msg, size_t max);
static void jg_rehacer_animaciones(int origen);
static void jg_hojas_liberar(void);
static void jg_fuentes_liberar(void);
static void jg_blancas_liberar(void);
static int _jg_recargar_codigo;          /* una hoja cambió de forma: volver a ejecutar el programa */
static double jg_reloj(void);
static int jg_opcion(Valor *op, const char *clave, double *out);
static Valor *jg_opcion_valor(Valor *op, const char *clave);
static void jg_dic_poner(Valor *d, const char *clave, Valor *v);

static Valor *jg_error(const char *tipo, const char *msg) {
    hay_error = 1;
    valor_error = valor_error_nuevo(tipo, msg, 0);
    return valor_nulo();
}

/* La cámara con la que se dibuja: la del juego más el temblor */
static void jg_cam_aplicar(void) {
    _jgs.cam_x = _jgs.cu_x + (_jgs.cam_quieta ? 0 : _jgs.tem_x);
    _jgs.cam_y = _jgs.cu_y + (_jgs.cam_quieta ? 0 : _jgs.tem_y);
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

/* Cierra el .wav de LINCE_JUEGO_AUDIO poniendo los tamaños en la cabecera */
static void jg_cerrar_audio_prueba(void) {
    FILE *f = _jgs.audio_prueba;
    if (!f) return;
    unsigned datos = (unsigned)_jgs.audio_bytes;
    unsigned char cab[44];
    memcpy(cab, "RIFF", 4);
    for (int i = 0; i < 4; i++) cab[4 + i] = (unsigned char)((36 + datos) >> (8 * i));
    memcpy(cab + 8, "WAVEfmt ", 8);
    const unsigned campos[] = {16, 1 | JG_CANALES << 16, JG_FRECUENCIA, JG_FRECUENCIA * JG_CANALES * 2, (JG_CANALES * 2) | 16 << 16};
    for (int c = 0; c < 5; c++) for (int i = 0; i < 4; i++) cab[16 + 4 * c + i] = (unsigned char)(campos[c] >> (8 * i));
    memcpy(cab + 36, "data", 4);
    for (int i = 0; i < 4; i++) cab[40 + i] = (unsigned char)(datos >> (8 * i));
    fseek(f, 0, SEEK_SET);
    fwrite(cab, 1, 44, f);
    fclose(f);
    _jgs.audio_prueba = NULL;
}

static void jg_cerrar_todo(void) {
    jg_cerrar_audio_prueba();
    jg_parar_flujo();
    if (_jgs.audio) { _jg.CloseAudio(_jgs.audio); _jgs.audio = 0; }
    for (int i = 0; i < JG_VOCES; i++) free(_jgs.voces[i].propio);
    memset(_jgs.voces, 0, sizeof _jgs.voces);
    for (int i = 0; i < _jgs.nsonidos; i++) { free(_jgs.sonidos[i].datos); free(_jgs.sonidos[i].ruta); }
    _jgs.nsonidos = 0;
    for (int i = 0; i < _jgs.nmapas; i++) { free(_jgs.mapas[i].celdas); _jgs.mapas[i].celdas = NULL; }
    _jgs.nmapas = 0;
    for (int i = 0; i < _jgs.nimagenes; i++) {
        _jg.DestroyTexture(_jgs.imagenes[i].tex);
        free(_jgs.imagenes[i].ruta);
        free(_jgs.imagenes[i].px);
    }
    _jgs.nimagenes = 0;
    jg_blancas_liberar();
    jg_hojas_liberar();
    jg_fuentes_liberar();
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

/* abrir(titulo, ancho, alto[, {"escala": n}]): con escala, la ventana es n
   veces mayor y cada píxel del juego se ve como un cuadrado de n × n (para
   juegos de píxeles grandes, como los de las consolas portátiles) */
static Valor *fn_jg_abrir(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "abrir() espera (título, ancho, alto)");
    double w = 800, h = 600, z = 1;
    if (n > 1 && !jg_num(a, n, 1, &w, "abrir")) return valor_nulo();
    if (n > 2 && !jg_num(a, n, 2, &h, "abrir")) return valor_nulo();
    if (n > 3 && a[3]->tipo != VAL_DICCIONARIO && a[3]->tipo != VAL_NULO)
        return jg_error("ErrorTipo", "abrir(): las opciones van en un diccionario, por ejemplo {\"escala\": 3}");
    if (n > 3 && jg_opcion(a[3], "escala", &z) && (z < 1 || z > 16 || z != floor(z)))
        return jg_error("Error", "abrir(): \"escala\" es un número entero de 1 a 16");
    if (w < 1 || h < 1 || w > 16384 || h > 16384) return jg_error("Error", "abrir(): tamaño de ventana no válido");
    if (_jgs.abierta && _jgs.reabrir) {          /* programa recargado: la ventana sigue */
        _jgs.reabrir = 0;
        _jg.SetTitle(_jgs.ventana, a[0]->texto);
        if ((int)w != _jgs.ancho || (int)h != _jgs.alto || (int)z != _jgs.zoom) {
            if (_jg.SetWindowSize && !_jgs.completa) _jg.SetWindowSize(_jgs.ventana, (int)(w * z), (int)(h * z));
            _jg.LogicalSize(_jgs.render, (int)w, (int)h);
            _jgs.ancho = (int)w; _jgs.alto = (int)h; _jgs.zoom = (int)z;
        }
        /* el tiempo vuelve a correr normal y se olvidan temblor y pulsaciones */
        _jgs.cu_x = _jgs.cu_y = _jgs.tem_x = _jgs.tem_y = 0;
        _jgs.tem_dur = 0;
        _jgs.velocidad = 1; _jgs.congelar_hasta = 0;
        _jgs.nhist = 0;
        jg_cam_aplicar();
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
    _jgs.ventana = _jg.CrearVentana(a[0]->texto, 0x2FFF0000, 0x2FFF0000, (int)(w * z), (int)(h * z), 0);
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
    _jgs.cam_x = _jgs.cam_y = _jgs.cu_x = _jgs.cu_y = _jgs.tem_x = _jgs.tem_y = 0;
    _jgs.tem_dur = 0; _jgs.cam_quieta = 0; _jgs.tem_semilla = 12345;
    _jgs.reloj = 0; _jgs.velocidad = 1; _jgs.congelar_hasta = 0; _jgs.reloj_propio = 0;
    _jgs.nhist = 0;
    _jgs.completa = 0;
    _jgs.njoy = -1;
    _jgs.volumen = 1;
    _jgs.voz_musica = 0;
    _jgs.t_inicio = jg_ahora();
    _jgs.ancho = (int)w; _jgs.alto = (int)h; _jgs.zoom = (int)z; _jgs.cierre = 0;
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
    _jgs.nguion = 0;
    e = getenv("LINCE_JUEGO_GUION");
    if (e) {
        char buf[4096];
        snprintf(buf, sizeof buf, "%s", e);
        for (char *t = strtok(buf, ","); t && _jgs.nguion < JG_GUION; t = strtok(NULL, ",")) {
            char tecla[32];
            long desde, cuantos = 1;
            if (sscanf(t, "%ld:%31[^:]:%ld", &desde, tecla, &cuantos) < 2) continue;
            int c = jg_tecla_codigo(tecla);
            if (c < 0 || cuantos < 1) continue;
            _jgs.guion[_jgs.nguion].desde = desde;
            _jgs.guion[_jgs.nguion].cuantos = cuantos;
            _jgs.guion[_jgs.nguion++].tecla = c;
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
    if (_jgs.limite && getenv("LINCE_JUEGO_AUDIO")) jg_audio();     /* el sonido empieza con el fotograma 0 */
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

/* El siguiente carácter UTF-8 de *p (su número Unicode); '?' si está mal formado */
static int jg_cp(const unsigned char **p) {
    const unsigned char *s = *p;
    if (s[0] < 0x80) { *p = s + 1; return s[0]; }
    int k = (s[0] & 0xE0) == 0xC0 ? 2 : (s[0] & 0xF0) == 0xE0 ? 3 : (s[0] & 0xF8) == 0xF0 ? 4 : 0;
    int cp = k == 2 ? s[0] & 0x1F : k == 3 ? s[0] & 0x0F : s[0] & 0x07;
    for (int i = 1; i < k; i++) {
        if ((s[i] & 0xC0) != 0x80) { k = 0; break; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if (k == 0) {                                 /* mal formado: se salta entero */
        int j = 1;
        while ((s[j] & 0xC0) == 0x80) j++;
        *p = s + j;
        return '?';
    }
    *p = s + k;
    return cp;
}

/* El carácter de la fuente de Lince para 'cp': el ASCII base (vocales
   acentuadas, ü, ñ, ¿, ¡ incluidos), con *acento a 1 si lleva marca */
static int jg_base(int cp, int *acento) {
    *acento = 0;
    if (cp < 0x80) return cp;
    switch (cp) {
        case 0xE1: case 0xC1: *acento = 1; return 'A';
        case 0xE9: case 0xC9: *acento = 1; return 'E';
        case 0xED: case 0xCD: *acento = 1; return 'I';
        case 0xF3: case 0xD3: *acento = 1; return 'O';
        case 0xFA: case 0xDA: case 0xFC: case 0xDC: *acento = 1; return 'U';
        case 0xF1: case 0xD1: *acento = 1; return 'N';
        case 0xA1: return '!';
        default: return '?';
    }
}

static int jg_siguiente(const unsigned char **p, int *acento) { return jg_base(jg_cp(p), acento); }

/* texto() y ancho_texto() con un diccionario de opciones (y fuentes): más abajo */
static Valor *jg_texto_opciones(Valor **a, int n, int medir);

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
    if (n < 3 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "texto() espera (contenido, x, y[, escala[, color]]) o (contenido, x, y, opciones)");
    if (n > 3 && a[3]->tipo == VAL_DICCIONARIO) return jg_texto_opciones(a, n, 0);
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
    if (n > 1 && a[1]->tipo == VAL_DICCIONARIO) return jg_texto_opciones(a, n, 1);
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
    /* el reloj del juego: parado mientras dura congelar(), al ritmo de velocidad() */
    double real = jg_reloj_real();
    _jgs.dt *= real < _jgs.congelar_hasta ? 0 : _jgs.velocidad;
    _jgs.reloj = _jgs.reloj_propio ? _jgs.reloj + _jgs.dt : real;
    /* temblor: cada fotograma, un desplazamiento al azar que se va apagando */
    _jgs.tem_x = _jgs.tem_y = 0;
    if (_jgs.tem_dur > 0 && real < _jgs.tem_desde + _jgs.tem_dur) {
        double f = _jgs.tem_fuerza * (1 - (real - _jgs.tem_desde) / _jgs.tem_dur);
        _jgs.tem_semilla = _jgs.tem_semilla * 1103515245u + 12345u;
        _jgs.tem_x = f * (((_jgs.tem_semilla >> 8) & 0xFFFF) / 32767.5 - 1);
        _jgs.tem_semilla = _jgs.tem_semilla * 1103515245u + 12345u;
        _jgs.tem_y = f * (((_jgs.tem_semilla >> 8) & 0xFFFF) / 32767.5 - 1);
    }
    _jgs.cam_quieta = 0;
    jg_cam_aplicar();
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
    jg_alimentar_flujo();
    if (_jgs.audio_prueba) {                         /* prueba: el sonido de este fotograma */
        short bloque[JG_FRECUENCIA / 60 * JG_CANALES];
        _jg.LockAudio(_jgs.audio);
        jg_mezclar(NULL, (unsigned char *)bloque, (int)sizeof bloque);
        _jg.UnlockAudio(_jgs.audio);
        for (size_t i = 0; i < sizeof bloque / sizeof bloque[0]; i++) {
            unsigned char m[2] = {(unsigned char)(bloque[i] & 0xFF), (unsigned char)((unsigned short)bloque[i] >> 8)};
            fwrite(m, 1, 2, _jgs.audio_prueba);
        }
        _jgs.audio_bytes += (long)sizeof bloque;
        jg_alimentar_flujo();
    }
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
    for (int i = 0; i < _jgs.nguion; i++)
        if (_jgs.total >= _jgs.guion[i].desde && _jgs.total < _jgs.guion[i].desde + _jgs.guion[i].cuantos)
            _jgs.teclas[_jgs.guion[i].tecla] = 1;
    jg_historial();
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
        jg_poner_textura(im, tex);
        _jg.QueryTexture(tex, NULL, NULL, &im->ancho, &im->alto);
        im->mtime = m; im->tam = t;
        jg_rehacer_animaciones(i);              /* las generadas con animar() a partir de ella */
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
            if ((int)vz->pos >= muestras) vz->pos = 0;
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
    if (_jg_recargar_codigo) { _jg_recargar_codigo = 0; r = 1; }   /* una hoja cambió de forma */
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
    /* Sin pantalla de verdad (pruebas automáticas) nadie va a arreglar nada:
       se termina con el error, como siempre */
    const char *driver = getenv("SDL_VIDEODRIVER");
    if (driver && (strcmp(driver, "dummy") == 0 || strcmp(driver, "offscreen") == 0) && !_jgs.limite) return 0;
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
    for (int i = 0; i + 1 < muestras; i += 2) {
        float acc[2] = {0, 0};
        for (int v = 0; v < JG_VOCES; v++) {
            JgVoz *voz = &_jgs.voces[v];
            if (!voz->activa) continue;
            if ((int)voz->pos >= voz->muestras) {
                if (voz->bucle && voz->muestras > 0) voz->pos = 0;
                else { voz->activa = 0; continue; }
            }
            const short *m = voz->datos + (size_t)voz->pos * JG_CANALES;
            acc[0] += m[0] * voz->vol * voz->izq;
            acc[1] += m[1] * voz->vol * voz->der;
            voz->pos += voz->paso;
        }
        for (int c = 0; c < 2; c++) {
            if (_jgs.flujo.activa && _jgs.flujo.lleno > 0) {       /* música en streaming */
                acc[c] += _jgs.flujo.anillo[_jgs.flujo.lee] * _jgs.flujo.vol;
                _jgs.flujo.lee = (_jgs.flujo.lee + 1) % _jgs.flujo.cap;
                _jgs.flujo.lleno--;
            }
            int t = (int)(acc[c] * _jgs.volumen);
            salida[i + c] = (short)(t > 32767 ? 32767 : t < -32768 ? -32768 : t);
        }
    }
}

static void jg_cerrar_audio_prueba(void);

/* Abre el dispositivo la primera vez; 0 si no hay audio (se juega en silencio) */
static int jg_audio(void) {
    if (_jgs.audio) return 1;
    JgAudioSpec want, have;
    memset(&want, 0, sizeof want);
    want.freq = JG_FRECUENCIA; want.formato = JG_S16; want.canales = JG_CANALES; want.muestras = 1024;
    want.callback = jg_mezclar;
    _jgs.audio = _jg.OpenAudio(NULL, 0, &want, &have, 0);      /* sin cambios permitidos: SDL convierte */
    if (!_jgs.audio) return 0;
    const char *ruta = getenv("LINCE_JUEGO_AUDIO");
    if (_jgs.limite && ruta && !_jgs.audio_prueba) {
        /* prueba: el dispositivo no suena; presentar() mezcla 1/60 s por fotograma
           y lo guarda, sincronizado con LINCE_JUEGO_FOTOS */
        _jgs.audio_prueba = fopen(ruta, "wb");
        if (_jgs.audio_prueba) {
            unsigned char cab[44] = {0};
            fwrite(cab, 1, 44, _jgs.audio_prueba);
            atexit(jg_cerrar_audio_prueba);          /* el programa puede acabar sin juego.cerrar() */
        }
        _jgs.audio_bytes = 0;
        return 1;
    }
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
    voz->izq = voz->der = voz->paso = 1;
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

/* ── Música en streaming ── */
static void jg_parar_flujo(void) {
    if (_jgs.audio) _jg.LockAudio(_jgs.audio);
    _jgs.flujo.activa = 0;
    if (_jgs.audio) _jg.UnlockAudio(_jgs.audio);
    ogg_cerrar(_jgs.flujo.ogg);
    if (_jgs.flujo.conv) _jg.FreeStream(_jgs.flujo.conv);
    free(_jgs.flujo.bytes); free(_jgs.flujo.anillo); free(_jgs.flujo.ruta);
    memset(&_jgs.flujo, 0, sizeof _jgs.flujo);
}

/* Decodifica lo que falte para tener el anillo lleno (unos 2 segundos por
   delante). Se llama en cada presentar(): un fotograma que tarde más que eso
   dejaría un hueco de silencio, nada peor. */
static void jg_alimentar_flujo(void) {
    if (!_jgs.flujo.activa) return;
    short ent[4096 * 2], sal[8192 * 2];
    int vueltas = 0;
    for (;;) {
        int libre = _jgs.flujo.cap - _jgs.flujo.lleno;        /* lo lee el hilo de audio: solo crece */
        if (libre < (int)(sizeof sal / sizeof sal[0])) break;
        int disp = _jg.StreamAvailable(_jgs.flujo.conv);
        if (disp <= 0) {
            int n = ogg_leer(_jgs.flujo.ogg, ent, 4096 / _jgs.flujo.canales);
            if (n <= 0) {                                       /* al acabar, vuelve a empezar */
                if (++vueltas > 2) break;                       /* archivo sin sonido: no insistir */
                ogg_rebobinar(_jgs.flujo.ogg);
                continue;
            }
            _jg.StreamPut(_jgs.flujo.conv, ent, n * (int)sizeof(short) * _jgs.flujo.canales);
            continue;
        }
        int pedir = disp < (int)sizeof sal ? disp : (int)sizeof sal;
        pedir -= pedir % 4;
        int r = _jg.StreamGet(_jgs.flujo.conv, sal, pedir);
        if (r <= 0) break;
        int muestras = r / (int)sizeof(short);
        _jg.LockAudio(_jgs.audio);
        for (int i = 0; i < muestras; i++) {
            _jgs.flujo.anillo[_jgs.flujo.escribe] = sal[i];
            _jgs.flujo.escribe = (_jgs.flujo.escribe + 1) % _jgs.flujo.cap;
        }
        _jgs.flujo.lleno += muestras;
        _jg.UnlockAudio(_jgs.audio);
    }
}

/* Empieza la música en streaming desde 'ruta' (un .ogg); 0 con el motivo en msg */
static int jg_empezar_flujo(const char *ruta, float vol, char *msg, size_t max) {
    if (_jgs.flujo.activa && strcmp(_jgs.flujo.ruta, ruta) == 0) {    /* ya suena: solo el volumen */
        _jgs.flujo.vol = vol;
        return 1;
    }
    jg_parar_flujo();
    if (!jg_audio()) return 1;                                  /* sin audio: se juega en silencio */
    size_t largo = 0;
    char *bytes = interprete_leer_bytes(ruta, &largo);
    if (!bytes) { snprintf(msg, max, "no se encuentra la música '%s'", ruta); return 0; }
    int canales = 0, freq = 0;
    OggFlujo *ogg = ogg_abrir((const unsigned char *)bytes, largo, &canales, &freq);
    if (!ogg) { free(bytes); snprintf(msg, max, "'%s' no es un OGG Vorbis válido", ruta); return 0; }
    void *conv = _jg.NewAudioStream(JG_S16, (unsigned char)canales, freq, JG_S16, JG_CANALES, JG_FRECUENCIA);
    int cap = JG_FRECUENCIA * JG_CANALES * 2;                   /* 2 segundos */
    short *anillo = malloc((size_t)cap * sizeof(short));
    if (!conv || !anillo) {
        ogg_cerrar(ogg); free(bytes); free(anillo);
        if (conv) _jg.FreeStream(conv);
        snprintf(msg, max, "no se pudo preparar la música '%s'", ruta);
        return 0;
    }
    _jgs.flujo.bytes = bytes; _jgs.flujo.largo = largo; _jgs.flujo.ogg = ogg; _jgs.flujo.conv = conv;
    _jgs.flujo.canales = canales;
    _jgs.flujo.anillo = anillo; _jgs.flujo.cap = cap;
    _jgs.flujo.lee = _jgs.flujo.escribe = _jgs.flujo.lleno = 0;
    _jgs.flujo.ruta = strdup(ruta);
    _jgs.flujo.vol = vol;
    _jg.LockAudio(_jgs.audio);
    _jgs.flujo.activa = 1;
    _jg.UnlockAudio(_jgs.audio);
    jg_alimentar_flujo();
    return 1;
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

/* reproducir(sonido[, volumen[, bucle]]) o reproducir(sonido, opciones):
   devuelve el número de la voz. Opciones: "volumen" (0 a 1), "bucle",
   "lado" (-1 izquierda, 0 en medio, 1 derecha: suena de donde pasa) y
   "tono" (1 normal, 2 una octava más agudo y el doble de rápido, 0.5 más
   grave; variarlo un poco hace que un golpe repetido no suene a calco). */
static Valor *fn_jg_reproducir(Valor **a, int n) {
    JG_ABIERTA();
    JgSonido *s = jg_sonido_arg(a, n, "reproducir");
    if (!s) return valor_nulo();
    if (n > 1 && a[1]->tipo == VAL_DICCIONARIO) {
        Valor *op = a[1];
        for (int j = 0; j < op->diccionario.cantidad; j++) {
            const char *c = op->diccionario.claves[j];
            if (strcmp(c, "volumen") && strcmp(c, "bucle") && strcmp(c, "lado") && strcmp(c, "tono")) {
                char msg[200];
                snprintf(msg, sizeof msg, "reproducir(): no conozco la opción '%s' (hay \"volumen\", \"bucle\", \"lado\" y \"tono\")", c);
                return jg_error("Error", msg);
            }
        }
        double vol = 1, lado = 0, tono = 1, b = 0;
        jg_opcion(op, "volumen", &vol); jg_opcion(op, "lado", &lado); jg_opcion(op, "tono", &tono);
        Valor *vb = jg_opcion_valor(op, "bucle");
        if (vb) { if (vb->tipo != VAL_BOOLEANO) return jg_error("ErrorTipo", "reproducir(): \"bucle\" es verdadero o falso"); b = vb->booleano; }
        if (vol < 0 || vol > 1) return jg_error("Error", "reproducir(): \"volumen\" va de 0 a 1");
        if (lado < -1 || lado > 1) return jg_error("Error", "reproducir(): \"lado\" va de -1 (izquierda) a 1 (derecha)");
        if (tono < 0.25 || tono > 4) return jg_error("Error", "reproducir(): \"tono\" va de 0.25 a 4 (1 es el normal)");
        int id = jg_sonar(s->datos, s->muestras, (float)vol, (int)b, NULL);
        if (id > 0) {
            _jg.LockAudio(_jgs.audio);
            for (int v = 0; v < JG_VOCES; v++)
                if (_jgs.voces[v].id == id) {
                    _jgs.voces[v].izq = (float)(lado > 0 ? 1 - lado : 1);
                    _jgs.voces[v].der = (float)(lado < 0 ? 1 + lado : 1);
                    _jgs.voces[v].paso = (float)tono;
                }
            _jg.UnlockAudio(_jgs.audio);
        }
        return valor_numero(id);
    }
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
    jg_parar_flujo();
    _jgs.voz_musica = 0;
    return valor_nulo();
}

/* ¿Sigue sonando la voz con ese id y esos datos? */
static int jg_voz_suena(int id, const short *datos) {
    if (!_jgs.audio || id <= 0) return 0;
    for (int v = 0; v < JG_VOCES; v++)
        if (_jgs.voces[v].id == id) return _jgs.voces[v].activa && _jgs.voces[v].datos == datos;
    return 0;
}

/* musica(ruta o sonido[, volumen]): suena en bucle y sustituye a la anterior;
   musica(nulo) la para. Con la ruta de un .ogg se lee poco a poco (streaming):
   una canción larga no ocupa decenas de MB. Si ya está sonando la misma, solo
   cambia el volumen (así no vuelve a empezar al recargar en caliente). */
static Valor *fn_jg_musica(Valor **a, int n) {
    JG_ABIERTA();
    float vol = jg_volumen_arg(a, n, 1, 0.6f);
    if (n >= 1 && a[0]->tipo == VAL_TEXTO) {
        size_t l = strlen(a[0]->texto);
        int es_ogg = l > 4 && (strcmp(a[0]->texto + l - 4, ".ogg") == 0 || strcmp(a[0]->texto + l - 4, ".OGG") == 0);
        if (es_ogg) {
            jg_parar_voz(_jgs.voz_musica);
            _jgs.voz_musica = 0;
            char msg[600];
            if (!jg_empezar_flujo(a[0]->texto, vol, msg, sizeof msg)) return jg_error("ErrorJuego", msg);
            return valor_nulo();
        }
        /* un .wav: se carga entero, como juego.sonido() */
        Valor *s = fn_jg_sonido(a, 1);
        if (hay_error) return s;
        Valor *args[2] = {s, n > 1 ? a[1] : NULL};
        Valor *r = fn_jg_musica(args, n > 1 ? 2 : 1);
        valor_destruir(s);
        return r;
    }
    jg_parar_flujo();
    if (n < 1 || a[0]->tipo == VAL_NULO) {
        jg_parar_voz(_jgs.voz_musica);
        _jgs.voz_musica = 0;
        return valor_nulo();
    }
    JgSonido *s = jg_sonido_arg(a, n, "musica");
    if (!s) return valor_nulo();
    if (jg_voz_suena(_jgs.voz_musica, s->datos)) {               /* la misma: solo el volumen */
        for (int v = 0; v < JG_VOCES; v++) if (_jgs.voces[v].id == _jgs.voz_musica) _jgs.voces[v].vol = vol;
        return valor_nulo();
    }
    jg_parar_voz(_jgs.voz_musica);
    _jgs.voz_musica = jg_sonar(s->datos, s->muestras, vol, 1, NULL);
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

/* Carga la imagen de la ruta a[0] tal cual (PNG, JPG, BMP, GIF o TGA);
   devuelve {"imagen", "ancho", "alto", "ruta"}. Ver fn_jg_imagen. */
static Valor *jg_imagen_archivo(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "imagen() espera la ruta de una imagen");
    for (int i = 0; i < _jgs.nimagenes; i++)
        if (strcmp(_jgs.imagenes[i].ruta, a[0]->texto) == 0) return jg_imagen_valor(i);
    if (_jgs.nimagenes == JG_MAX_IMAGENES) return jg_error("ErrorJuego", "demasiadas imágenes cargadas");
    char msg[600];
    void *tex;
    if (!jg_leer_textura(a[0]->texto, &tex, msg, sizeof msg)) return jg_error("ErrorJuego", msg);
    int k = _jgs.nimagenes++;
    memset(&_jgs.imagenes[k], 0, sizeof _jgs.imagenes[k]);
    _jgs.imagenes[k].origen = -1;
    jg_poner_textura(&_jgs.imagenes[k], tex);
    _jgs.imagenes[k].ruta = strdup(a[0]->texto);
    recarga_huella(a[0]->texto, &_jgs.imagenes[k].mtime, &_jgs.imagenes[k].tam);
    _jg.QueryTexture(tex, NULL, NULL, &_jgs.imagenes[k].ancho, &_jgs.imagenes[k].alto);
    return jg_imagen_valor(k);
}

/* Textura a partir de píxeles RGBA (que pasan a ser de la imagen) */
static void *jg_textura_rgba(const unsigned char *rgba, int ancho, int alto) {
    void *sup = _jg.SurfaceFrom((void *)rgba, ancho, alto, 32, ancho * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    void *tex = sup ? _jg.TextureFromSurface(_jgs.render, sup) : NULL;
    if (sup) _jg.FreeSurface(sup);
    if (tex) _jg.TextureBlend(tex, 1);
    return tex;
}

/* Nueva imagen hecha en memoria (sprite, animación): se queda con 'rgba'.
   Devuelve su número, o -1 (y libera 'rgba'). */
static int jg_nueva_imagen(const char *nombre, unsigned char *rgba, int ancho, int alto) {
    void *tex = _jgs.nimagenes < JG_MAX_IMAGENES ? jg_textura_rgba(rgba, ancho, alto) : NULL;
    if (!tex) { free(rgba); return -1; }
    int k = _jgs.nimagenes++;
    memset(&_jgs.imagenes[k], 0, sizeof _jgs.imagenes[k]);
    jg_poner_textura(&_jgs.imagenes[k], tex);
    _jgs.imagenes[k].ruta = strdup(nombre);
    _jgs.imagenes[k].ancho = ancho;
    _jgs.imagenes[k].alto = alto;
    _jgs.imagenes[k].px = rgba;
    _jgs.imagenes[k].origen = -1;
    return k;
}

/* Los píxeles RGBA de una imagen (copia nueva, liberar con free) */
static unsigned char *jg_pixeles(int k) {
    JgImagen *im = &_jgs.imagenes[k];
    size_t n = (size_t)im->ancho * im->alto * 4;
    if (im->px) {
        unsigned char *c = malloc(n);
        if (c) memcpy(c, im->px, n);
        return c;
    }
    size_t largo = 0;
    char *bytes = interprete_leer_bytes(im->ruta, &largo);
    if (!bytes) return NULL;
    int w = 0, h = 0;
    unsigned char *px = imagen_decodificar((const unsigned char *)bytes, largo, &w, &h, NULL);
    free(bytes);
    if (px && (w != im->ancho || h != im->alto)) { free(px); px = NULL; }
    return px;
}

/* ── Imágenes descargadas: quitar el fondo y reducirlas ──
   Muchas hojas de sprites (y dibujos sueltos) vienen en JPG con fondo blanco,
   y a menudo muy grandes. "fondo" lo vuelve transparente: rellena desde los
   bordes de la imagen todo lo que se parece al color del fondo, así lo blanco
   de dentro del dibujo (ojos, calcetines) no se toca si tiene contorno; en el
   filo, los píxeles a medio camino se vuelven semitransparentes y se les quita
   el tinte del fondo. "escala" la reduce al cargarla haciendo la media de los
   píxeles, que queda mucho mejor que reducirla al dibujar. */
#define JG_TOLERANCIA_FONDO 48

static int jg_dif_color(const unsigned char *p, const unsigned char *c) {
    int d = 0;
    for (int i = 0; i < 3; i++) { int e = abs(p[i] - c[i]); if (e > d) d = e; }
    return d;
}

/* Quita el fondo; 1 si había fondo que quitar */
static int jg_quitar_fondo(unsigned char *px, int w, int h, int modo, const unsigned char *color) {
    unsigned char bg[3];
    if (modo == 0 || w < 2 || h < 2) return 0;
    if (modo == 1) {
        /* auto: solo si no tiene nada transparente y las cuatro esquinas son del mismo color */
        for (size_t i = 0; i < (size_t)w * h; i++) if (px[i * 4 + 3] < 255) return 0;
        const unsigned char *esq[4] = {px, px + (size_t)(w - 1) * 4, px + (size_t)(h - 1) * w * 4, px + ((size_t)h * w - 1) * 4};
        for (int i = 1; i < 4; i++) if (jg_dif_color(esq[i], esq[0]) > JG_TOLERANCIA_FONDO / 2) return 0;
        memcpy(bg, esq[0], 3);
    } else memcpy(bg, color, 3);
    size_t total = (size_t)w * h, n = 0;
    int *pila = malloc(sizeof(int) * total);
    unsigned char *fuera = calloc(total, 1);
    if (!pila || !fuera) { free(pila); free(fuera); return 0; }
    for (int x = 0; x < w; x++) { pila[n++] = x; pila[n++] = (h - 1) * w + x; }
    for (int y = 1; y < h - 1; y++) { pila[n++] = y * w; pila[n++] = y * w + w - 1; }
    while (n > 0) {
        int i = pila[--n];
        if (fuera[i] || px[(size_t)i * 4 + 3] == 0 || jg_dif_color(px + (size_t)i * 4, bg) > JG_TOLERANCIA_FONDO) {
            if (px[(size_t)i * 4 + 3] == 0 && !fuera[i]) fuera[i] = 1;     /* ya transparente: también es fondo */
            else continue;
        }
        fuera[i] = 1;
        int x = i % w, y = i / w;
        if (x > 0 && !fuera[i - 1]) pila[n++] = i - 1;
        if (x < w - 1 && !fuera[i + 1]) pila[n++] = i + 1;
        if (y > 0 && !fuera[i - w]) pila[n++] = i - w;
        if (y < h - 1 && !fuera[i + w]) pila[n++] = i + w;
        if (n + 4 > total) n = total - 4;                   /* no debería pasar: cada píxel entra pocas veces */
    }
    free(pila);
    for (size_t i = 0; i < total; i++) if (fuera[i]) px[i * 4 + 3] = 0;
    /* el filo: lo que se parece al fondo y lo toca, semitransparente y sin el tinte del fondo */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t i = (size_t)y * w + x;
            if (fuera[i]) continue;
            int toca = (x > 0 && fuera[i - 1]) || (x < w - 1 && fuera[i + 1]) || (y > 0 && fuera[i - w]) || (y < h - 1 && fuera[i + w]);
            if (!toca) continue;
            unsigned char *p = px + i * 4;
            int d = jg_dif_color(p, bg);
            if (d >= 3 * JG_TOLERANCIA_FONDO) continue;
            double al = (double)d / (3 * JG_TOLERANCIA_FONDO);
            if (al < 0.05) al = 0.05;
            for (int c = 0; c < 3; c++) {
                double v = (p[c] - bg[c] * (1 - al)) / al;
                p[c] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
            p[3] = (unsigned char)(p[3] * al);
        }
    free(fuera);
    return 1;
}

/* Reduce la imagen al factor 'f' (0 < f < 1) con la media de los píxeles
   (pesada por la transparencia, para que el filo no se oscurezca) */
static unsigned char *jg_reducir(const unsigned char *px, int w, int h, double f, int *nw, int *nh) {
    *nw = (int)round(w * f); *nh = (int)round(h * f);
    if (*nw < 1) *nw = 1;
    if (*nh < 1) *nh = 1;
    unsigned char *o = calloc((size_t)*nw * *nh, 4);
    if (!o) return NULL;
    double sx = (double)w / *nw, sy = (double)h / *nh;
    for (int y = 0; y < *nh; y++) {
        int y0 = (int)(y * sy), y1 = (int)((y + 1) * sy);
        if (y1 <= y0) y1 = y0 + 1;
        if (y1 > h) y1 = h;
        for (int x = 0; x < *nw; x++) {
            int x0 = (int)(x * sx), x1 = (int)((x + 1) * sx);
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > w) x1 = w;
            double r = 0, g = 0, b = 0, a = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    const unsigned char *p = px + ((size_t)yy * w + xx) * 4;
                    r += p[0] * p[3]; g += p[1] * p[3]; b += p[2] * p[3]; a += p[3];
                }
            unsigned char *q = o + ((size_t)y * *nw + x) * 4;
            int cuantos = (x1 - x0) * (y1 - y0);
            if (a > 0) { q[0] = (unsigned char)(r / a); q[1] = (unsigned char)(g / a); q[2] = (unsigned char)(b / a); }
            q[3] = (unsigned char)(a / cuantos + 0.5);
        }
    }
    return o;
}

/* "colores": cada par [de, a] cambia el color 'de', y sus sombras y brillos
   (los de un tono parecido), por 'a', conservando cuánto más claros u oscuros
   eran: así se cambia la ropa de un personaje (el segundo jugador con otro
   traje) con un solo color. */
static void jg_rgb_hsv(const unsigned char *c, double *h, double *s, double *v) {
    double r = c[0] / 255.0, g = c[1] / 255.0, b = c[2] / 255.0;
    double mx = fmax(r, fmax(g, b)), mn = fmin(r, fmin(g, b)), d = mx - mn;
    *v = mx; *s = mx > 0 ? d / mx : 0;
    if (d <= 0) *h = 0;
    else if (mx == r) *h = fmod((g - b) / d + 6, 6) * 60;
    else if (mx == g) *h = ((b - r) / d + 2) * 60;
    else *h = ((r - g) / d + 4) * 60;
}
static void jg_hsv_rgb(double h, double s, double v, unsigned char *c) {
    h = fmod(h + 360, 360);
    double f = h / 60, x = v * s * (1 - fabs(fmod(f, 2) - 1)), m = v - v * s, r, g, b;
    double k = v * s;
    if (f < 1) { r = k; g = x; b = 0; } else if (f < 2) { r = x; g = k; b = 0; }
    else if (f < 3) { r = 0; g = k; b = x; } else if (f < 4) { r = 0; g = x; b = k; }
    else if (f < 5) { r = x; g = 0; b = k; } else { r = k; g = 0; b = x; }
    c[0] = (unsigned char)lround((r + m) * 255); c[1] = (unsigned char)lround((g + m) * 255); c[2] = (unsigned char)lround((b + m) * 255);
}
static void jg_cambiar_colores(unsigned char *px, int w, int h, const JgPreparar *p) {
    if (p->ncolores == 0) return;
    double dh[JG_COLORES], ds[JG_COLORES], dv[JG_COLORES], ah[JG_COLORES], as[JG_COLORES], av[JG_COLORES];
    for (int j = 0; j < p->ncolores; j++) {
        jg_rgb_hsv(p->colores[j], &dh[j], &ds[j], &dv[j]);
        jg_rgb_hsv(p->colores[j] + 3, &ah[j], &as[j], &av[j]);
    }
    for (size_t i = 0; i < (size_t)w * h; i++) {
        unsigned char *q = px + i * 4;
        if (q[3] == 0) continue;
        int mejor = -1;
        double hh, ss, vv, cerca = 1e9;
        jg_rgb_hsv(q, &hh, &ss, &vv);
        for (int j = 0; j < p->ncolores; j++) {
            if (!memcmp(q, p->colores[j], 3)) { mejor = j; break; }     /* el mismo color */
            double dt = fabs(hh - dh[j]);
            if (dt > 180) dt = 360 - dt;
            /* de un tono parecido, con color (los grises y la piel no) */
            if (ds[j] > 0.15 && ss > 0.15 && vv > 0.08 && dt < 20 && dt < cerca) { cerca = dt; mejor = j; }
        }
        if (mejor < 0) continue;
        int j = mejor;
        double s2 = ds[j] > 0 ? ss * as[j] / ds[j] : as[j], v2 = dv[j] > 0 ? vv * av[j] / dv[j] : av[j];
        jg_hsv_rgb(hh + ah[j] - dh[j], s2 > 1 ? 1 : s2, v2 > 1 ? 1 : v2, q);
    }
}

/* Lee "fondo" y "escala" de unas opciones ('p->modo' trae el fondo por
   defecto). 0 si están mal: el motivo en 'msg', o ya puesto (hay_error). */
static int jg_leer_preparar(Valor *op, JgPreparar *p, const char *fn, char *msg, size_t max) {
    p->escala = 1;
    Valor *f = jg_opcion_valor(op, "fondo");
    if (f) {
        if (f->tipo == VAL_BOOLEANO && !f->booleano) p->modo = 0;
        else if (f->tipo == VAL_TEXTO && !strcmp(f->texto, "auto")) p->modo = 1;
        else {
            unsigned char c[4];
            Valor *arg[1] = {f};
            if (f->tipo != VAL_LISTA) {
                snprintf(msg, max, "%s(): \"fondo\" es \"auto\", un color como [255, 255, 255] o falso", fn);
                return 0;
            }
            if (!jg_color(arg, 1, 0, c)) return 0;          /* el error ya lo puso jg_color */
            p->modo = 2;
            memcpy(p->color, c, 3);
        }
    }
    Valor *e = jg_opcion_valor(op, "escala");
    if (e) {
        if (e->tipo != VAL_NUMERO || e->numero <= 0 || e->numero > 1) {
            snprintf(msg, max, "%s(): \"escala\" reduce la imagen al cargarla: un número mayor que 0 y hasta 1 (0.5 es la mitad)", fn);
            return 0;
        }
        p->escala = e->numero;
    }
    Valor *cs = jg_opcion_valor(op, "colores");
    if (cs) {
        int ok = cs->tipo == VAL_LISTA && cs->lista.cantidad <= JG_COLORES;
        for (int j = 0; ok && j < cs->lista.cantidad; j++) {
            Valor *par = cs->lista.elementos[j];
            ok = par->tipo == VAL_LISTA && par->lista.cantidad == 2;
            for (int m = 0; ok && m < 2; m++) {
                unsigned char c[4];
                Valor *arg[1] = {par->lista.elementos[m]};
                ok = arg[0]->tipo == VAL_LISTA && jg_color(arg, 1, 0, c);
                if (hay_error) return 0;
                if (ok) memcpy(p->colores[j] + m * 3, c, 3);
            }
        }
        if (!ok) {
            snprintf(msg, max, "%s(): \"colores\" es una lista de hasta %d pares [de, a], por ejemplo [[[213, 89, 106], [60, 110, 220]]]", fn, JG_COLORES);
            return 0;
        }
        p->ncolores = cs->lista.cantidad;
    }
    return 1;
}

/* La imagen k preparada (sin fondo y reducida), como imagen nueva; la misma
   preparación de la misma imagen se reutiliza. Devuelve su número (k si no
   hay nada que hacer) o -1 con el motivo en 'msg'. */
static int jg_preparar(int k, const JgPreparar *p, char *msg, size_t max) {
    if (p->modo == 0 && p->escala >= 1 && p->ncolores == 0) return k;
    char nombre[160];
    unsigned huella = 2166136261u;                      /* de los colores, para distinguir las preparaciones */
    for (int j = 0; j < p->ncolores * 6; j++) huella = (huella ^ p->colores[j / 6][j % 6]) * 16777619u;
    snprintf(nombre, sizeof nombre, "<preparada %d %d %d,%d,%d %g %d:%08x>", k, p->modo, p->color[0], p->color[1], p->color[2],
             p->escala, p->ncolores, huella);
    for (int i = 0; i < _jgs.nimagenes; i++)
        if (strcmp(_jgs.imagenes[i].ruta, nombre) == 0) return i;
    unsigned char *px = jg_pixeles(k);
    if (!px) { snprintf(msg, max, "no se pudieron leer los píxeles de la imagen"); return -1; }
    int w = _jgs.imagenes[k].ancho, h = _jgs.imagenes[k].alto;
    jg_quitar_fondo(px, w, h, p->modo, p->color);
    jg_cambiar_colores(px, w, h, p);
    if (p->escala < 1) {
        int nw, nh;
        unsigned char *r = jg_reducir(px, w, h, p->escala, &nw, &nh);
        free(px);
        if (!r) { snprintf(msg, max, "sin memoria"); return -1; }
        px = r; w = nw; h = nh;
    }
    int nk = jg_nueva_imagen(nombre, px, w, h);
    if (nk < 0) { snprintf(msg, max, "no se pudo crear la imagen (¿demasiado grande o demasiadas imágenes?)"); return -1; }
    _jgs.imagenes[nk].origen = k;                       /* para rehacerla si la imagen cambia en el disco */
    snprintf(_jgs.imagenes[nk].receta.tipo, sizeof _jgs.imagenes[nk].receta.tipo, "preparar");
    _jgs.imagenes[nk].prep = *p;
    return nk;
}

/* imagen(ruta[, opciones]): carga un PNG, JPG, BMP, GIF o TGA. Opciones:
   "fondo" ("auto": el color de las esquinas, si no tiene transparencia;
   o un color) lo vuelve transparente, "escala" (hasta 1) la reduce y
   "colores" cambia colores (ver jg_cambiar_colores). */
static Valor *fn_jg_imagen(Valor **a, int n) {
    Valor *img = jg_imagen_archivo(a, n < 1 ? 0 : 1);
    if (hay_error || n < 2 || a[1]->tipo == VAL_NULO) return img;
    char msg[300];
    if (a[1]->tipo != VAL_DICCIONARIO)
        return jg_error("ErrorTipo", "imagen(): las opciones van en un diccionario, por ejemplo {\"fondo\": \"auto\"}");
    for (int j = 0; j < a[1]->diccionario.cantidad; j++) {
        const char *c = a[1]->diccionario.claves[j];
        if (strcmp(c, "fondo") && strcmp(c, "escala") && strcmp(c, "colores")) {
            snprintf(msg, sizeof msg, "imagen(): no conozco la opción '%s' (hay \"fondo\", \"escala\" y \"colores\")", c);
            valor_destruir(img);
            return jg_error("Error", msg);
        }
    }
    JgPreparar p;
    memset(&p, 0, sizeof p);
    if (!jg_leer_preparar(a[1], &p, "imagen", msg, sizeof msg)) {
        valor_destruir(img);
        return hay_error ? valor_nulo() : jg_error("Error", msg);
    }
    int k = jg_id_de(img, "imagen");
    valor_destruir(img);
    int nk = jg_preparar(k, &p, msg, sizeof msg);
    if (nk < 0) return jg_error("ErrorJuego", msg);
    return jg_imagen_valor(nk);
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
    unsigned char *rgba = malloc((size_t)ancho * alto * 4);        /* copia para animar() y guardar_imagen() */
    if (!rgba) { free(px); return jg_error("Error", "sin memoria"); }
    for (int i = 0; i < ancho * alto; i++) {
        rgba[i * 4] = (unsigned char)(px[i] >> 16); rgba[i * 4 + 1] = (unsigned char)(px[i] >> 8);
        rgba[i * 4 + 2] = (unsigned char)px[i]; rgba[i * 4 + 3] = (unsigned char)(px[i] >> 24);
    }
    free(px);
    int k = jg_nueva_imagen(nombre, rgba, ancho, alto);
    if (k < 0) return jg_error("ErrorJuego", "no se pudo crear el sprite");
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

/* Rejilla de una hoja de sprites o de tiles: cuadros de cw×ch separados por
   'sep' píxeles, con 'margen' alrededor. Cuenta de izquierda a derecha y de
   arriba abajo desde 0; un número fuera de rango da la vuelta. */
typedef struct { int cw, ch, sep, margen; } JgRejilla;

static int jg_rejilla_cuadros(const JgImagen *img, const JgRejilla *g, int *columnas) {
    int cols = (img->ancho - 2 * g->margen + g->sep) / (g->cw + g->sep);
    int filas = (img->alto - 2 * g->margen + g->sep) / (g->ch + g->sep);
    if (cols < 1) cols = 1;
    if (filas < 1) filas = 1;
    if (columnas) *columnas = cols;
    return cols * filas;
}

static JgRect jg_rejilla_rect(const JgImagen *img, const JgRejilla *g, int cuadro) {
    int cols, total = jg_rejilla_cuadros(img, g, &cols);
    cuadro %= total;
    if (cuadro < 0) cuadro += total;
    JgRect r = {g->margen + (cuadro % cols) * (g->cw + g->sep), g->margen + (cuadro / cols) * (g->ch + g->sep), g->cw, g->ch};
    return r;
}

/* Divide sola una hoja de sprites. Cada figura es un trozo de píxeles
   conectados (sin contar lo casi transparente); los trocitos sueltos (una gota,
   una chispa) se unen a la figura grande más cercana, y si están lejos de
   todas se descartan. Las figuras se agrupan en filas (por su altura en la
   hoja) y de izquierda a derecha, y se copian a una hoja nueva en rejilla:
   cada una centrada en su celda, y dentro de su fila a la misma altura que en
   la original (los pies de una fila siguen alineados y un salto sigue en el
   aire). Devuelve los píxeles de la hoja nueva, o NULL con 'msg'. */
#define JG_MAX_RECORTES 1024
typedef struct { int x0, y0, x1, y1; long area; int dueno, fila; double cy; } JgPieza;

static int jg_pieza_cmp_y(const void *a, const void *b) {
    double d = ((const JgPieza *)a)->cy - ((const JgPieza *)b)->cy;
    return d < 0 ? -1 : d > 0;
}

static unsigned char *jg_dividir_hoja(const unsigned char *px, int w, int h, int *cw, int *ch, int *ncuadros,
                                      int *por_fila, int *nfilas, char *msg, size_t max) {
    size_t total = (size_t)w * h;
    int *etiqueta = calloc(total, sizeof(int)), *pila = malloc(total * sizeof(int));
    int cap = 256, np = 0;
    JgPieza *pz = malloc(sizeof(JgPieza) * cap);
    unsigned char *hoja = NULL;
    int *orden = NULL;
    if (!etiqueta || !pila || !pz) { snprintf(msg, max, "sin memoria"); goto fin; }
    /* 1. trozos conectados (también en diagonal) */
    for (size_t i0 = 0; i0 < total; i0++) {
        if (etiqueta[i0] || px[i0 * 4 + 3] < 32) continue;
        if (np == cap) {
            JgPieza *m = realloc(pz, sizeof(JgPieza) * cap * 2);
            if (!m) { snprintf(msg, max, "sin memoria"); goto fin; }
            pz = m; cap *= 2;
        }
        JgPieza q = {w, h, -1, -1, 0, np, 0, 0};
        size_t n = 0;
        pila[n++] = (int)i0;
        etiqueta[i0] = np + 1;
        while (n > 0) {
            int i = pila[--n], x = i % w, y = i / w;
            q.area++;
            if (x < q.x0) q.x0 = x;
            if (x > q.x1) q.x1 = x;
            if (y < q.y0) q.y0 = y;
            if (y > q.y1) q.y1 = y;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    size_t j = (size_t)yy * w + xx;
                    if (etiqueta[j] || px[j * 4 + 3] < 32) continue;
                    etiqueta[j] = np + 1;
                    pila[n++] = (int)j;
                }
        }
        pz[np++] = q;
    }
    if (np == 0) { snprintf(msg, max, "la imagen es transparente del todo"); goto fin; }
    /* 2. las figuras grandes; los trocitos, con la más cercana */
    long mayor = 0;
    for (int i = 0; i < np; i++) if (pz[i].area > mayor) mayor = pz[i].area;
    int grandes = 0;
    double alto_medio = 0;
    for (int i = 0; i < np; i++)
        if (pz[i].area * 10 >= mayor) { grandes++; alto_medio += pz[i].y1 - pz[i].y0 + 1; }
    alto_medio /= grandes;
    if (grandes > JG_MAX_RECORTES) { snprintf(msg, max, "hay demasiadas figuras (más de %d)", JG_MAX_RECORTES); goto fin; }
    double cerca = fmax(2, alto_medio * 0.1);
    for (int i = 0; i < np; i++) {
        if (pz[i].area * 10 >= mayor) continue;
        int mejor = -1;
        double dmin = cerca;
        for (int j = 0; j < np; j++) {
            if (pz[j].area * 10 < mayor) continue;
            double dx = fmax(0, fmax(pz[j].x0 - pz[i].x1, pz[i].x0 - pz[j].x1));
            double dy = fmax(0, fmax(pz[j].y0 - pz[i].y1, pz[i].y0 - pz[j].y1));
            double d = fmax(dx, dy);
            if (d <= dmin) { dmin = d; mejor = j; }
        }
        pz[i].dueno = mejor;
    }
    for (int i = 0; i < np; i++) {                      /* las figuras crecen con sus trocitos */
        int d = pz[i].dueno;
        if (d == i || d < 0) continue;
        if (pz[i].x0 < pz[d].x0) pz[d].x0 = pz[i].x0;
        if (pz[i].y0 < pz[d].y0) pz[d].y0 = pz[i].y0;
        if (pz[i].x1 > pz[d].x1) pz[d].x1 = pz[i].x1;
        if (pz[i].y1 > pz[d].y1) pz[d].y1 = pz[i].y1;
    }
    /* 3. filas: por la altura del centro; una figura cuyo centro cae dentro de la fila es de ella */
    orden = malloc(sizeof(int) * grandes);
    JgPieza *g = malloc(sizeof(JgPieza) * grandes);
    if (!orden || !g) { free(g); snprintf(msg, max, "sin memoria"); goto fin; }
    int ng = 0;
    for (int i = 0; i < np; i++)
        if (pz[i].dueno == i) { g[ng] = pz[i]; g[ng].cy = (pz[i].y0 + pz[i].y1) / 2.0; ng++; }
    qsort(g, ng, sizeof(JgPieza), jg_pieza_cmp_y);
    int nf = 0, fondo_fila = -1;
    int arriba[JG_MAX_RECORTES], abajo[JG_MAX_RECORTES];
    for (int i = 0; i < ng; i++) {
        if (nf == 0 || g[i].cy > fondo_fila) { arriba[nf] = g[i].y0; abajo[nf] = g[i].y1; nf++; }
        g[i].fila = nf - 1;
        if (g[i].y0 < arriba[nf - 1]) arriba[nf - 1] = g[i].y0;
        if (g[i].y1 > abajo[nf - 1]) abajo[nf - 1] = g[i].y1;
        fondo_fila = abajo[nf - 1];
    }
    /* dentro de cada fila, de izquierda a derecha */
    for (int i = 1; i < ng; i++)
        for (int j = i; j > 0 && (g[j].fila < g[j - 1].fila || (g[j].fila == g[j - 1].fila && g[j].x0 < g[j - 1].x0)); j--) {
            JgPieza t = g[j]; g[j] = g[j - 1]; g[j - 1] = t;
        }
    /* 4. la hoja nueva */
    int W = 0, H = 0, cols = 0;
    for (int f = 0; f < nf; f++) { por_fila[f] = 0; if (abajo[f] - arriba[f] + 1 > H) H = abajo[f] - arriba[f] + 1; }
    for (int i = 0; i < ng; i++) {
        if (g[i].x1 - g[i].x0 + 1 > W) W = g[i].x1 - g[i].x0 + 1;
        if (++por_fila[g[i].fila] > cols) cols = por_fila[g[i].fila];
    }
    if ((long)W * cols > 16384 || (long)H * nf > 16384) { free(g); snprintf(msg, max, "la hoja saldría demasiado grande; redúcela con \"escala\""); goto fin; }
    int ancho = W * cols;
    hoja = calloc((size_t)ancho * H * nf, 4);
    if (!hoja) { free(g); snprintf(msg, max, "sin memoria"); goto fin; }
    int col = 0;
    for (int i = 0; i < ng; i++) {
        if (i > 0 && g[i].fila != g[i - 1].fila) col = 0;
        int f = g[i].fila, fw = g[i].x1 - g[i].x0 + 1, bh = abajo[f] - arriba[f] + 1;
        int dx = col * W + (W - fw) / 2 - g[i].x0, dy = f * H + (H - bh) - arriba[f];
        for (int y = g[i].y0; y <= g[i].y1; y++)
            for (int x = g[i].x0; x <= g[i].x1; x++) {
                size_t j = (size_t)y * w + x;
                int e = etiqueta[j];
                int suyo = e ? pz[e - 1].dueno == g[i].dueno : px[j * 4 + 3] > 0;    /* lo casi transparente, del que esté */
                if (!suyo) continue;
                memcpy(hoja + ((size_t)(y + dy) * ancho + x + dx) * 4, px + j * 4, 4);
            }
        col++;
    }
    free(g);
    *cw = W; *ch = H; *ncuadros = ng; *nfilas = nf;
fin:
    free(etiqueta); free(pila); free(pz); free(orden);
    return hoja;
}

/* ── Hojas ──
   Cada hoja vive aquí: su receta (de qué imagen sale, cómo se prepara y se
   divide, qué cuadros se quitan) y el resultado (dónde está cada cuadro). El
   diccionario que recibe el programa lleva su número ("hoja") y un resumen
   para leer ("cuadros", "celda", "filas"...). Si la imagen cambia en el disco
   con el juego en marcha, la hoja se rehace y todo lo que la usa (dibujar,
   los animadores guardados en variables, los mapas) ve los cuadros nuevos;
   si además cambió de forma (otro número de cuadros, otra celda), el programa
   se vuelve a ejecutar como al guardar el código, para que el resumen también
   se ponga al día. */
#define JG_MAX_HOJAS 512
enum { JG_HOJA_SOLA, JG_HOJA_TAMANO, JG_HOJA_CUENTA };
typedef struct {
    /* receta */
    int base, modo, columnas, filas, *quitar, nquitar;
    JgPreparar prep;
    JgRejilla g;                /* por tamaño: en píxeles de la imagen original */
    /* resultado */
    int fuente;                 /* la imagen preparada (o la base, si no hay que prepararla) */
    int imagen;                 /* la imagen de la que se recortan los cuadros */
    int total, nf, cw, ch, cols, *por_fila;
    JgRect *rec;
    JgRect *caja;               /* lo que se ve de cada cuadro, en coordenadas de su celda (para chocar) */
    int version;
} JgHoja;
static JgHoja _jg_hojas[JG_MAX_HOJAS];
static int _jg_nhojas;

static int jg_resto(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }

/* El rectángulo de lo que se ve (alfa > 64) dentro de 'r', relativo a 'r';
   todo a 0 si no se ve nada */
static JgRect jg_visible(const unsigned char *px, int W, int H, JgRect r) {
    int x0 = r.w, y0 = r.h, x1 = -1, y1 = -1;
    for (int y = 0; y < r.h; y++) {
        int sy = r.y + y;
        if (sy < 0 || sy >= H) continue;
        for (int x = 0; x < r.w; x++) {
            int sx = r.x + x;
            if (sx < 0 || sx >= W || px[((size_t)sy * W + sx) * 4 + 3] <= 64) continue;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    }
    JgRect v = {0, 0, 0, 0};
    if (x1 >= 0) { v.x = x0; v.y = y0; v.w = x1 - x0 + 1; v.h = y1 - y0 + 1; }
    return v;
}

static void jg_hojas_liberar(void) {
    for (int i = 0; i < _jg_nhojas; i++) {
        free(_jg_hojas[i].quitar); free(_jg_hojas[i].por_fila); free(_jg_hojas[i].rec); free(_jg_hojas[i].caja);
    }
    memset(_jg_hojas, 0, sizeof _jg_hojas);
    _jg_nhojas = 0;
}

/* Calcula (o rehace) el resultado de la hoja i a partir de su receta. 0 si
   no se pudo, con el motivo en 'msg' (la hoja se queda como estaba). */
static int jg_hoja_calcular(int hi, char *msg, size_t max) {
    JgHoja *h = &_jg_hojas[hi];
    int fuente = jg_preparar(h->base, &h->prep, msg, max);
    if (fuente < 0) return 0;
    JgImagen *im = &_jgs.imagenes[fuente];
    int total = 0, nf = 0, cw = 0, ch = 0, imagen = fuente;
    JgRect *rec = NULL;
    int *pf = NULL;
    if (h->modo == JG_HOJA_SOLA) {
        static int por_fila[JG_MAX_RECORTES];
        unsigned char *px = jg_pixeles(fuente);
        if (!px) { snprintf(msg, max, "no se pudieron leer los píxeles de la imagen"); return 0; }
        unsigned char *nueva = jg_dividir_hoja(px, im->ancho, im->alto, &cw, &ch, &total, por_fila, &nf, msg, max);
        free(px);
        if (!nueva) return 0;
        if (total == 1 && cw * 10 >= im->ancho * 9 && ch * 10 >= im->alto * 9) {
            free(nueva);
            snprintf(msg, max, "no encuentro huecos transparentes entre los cuadros; di cuántos hay, por ejemplo "
                     "hoja(imagen, {\"columnas\": 6}), o cuánto mide cada uno: hoja(imagen, 32)");
            return 0;
        }
        int cols = 0;
        for (int f = 0; f < nf; f++) if (por_fila[f] > cols) cols = por_fila[f];
        int W = cw * cols, H = ch * nf;
        /* la imagen de la hoja: nueva, o la de antes rehecha en su sitio */
        if (h->imagen >= 0 && h->imagen != h->fuente && h->imagen < _jgs.nimagenes) {
            void *tex = jg_textura_rgba(nueva, W, H);
            if (!tex) { free(nueva); snprintf(msg, max, "no se pudo crear la hoja"); return 0; }
            JgImagen *hm = &_jgs.imagenes[h->imagen];
            _jg.DestroyTexture(hm->tex);
            free(hm->px);
            jg_poner_textura(hm, tex); hm->px = nueva; hm->ancho = W; hm->alto = H;
            imagen = h->imagen;
        } else {
            char nombre[48];
            snprintf(nombre, sizeof nombre, "<hoja %d>", hi);
            imagen = jg_nueva_imagen(nombre, nueva, W, H);
            if (imagen < 0) { snprintf(msg, max, "no se pudo crear la hoja (¿demasiado grande? prueba con \"escala\")"); return 0; }
        }
        rec = malloc(sizeof(JgRect) * total);
        pf = malloc(sizeof(int) * nf);
        if (!rec || !pf) { free(rec); free(pf); snprintf(msg, max, "sin memoria"); return 0; }
        for (int f = 0, c = 0; f < nf; f++) {
            pf[f] = por_fila[f];
            for (int e = 0; e < por_fila[f]; e++, c++) { JgRect r = {e * cw, f * ch, cw, ch}; rec[c] = r; }
        }
    } else {
        JgRejilla g = h->g;
        if (h->modo == JG_HOJA_TAMANO && h->prep.escala < 1) {    /* los tamaños se dan en la imagen original */
            g.cw = (int)round(g.cw * h->prep.escala); g.ch = (int)round(g.ch * h->prep.escala);
            g.sep = (int)round(g.sep * h->prep.escala); g.margen = (int)round(g.margen * h->prep.escala);
        } else if (h->modo == JG_HOJA_CUENTA) {
            if (h->prep.escala < 1) { g.sep = (int)round(g.sep * h->prep.escala); g.margen = (int)round(g.margen * h->prep.escala); }
            g.cw = (im->ancho - 2 * g.margen - (h->columnas - 1) * g.sep) / h->columnas;
            g.ch = (im->alto - 2 * g.margen - (h->filas - 1) * g.sep) / h->filas;
        }
        if (g.cw < 1 || g.ch < 1 || g.sep < 0 || g.margen < 0) {
            snprintf(msg, max, "los cuadros miden al menos 1×1 y la separación y el margen no son negativos");
            return 0;
        }
        if (g.cw + 2 * g.margen > im->ancho || g.ch + 2 * g.margen > im->alto) {
            snprintf(msg, max, "la imagen mide %d×%d, no cabe ni un cuadro de %d×%d", im->ancho, im->alto, g.cw, g.ch);
            return 0;
        }
        int cols;
        total = jg_rejilla_cuadros(im, &g, &cols);
        nf = total / cols;
        rec = malloc(sizeof(JgRect) * total);
        pf = malloc(sizeof(int) * nf);
        if (!rec || !pf) { free(rec); free(pf); snprintf(msg, max, "sin memoria"); return 0; }
        for (int i = 0; i < total; i++) rec[i] = jg_rejilla_rect(im, &g, i);
        for (int f = 0; f < nf; f++) pf[f] = cols;
        cw = g.cw; ch = g.ch;
    }
    /* quitar: los demás se numeran seguidos; las filas vacías desaparecen */
    if (h->nquitar > 0) {
        char *fuera = calloc((size_t)total, 1);
        if (!fuera) { free(rec); free(pf); snprintf(msg, max, "sin memoria"); return 0; }
        for (int q = 0; q < h->nquitar; q++) if (h->quitar[q] >= 0 && h->quitar[q] < total) fuera[h->quitar[q]] = 1;
        int n = 0, f2 = 0;
        for (int f = 0, c = 0; f < nf; f++) {
            int quedan = 0;
            for (int e = 0; e < pf[f]; e++, c++) if (!fuera[c]) { rec[n++] = rec[c]; quedan++; }
            if (quedan) pf[f2++] = quedan;
        }
        free(fuera);
        if (n == 0) { free(rec); free(pf); snprintf(msg, max, "con \"quitar\" no queda ningún cuadro"); return 0; }
        total = n; nf = f2;
    }
    int cols = 0;
    for (int f = 0; f < nf; f++) if (pf[f] > cols) cols = pf[f];
    /* si cambia de forma, el programa se vuelve a ejecutar para poner al día los diccionarios */
    int forma = h->version > 0 && (total != h->total || nf != h->nf || cw != h->cw || ch != h->ch);
    for (int f = 0; f < nf && h->version > 0 && !forma; f++) forma = pf[f] != h->por_fila[f];
    if (forma) _jg_recargar_codigo = 1;
    /* la caja de cada cuadro, dentro de su celda (como lo coloca jg_hoja_cuadro) */
    JgRect *caja = malloc(sizeof(JgRect) * total);
    if (!caja) { free(rec); free(pf); snprintf(msg, max, "sin memoria"); return 0; }
    {
        JgImagen *ci = &_jgs.imagenes[imagen];
        unsigned char *px = jg_pixeles(imagen);
        for (int i = 0; i < total; i++) {
            JgRect v = {0, 0, rec[i].w, rec[i].h};
            if (px) v = jg_visible(px, ci->ancho, ci->alto, rec[i]);
            v.x += (cw - rec[i].w) / 2;
            v.y += ch - rec[i].h;
            caja[i] = v;
        }
        free(px);
    }
    free(h->rec); free(h->por_fila); free(h->caja);
    h->rec = rec; h->por_fila = pf; h->caja = caja;
    h->total = total; h->nf = nf; h->cw = cw; h->ch = ch; h->cols = cols;
    h->fuente = fuente; h->imagen = imagen;
    h->version++;
    return 1;
}

/* La hoja con esa receta (la misma receta da la misma hoja: así al recargar en
   caliente no se repite el trabajo); -1 con 'msg' si no se pudo hacer */
static int jg_hoja_obtener(const JgHoja *receta, char *msg, size_t max) {
    for (int i = 0; i < _jg_nhojas; i++) {
        JgHoja *h = &_jg_hojas[i];
        if (h->base != receta->base || h->modo != receta->modo || h->columnas != receta->columnas ||
            h->filas != receta->filas || h->nquitar != receta->nquitar ||
            memcmp(&h->prep, &receta->prep, sizeof h->prep) || memcmp(&h->g, &receta->g, sizeof h->g)) continue;
        if (h->nquitar && memcmp(h->quitar, receta->quitar, sizeof(int) * h->nquitar)) continue;
        return i;
    }
    if (_jg_nhojas == JG_MAX_HOJAS) { snprintf(msg, max, "demasiadas hojas"); return -1; }
    int i = _jg_nhojas;
    JgHoja *h = &_jg_hojas[i];
    *h = *receta;
    h->quitar = NULL;
    if (receta->nquitar) {
        h->quitar = malloc(sizeof(int) * receta->nquitar);
        if (!h->quitar) { snprintf(msg, max, "sin memoria"); return -1; }
        memcpy(h->quitar, receta->quitar, sizeof(int) * receta->nquitar);
    }
    h->imagen = -1; h->fuente = -1; h->rec = NULL; h->por_fila = NULL; h->caja = NULL; h->version = 0;
    if (!jg_hoja_calcular(i, msg, max)) { free(h->quitar); memset(h, 0, sizeof *h); return -1; }
    _jg_nhojas++;
    return i;
}

/* El diccionario de la hoja i: la imagen, su número y el resumen */
static Valor *jg_hoja_valor(int hi) {
    JgHoja *h = &_jg_hojas[hi];
    Valor *d = jg_imagen_valor(h->imagen);
    valor_diccionario_agregar(d, "hoja", valor_numero(hi));
    Valor *celda = valor_lista_crear();
    lista_agregar(celda, valor_numero(h->cw));
    lista_agregar(celda, valor_numero(h->ch));
    valor_diccionario_agregar(d, "celda", celda);
    valor_diccionario_agregar(d, "cuadros", valor_numero(h->total));
    valor_diccionario_agregar(d, "columnas", valor_numero(h->cols));
    valor_diccionario_agregar(d, "filas", valor_numero(h->nf));
    Valor *l = valor_lista_crear();
    for (int f = 0; f < h->nf; f++) lista_agregar(l, valor_numero(h->por_fila[f]));
    valor_diccionario_agregar(d, "por_fila", l);
    return d;
}

/* La hoja de un diccionario de hoja(), o NULL */
static JgHoja *jg_hoja_de(Valor *v) {
    if (!v || v->tipo != VAL_DICCIONARIO) return NULL;
    int k = valor_diccionario_buscar(v, "hoja");
    if (k < 0 || v->diccionario.valores[k]->tipo != VAL_NUMERO) return NULL;
    int i = (int)v->diccionario.valores[k]->numero;
    return i >= 0 && i < _jg_nhojas ? &_jg_hojas[i] : NULL;
}

/* Dónde está un cuadro de la hoja. 'fila' >= 0 cuenta dentro de esa fila
   (cada fila, una animación); -1, todos seguidos. Un número fuera de rango
   da la vuelta. 'src' es el trozo de la imagen y 'ox', 'oy' dónde va dentro
   de la celda (h->cw × h->ch). Devuelve el número de cuadro en la hoja, o -1
   si la fila no existe (con 'msg'). */
static int jg_hoja_cuadro(const JgHoja *h, int fila, int cuadro, JgRect *src, int *ox, int *oy, char *msg, size_t max) {
    int i;
    if (fila >= 0) {
        if (fila >= h->nf) {
            snprintf(msg, max, "la hoja tiene %d fila%s (de 0 a %d); no hay fila %d", h->nf, h->nf == 1 ? "" : "s", h->nf - 1, fila);
            return -1;
        }
        int base = 0;
        for (int f = 0; f < fila; f++) base += h->por_fila[f];
        i = base + jg_resto(cuadro, h->por_fila[fila]);
    } else i = jg_resto(cuadro, h->total);
    *src = h->rec[i];
    *ox = (h->cw - src->w) / 2;
    *oy = h->ch - src->h;
    return i;
}

/* Pisadas de una secuencia de cuadros de la hoja (fila[i], cuadro[i]): deja en
   pisa[i] si en el cuadro i un pie toca el suelo. En cada cuadro mira qué hay
   pegado al suelo (las filas de abajo de la celda) y lo compara con el cuadro
   anterior (el último cuenta como anterior del primero, porque las animaciones
   de andar dan la vuelta). Supone que el personaje mira a la derecha. */
#define JG_MAX_APOYOS 16
static int jg_pisadas_secuencia(const JgHoja *h, const int *filas, const int *cuadros, int n, char *pisa) {
    memset(pisa, 0, (size_t)n);
    if (n < 2) return 1;
    /* una hoja de animar("andar"/"correr"): se sabe dónde apoya, porque la hizo Lince
       (con las piernas más abiertas: a un cuarto y a tres cuartos de la vuelta) */
    const JgReceta *rc = &_jgs.imagenes[h->base].receta;
    if (h->modo == JG_HOJA_TAMANO && _jgs.imagenes[h->base].origen >= 0 &&
        (!strcmp(rc->tipo, "andar") || !strcmp(rc->tipo, "correr"))) {
        for (int i = 0; i < n; i++) pisa[i] = cuadros[i] == rc->cuadros / 4 || cuadros[i] == 3 * rc->cuadros / 4;
        return 1;
    }
    JgImagen *img = &_jgs.imagenes[h->imagen];
    unsigned char *px = jg_pixeles(h->imagen);
    int (*apoyo)[JG_MAX_APOYOS][2] = malloc(sizeof *apoyo * n);
    int *napoyos = calloc((size_t)n, sizeof(int));
    if (!px || !apoyo || !napoyos) { free(px); free(apoyo); free(napoyos); return 0; }
    char msg[160];
    /* el suelo: lo más bajo de todos los cuadros, en coordenadas de la celda */
    int suelo = -1;
    for (int i = 0; i < n; i++) {
        JgRect src;
        int ox, oy;
        if (jg_hoja_cuadro(h, filas[i], cuadros[i], &src, &ox, &oy, msg, sizeof msg) < 0) continue;
        for (int y = src.y + src.h - 1; y >= src.y; y--) {
            int hay = 0;
            for (int x = src.x; x < src.x + src.w && !hay; x++)
                hay = x >= 0 && y >= 0 && x < img->ancho && y < img->alto && px[((size_t)y * img->ancho + x) * 4 + 3] > 64;
            if (hay) { if (oy + y - src.y > suelo) suelo = oy + y - src.y; break; }
        }
    }
    int margen = (int)floor(h->ch * 0.04);              /* los pies no siempre llegan justo al mismo suelo */
    int junta = (int)fmax(1, round(h->cw * 0.025));     /* huecos más pequeños no separan dos apoyos */
    int salto = (int)fmax(2, round(h->cw * 0.06));
    for (int i = 0; i < n; i++) {
        JgRect src;
        int ox, oy;
        if (jg_hoja_cuadro(h, filas[i], cuadros[i], &src, &ox, &oy, msg, sizeof msg) < 0) continue;
        int desde = -1, hueco = 0;
        for (int x = 0; x <= src.w; x++) {              /* columnas con algo pegado al suelo */
            int toca = 0;
            for (int y = suelo - margen; y <= suelo && x < src.w && !toca; y++) {
                int sy = src.y + y - oy, sx = src.x + x;
                if (sy >= src.y && sy < src.y + src.h && sx < img->ancho && sy < img->alto)
                    toca = px[((size_t)sy * img->ancho + sx) * 4 + 3] > 64;
            }
            if (toca) { if (desde < 0) desde = x; hueco = 0; continue; }
            if (desde >= 0 && (++hueco > junta || x == src.w) && napoyos[i] < JG_MAX_APOYOS) {
                apoyo[i][napoyos[i]][0] = ox + desde;
                apoyo[i][napoyos[i]][1] = ox + x - hueco;
                napoyos[i]++;
                desde = -1;
            }
        }
    }
    free(px);
    /* hay pisada si ahora hay más apoyos que antes (un pie aterriza: tras ir por el aire, o
       al lado del otro) o si el apoyo de delante salta hacia delante (andando con los dos
       pies casi siempre en el suelo, el que avanzaba aterriza por delante del otro) */
    char *candidata = calloc((size_t)n, 1);
    if (!candidata) { free(apoyo); free(napoyos); return 0; }
    for (int i = 0; i < n; i++) {
        int p = (i + n - 1) % n, delante_i = -1, delante_p = -1;
        for (int s = 0; s < napoyos[i]; s++) if (apoyo[i][s][1] > delante_i) delante_i = apoyo[i][s][1];
        for (int s = 0; s < napoyos[p]; s++) if (apoyo[p][s][1] > delante_p) delante_p = apoyo[p][s][1];
        candidata[i] = napoyos[i] > napoyos[p] || (delante_p >= 0 && delante_i - delante_p > salto);
    }
    /* una pisada puede tardar dos cuadros en asentarse (la punta y luego el pie): cuenta una */
    int primera = -1, ultima = -1000;
    for (int i = 0; i < n; i++) {
        if (!candidata[i] || i - ultima < 2) continue;
        if (primera >= 0 && i + n - primera < 2) continue;          /* pegada a la primera, dando la vuelta */
        if (primera < 0) primera = i;
        ultima = i;
        pisa[i] = 1;
    }
    free(candidata); free(apoyo); free(napoyos);
    return 1;
}

/* pisadas(hoja[, fila]): los cuadros en que un pie toca el suelo, para que
   suenen los pasos a su tiempo. Con 'fila', los números son dentro de esa fila. */
static Valor *fn_jg_pisadas(Valor **a, int n) {
    JG_ABIERTA();
    JgHoja *h = n > 0 ? jg_hoja_de(a[0]) : NULL;
    if (!h) return jg_error("ErrorTipo", "pisadas() espera una hoja de juego.hoja() (y, si quieres, una fila)");
    int fila = -1;
    if (n > 1) {
        if (a[1]->tipo != VAL_NUMERO || a[1]->numero < 0) return jg_error("ErrorTipo", "pisadas(): la fila es un número desde 0");
        fila = (int)a[1]->numero;
        if (fila >= h->nf) {
            char msg[120];
            snprintf(msg, sizeof msg, "pisadas(): la hoja no tiene fila %d (tiene %d)", fila, h->nf);
            return jg_error("Error", msg);
        }
    }
    int cuantos = fila >= 0 ? h->por_fila[fila] : h->total;
    int *filas = malloc(sizeof(int) * cuantos), *cuadros = malloc(sizeof(int) * cuantos);
    char *pisa = malloc((size_t)cuantos);
    if (!filas || !cuadros || !pisa) { free(filas); free(cuadros); free(pisa); return jg_error("Error", "sin memoria"); }
    for (int i = 0; i < cuantos; i++) { filas[i] = fila; cuadros[i] = i; }
    int ok = jg_pisadas_secuencia(h, filas, cuadros, cuantos, pisa);
    Valor *l = valor_lista_crear();
    for (int i = 0; ok && i < cuantos; i++) if (pisa[i]) lista_agregar(l, valor_numero(i));
    free(filas); free(cuadros); free(pisa);
    if (!ok) { valor_destruir(l); return jg_error("Error", "sin memoria"); }
    return l;
}

/* hoja(imagen o ruta[, cómo dividirla]): una imagen con varios cuadros (los
   de una animación, o los tiles de un mapa) partida en esos cuadros:
     hoja("andar.png")                               sola: busca las figuras
                                                     (ver jg_dividir_hoja)
     hoja("andar.png", {"columnas": 6[, "filas": 2]}) en cuadros iguales, por número
     hoja("tiles.png", 16[, alto])                   en cuadros iguales, por tamaño
   Las dos últimas admiten "separacion" y "margen". Todas admiten "fondo" y
   "escala" (ver jg_preparar) y "quitar"; la primera quita sola el fondo de
   una imagen sin transparencia (por ejemplo, un JPG con fondo blanco).
   hoja(otra_hoja, {"quitar": [...]}) quita cuadros de una hoja ya hecha. */
static Valor *fn_jg_hoja(Valor **a, int n) {
    JG_ABIERTA();
    char msg[400];
    JgHoja *otra = n > 0 ? jg_hoja_de(a[0]) : NULL;
    Valor *img = NULL;
    if (n >= 1 && a[0]->tipo == VAL_TEXTO) { img = fn_jg_imagen(a, 1); if (hay_error) return img; }
    else if (n >= 1 && jg_id_de(a[0], "imagen") >= 0 && jg_id_de(a[0], "imagen") < _jgs.nimagenes) img = a[0];
    if (!img || (n >= 2 && a[1]->tipo != VAL_NUMERO && a[1]->tipo != VAL_DICCIONARIO)) {
        if (img && img != a[0]) valor_destruir(img);
        return jg_error("ErrorTipo", "hoja() espera (imagen o ruta), (imagen, {\"columnas\": n, \"filas\": n}) "
                        "o (imagen, ancho de cada cuadro[, alto])");
    }
    int k = jg_id_de(img, "imagen");
    if (img != a[0]) valor_destruir(img);
    JgHoja r;
    memset(&r, 0, sizeof r);
    r.base = k;
    r.modo = JG_HOJA_SOLA;
    int i = 1;
    if (n > 1 && a[1]->tipo == VAL_NUMERO) {
        r.modo = JG_HOJA_TAMANO;
        r.g.cw = r.g.ch = (int)a[1]->numero;
        i = 2;
        if (n > i && a[i]->tipo == VAL_NUMERO) r.g.ch = (int)a[i++]->numero;
    }
    Valor *op = NULL;
    if (n > i) {
        if (a[i]->tipo != VAL_DICCIONARIO)
            return jg_error("ErrorTipo", "hoja(): las opciones van en un diccionario, por ejemplo {\"columnas\": 4}");
        op = a[i];
        for (int j = 0; j < op->diccionario.cantidad; j++) {
            const char *c = op->diccionario.claves[j];
            Valor *v = op->diccionario.valores[j];
            if (!strcmp(c, "fondo") || !strcmp(c, "escala") || !strcmp(c, "colores")) continue;      /* se leen abajo */
            if (!strcmp(c, "quitar")) {
                if (v->tipo != VAL_LISTA)
                    return jg_error("ErrorTipo", "hoja(): \"quitar\" es la lista de los cuadros que no se usan, por ejemplo [0, 1, 2]");
                continue;
            }
            int de_cuenta = !strcmp(c, "columnas") || !strcmp(c, "filas");
            if (strcmp(c, "separacion") && strcmp(c, "margen") && !de_cuenta) {
                snprintf(msg, sizeof msg, "hoja(): no conozco la opción '%s' (hay \"columnas\", \"filas\", "
                         "\"separacion\", \"margen\", \"quitar\", \"fondo\", \"escala\" y \"colores\")", c);
                return jg_error("Error", msg);
            }
            if (de_cuenta && r.modo == JG_HOJA_TAMANO)
                return jg_error("Error", "hoja(): di el tamaño de los cuadros o cuántos hay (\"columnas\", \"filas\"), no las dos cosas");
            if (v->tipo != VAL_NUMERO) {
                snprintf(msg, sizeof msg, "hoja(): \"%s\" es un número", c);
                return jg_error("ErrorTipo", msg);
            }
        }
        double v;
        if (jg_opcion(op, "separacion", &v)) r.g.sep = (int)v;
        if (jg_opcion(op, "margen", &v)) r.g.margen = (int)v;
        double columnas = 0, filas = 0;
        if (jg_opcion(op, "columnas", &columnas) | jg_opcion(op, "filas", &filas)) {
            if (columnas == 0) columnas = 1;
            if (filas == 0) filas = 1;
            if (columnas < 1 || filas < 1 || columnas > 4096 || filas > 4096)
                return jg_error("Error", "hoja(): \"columnas\" y \"filas\" son al menos 1");
            r.modo = JG_HOJA_CUENTA;
            r.columnas = (int)columnas; r.filas = (int)filas;
        }
    }
    Valor *quitar = jg_opcion_valor(op, "quitar");
    /* de una hoja ya hecha solo se quitan cuadros: se parte de su receta */
    int solo_quitar = otra && op && op->diccionario.cantidad == 1 && quitar;
    if (solo_quitar) {
        r = *otra;
        r.quitar = NULL; r.nquitar = 0;
    } else {
        r.prep.modo = r.modo == JG_HOJA_SOLA ? 1 : 0;     /* al dividirla sola, el fondo se quita solo */
        if (!jg_leer_preparar(op, &r.prep, "hoja", msg, sizeof msg)) return hay_error ? valor_nulo() : jg_error("Error", msg);
    }
    /* los cuadros que se quitan, en la numeración de la hoja sin quitar nada */
    int *lista = NULL, nlista = 0;
    if (quitar) {
        int total_antes = solo_quitar ? otra->total + otra->nquitar : -1;
        int *quedaban = NULL;               /* de la otra hoja: qué cuadro original es cada uno */
        if (solo_quitar) {
            quedaban = malloc(sizeof(int) * (total_antes > 0 ? total_antes : 1));
            char *fuera = calloc((size_t)(total_antes > 0 ? total_antes : 1), 1);
            if (!quedaban || !fuera) { free(quedaban); free(fuera); return jg_error("Error", "sin memoria"); }
            for (int q = 0; q < otra->nquitar; q++) if (otra->quitar[q] < total_antes) fuera[otra->quitar[q]] = 1;
            for (int c = 0, m = 0; c < total_antes; c++) if (!fuera[c]) quedaban[m++] = c;
            free(fuera);
        }
        int cuenta = (solo_quitar ? otra->nquitar : 0) + quitar->lista.cantidad;
        lista = malloc(sizeof(int) * (cuenta > 0 ? cuenta : 1));
        if (!lista) { free(quedaban); return jg_error("Error", "sin memoria"); }
        if (solo_quitar) for (int q = 0; q < otra->nquitar; q++) lista[nlista++] = otra->quitar[q];
        int maximo = solo_quitar ? otra->total : -1;
        for (int q = 0; q < quitar->lista.cantidad; q++) {
            Valor *e = quitar->lista.elementos[q];
            if (e->tipo != VAL_NUMERO || e->numero != floor(e->numero) || e->numero < 0 || (maximo >= 0 && e->numero >= maximo)) {
                if (maximo >= 0) snprintf(msg, sizeof msg, "hoja(): en \"quitar\", cada cuadro es un número de 0 a %d", maximo - 1);
                else snprintf(msg, sizeof msg, "hoja(): en \"quitar\", cada cuadro es un número desde 0");
                free(lista); free(quedaban);
                return jg_error("Error", msg);
            }
            lista[nlista++] = solo_quitar ? quedaban[(int)e->numero] : (int)e->numero;
        }
        free(quedaban);
        /* ordenados y sin repetir: así la misma receta se reconoce */
        for (int x = 1; x < nlista; x++)
            for (int y = x; y > 0 && lista[y] < lista[y - 1]; y--) { int t = lista[y]; lista[y] = lista[y - 1]; lista[y - 1] = t; }
        int m = 0;
        for (int x = 0; x < nlista; x++) if (m == 0 || lista[x] != lista[m - 1]) lista[m++] = lista[x];
        nlista = m;
    }
    r.quitar = lista; r.nquitar = nlista;
    int hi = jg_hoja_obtener(&r, msg, sizeof msg);
    free(lista);
    if (hi < 0) {
        char m2[460];
        snprintf(m2, sizeof m2, "hoja(): %s", msg);
        return jg_error("Error", m2);
    }
    /* un cuadro de "quitar" que no existe: se avisa (la primera vez se calculó con él) */
    JgHoja *h = &_jg_hojas[hi];
    for (int q = 0; q < h->nquitar && !solo_quitar; q++)
        if (h->quitar[q] >= h->total + h->nquitar) {
            snprintf(msg, sizeof msg, "hoja(): en \"quitar\", cada cuadro es un número de 0 a %d", h->total + h->nquitar - 1);
            return jg_error("Error", msg);
        }
    return jg_hoja_valor(hi);
}

/* ── Animador ──
   Lleva por ti qué animación toca, desde cuándo, si se repite o se queda en
   el último cuadro (y cuál va después), y cuándo hay una pisada:
     sea pj = juego.animador(HOJA, {"andar": {"fila": 0, "por_segundo": 10},
                                    "saltar": {"fila": 2, "repetir": falso, "siguiente": "quieto"},
                                    "quieto": {"fila": 2, "cuadros": [0]}})
     juego.animacion(pj, "andar")           (cambia solo si no estaba ya)
     juego.dibujar(pj, x, y, opciones)
     si juego.pisada(pj) { ... }            (un pie acaba de tocar el suelo)
   Cada animación puede ir en otra hoja ("hoja": OTRA), por ejemplo las que
   hace animar(). Todo su estado va en el diccionario (se puede leer:
   "actual", "cuadro"), así que sobrevive a la recarga en caliente. */
static Valor *jg_dic(Valor *d, const char *clave) {
    int k = valor_diccionario_buscar(d, clave);
    return k < 0 ? NULL : d->diccionario.valores[k];
}

/* La secuencia de cuadros de una animación: fila (o -1) y cuadro de cada uno */
static int jg_anim_secuencia(const JgHoja *h, Valor *cfg, int *fila, int *cuadros, int max) {
    Valor *f = jg_dic(cfg, "fila"), *c = jg_dic(cfg, "cuadros");
    int fl = f ? (int)f->numero : -1, n = 0;
    *fila = fl < h->nf ? fl : h->nf - 1;
    if (c) {
        for (int i = 0; i < c->lista.cantidad && n < max; i++) cuadros[n++] = (int)c->lista.elementos[i]->numero;
    } else {
        int total = fl >= 0 ? h->por_fila[*fila] : h->total, base = 0;
        for (int f = 0; fl >= 0 && f < *fila; f++) base += h->por_fila[f];
        /* en una rejilla, las celdas vacías del final de la fila no son cuadros de la animación */
        while (total > 1 && h->caja && h->caja[base + total - 1].w == 0) total--;
        for (int i = 0; i < total && n < max; i++) cuadros[n++] = i;
    }
    return n;
}

#define JG_MAX_SECUENCIA 1024
typedef struct {
    JgHoja *h;
    Valor *cfg;
    int fila, n, cuadros[JG_MAX_SECUENCIA];
    long k;                 /* cuántos cuadros han pasado desde que empezó (sin dar la vuelta) */
    int pos, acabada, repetir;
    double por_segundo;
} JgAnimEstado;

/* Dónde va el animador ahora; pasa a la siguiente animación si la que había acabó */
static int jg_anim_estado(Valor *pj, JgAnimEstado *e) {
    e->h = jg_hoja_de(jg_dic(pj, "hoja"));
    Valor *anims = jg_dic(pj, "animaciones");
    if (!e->h || !anims) return 0;
    for (int vueltas = 0; vueltas < 8; vueltas++) {
        Valor *actual = jg_dic(pj, "actual"), *desde = jg_dic(pj, "desde");
        e->cfg = actual && actual->tipo == VAL_TEXTO ? jg_dic(anims, actual->texto) : NULL;
        if (!e->cfg || !desde) return 0;
        Valor *ps = jg_dic(e->cfg, "por_segundo"), *rep = jg_dic(e->cfg, "repetir"), *sig = jg_dic(e->cfg, "siguiente");
        JgHoja *propia = jg_hoja_de(jg_dic(e->cfg, "hoja"));      /* una animación puede ir en otra hoja */
        e->h = propia ? propia : jg_hoja_de(jg_dic(pj, "hoja"));
        e->por_segundo = ps ? ps->numero : 10;
        e->repetir = rep ? rep->booleano : !sig;
        e->n = jg_anim_secuencia(e->h, e->cfg, &e->fila, e->cuadros, JG_MAX_SECUENCIA);
        if (e->n < 1) return 0;
        double t = jg_reloj() - desde->numero;
        e->k = (long)floor(t * e->por_segundo + 1e-9);
        if (e->k < 0) e->k = 0;
        e->acabada = !e->repetir && e->k >= e->n;
        if (e->acabada && sig && sig->tipo == VAL_TEXTO && jg_dic(anims, sig->texto)) {
            /* sigue la siguiente, como si hubiera empezado justo al acabar esta */
            jg_dic_poner(pj, "actual", valor_texto(sig->texto));
            jg_dic_poner(pj, "desde", valor_numero(desde->numero + e->n / e->por_segundo));
            continue;
        }
        e->pos = e->repetir ? (int)(e->k % e->n) : (int)(e->k < e->n ? e->k : e->n - 1);
        return 1;
    }
    return 0;
}

static Valor *jg_animador_arg(Valor **a, int n, const char *fn) {
    if (n < 1 || a[0]->tipo != VAL_DICCIONARIO || !jg_dic(a[0], "animador")) {
        char msg[120];
        snprintf(msg, sizeof msg, "%s() espera un animador de juego.animador()", fn);
        jg_error("ErrorTipo", msg);
        return NULL;
    }
    return a[0];
}

/* animador(hoja, animaciones[, inicial]) */
static Valor *fn_jg_animador(Valor **a, int n) {
    JG_ABIERTA();
    JgHoja *h = n > 0 ? jg_hoja_de(a[0]) : NULL;
    char msg[300];
    if (!h || n < 2 || a[1]->tipo != VAL_DICCIONARIO || a[1]->diccionario.cantidad == 0)
        return jg_error("ErrorTipo", "animador() espera (hoja, {nombre: {\"fila\": n, \"por_segundo\": n, ...}, ...}[, inicial])");
    Valor *anims = a[1];
    for (int i = 0; i < anims->diccionario.cantidad; i++) {
        const char *nombre = anims->diccionario.claves[i];
        Valor *cfg = anims->diccionario.valores[i];
        if (cfg->tipo != VAL_DICCIONARIO) {
            snprintf(msg, sizeof msg, "animador(): \"%s\" va con un diccionario, por ejemplo {\"fila\": 0}", nombre);
            return jg_error("ErrorTipo", msg);
        }
        int fila = -1;
        JgHoja *hc = jg_dic(cfg, "hoja") ? jg_hoja_de(jg_dic(cfg, "hoja")) : h;
        if (!hc) {
            snprintf(msg, sizeof msg, "animador(): en \"%s\", \"hoja\" es una hoja de juego.hoja() o juego.animar()", nombre);
            return jg_error("ErrorTipo", msg);
        }
        for (int j = 0; j < cfg->diccionario.cantidad; j++) {
            const char *c = cfg->diccionario.claves[j];
            Valor *v = cfg->diccionario.valores[j];
            int ok = 1;
            if (!strcmp(c, "hoja")) continue;
            if (!strcmp(c, "fila")) {
                ok = v->tipo == VAL_NUMERO && v->numero >= 0 && v->numero < hc->nf && v->numero == floor(v->numero);
                if (ok) fila = (int)v->numero;
                else snprintf(msg, sizeof msg, "animador(): en \"%s\", \"fila\" va de 0 a %d", nombre, hc->nf - 1);
            } else if (!strcmp(c, "por_segundo")) {
                ok = v->tipo == VAL_NUMERO && v->numero > 0;
                if (!ok) snprintf(msg, sizeof msg, "animador(): en \"%s\", \"por_segundo\" es un número mayor que 0", nombre);
            } else if (!strcmp(c, "repetir")) {
                ok = v->tipo == VAL_BOOLEANO;
                if (!ok) snprintf(msg, sizeof msg, "animador(): en \"%s\", \"repetir\" es verdadero o falso", nombre);
            } else if (!strcmp(c, "siguiente")) {
                ok = v->tipo == VAL_TEXTO && jg_dic(anims, v->texto);
                if (!ok) snprintf(msg, sizeof msg, "animador(): en \"%s\", \"siguiente\" es el nombre de otra de las animaciones", nombre);
            } else if (!strcmp(c, "pisadas")) {
                ok = v->tipo == VAL_BOOLEANO || v->tipo == VAL_LISTA;
                for (int q = 0; ok && v->tipo == VAL_LISTA && q < v->lista.cantidad; q++) ok = v->lista.elementos[q]->tipo == VAL_NUMERO;
                if (!ok) snprintf(msg, sizeof msg, "animador(): en \"%s\", \"pisadas\" es verdadero, falso o la lista de cuadros con pisada", nombre);
            } else if (!strcmp(c, "cuadros")) {
                ok = v->tipo == VAL_LISTA && v->lista.cantidad > 0 && v->lista.cantidad <= JG_MAX_SECUENCIA;
                for (int q = 0; ok && q < v->lista.cantidad; q++) ok = v->lista.elementos[q]->tipo == VAL_NUMERO;
                if (!ok) snprintf(msg, sizeof msg, "animador(): en \"%s\", \"cuadros\" es la lista de cuadros que se van mostrando", nombre);
            } else if (!strcmp(c, "golpes")) {
                ok = v->tipo == VAL_LISTA;
                for (int q = 0; ok && q < v->lista.cantidad; q++) {
                    Valor *g = v->lista.elementos[q];
                    ok = g->tipo == VAL_LISTA && g->lista.cantidad == 5;
                    for (int r = 0; ok && r < 5; r++) ok = g->lista.elementos[r]->tipo == VAL_NUMERO;
                    if (ok) ok = g->lista.elementos[0]->numero >= 0 && g->lista.elementos[3]->numero > 0 && g->lista.elementos[4]->numero > 0;
                }
                if (!ok) snprintf(msg, sizeof msg, "animador(): en \"%s\", \"golpes\" es una lista de [cuadro, x, y, ancho, alto] "
                                  "(la caja que golpea en ese cuadro, medida dentro de la celda)", nombre);
            } else if (!strcmp(c, "cancelar")) {
                ok = v->tipo == VAL_NUMERO && v->numero >= 0;
                if (!ok) snprintf(msg, sizeof msg, "animador(): en \"%s\", \"cancelar\" es el cuadro desde el que se puede pasar a otra animación", nombre);
            } else {
                snprintf(msg, sizeof msg, "animador(): en \"%s\", no conozco '%s' (hay \"fila\", \"cuadros\", \"por_segundo\", "
                         "\"repetir\", \"siguiente\", \"pisadas\", \"golpes\", \"cancelar\" y \"hoja\")", nombre, c);
                return jg_error("Error", msg);
            }
            if (!ok) return jg_error("Error", msg);
        }
        Valor *c = jg_dic(cfg, "cuadros");
        int limite = fila >= 0 ? hc->por_fila[fila] : hc->total;
        for (int q = 0; c && q < c->lista.cantidad; q++) {
            double v = c->lista.elementos[q]->numero;
            if (v < 0 || v >= limite || v != floor(v)) {
                snprintf(msg, sizeof msg, "animador(): en \"%s\", los cuadros van de 0 a %d%s", nombre, limite - 1,
                         fila >= 0 ? " (contando dentro de su fila)" : "");
                return jg_error("Error", msg);
            }
        }
    }
    const char *inicial = anims->diccionario.claves[0];
    if (n > 2) {
        if (a[2]->tipo != VAL_TEXTO || !jg_dic(anims, a[2]->texto))
            return jg_error("Error", "animador(): la animación inicial es el nombre de una de las animaciones");
        inicial = a[2]->texto;
    }
    Valor *pj = valor_diccionario_crear();
    valor_diccionario_agregar(pj, "animador", valor_booleano(1));
    a[0]->refs++;
    valor_diccionario_agregar(pj, "hoja", a[0]);
    anims->refs++;
    valor_diccionario_agregar(pj, "animaciones", anims);
    valor_diccionario_agregar(pj, "actual", valor_texto(inicial));
    valor_diccionario_agregar(pj, "desde", valor_numero(jg_reloj()));
    valor_diccionario_agregar(pj, "cuadro", valor_numero(0));
    return pj;
}

/* animacion(animador, nombre[, desde_el_principio]): pone esa animación; si ya
   estaba puesta sigue por donde iba, salvo que se pida empezar de nuevo */
static Valor *fn_jg_animacion(Valor **a, int n) {
    JG_ABIERTA();
    Valor *pj = jg_animador_arg(a, n, "animacion");
    if (!pj) return valor_nulo();
    Valor *anims = jg_dic(pj, "animaciones");
    if (n < 2 || a[1]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "animacion() espera (animador, nombre)");
    if (!jg_dic(anims, a[1]->texto)) {
        char msg[400];
        int l = snprintf(msg, sizeof msg, "animacion(): no hay ninguna animación '%s'; hay", a[1]->texto);
        for (int i = 0; i < anims->diccionario.cantidad && l < (int)sizeof msg - 40; i++)
            l += snprintf(msg + l, sizeof msg - l, "%s \"%s\"", i == 0 ? "" : (i == anims->diccionario.cantidad - 1 ? " y" : ","),
                          anims->diccionario.claves[i]);
        return jg_error("Error", msg);
    }
    JgAnimEstado e;
    jg_anim_estado(pj, &e);                  /* por si acabó una con "siguiente" */
    Valor *actual = jg_dic(pj, "actual");
    int reiniciar = n > 2 && a[2]->tipo == VAL_BOOLEANO && a[2]->booleano;
    if (reiniciar || !actual || strcmp(actual->texto, a[1]->texto)) {
        jg_dic_poner(pj, "actual", valor_texto(a[1]->texto));
        jg_dic_poner(pj, "desde", valor_numero(jg_reloj()));
    }
    return valor_nulo();
}

/* acabada(animador): ¿ha llegado al final una animación que no se repite? */
static Valor *fn_jg_acabada(Valor **a, int n) {
    JG_ABIERTA();
    Valor *pj = jg_animador_arg(a, n, "acabada");
    if (!pj) return valor_nulo();
    JgAnimEstado e;
    if (!jg_anim_estado(pj, &e)) return valor_booleano(0);
    return valor_booleano(e.acabada);
}

/* Los cuadros con pisada de la animación actual (posiciones en su secuencia):
   los que diga su "pisadas", o calculados (y guardados) si se repite */
static char *jg_anim_pasos(Valor *pj, JgAnimEstado *e) {
    char *pisa = calloc((size_t)e->n, 1);
    if (!pisa) return NULL;
    Valor *p = jg_dic(e->cfg, "pisadas");
    if (p && p->tipo == VAL_LISTA) {
        for (int i = 0; i < p->lista.cantidad; i++) {
            int q = (int)p->lista.elementos[i]->numero;
            if (q >= 0 && q < e->n) pisa[q] = 1;
        }
        return pisa;
    }
    if (p ? !p->booleano : !e->repetir) return pisa;
    /* calculadas: se guardan en el animador, con la versión de la hoja */
    Valor *actual = jg_dic(pj, "actual");
    Valor *cache = jg_dic(pj, "pisadas_calculadas");
    if (!cache) { cache = valor_diccionario_crear(); jg_dic_poner(pj, "pisadas_calculadas", cache); }
    Valor *g = jg_dic(cache, actual->texto);
    if (g && g->tipo == VAL_LISTA && g->lista.cantidad == 2 && g->lista.elementos[0]->numero == e->h->version &&
        g->lista.elementos[1]->tipo == VAL_LISTA) {
        Valor *l = g->lista.elementos[1];
        for (int i = 0; i < l->lista.cantidad; i++) {
            int q = (int)l->lista.elementos[i]->numero;
            if (q >= 0 && q < e->n) pisa[q] = 1;
        }
        return pisa;
    }
    int *filas = malloc(sizeof(int) * e->n);
    if (!filas) return pisa;
    for (int i = 0; i < e->n; i++) filas[i] = e->fila;
    jg_pisadas_secuencia(e->h, filas, e->cuadros, e->n, pisa);
    free(filas);
    Valor *l = valor_lista_crear();
    for (int i = 0; i < e->n; i++) if (pisa[i]) lista_agregar(l, valor_numero(i));
    Valor *par = valor_lista_crear();
    lista_agregar(par, valor_numero(e->h->version));
    lista_agregar(par, l);
    jg_dic_poner(cache, actual->texto, par);
    return pisa;
}

/* pisada(animador): ¿ha tocado un pie el suelo desde la última vez que se
   preguntó? (llámala una vez por fotograma) */
static Valor *fn_jg_pisada(Valor **a, int n) {
    JG_ABIERTA();
    Valor *pj = jg_animador_arg(a, n, "pisada");
    if (!pj) return valor_nulo();
    JgAnimEstado e;
    if (!jg_anim_estado(pj, &e)) return valor_booleano(0);
    /* desde dónde mirar: lo último visto de esta misma animación, o justo antes de empezar */
    Valor *actual = jg_dic(pj, "actual"), *desde = jg_dic(pj, "desde");
    Valor *visto = jg_dic(pj, "visto"), *visto_de = jg_dic(pj, "visto_de"), *visto_desde = jg_dic(pj, "visto_desde");
    long ultimo = e.k - 1;
    if (visto && visto_de && visto_desde && !strcmp(visto_de->texto, actual->texto) && visto_desde->numero == desde->numero)
        ultimo = (long)visto->numero;
    int hay = 0;
    if (e.k > ultimo) {
        char *pisa = jg_anim_pasos(pj, &e);
        long desde_k = e.k - ultimo > e.n ? e.k - e.n + 1 : ultimo + 1;
        for (long j = desde_k; pisa && j <= e.k && !hay; j++) {
            if (!e.repetir && j >= e.n) break;
            hay = pisa[e.repetir ? j % e.n : j];
        }
        free(pisa);
    }
    jg_dic_poner(pj, "visto", valor_numero((double)e.k));
    jg_dic_poner(pj, "visto_de", valor_texto(actual->texto));
    jg_dic_poner(pj, "visto_desde", valor_numero(desde->numero));
    return valor_booleano(hay);
}

/* ── Animaciones generadas ──
   animar(imagen, "andar") hace, a partir de un solo dibujo, una hoja con los
   cuadros de una animación. Hay dos maneras:
   - andar y correr mueven las piernas: busca dónde se separan (las filas de
     abajo con hueco en medio), las parte en izquierda y derecha y las balancea
     por turnos (la que avanza se encoge para levantar el pie) mientras el
     cuerpo baja un píxel cuando apoya los dos pies. Es el truco clásico del
     pixel art; con un dibujo sin piernas separadas se toma el 30 % de abajo.
   - las demás deforman el dibujo entero (estirar, aplastar, girar, mover,
     teñir de blanco), con el ancla en los pies o en el centro.
   Los cuadros son algo mayores que el dibujo (lo que asoma al moverse); el
   dibujo queda en el centro, así que con "centrado" se dibuja en el mismo
   sitio que la imagen original. */
static const char *const JG_ANIMACIONES[] = {
    "respirar", "andar", "correr", "saltar", "golpe", "morir", "flotar", "girar", NULL };

typedef struct { double sx, sy, ang, dx, dy, blanco, alfa; } JgPose;

static int jg_opaco(const unsigned char *px, int w, int h, int x, int y) {
    return x >= 0 && y >= 0 && x < w && y < h && px[((size_t)y * w + x) * 4 + 3] > 0;
}

/* Tramos opacos de la fila y (con x en [x0, x1]); 'ini'/'fin' de los dos primeros */
static int jg_tramos(const unsigned char *px, int w, int h, int y, int x0, int x1, int *fin1, int *ini2) {
    int n = 0, dentro = 0;
    for (int x = x0; x <= x1; x++) {
        int o = jg_opaco(px, w, h, x, y);
        if (o && !dentro) { n++; if (n == 2 && ini2) *ini2 = x; }
        if (!o && dentro && n == 1 && fin1) *fin1 = x - 1;
        dentro = o;
    }
    return n;
}

/* Valor entre fotogramas clave (t de 0 a 1): k[i] = {t, a, b} */
static void jg_clave(const double (*k)[3], int n, double t, double *a, double *b) {
    int i = 0;
    while (i < n - 2 && t > k[i + 1][0]) i++;
    double f = (t - k[i][0]) / (k[i + 1][0] - k[i][0]);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    *a = k[i][1] + (k[i + 1][1] - k[i][1]) * f;
    *b = k[i][2] + (k[i + 1][2] - k[i][2]) * f;
}

static JgPose jg_pose(const char *tipo, int i, int n, double k, double u, int bw, int bh) {
    JgPose p = {1, 1, 0, 0, 0, 0, 1};
    double t = 2 * M_PI * i / n, lineal = n > 1 ? (double)i / (n - 1) : 1;
    if (!strcmp(tipo, "respirar")) {
        double a = fmax(0.05, 1.0 / bh) * k;
        p.sy = 1 + a * sin(t); p.sx = 1 - a / 2 * sin(t);
    } else if (!strcmp(tipo, "flotar")) {
        p.dy = -fmax(u, bh * 0.1) * k * sin(t);
    } else if (!strcmp(tipo, "saltar")) {           /* se agacha, sale estirado y vuelve */
        static const double c[][3] = {{0, 1, 1}, {0.2, 1.25, 0.75}, {0.45, 0.85, 1.2}, {0.75, 0.95, 1.06}, {1, 1, 1}};
        jg_clave(c, 5, lineal, &p.sx, &p.sy);
        p.sx = 1 + (p.sx - 1) * k; p.sy = 1 + (p.sy - 1) * k;
    } else if (!strcmp(tipo, "golpe")) {            /* destello blanco y sacudida */
        double f = 1 - (double)i / n;
        p.blanco = f;
        p.dx = 2 * u * k * f * (i % 2 ? -1 : 1);
    } else if (!strcmp(tipo, "morir")) {            /* salta un poco y cae de lado */
        double e = 1 - (1 - lineal) * (1 - lineal);
        p.ang = 90 * e;
        p.dy = -bh * 0.25 * k * sin(M_PI * lineal) - bw / 2.0 * sin(p.ang * M_PI / 180);
        p.blanco = i == 0 ? 0.8 : 0;
    } else if (!strcmp(tipo, "girar")) {
        p.ang = 360.0 * i / n;
    }
    return p;
}

/* Hace la hoja: los cuadros en una fila, cada uno de (*cw)×(*ch). Devuelve los
   píxeles RGBA (con malloc) o NULL con el motivo en 'msg'. */
static unsigned char *jg_generar_animacion(const unsigned char *px, int w, int h, const JgReceta *r,
                                           int *cw, int *ch, char *msg, size_t max) {
    int bx0 = w, by0 = h, bx1 = -1, by1 = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (jg_opaco(px, w, h, x, y)) {
                if (x < bx0) bx0 = x;
                if (x > bx1) bx1 = x;
                if (y < by0) by0 = y;
                if (y > by1) by1 = y;
            }
    if (bx1 < 0) { snprintf(msg, max, "animar(): la imagen es transparente del todo"); return NULL; }
    int bw = bx1 - bx0 + 1, bh = by1 - by0 + 1;
    double k = r->intensidad, u = fmax(1, round(bh / 16.0));
    int andar = !strcmp(r->tipo, "andar"), correr = !strcmp(r->tipo, "correr");
    int n = r->cuadros;
    int rx = 0, ry = 0;

    /* piernas: filas de abajo con hueco en medio, partidas por la columna 'corte' */
    int pie = by1 + 1, cadera = pie, corte = (bx0 + bx1 + 1) / 2;
    double paso = 0, alza = 0, bote = 0;
    if (andar || correr) {
        if (r->piernas > 0) cadera = pie - r->piernas;
        else {
            for (int y = by1; y > by0; y--) {
                if (jg_tramos(px, w, h, y, bx0, bx1, NULL, NULL) < 2) break;
                cadera = y;
            }
            if (pie - cadera < 2) cadera = pie - (int)fmax(2, round(bh * 0.3));
        }
        if (cadera <= by0) cadera = by0 + 1;
        int fin1 = -1, ini2 = -1;
        if (r->centro >= 0) corte = r->centro;
        else if (jg_tramos(px, w, h, by1, bx0, bx1, &fin1, &ini2) >= 2) corte = (fin1 + 1 + ini2) / 2;
        int alto_piernas = pie - cadera;
        paso = r->frente ? 0 : (correr ? 2.5 : 1.5) * u * k;          /* de frente, los pies solo suben */
        alza = fmin(alto_piernas - 1, round((correr ? 3 : r->frente ? 1 : 2) * u * k));
        bote = round((correr ? 1.5 : 1) * u * k);
        rx = (int)ceil(paso);
    } else {
        /* lo que ocupa el dibujo en el peor cuadro, para el tamaño de la celda */
        double ax = (bx0 + bx1 + 1) / 2.0, ay = !strcmp(r->tipo, "girar") ? (by0 + by1 + 1) / 2.0 : by1 + 1;
        double esq[4][2] = {{bx0, by0}, {bx1 + 1, by0}, {bx0, by1 + 1}, {bx1 + 1, by1 + 1}};
        double mx = 0, my = 0;
        for (int i = 0; i < n; i++) {
            JgPose p = jg_pose(r->tipo, i, n, k, u, bw, bh);
            double c = cos(p.ang * M_PI / 180), s = sin(p.ang * M_PI / 180);
            for (int e = 0; e < 4; e++) {
                double vx = (esq[e][0] - ax) * p.sx, vy = (esq[e][1] - ay) * p.sy;
                double X = ax + p.dx + c * vx - s * vy, Y = ay + p.dy + s * vx + c * vy;
                mx = fmax(mx, fmax(-X, X - w));
                my = fmax(my, fmax(-Y, Y - h));
            }
        }
        rx = (int)ceil(mx - 1e-9); ry = (int)ceil(my - 1e-9);
    }
    *cw = w + 2 * rx; *ch = h + 2 * ry;
    if ((long)*cw * n > 16384 || *ch > 16384) { snprintf(msg, max, "animar(): la hoja saldría demasiado grande"); return NULL; }
    int ancho = *cw * n;
    unsigned char *hoja = calloc((size_t)ancho * *ch, 4);
    if (!hoja) { snprintf(msg, max, "sin memoria"); return NULL; }

    for (int i = 0; i < n; i++) {
        JgPose p = jg_pose(r->tipo, i, n, k, u, bw, bh);
        double t = 2 * M_PI * i / n;
        int baja = (int)round(bote * fabs(sin(t)));                      /* el cuerpo, al apoyar los dos pies */
        double avance[2] = {paso * sin(t), -paso * sin(t)};
        int sube[2] = {(int)round(alza * fmax(0, cos(t))), (int)round(alza * fmax(0, -cos(t)))};
        int delante = sube[0] >= sube[1] ? 0 : 1;                       /* la pierna que se levanta, encima */
        double ax = (bx0 + bx1 + 1) / 2.0, ay = !strcmp(r->tipo, "girar") ? (by0 + by1 + 1) / 2.0 : by1 + 1;
        double c = cos(p.ang * M_PI / 180), s = sin(p.ang * M_PI / 180);
        for (int oy = 0; oy < *ch; oy++)
            for (int ox = 0; ox < *cw; ox++) {
                int x = ox - rx, y = oy - ry, sx = -1, sy = -1;
                if (andar || correr) {
                    if (y - baja < cadera && jg_opaco(px, w, h, x, y - baja)) { sx = x; sy = y - baja; }
                    else
                        for (int o = 0; o < 2 && sx < 0; o++) {
                            /* la pierna sube entera (lo de arriba queda tapado por el cuerpo) y
                               se inclina desde la cadera: el pie es lo que más avanza */
                            int pierna = o == 0 ? delante : 1 - delante;
                            int fila = y + sube[pierna];
                            if (fila < cadera || fila >= pie) continue;
                            double f = (fila + 0.5 - cadera) / (pie - cadera);
                            int col = x - (int)round(avance[pierna] * f);
                            if ((col < corte ? 0 : 1) == pierna && jg_opaco(px, w, h, col, fila)) { sx = col; sy = fila; }
                        }
                } else {
                    double vx = x + 0.5 - ax - p.dx, vy = y + 0.5 - ay - p.dy;
                    double qx = (c * vx + s * vy) / p.sx, qy = (-s * vx + c * vy) / p.sy;
                    int X = (int)floor(ax + qx), Y = (int)floor(ay + qy);
                    if (jg_opaco(px, w, h, X, Y)) { sx = X; sy = Y; }
                }
                if (sx < 0) continue;
                const unsigned char *o = px + ((size_t)sy * w + sx) * 4;
                unsigned char *d = hoja + ((size_t)oy * ancho + i * *cw + ox) * 4;
                for (int j = 0; j < 3; j++) d[j] = (unsigned char)(o[j] + (255 - o[j]) * p.blanco);
                d[3] = (unsigned char)(o[3] * p.alfa);
            }
    }
    return hoja;
}

/* La hoja de una animación generada con animar(): como las de hoja() (es una
   hoja por tamaño de su imagen), más "relleno": cuánto más ancha y alta es
   cada celda que el dibujo, por cada lado */
static Valor *jg_animacion_valor(int k) {
    JgImagen *im = &_jgs.imagenes[k], *o = &_jgs.imagenes[im->origen];
    int n = im->receta.cuadros, cw = im->ancho / n;
    JgHoja r;
    memset(&r, 0, sizeof r);
    r.base = k; r.modo = JG_HOJA_TAMANO; r.g.cw = cw; r.g.ch = im->alto; r.prep.escala = 1;
    char msg[300];
    int hi = jg_hoja_obtener(&r, msg, sizeof msg);
    if (hi < 0) return jg_error("Error", msg);
    Valor *d = jg_hoja_valor(hi);
    Valor *rel = valor_lista_crear();
    lista_agregar(rel, valor_numero((cw - o->ancho) / 2));
    lista_agregar(rel, valor_numero((im->alto - o->alto) / 2));
    valor_diccionario_agregar(d, "relleno", rel);
    return d;
}

/* La imagen j se rehace a partir de su origen (que acaba de cambiar): una
   preparada (sin fondo, reducida) o una animación generada. 1 si cambió. */
static int jg_rehacer_imagen(int j) {
    JgImagen *im = &_jgs.imagenes[j];
    int origen = im->origen, w = _jgs.imagenes[origen].ancho, h = _jgs.imagenes[origen].alto;
    unsigned char *px = jg_pixeles(origen);
    if (!px) return 0;
    if (!strcmp(im->receta.tipo, "preparar")) {
        jg_quitar_fondo(px, w, h, im->prep.modo, im->prep.color);
        jg_cambiar_colores(px, w, h, &im->prep);
        if (im->prep.escala < 1) {
            int nw, nh;
            unsigned char *r = jg_reducir(px, w, h, im->prep.escala, &nw, &nh);
            free(px);
            if (!r) return 0;
            px = r; w = nw; h = nh;
        }
    } else {
        int cw, ch;
        char msg[200];
        unsigned char *hoja = jg_generar_animacion(px, w, h, &im->receta, &cw, &ch, msg, sizeof msg);
        free(px);
        if (!hoja) return 0;
        px = hoja; w = cw * im->receta.cuadros; h = ch;
    }
    void *tex = jg_textura_rgba(px, w, h);
    if (!tex) { free(px); return 0; }
    _jg.DestroyTexture(im->tex);
    free(im->px);
    jg_poner_textura(im, tex); im->px = px; im->ancho = w; im->alto = h;
    return 1;
}

/* La imagen 'origen' acaba de cambiar en el disco (o de rehacerse): se rehacen
   las imágenes que salen de ella (preparadas, animaciones generadas), y luego
   las que salen de esas, y las hojas que se recortan de cualquiera de ellas */
static void jg_rehacer_animaciones(int origen) {
    for (int j = 0; j < _jgs.nimagenes; j++)
        if (j != origen && _jgs.imagenes[j].origen == origen && jg_rehacer_imagen(j)) jg_rehacer_animaciones(j);
    char msg[300];
    for (int i = 0; i < _jg_nhojas; i++)
        if (_jg_hojas[i].fuente == origen) jg_hoja_calcular(i, msg, sizeof msg);   /* si falla, se queda como estaba */
}

/* animar(imagen o ruta, tipo[, opciones]) */
static Valor *fn_jg_animar(Valor **a, int n) {
    JG_ABIERTA();
    int src = -1;
    if (n >= 1 && a[0]->tipo == VAL_TEXTO) {
        Valor *img = fn_jg_imagen(a, 1);
        if (hay_error) return img;
        src = jg_id_de(img, "imagen");
        valor_destruir(img);
    } else if (n >= 1) src = jg_id_de(a[0], "imagen");
    char msg[400];
    if (src < 0 || src >= _jgs.nimagenes || n < 2 || a[1]->tipo != VAL_TEXTO)
        return jg_error("ErrorTipo", "animar() espera (imagen o ruta, tipo[, opciones]), por ejemplo animar(\"pj.png\", \"andar\")");
    JgReceta r;
    memset(&r, 0, sizeof r);
    r.intensidad = 1; r.centro = -1;
    int valido = 0;
    for (int i = 0; JG_ANIMACIONES[i]; i++) if (!strcmp(a[1]->texto, JG_ANIMACIONES[i])) valido = 1;
    if (!valido) {
        snprintf(msg, sizeof msg, "animar(): no conozco la animación '%s'; hay respirar, andar, correr, saltar, "
                 "golpe, morir, flotar y girar", a[1]->texto);
        return jg_error("Error", msg);
    }
    snprintf(r.tipo, sizeof r.tipo, "%s", a[1]->texto);
    r.cuadros = !strcmp(r.tipo, "golpe") ? 4 : !strcmp(r.tipo, "correr") || !strcmp(r.tipo, "saltar") ? 6 : 8;
    if (n > 2 && a[2]->tipo != VAL_NULO) {
        if (a[2]->tipo != VAL_DICCIONARIO)
            return jg_error("ErrorTipo", "animar(): las opciones van en un diccionario, por ejemplo {\"cuadros\": 6}");
        Valor *op = a[2];
        for (int j = 0; j < op->diccionario.cantidad; j++) {
            const char *c = op->diccionario.claves[j];
            Valor *v = op->diccionario.valores[j];
            if (strcmp(c, "cuadros") && strcmp(c, "intensidad") && strcmp(c, "piernas") && strcmp(c, "centro") &&
                strcmp(c, "vista")) {
                snprintf(msg, sizeof msg, "animar(): no conozco la opción '%s' (hay \"cuadros\", \"intensidad\", "
                         "\"vista\", \"piernas\" y \"centro\")", c);
                return jg_error("Error", msg);
            }
            if (!strcmp(c, "vista")) {
                if (v->tipo != VAL_TEXTO || (strcmp(v->texto, "lado") && strcmp(v->texto, "frente")))
                    return jg_error("Error", "animar(): \"vista\" es \"lado\" (de perfil) o \"frente\"");
                r.frente = !strcmp(v->texto, "frente");
                continue;
            }
            if (v->tipo != VAL_NUMERO) {
                snprintf(msg, sizeof msg, "animar(): la opción '%s' es un número", c);
                return jg_error("ErrorTipo", msg);
            }
        }
        double v;
        if (jg_opcion(op, "cuadros", &v)) {
            if (v < 1 || v > 64) return jg_error("Error", "animar(): \"cuadros\" va de 1 a 64");
            r.cuadros = (int)v;
        }
        if (jg_opcion(op, "intensidad", &v)) {
            if (v <= 0 || v > 10) return jg_error("Error", "animar(): \"intensidad\" es mayor que 0 (1 es lo normal)");
            r.intensidad = v;
        }
        if (jg_opcion(op, "piernas", &v)) {
            if (v < 2 || v >= _jgs.imagenes[src].alto)
                return jg_error("Error", "animar(): \"piernas\" es cuántas filas de abajo son las piernas (al menos 2)");
            r.piernas = (int)v;
        }
        if (jg_opcion(op, "centro", &v)) {
            if (v < 0 || v >= _jgs.imagenes[src].ancho)
                return jg_error("Error", "animar(): \"centro\" es la columna que separa las dos piernas");
            r.centro = (int)v;
        }
    }
    /* la misma animación de la misma imagen ya está hecha (por ejemplo, al recargar en caliente) */
    char nombre[160];
    snprintf(nombre, sizeof nombre, "<animar %d %s %d %g %d %d %d>", src, r.tipo, r.cuadros, r.intensidad, r.piernas,
             r.centro, r.frente);
    for (int i = 0; i < _jgs.nimagenes; i++)
        if (strcmp(_jgs.imagenes[i].ruta, nombre) == 0) return jg_animacion_valor(i);
    unsigned char *px = jg_pixeles(src);
    if (!px) return jg_error("ErrorJuego", "animar(): no se pudieron leer los píxeles de la imagen");
    int cw, ch;
    unsigned char *hoja = jg_generar_animacion(px, _jgs.imagenes[src].ancho, _jgs.imagenes[src].alto, &r, &cw, &ch, msg, sizeof msg);
    free(px);
    if (!hoja) return jg_error("Error", msg);
    int k = jg_nueva_imagen(nombre, hoja, cw * r.cuadros, ch);
    if (k < 0) return jg_error("ErrorJuego", "animar(): no se pudo crear la hoja (¿demasiadas imágenes?)");
    _jgs.imagenes[k].origen = src;
    _jgs.imagenes[k].receta = r;
    return jg_animacion_valor(k);
}

/* guardar_imagen(imagen, ruta): guarda una imagen (un sprite, una hoja
   generada con animar()...) como PNG, por ejemplo para retocarla en un editor */
static Valor *fn_jg_guardar_imagen(Valor **a, int n) {
    JG_ABIERTA();
    int k = n > 0 ? jg_id_de(a[0], "imagen") : -1;
    if (k < 0 || k >= _jgs.nimagenes || n < 2 || a[1]->tipo != VAL_TEXTO)
        return jg_error("ErrorTipo", "guardar_imagen() espera (imagen, ruta del .png)");
    size_t l = strlen(a[1]->texto);
    if (l < 4 || (strcmp(a[1]->texto + l - 4, ".png") && strcmp(a[1]->texto + l - 4, ".PNG")))
        return jg_error("Error", "guardar_imagen(): la ruta debe acabar en .png");
    unsigned char *px = jg_pixeles(k);
    if (!px) return jg_error("ErrorJuego", "guardar_imagen(): no se pudieron leer los píxeles de la imagen");
    int ok = imagen_guardar_png(a[1]->texto, px, _jgs.imagenes[k].ancho, _jgs.imagenes[k].alto);
    free(px);
    if (!ok) return jg_error("Error", "guardar_imagen(): no se pudo escribir el archivo");
    return valor_nulo();
}

/* La silueta blanca de una imagen (para el destello de "blanco"): se hace la
   primera vez y otra si la imagen cambia */
static struct { unsigned version; void *blanca; } _jg_blanca[JG_MAX_IMAGENES];
static void *jg_textura_blanca(int k) {
    JgImagen *im = &_jgs.imagenes[k];
    if (_jg_blanca[k].version == im->version && _jg_blanca[k].blanca) return _jg_blanca[k].blanca;
    if (_jg_blanca[k].blanca) _jg.DestroyTexture(_jg_blanca[k].blanca);
    _jg_blanca[k].blanca = NULL;
    _jg_blanca[k].version = im->version;
    unsigned char *px = jg_pixeles(k);
    if (!px) return NULL;
    for (size_t i = 0; i < (size_t)im->ancho * im->alto; i++) px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = 255;
    _jg_blanca[k].blanca = jg_textura_rgba(px, im->ancho, im->alto);
    free(px);
    return _jg_blanca[k].blanca;
}
static void jg_blancas_liberar(void) {
    for (int i = 0; i < JG_MAX_IMAGENES; i++) if (_jg_blanca[i].blanca) _jg.DestroyTexture(_jg_blanca[i].blanca);
    memset(_jg_blanca, 0, sizeof _jg_blanca);
}

/* dibujar(imagen, x, y[, opciones]). Opciones (todas opcionales):
     escala, ancho, alto, angulo (grados), espejo, voltear, centrado, alfa (0-255),
     color ([r, g, b] para teñir), recorte ([x, y, ancho, alto]),
     blanco (0 a 1: el destello de cuando a alguien le dan), mezcla ("sumar"
     para brillos y energía, "multiplicar" para sombras),
     cuadro (y fila) de una hoja de hoja() o animar(); con una imagen
     normal, cuadro + celda ([ancho, alto] de cada cuadro).
   Con una hoja, x, y (y "centrado") se refieren a la celda entera, así los
   cuadros de distinto tamaño quedan en su sitio. */
static Valor *fn_jg_dibujar(Valor **a, int n) {
    JG_ABIERTA();
    /* una hoja, o un animador (que dice qué cuadro de su hoja toca) */
    JgHoja *hoja = NULL;
    int anim_fila = -1, anim_cuadro = 0, es_animador = n > 0 && a[0]->tipo == VAL_DICCIONARIO && jg_dic(a[0], "animador");
    if (es_animador) {
        JgAnimEstado est, *e = &est;          /* en la pila, como en animacion() */
        int ok = jg_anim_estado(a[0], e);
        if (ok) { hoja = e->h; anim_fila = e->fila; anim_cuadro = e->cuadros[e->pos]; }
        if (!ok) return jg_error("Error", "dibujar(): el animador no tiene una animación que se pueda dibujar");
    } else if (n > 0) hoja = jg_hoja_de(a[0]);
    int k = hoja ? hoja->imagen : n > 0 ? jg_id_de(a[0], "imagen") : -1;
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
    double cuadro = 0, fv;
    jg_opcion(op, "cuadro", &cuadro);
    int fila = jg_opcion(op, "fila", &fv) ? (int)floor(fv) : -1;
    int ox = 0, oy = 0, cw = 0, ch = 0, es_hoja = 0;
    if (fila < -1 || (fila == -1 && jg_opcion(op, "fila", &fv)))
        return jg_error("Error", "dibujar(): \"fila\" empieza en 0");
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
        JgRejilla g = {(int)celda->lista.elementos[0]->numero, (int)celda->lista.elementos[1]->numero, 0, 0};
        if (g.cw < 1 || g.ch < 1) return jg_error("Error", "dibujar(): la celda debe medir al menos 1×1");
        src = jg_rejilla_rect(img, &g, (int)floor(cuadro));
    } else if (hoja) {                              /* una hoja: ya sabe dónde está cada cuadro */
        char msg[200];
        if (es_animador) { fila = anim_fila; cuadro = anim_cuadro; }
        int i = jg_hoja_cuadro(hoja, fila, (int)floor(cuadro), &src, &ox, &oy, msg, sizeof msg);
        if (i < 0) {
            char m2[260];
            snprintf(m2, sizeof m2, "dibujar(): %s", msg);
            return jg_error("Error", m2);
        }
        es_hoja = 1;
        cw = hoja->cw; ch = hoja->ch;
        if (es_animador) jg_dic_poner(a[0], "cuadro", valor_numero(i));
    }
    if (fila >= 0 && es_hoja != 1)
        return jg_error("Error", "dibujar(): \"fila\" es para hojas de juego.hoja() (cada fila, una animación)");
    if (es_hoja != 1) { cw = src.w; ch = src.h; }
    double escala = 1, fx, fy, angulo = 0, v;
    jg_opcion(op, "escala", &escala);
    fx = fy = escala;
    if (jg_opcion(op, "ancho", &v)) fx = v / cw;
    if (jg_opcion(op, "alto", &v)) fy = v / ch;
    double w = src.w * fx, h = src.h * fy;
    jg_opcion(op, "angulo", &angulo);
    if (jg_opcion(op, "centrado", &v) && v) { x -= cw * fx / 2; y -= ch * fy / 2; }
    int volteo = 0;
    if (jg_opcion(op, "espejo", &v) && v) volteo |= 1;
    if (jg_opcion(op, "voltear", &v) && v) volteo |= 2;
    /* el cuadro dentro de su celda (de lado a lado si está en espejo) */
    x += ((volteo & 1) ? cw - src.w - ox : ox) * fx;
    y += ((volteo & 2) ? ch - src.h - oy : oy) * fy;
    double alfa = 255, blanco = 0;
    jg_opcion(op, "alfa", &alfa);
    jg_opcion(op, "blanco", &blanco);
    if (alfa < 0) alfa = 0;
    if (alfa > 255) alfa = 255;
    if (blanco < 0 || blanco > 1) return jg_error("Error", "dibujar(): \"blanco\" va de 0 (nada) a 1 (todo blanco)");
    int mezcla = 1;                                 /* SDL_BLENDMODE_BLEND */
    Valor *mz = jg_opcion_valor(op, "mezcla");
    if (mz) {
        if (mz->tipo == VAL_TEXTO && !strcmp(mz->texto, "normal")) mezcla = 1;
        else if (mz->tipo == VAL_TEXTO && !strcmp(mz->texto, "sumar")) mezcla = 2;
        else if (mz->tipo == VAL_TEXTO && !strcmp(mz->texto, "multiplicar")) mezcla = 4;
        else return jg_error("Error", "dibujar(): \"mezcla\" es \"normal\", \"sumar\" (brillos, energía) o \"multiplicar\" (sombras)");
    }
    unsigned char tinte[4] = {255, 255, 255, 255};
    Valor *col = jg_opcion_valor(op, "color");
    if (col) { Valor *arg[1] = {col}; if (!jg_color(arg, 1, 0, tinte)) return valor_nulo(); }
    JgRect dst = {(int)floor(x - _jgs.cam_x), (int)floor(y - _jgs.cam_y), (int)ceil(w), (int)ceil(h)};
    if (blanco < 1) {
        _jg.TextureAlpha(img->tex, (unsigned char)alfa);
        _jg.TextureColor(img->tex, tinte[0], tinte[1], tinte[2]);
        if (mezcla != 1) _jg.TextureBlend(img->tex, mezcla);
        _jg.RenderCopyEx(_jgs.render, img->tex, &src, &dst, angulo, NULL, volteo);
        if (mezcla != 1) _jg.TextureBlend(img->tex, 1);
    }
    void *bl = blanco > 0 ? jg_textura_blanca(k) : NULL;
    if (bl) {                                       /* la silueta blanca encima */
        _jg.TextureAlpha(bl, (unsigned char)(alfa * blanco));
        if (mezcla != 1) _jg.TextureBlend(bl, mezcla);
        _jg.RenderCopyEx(_jgs.render, bl, &src, &dst, angulo, NULL, volteo);
        if (mezcla != 1) _jg.TextureBlend(bl, 1);
    }
    return valor_nulo();
}

/* caja(cosa, x, y[, opciones]): el rectángulo [x, y, ancho, alto] de lo que se
   ve de verdad al dibujar 'cosa' así (con las mismas x, y y opciones que
   dibujar: escala, ancho, alto, centrado, espejo, voltear, cuadro, fila), sin
   el hueco transparente de alrededor. Con un animador, el cuadro que toca
   ahora. "margen" la encoge (o la agranda, si es negativo) por cada lado, en
   píxeles de pantalla. El ángulo no cuenta. Va directa a colision(). */
static struct { unsigned version; JgRect r; } _jg_caja_imagen[JG_MAX_IMAGENES];

/* Una caja medida dentro de una celda de cw × ch, puesta en pantalla como la
   pondría dibujar(…, x, y, op) (escala, ancho, alto, centrado, espejo,
   voltear), y encogida por "margen" */
static Valor *jg_caja_en_pantalla(JgRect c, int cw, int ch, double x, double y, Valor *op) {
    double escala = 1, fx, fy, v;
    jg_opcion(op, "escala", &escala);
    fx = fy = escala;
    if (jg_opcion(op, "ancho", &v)) fx = v / cw;
    if (jg_opcion(op, "alto", &v)) fy = v / ch;
    if (jg_opcion(op, "centrado", &v) && v) { x -= cw * fx / 2; y -= ch * fy / 2; }
    double bx = (jg_opcion(op, "espejo", &v) && v) ? cw - c.x - c.w : c.x;
    double by = (jg_opcion(op, "voltear", &v) && v) ? ch - c.y - c.h : c.y;
    double rx = x + bx * fx, ry = y + by * fy, rw = c.w * fx, rh = c.h * fy, m = 0;
    if (jg_opcion(op, "margen", &m) && (c.w > 0 || m < 0)) {
        double mx = fmin(m, rw / 2), my = fmin(m, rh / 2);
        rx += mx; ry += my; rw -= 2 * mx; rh -= 2 * my;
    }
    Valor *l = valor_lista_crear();
    lista_agregar(l, valor_numero(rx));
    lista_agregar(l, valor_numero(ry));
    lista_agregar(l, valor_numero(rw));
    lista_agregar(l, valor_numero(rh));
    return l;
}
static Valor *fn_jg_caja(Valor **a, int n) {
    JG_ABIERTA();
    JgHoja *hoja = NULL;
    int fila = -1, cuadro = 0, es_animador = n > 0 && a[0]->tipo == VAL_DICCIONARIO && jg_dic(a[0], "animador");
    Valor *op = n > 3 ? a[3] : NULL;
    if (op && op->tipo != VAL_DICCIONARIO && op->tipo != VAL_NULO)
        return jg_error("ErrorTipo", "caja(): las opciones van en un diccionario, como en dibujar()");
    double v;
    if (es_animador) {
        JgAnimEstado est, *e = &est;          /* en la pila, como en animacion() */
        int ok = jg_anim_estado(a[0], e);
        if (ok) { hoja = e->h; fila = e->fila; cuadro = e->cuadros[e->pos]; }
        if (!ok) return jg_error("Error", "caja(): el animador no tiene ninguna animación puesta");
    } else if (n > 0 && (hoja = jg_hoja_de(a[0])) != NULL) {
        if (jg_opcion(op, "cuadro", &v)) cuadro = (int)floor(v);
        if (jg_opcion(op, "fila", &v)) {
            if (v < 0) return jg_error("Error", "caja(): \"fila\" empieza en 0");
            fila = (int)floor(v);
        }
    }
    int k = hoja ? hoja->imagen : n > 0 ? jg_id_de(a[0], "imagen") : -1;
    if (k < 0 || k >= _jgs.nimagenes)
        return jg_error("ErrorTipo", "caja() espera una imagen, una hoja o un animador, como dibujar()");
    double x, y;
    if (!jg_num(a, n, 1, &x, "caja") || !jg_num(a, n, 2, &y, "caja")) return valor_nulo();
    JgRect c;
    int cw, ch;
    if (hoja) {
        JgRect src;
        int ox, oy;
        char msg[200];
        int i = jg_hoja_cuadro(hoja, fila, cuadro, &src, &ox, &oy, msg, sizeof msg);
        if (i < 0) {
            char m2[260];
            snprintf(m2, sizeof m2, "caja(): %s", msg);
            return jg_error("Error", m2);
        }
        c = hoja->caja[i];
        cw = hoja->cw; ch = hoja->ch;
    } else {
        JgImagen *im = &_jgs.imagenes[k];
        if (_jg_caja_imagen[k].version != im->version) {      /* se calcula una vez (y otra si la imagen cambia) */
            JgRect todo = {0, 0, im->ancho, im->alto};
            unsigned char *px = jg_pixeles(k);
            _jg_caja_imagen[k].r = px ? jg_visible(px, im->ancho, im->alto, todo) : todo;
            _jg_caja_imagen[k].version = im->version;
            free(px);
        }
        c = _jg_caja_imagen[k].r;
        cw = im->ancho; ch = im->alto;
    }
    return jg_caja_en_pantalla(c, cw, ch, x, y, op);
}

/* golpe(animador, x, y[, opciones]): la caja [x, y, ancho, alto] con la que
   golpea ahora el animador (la de "golpes" del cuadro que toca), puesta como
   se dibuja (las mismas opciones que dibujar: escala, centrado, espejo...).
   nulo si en este cuadro no golpea. Varias cajas en el mismo cuadro se juntan. */
static Valor *fn_jg_golpe(Valor **a, int n) {
    JG_ABIERTA();
    Valor *pj = jg_animador_arg(a, n, "golpe");
    if (!pj) return valor_nulo();
    Valor *op = n > 3 ? a[3] : NULL;
    if (op && op->tipo != VAL_DICCIONARIO && op->tipo != VAL_NULO)
        return jg_error("ErrorTipo", "golpe(): las opciones van en un diccionario, como en dibujar()");
    double x, y;
    if (!jg_num(a, n, 1, &x, "golpe") || !jg_num(a, n, 2, &y, "golpe")) return valor_nulo();
    JgAnimEstado est, *e = &est;          /* en la pila, como en animacion() */
    int ok = jg_anim_estado(pj, e);
    Valor *gs = ok ? jg_dic(e->cfg, "golpes") : NULL;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, hay = 0, pos = ok ? e->pos : 0, cw = ok ? e->h->cw : 0, ch = ok ? e->h->ch : 0;
    if (ok && e->acabada) gs = NULL;             /* quieta en el último cuadro: ya no golpea */
    for (int i = 0; gs && i < gs->lista.cantidad; i++) {
        Valor **g = gs->lista.elementos[i]->lista.elementos;
        if ((int)g[0]->numero != pos) continue;
        int gx0 = (int)g[1]->numero, gy0 = (int)g[2]->numero, gx1 = gx0 + (int)g[3]->numero, gy1 = gy0 + (int)g[4]->numero;
        if (!hay || gx0 < x0) x0 = gx0;
        if (!hay || gy0 < y0) y0 = gy0;
        if (!hay || gx1 > x1) x1 = gx1;
        if (!hay || gy1 > y1) y1 = gy1;
        hay = 1;
    }
    if (!hay) return valor_nulo();
    JgRect c = {x0, y0, x1 - x0, y1 - y0};
    return jg_caja_en_pantalla(c, cw, ch, x, y, op);
}

/* libre(animador): ¿se puede pasar ya a otra animación? Sí si la actual se
   repite, si ha acabado o si ha llegado a su cuadro "cancelar" (así se
   encadenan los golpes de un combo sin esperar a que acabe cada uno). */
static Valor *fn_jg_libre(Valor **a, int n) {
    JG_ABIERTA();
    Valor *pj = jg_animador_arg(a, n, "libre");
    if (!pj) return valor_nulo();
    JgAnimEstado est, *e = &est;          /* en la pila, como en animacion() */
    int libre = 1;
    if (jg_anim_estado(pj, e) && !e->repetir && !e->acabada) {
        Valor *c = jg_dic(e->cfg, "cancelar");
        libre = c && e->k >= (long)c->numero;
    }
    return valor_booleano(libre);
}

/* ── Fuentes ──
   fuente("letra.ttf", 32): una fuente TrueType u OpenType a ese tamaño (las
   letras se dibujan una vez en una imagen y luego se copian, así que es tan
   rápido como dibujar sprites). fuente(imagen, "ABC...") hace una fuente con
   letras dibujadas en una imagen: cada letra separada de la siguiente por
   columnas transparentes (y las filas, por filas transparentes), o en cuadros
   iguales con {"celda": [ancho, alto]}. Se usan con texto(t, x, y, FUENTE) o
   texto(t, x, y, {"fuente": FUENTE, "color": ..., "alinear": ..., "ancho": ...}). */
#define JG_MAX_FUENTES 64
typedef struct {
    char *clave;
    int imagen;                 /* de donde se copian las letras */
    LetraGlifo *g;
    int ng;
    short indice[256];          /* posición en g de los caracteres < 256, o -1 */
    float *kerning;
    float ascenso, alto_linea, espacio;
} JgFuente;
static JgFuente _jg_fuentes[JG_MAX_FUENTES];
static int _jg_nfuentes;

static void jg_fuentes_liberar(void) {
    for (int i = 0; i < _jg_nfuentes; i++) { free(_jg_fuentes[i].clave); free(_jg_fuentes[i].g); free(_jg_fuentes[i].kerning); }
    memset(_jg_fuentes, 0, sizeof _jg_fuentes);
    _jg_nfuentes = 0;
}

static Valor *jg_fuente_valor(int i) {
    Valor *d = valor_diccionario_crear();
    valor_diccionario_agregar(d, "fuente", valor_numero(i));
    valor_diccionario_agregar(d, "alto", valor_numero(_jg_fuentes[i].alto_linea));
    return d;
}

/* La fuente de un diccionario de fuente() (o de unas opciones con "fuente"), o NULL */
static JgFuente *jg_fuente_de(Valor *v) {
    for (int vueltas = 0; vueltas < 2 && v && v->tipo == VAL_DICCIONARIO; vueltas++) {
        int k = valor_diccionario_buscar(v, "fuente");
        if (k < 0) return NULL;
        v = v->diccionario.valores[k];
        if (v->tipo == VAL_NUMERO) {
            int i = (int)v->numero;
            return i >= 0 && i < _jg_nfuentes ? &_jg_fuentes[i] : NULL;
        }
    }
    return NULL;
}

static int jg_fuente_nueva(const char *clave, JgFuente *f) {
    if (_jg_nfuentes == JG_MAX_FUENTES) return -1;
    f->clave = strdup(clave);
    for (int c = 0; c < 256; c++) f->indice[c] = -1;
    for (int i = 0; i < f->ng; i++) if (f->g[i].cp >= 0 && f->g[i].cp < 256) f->indice[f->g[i].cp] = (short)i;
    _jg_fuentes[_jg_nfuentes] = *f;
    return _jg_nfuentes++;
}

static const LetraGlifo *jg_letra_exacta(const JgFuente *f, int cp) {
    if (cp >= 0 && cp < 256) return f->indice[cp] >= 0 ? &f->g[f->indice[cp]] : NULL;
    for (int i = 0; i < f->ng; i++) if (f->g[i].cp == cp) return &f->g[i];
    return NULL;
}

/* La letra, o la más parecida que tenga la fuente: en mayúscula o minúscula
   (muchas fuentes dibujadas solo tienen una), sin tilde, o '?' */
static const LetraGlifo *jg_letra(const JgFuente *f, int cp) {
    const LetraGlifo *g = jg_letra_exacta(f, cp);
    if (g) return g;
    int ac, base = cp >= 0x80 ? jg_base(cp, &ac) : cp;
    if (base == '?' && cp != '?') base = cp;
    if (cp >= 0xE0 && cp <= 0xFF && base >= 'A' && base <= 'Z') base += 32;   /* á → a, no A */
    int otras[3] = {base, base >= 'a' && base <= 'z' ? base - 32 : base >= 'A' && base <= 'Z' ? base + 32 : -1, '?'};
    for (int i = 0; i < 3; i++) if (otras[i] >= 0 && (g = jg_letra_exacta(f, otras[i]))) return g;
    return NULL;
}

/* Cuánto avanza el carácter cp (seguido de sig) en la fuente f (NULL: la de Lince) a escala e */
static double jg_avance(const JgFuente *f, double e, int cp, int sig) {
    if (!f) return 6 * e;
    if (cp == ' ' && !jg_letra_exacta(f, ' ')) return f->espacio * e;
    const LetraGlifo *g = jg_letra(f, cp);
    double a = g ? g->avance : f->espacio;
    if (f->kerning && cp >= 32 && cp < 127 && sig >= 32 && sig < 127) a += f->kerning[(cp - 32) * 95 + (sig - 32)];
    return a * e;
}

/* El ancho de t[desde, hasta) */
static double jg_ancho_tramo(const JgFuente *f, double e, const unsigned char *p, const unsigned char *hasta) {
    double w = 0;
    int n = 0;
    while (p < hasta) {
        int cp = jg_cp(&p);
        const unsigned char *q = p;
        int sig = q < hasta ? jg_cp(&q) : 0;
        w += jg_avance(f, e, cp, sig);
        n++;
    }
    return !f && n ? w - e : w;               /* la de Lince: sin el hueco tras la última */
}

/* Parte el texto en líneas (por los saltos de línea y, con ancho > 0, por los
   espacios para no pasar de ese ancho). Devuelve cuántas; en ini/fin el
   principio y el final de cada una (hasta 'max'). */
static int jg_lineas(const JgFuente *f, double e, const char *t, double ancho,
                     const unsigned char **ini, const unsigned char **fin, int max) {
    const unsigned char *p = (const unsigned char *)t, *inicio = p;
    int nl = 0;
    while (1) {
        const unsigned char *corte = NULL;      /* el último espacio de la línea */
        const unsigned char *q = inicio;
        while (*q && *q != '\n') {
            if (ancho > 0 && *q == ' ' && q > inicio) {
                if (jg_ancho_tramo(f, e, inicio, q) > ancho && corte) break;
                corte = q;
            }
            q += utf8_tam((const char *)q);
        }
        const unsigned char *final = q;
        if (ancho > 0 && corte && jg_ancho_tramo(f, e, inicio, final) > ancho) final = corte;
        if (nl < max) { ini[nl] = inicio; fin[nl] = final; }
        nl++;
        if (!*final) break;
        inicio = final + 1;                     /* el espacio o el salto de línea no se escriben */
    }
    return nl;
}

static double jg_alto_linea(const JgFuente *f, double e) { return f ? f->alto_linea * e : 9 * e; }

/* texto(t, x, y, opciones) y ancho_texto / medir_texto(t, opciones).
   Opciones: fuente, escala, color, alinear ("izquierda", "centro", "derecha";
   x es entonces el centro o el borde derecho), ancho (parte las líneas). */
static Valor *jg_texto_opciones(Valor **a, int n, int medir) {
    const char *fn = medir == 0 ? "texto" : medir == 1 ? "ancho_texto" : "medir_texto";
    Valor *op = medir ? (n > 1 ? a[1] : NULL) : a[3];
    char msg[300];
    if (op && op->tipo == VAL_NULO) op = NULL;
    if (op && op->tipo != VAL_DICCIONARIO) {
        snprintf(msg, sizeof msg, "%s(): las opciones van en un diccionario, por ejemplo {\"fuente\": LETRA, \"color\": [255, 255, 255]}", fn);
        return jg_error("ErrorTipo", msg);
    }
    JgFuente *f = NULL;
    int es_fuente = op && valor_diccionario_buscar(op, "fuente") >= 0 && op->diccionario.valores[valor_diccionario_buscar(op, "fuente")]->tipo == VAL_NUMERO;
    if (op && !es_fuente) {                     /* unas opciones (no una fuente pasada tal cual) */
        for (int j = 0; j < op->diccionario.cantidad; j++) {
            const char *c = op->diccionario.claves[j];
            if (strcmp(c, "fuente") && strcmp(c, "escala") && strcmp(c, "color") && strcmp(c, "alinear") && strcmp(c, "ancho")) {
                snprintf(msg, sizeof msg, "%s(): no conozco la opción '%s' (hay \"fuente\", \"escala\", \"color\", \"alinear\" y \"ancho\")", fn, c);
                return jg_error("Error", msg);
            }
        }
    }
    if (op && valor_diccionario_buscar(op, "fuente") >= 0) {
        f = jg_fuente_de(op);
        if (!f) {
            snprintf(msg, sizeof msg, "%s(): \"fuente\" es una fuente de juego.fuente()", fn);
            return jg_error("ErrorTipo", msg);
        }
    }
    double e = f ? 1 : 2, ancho = 0, v;
    if (jg_opcion(op, "escala", &v)) e = v;
    if (e <= 0) { snprintf(msg, sizeof msg, "%s(): \"escala\" es mayor que 0", fn); return jg_error("Error", msg); }
    if (!f) e = e < 1 ? 1 : e > 64 ? 64 : (int)e;        /* la de Lince va en píxeles enteros */
    jg_opcion(op, "ancho", &ancho);
    int alinear = 0;
    Valor *al = jg_opcion_valor(op, "alinear");
    if (al) {
        if (al->tipo == VAL_TEXTO && !strcmp(al->texto, "izquierda")) alinear = 0;
        else if (al->tipo == VAL_TEXTO && !strcmp(al->texto, "centro")) alinear = 1;
        else if (al->tipo == VAL_TEXTO && !strcmp(al->texto, "derecha")) alinear = 2;
        else { snprintf(msg, sizeof msg, "%s(): \"alinear\" es \"izquierda\", \"centro\" o \"derecha\"", fn); return jg_error("Error", msg); }
    }
    enum { MAXL = 512 };
    const unsigned char *ini[MAXL], *fin[MAXL];
    int nl = jg_lineas(f, e, a[0]->texto, ancho, ini, fin, MAXL);
    if (nl > MAXL) nl = MAXL;
    double alto = jg_alto_linea(f, e);
    if (medir) {
        double w = 0;
        for (int l = 0; l < nl; l++) { double lw = jg_ancho_tramo(f, e, ini[l], fin[l]); if (lw > w) w = lw; }
        if (medir == 1) return valor_numero(w);
        Valor *r = valor_lista_crear();
        lista_agregar(r, valor_numero(w));
        lista_agregar(r, valor_numero(f ? nl * alto : nl * alto - 2 * e));    /* la de Lince: 7 de alto + 2 entre líneas */
        return r;
    }
    double x, y;
    if (!jg_num(a, n, 1, &x, fn) || !jg_num(a, n, 2, &y, fn)) return valor_nulo();
    unsigned char c[4] = {255, 255, 255, 255};
    Valor *col = jg_opcion_valor(op, "color");
    if (col) { Valor *arg[1] = {col}; if (!jg_color(arg, 1, 0, c)) return valor_nulo(); }
    JgImagen *img = f ? &_jgs.imagenes[f->imagen] : NULL;
    if (img) { _jg.TextureColor(img->tex, c[0], c[1], c[2]); _jg.TextureAlpha(img->tex, c[3]); }
    else jg_pintar(c);
    for (int l = 0; l < nl; l++) {
        double px = x - _jgs.cam_x, py = y - _jgs.cam_y + l * alto;
        if (alinear) px -= jg_ancho_tramo(f, e, ini[l], fin[l]) * (alinear == 1 ? 0.5 : 1);
        const unsigned char *p = ini[l];
        while (p < fin[l]) {
            int cp = jg_cp(&p);
            const unsigned char *q = p;
            int sig = q < fin[l] ? jg_cp(&q) : 0;
            if (!f) {
                int ac, ch = jg_base(cp, &ac);
                jg_glifo(ch, (int)floor(px), (int)floor(py), (int)e, ac);
            } else if (cp != ' ') {
                const LetraGlifo *g = jg_letra(f, cp);
                if (g && g->sw > 0 && g->sh > 0) {
                    JgRect src = {g->sx, g->sy, g->sw, g->sh};
                    JgRect dst = {(int)floor(px + g->dx * e + 0.5), (int)floor(py + (f->ascenso + g->dy) * e + 0.5),
                                  (int)floor(g->sw * e + 0.5), (int)floor(g->sh * e + 0.5)};
                    _jg.RenderCopyEx(_jgs.render, img->tex, &src, &dst, 0, NULL, 0);
                }
            }
            px += jg_avance(f, e, cp, sig);
        }
    }
    if (img) { _jg.TextureColor(img->tex, 255, 255, 255); _jg.TextureAlpha(img->tex, 255); }
    return valor_nulo();
}

/* medir_texto(t[, opciones]): [ancho, alto] que ocupa (con "ancho", ya partido en líneas) */
static Valor *fn_jg_medir_texto(Valor **a, int n) {
    if (n < 1 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "medir_texto() espera (texto[, opciones])");
    return jg_texto_opciones(a, n, 2);
}

static int jg_termina_en(const char *s, const char *fin) {
    size_t a = strlen(s), b = strlen(fin);
    if (a < b) return 0;
    for (size_t i = 0; i < b; i++) if ((s[a - b + i] | 32) != fin[i]) return 0;
    return 1;
}

/* Las letras de una imagen: por huecos transparentes, o en cuadros de 'celda' */
static int jg_fuente_imagen(int k, const char *caracteres, int cw, int ch, int sep, int espacio, JgFuente *f, char *msg, size_t max) {
    JgImagen *im = &_jgs.imagenes[k];
    unsigned char *px = jg_pixeles(k);
    if (!px) { snprintf(msg, max, "no se pudieron leer los píxeles de la imagen"); return 0; }
    int W = im->ancho, H = im->alto;
    #define JG_OPACO(x, y) (px[((size_t)(y) * W + (x)) * 4 + 3] > 0)
    /* los caracteres (los espacios solo cuentan en cuadros iguales: en la imagen no se ven) */
    int cps[1024], ncps = 0;
    const unsigned char *p = (const unsigned char *)caracteres;
    while (*p && ncps < 1024) { int cp = jg_cp(&p); if (cw > 0 || cp != ' ') cps[ncps++] = cp; }
    JgRect r[1024];
    int nr = 0, alto_max = 0;
    if (cw > 0) {
        int cols = W / cw, filas = H / ch;
        for (int f2 = 0; f2 < filas; f2++)
            for (int c = 0; c < cols && nr < 1024; c++) { JgRect q = {c * cw, f2 * ch, cw, ch}; r[nr++] = q; }
        alto_max = ch;
        if (ncps > nr) {
            snprintf(msg, max, "la imagen tiene %d cuadros de %d×%d y hay %d caracteres", nr, cw, ch, ncps);
            free(px); return 0;
        }
        nr = ncps;
    } else {
        int y = 0;
        while (y < H) {                         /* bandas: filas con algo, entre filas vacías */
            int hay = 0;
            for (int x = 0; x < W && !hay; x++) hay = JG_OPACO(x, y);
            if (!hay) { y++; continue; }
            int y0 = y;
            while (y < H) {
                hay = 0;
                for (int x = 0; x < W && !hay; x++) hay = JG_OPACO(x, y);
                if (!hay) break;
                y++;
            }
            if (y - y0 > alto_max) alto_max = y - y0;
            for (int x = 0; x < W;) {           /* letras: columnas con algo, entre columnas vacías */
                int col = 0;
                for (int yy = y0; yy < y && !col; yy++) col = JG_OPACO(x, yy);
                if (!col) { x++; continue; }
                int x0 = x;
                while (x < W) {
                    col = 0;
                    for (int yy = y0; yy < y && !col; yy++) col = JG_OPACO(x, yy);
                    if (!col) break;
                    x++;
                }
                if (nr < 1024) { JgRect q = {x0, y0, x - x0, y - y0}; r[nr] = q; }
                nr++;
            }
        }
        if (nr != ncps) {
            snprintf(msg, max, "encuentro %d letras en la imagen y hay %d caracteres (sin contar los espacios)%s", nr, ncps,
                     nr > ncps ? "; si una letra tiene partes separadas (\", %), di lo que mide cada una: {\"celda\": [ancho, alto]}" : "");
            free(px); return 0;
        }
    }
    #undef JG_OPACO
    free(px);
    f->g = malloc(sizeof(LetraGlifo) * (size_t)(nr ? nr : 1));
    if (!f->g) { snprintf(msg, max, "sin memoria"); return 0; }
    double suma = 0;
    for (int i = 0; i < nr; i++) {
        LetraGlifo g = {cps[i], r[i].x, r[i].y, r[i].w, r[i].h, 0, 0, (float)(r[i].w + sep)};
        f->g[i] = g;
        suma += r[i].w;
    }
    f->ng = nr;
    f->imagen = k;
    f->ascenso = 0;
    f->alto_linea = (float)(alto_max + (alto_max + 3) / 4);
    f->espacio = espacio >= 0 ? (float)(espacio + sep) : (float)(nr ? floor(suma / nr * 0.6 + 0.5) + sep : alto_max / 2);
    return 1;
}

/* fuente(ruta.ttf, tamaño) · fuente(imagen, caracteres[, opciones]) */
static Valor *fn_jg_fuente(Valor **a, int n) {
    JG_ABIERTA();
    char clave[1200], msg[400];
    JgFuente f;
    memset(&f, 0, sizeof f);
    if (n >= 1 && a[0]->tipo == VAL_TEXTO && (jg_termina_en(a[0]->texto, ".ttf") || jg_termina_en(a[0]->texto, ".otf"))) {
        if (n < 2 || a[1]->tipo != VAL_NUMERO)
            return jg_error("ErrorTipo", "fuente() espera (ruta de la fuente, tamaño en píxeles), por ejemplo (\"letra.ttf\", 24)");
        double tam = a[1]->numero;
        if (tam < 4 || tam > 400) return jg_error("Error", "fuente(): el tamaño va de 4 a 400 píxeles");
        snprintf(clave, sizeof clave, "ttf|%s|%g", a[0]->texto, tam);
        for (int i = 0; i < _jg_nfuentes; i++) if (!strcmp(_jg_fuentes[i].clave, clave)) return jg_fuente_valor(i);
        size_t largo = 0;
        char *bytes = interprete_leer_bytes(a[0]->texto, &largo);
        if (!bytes) { snprintf(msg, sizeof msg, "fuente(): no se encuentra la fuente '%s'", a[0]->texto); return jg_error("ErrorJuego", msg); }
        /* lo visible del ASCII y del Latin-1 (tildes, ñ, ¿, ¡...), más comillas, guiones, puntos suspensivos y el euro */
        int cps[256], ncps = 0;
        for (int c = 32; c < 127; c++) cps[ncps++] = c;
        for (int c = 161; c < 256; c++) cps[ncps++] = c;
        static const int MAS[] = {0x20AC, 0x2026, 0x201C, 0x201D, 0x2018, 0x2019, 0x2013, 0x2014, 0x2022};
        for (size_t i = 0; i < sizeof MAS / sizeof MAS[0]; i++) cps[ncps++] = MAS[i];
        LetraAtlas at;
        int ok = letra_preparar((const unsigned char *)bytes, largo, (float)tam, cps, ncps, &at);
        free(bytes);
        if (!ok) { snprintf(msg, sizeof msg, "fuente(): '%s' no es una fuente TrueType u OpenType que Lince pueda leer", a[0]->texto); return jg_error("ErrorJuego", msg); }
        char nombre[1300];
        snprintf(nombre, sizeof nombre, "<fuente %s>", clave);
        int k = jg_nueva_imagen(nombre, at.rgba, at.ancho, at.alto);
        at.rgba = NULL;                          /* ya es de la imagen */
        if (k < 0) { letra_liberar(&at); return jg_error("ErrorJuego", "fuente(): no se pudo crear la imagen de las letras"); }
        f.imagen = k;
        f.g = at.glifos; f.ng = at.nglifos; f.kerning = at.kerning;
        f.ascenso = at.ascenso; f.alto_linea = at.alto_linea;
        const LetraGlifo *esp = NULL;
        for (int i = 0; i < f.ng; i++) if (f.g[i].cp == ' ') esp = &f.g[i];
        f.espacio = esp ? esp->avance : (float)(tam / 4);
        at.glifos = NULL; at.kerning = NULL;
    } else {
        Valor *img = NULL;
        if (n >= 1 && a[0]->tipo == VAL_TEXTO) {
            img = jg_imagen_archivo(a, 1);
            if (hay_error) return valor_nulo();
        }
        int k = jg_id_de(img ? img : n >= 1 ? a[0] : NULL, "imagen");
        if (img) valor_destruir(img);
        if (k < 0 || k >= _jgs.nimagenes || n < 2 || a[1]->tipo != VAL_TEXTO || !a[1]->texto[0])
            return jg_error("ErrorTipo", "fuente() espera (\"letra.ttf\", tamaño) o (imagen con las letras, \"ABC...\"[, opciones])");
        Valor *op = n > 2 ? a[2] : NULL;
        if (op && op->tipo != VAL_DICCIONARIO && op->tipo != VAL_NULO)
            return jg_error("ErrorTipo", "fuente(): las opciones van en un diccionario, por ejemplo {\"celda\": [8, 8]}");
        for (int j = 0; op && op->tipo == VAL_DICCIONARIO && j < op->diccionario.cantidad; j++) {
            const char *c = op->diccionario.claves[j];
            if (strcmp(c, "celda") && strcmp(c, "separacion") && strcmp(c, "espacio")) {
                snprintf(msg, sizeof msg, "fuente(): no conozco la opción '%s' (hay \"celda\", \"separacion\" y \"espacio\")", c);
                return jg_error("Error", msg);
            }
        }
        int cw = 0, ch = 0, espacio = -1;
        double v, sep;
        Valor *celda = jg_opcion_valor(op, "celda");
        if (celda) {
            if (celda->tipo != VAL_LISTA || celda->lista.cantidad != 2 || celda->lista.elementos[0]->tipo != VAL_NUMERO ||
                celda->lista.elementos[1]->tipo != VAL_NUMERO || celda->lista.elementos[0]->numero < 1 || celda->lista.elementos[1]->numero < 1)
                return jg_error("Error", "fuente(): \"celda\" es [ancho, alto] de cada letra");
            cw = (int)celda->lista.elementos[0]->numero; ch = (int)celda->lista.elementos[1]->numero;
        }
        sep = cw > 0 ? 0 : 1;
        if (jg_opcion(op, "separacion", &v)) sep = v;
        if (jg_opcion(op, "espacio", &v)) espacio = (int)v;
        if (sep < 0 || sep > 100) return jg_error("Error", "fuente(): \"separacion\" va de 0 a 100 píxeles");
        snprintf(clave, sizeof clave, "img|%d|%d|%d|%g|%d|%.1000s", k, cw, ch, sep, espacio, a[1]->texto);
        for (int i = 0; i < _jg_nfuentes; i++) if (!strcmp(_jg_fuentes[i].clave, clave)) return jg_fuente_valor(i);
        if (!jg_fuente_imagen(k, a[1]->texto, cw, ch, (int)sep, espacio, &f, msg, sizeof msg)) {
            char m2[460];
            snprintf(m2, sizeof m2, "fuente(): %s", msg);
            return jg_error("Error", m2);
        }
    }
    int i = jg_fuente_nueva(clave, &f);
    if (i < 0) { free(f.g); free(f.kerning); return jg_error("ErrorJuego", "fuente(): demasiadas fuentes"); }
    return jg_fuente_valor(i);
}

/* ── Cámara y pantalla ── */
/* camara(x, y): lo que se dibuja se desplaza; camara() vuelve a (0, 0) y sin
   temblor (para el marcador). Devuelve [x, y] */
static Valor *fn_jg_camara(Valor **a, int n) {
    if (n >= 2) {
        double x, y;
        if (!jg_num(a, n, 0, &x, "camara") || !jg_num(a, n, 1, &y, "camara")) return valor_nulo();
        _jgs.cu_x = x; _jgs.cu_y = y;
        _jgs.cam_quieta = 0;
    } else if (n == 0) {
        _jgs.cu_x = _jgs.cu_y = 0;
        _jgs.cam_quieta = 1;
    } else {
        return jg_error("ErrorTipo", "camara() espera (x, y) o nada");
    }
    jg_cam_aplicar();
    Valor *l = valor_lista_crear();
    lista_agregar(l, valor_numero(_jgs.cu_x));
    lista_agregar(l, valor_numero(_jgs.cu_y));
    return l;
}

/* temblor(fuerza[, segundos]): la cámara tiembla hasta 'fuerza' píxeles y se
   va calmando (0,3 s si no se dice). Uno nuevo más fuerte sustituye al de antes. */
static Valor *fn_jg_temblor(Valor **a, int n) {
    JG_ABIERTA();
    double f, s = 0.3;
    if (!jg_num(a, n, 0, &f, "temblor")) return valor_nulo();
    if (n > 1 && !jg_num(a, n, 1, &s, "temblor")) return valor_nulo();
    if (f < 0 || s < 0) return jg_error("Error", "temblor(): la fuerza y los segundos no pueden ser negativos");
    double real = jg_reloj_real(), queda = 0;
    if (_jgs.tem_dur > 0 && real < _jgs.tem_desde + _jgs.tem_dur)
        queda = _jgs.tem_fuerza * (1 - (real - _jgs.tem_desde) / _jgs.tem_dur);
    if (f >= queda && s > 0) { _jgs.tem_fuerza = f; _jgs.tem_desde = real; _jgs.tem_dur = s; }
    return valor_nulo();
}

/* congelar(segundos): el juego se para ese tiempo (de verdad, no del juego):
   dt() da 0 y las animaciones, partículas y suavizar() se quedan quietas. Es
   el parón de cuando un golpe acierta; las escenas y el temblor siguen. */
static Valor *fn_jg_congelar(Valor **a, int n) {
    JG_ABIERTA();
    double s;
    if (!jg_num(a, n, 0, &s, "congelar")) return valor_nulo();
    if (s < 0 || s > 10) return jg_error("Error", "congelar(): los segundos van de 0 a 10");
    if (!_jgs.reloj_propio) { _jgs.reloj = jg_reloj_real(); _jgs.reloj_propio = 1; }
    double hasta = jg_reloj_real() + s;
    if (hasta > _jgs.congelar_hasta) _jgs.congelar_hasta = hasta;
    return valor_nulo();
}

/* velocidad([v]): lo rápido que pasa el tiempo del juego (1 normal, 0,3 cámara
   lenta, 0 pausa). Sin nada, dice la que hay. */
static Valor *fn_jg_velocidad(Valor **a, int n) {
    JG_ABIERTA();
    if (n > 0) {
        double v;
        if (!jg_num(a, n, 0, &v, "velocidad")) return valor_nulo();
        if (v < 0 || v > 10) return jg_error("Error", "velocidad(): va de 0 (parado) a 10 (1 es lo normal)");
        if (!_jgs.reloj_propio) { _jgs.reloj = jg_reloj_real(); _jgs.reloj_propio = 1; }
        _jgs.velocidad = v;
    }
    return valor_numero(_jgs.velocidad);
}

/* pantalla_completa([sí/no]): sin argumento alterna. El juego se escala sin deformarse. */
static Valor *fn_jg_pantalla_completa(Valor **a, int n) {
    JG_ABIERTA();
    int quiere = !_jgs.completa;
    if (n > 0 && a[0]->tipo == VAL_BOOLEANO) quiere = a[0]->booleano;
    if (_jg.SetFullscreen(_jgs.ventana, quiere ? 0x1001u : 0) == 0) _jgs.completa = quiere;
    return valor_booleano(_jgs.completa);
}

/* Segundos de verdad desde que se abrió la ventana (en el modo de prueba, fotogramas / 60) */
static double jg_reloj_real(void) {
    if (!_jgs.abierta) return 0;
    if (_jgs.limite) return _jgs.total / 60.0;
    return jg_ahora() - _jgs.t_inicio;
}

/* tiempo(): segundos de juego desde que se abrió la ventana (para animaciones):
   la suma de los dt, así que se para con congelar() y sigue a velocidad() */
static double jg_reloj(void) {
    if (!_jgs.abierta) return 0;
    return _jgs.reloj_propio ? _jgs.reloj : jg_reloj_real();
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
        acc->desde = acc->soltada = -1; acc->duro = 0;
    }
    nueva.nombre = acc->nombre;
    nueva.desde = acc->desde; nueva.soltada = acc->soltada; nueva.duro = acc->duro;   /* se puede redefinir sin perder la carga */
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

/* En presentar(): apunta las acciones que se acaban de pulsar (para combo()) */
static void jg_historial(void) {
    double t = jg_reloj_real();
    for (int i = 0; i < _jgs.nacciones; i++) {
        JgAccion *acc = &_jgs.acciones[i];
        int ahora = jg_accion_fuerza(acc, 0) >= 0.5f;
        if (!ahora && acc->desde >= 0) { acc->soltada = t; acc->duro = t - acc->desde; acc->desde = -1; }
        if (ahora && acc->desde < 0) acc->desde = t;
        if (!ahora || jg_accion_fuerza(acc, 1) >= 0.5f) continue;
        if (_jgs.nhist == JG_HISTORIAL) memmove(_jgs.hist, _jgs.hist + 1, sizeof _jgs.hist[0] * --_jgs.nhist);
        _jgs.hist[_jgs.nhist].accion = i;
        _jgs.hist[_jgs.nhist].t = t;
        _jgs.hist[_jgs.nhist++].usada = 0;
    }
}

/* combo(acciones[, segundos]): ¿se han pulsado esas acciones en ese orden en
   los últimos 'segundos' (0,5 si no se dice)? Puede haber otras pulsaciones en
   medio. Cuando da verdadero, esas pulsaciones se gastan: no valen para otro
   combo (así que se pregunta antes por los combos largos). Con una sola
   acción sirve para no perder una pulsación hecha un poco antes de tiempo:
   combo(["golpe"], 0.2) en vez de accion_pulsada("golpe"). */
static Valor *fn_jg_combo(Valor **a, int n) {
    JG_ABIERTA();
    if (n < 1 || a[0]->tipo != VAL_LISTA || a[0]->lista.cantidad < 1 || a[0]->lista.cantidad > JG_HISTORIAL)
        return jg_error("ErrorTipo", "combo() espera (lista de acciones[, segundos]), por ejemplo ([\"abajo\", \"adelante\", \"golpe\"], 0.5)");
    double s = 0.5;
    if (n > 1 && !jg_num(a, n, 1, &s, "combo")) return valor_nulo();
    if (s < 0 || s > 10) return jg_error("Error", "combo(): los segundos van de 0 a 10");
    int m = a[0]->lista.cantidad, idx[JG_HISTORIAL], usadas[JG_HISTORIAL];
    for (int i = 0; i < m; i++) {
        Valor *v = a[0]->lista.elementos[i];
        Valor *arg[1] = {v};
        JgAccion *acc = jg_accion_arg(arg, 1, 0, "combo");
        if (!acc) return valor_nulo();
        idx[i] = (int)(acc - _jgs.acciones);
    }
    double limite = jg_reloj_real() - s - 1e-9;
    int j = _jgs.nhist - 1;
    for (int i = m - 1; i >= 0; i--) {
        while (j >= 0 && _jgs.hist[j].t >= limite && (_jgs.hist[j].usada || _jgs.hist[j].accion != idx[i])) j--;
        if (j < 0 || _jgs.hist[j].t < limite) return valor_booleano(0);
        usadas[i] = j--;
    }
    for (int i = 0; i < m; i++) _jgs.hist[usadas[i]].usada = 1;
    return valor_booleano(1);
}

/* pulsaciones(accion[, segundos]): cuántas veces se ha pulsado en los últimos
   'segundos' (0,5 si no se dice), también las que ya ha gastado combo(). Para
   los golpes de machacar un botón (pulsar puño muy deprisa) y para soltarse
   de un agarre. */
static Valor *fn_jg_pulsaciones(Valor **a, int n) {
    JG_ABIERTA();
    JgAccion *acc = jg_accion_arg(a, n, 0, "pulsaciones");
    if (!acc) return valor_nulo();
    double s = 0.5;
    if (n > 1 && !jg_num(a, n, 1, &s, "pulsaciones")) return valor_nulo();
    if (s < 0 || s > 10) return jg_error("Error", "pulsaciones(): los segundos van de 0 a 10");
    int k = (int)(acc - _jgs.acciones), cuenta = 0;
    double limite = jg_reloj_real() - s - 1e-9;
    for (int i = _jgs.nhist - 1; i >= 0 && _jgs.hist[i].t >= limite; i--)
        if (_jgs.hist[i].accion == k) cuenta++;
    return valor_numero(cuenta);
}

/* mantenida(accion[, margen]): cuántos segundos lleva pulsada (0 si no lo
   está). Con 'margen', si se soltó hace menos de esos segundos dice cuánto
   estuvo pulsada: para los golpes cargados (mantener atrás un segundo y
   luego adelante y golpe: al pulsar adelante, atrás ya se ha soltado). */
static Valor *fn_jg_mantenida(Valor **a, int n) {
    JG_ABIERTA();
    JgAccion *acc = jg_accion_arg(a, n, 0, "mantenida");
    if (!acc) return valor_nulo();
    double margen = 0;
    if (n > 1 && !jg_num(a, n, 1, &margen, "mantenida")) return valor_nulo();
    if (margen < 0 || margen > 10) return jg_error("Error", "mantenida(): el margen va de 0 a 10 segundos");
    double t = jg_reloj_real();
    if (acc->desde >= 0) return valor_numero(t - acc->desde);
    if (margen > 0 && acc->soltada >= 0 && t - acc->soltada <= margen + 1e-9) return valor_numero(acc->duro);
    return valor_numero(0);
}

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

/* Los 'cuantos' números de los argumentos, donde un rectángulo puede ir en
   una lista [x, y, ancho, alto] (como los que da caja()) */
static int jg_numeros(Valor **a, int n, double *v, int cuantos, const char *fn, const char *forma) {
    int k = 0, ok = 1;
    for (int i = 0; i < n && ok; i++) {
        if (a[i]->tipo == VAL_NUMERO) { if (k < cuantos) v[k] = a[i]->numero; k++; continue; }
        ok = a[i]->tipo == VAL_LISTA && a[i]->lista.cantidad == 4;
        for (int j = 0; ok && j < 4; j++) {
            ok = a[i]->lista.elementos[j]->tipo == VAL_NUMERO;
            if (ok && k < cuantos) v[k] = a[i]->lista.elementos[j]->numero;
            k++;
        }
    }
    if (ok && k == cuantos) return 1;
    char msg[200];
    snprintf(msg, sizeof msg, "%s() espera %s (cada rectángulo, en números o en una lista [x, y, ancho, alto])", fn, forma);
    jg_error("ErrorTipo", msg);
    return 0;
}

/* colision(x1, y1, w1, h1, x2, y2, w2, h2): ¿se solapan los dos rectángulos?
   También colision(caja1, caja2), con listas [x, y, ancho, alto]. */
static Valor *fn_jg_colision(Valor **a, int n) {
    double v[8];
    if (!jg_numeros(a, n, v, 8, "colision", "(x1, y1, ancho1, alto1, x2, y2, ancho2, alto2)")) return valor_nulo();
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
    if (!jg_numeros(a, n, v, 7, "colision_circulo_rect", "(cx, cy, radio, x, y, ancho, alto)")) return valor_nulo();
    double px = v[0] < v[3] ? v[3] : v[0] > v[3] + v[5] ? v[3] + v[5] : v[0];   /* punto del rectángulo más cercano */
    double py = v[1] < v[4] ? v[4] : v[1] > v[4] + v[6] ? v[4] + v[6] : v[1];
    double dx = v[0] - px, dy = v[1] - py;
    return valor_booleano(dx * dx + dy * dy < v[2] * v[2]);
}

/* dentro(px, py, x, y, ancho, alto): ¿está el punto dentro del rectángulo? (botones, clics) */
static Valor *fn_jg_dentro(Valor **a, int n) {
    double v[6];
    if (!jg_numeros(a, n, v, 6, "dentro", "(px, py, x, y, ancho, alto)")) return valor_nulo();
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

/* ── Movimiento suave ──
   Una curva dice cómo se reparte un cambio en el tiempo: t va de 0 (empieza)
   a 1 (acaba) y la curva da cuánto del camino se ha hecho. "lineal" va igual
   todo el rato; las demás arrancan o paran poco a poco, se pasan un poco y
   vuelven ("atras"), rebotan al llegar ("rebote") o vibran como un muelle
   ("elastico"). */
static const char *const JG_CURVAS[] = { "lineal", "suave", "entrar", "salir", "rebote", "elastico", "atras", NULL };

static double jg_curva(int c, double t) {
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    switch (c) {
        case 0: return t;
        case 1: return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
        case 2: return t * t * t;
        case 3: return 1 - pow(1 - t, 3);
        case 4: {
            const double n1 = 7.5625, d1 = 2.75;
            if (t < 1 / d1) return n1 * t * t;
            if (t < 2 / d1) { t -= 1.5 / d1; return n1 * t * t + 0.75; }
            if (t < 2.5 / d1) { t -= 2.25 / d1; return n1 * t * t + 0.9375; }
            t -= 2.625 / d1;
            return n1 * t * t + 0.984375;
        }
        case 5:
            if (t == 0 || t == 1) return t;
            return pow(2, -10 * t) * sin((t * 10 - 0.75) * (2 * M_PI / 3)) + 1;
        default: {
            const double c1 = 1.70158, c3 = c1 + 1;
            return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2);
        }
    }
}

/* El número de la curva a[i] ("suave" si no se dice); -1 con el error puesto */
static int jg_curva_arg(Valor **a, int n, int i, const char *fn) {
    if (i >= n || a[i]->tipo == VAL_NULO) return 1;
    if (a[i]->tipo == VAL_TEXTO)
        for (int c = 0; JG_CURVAS[c]; c++) if (!strcmp(a[i]->texto, JG_CURVAS[c])) return c;
    char msg[300];
    snprintf(msg, sizeof msg, "%s(): la curva es \"lineal\", \"suave\", \"entrar\", \"salir\", \"rebote\", \"elastico\" o \"atras\"", fn);
    jg_error("Error", msg);
    return -1;
}

/* desde + (hasta - desde) * f, con números o con listas de números (posiciones, colores) */
static Valor *jg_interpolar(Valor *desde, Valor *hasta, double f, const char *fn) {
    if (desde->tipo == VAL_NUMERO && hasta->tipo == VAL_NUMERO) return valor_numero(desde->numero + (hasta->numero - desde->numero) * f);
    int ok = desde->tipo == VAL_LISTA && hasta->tipo == VAL_LISTA && desde->lista.cantidad == hasta->lista.cantidad;
    for (int i = 0; ok && i < desde->lista.cantidad; i++)
        ok = desde->lista.elementos[i]->tipo == VAL_NUMERO && hasta->lista.elementos[i]->tipo == VAL_NUMERO;
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof msg, "%s(): los dos valores son números, o listas de números del mismo largo (como [x, y] o un color)", fn);
        return jg_error("ErrorTipo", msg);
    }
    Valor *r = valor_lista_crear();
    for (int i = 0; i < desde->lista.cantidad; i++) {
        double a = desde->lista.elementos[i]->numero, b = hasta->lista.elementos[i]->numero;
        lista_agregar(r, valor_numero(a + (b - a) * f));
    }
    return r;
}

/* curva(nombre, t): cuánto del camino se ha hecho en el momento t (de 0 a 1) */
static Valor *fn_jg_curva(Valor **a, int n) {
    double t;
    if (n < 2 || a[0]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "curva() espera (nombre de la curva, t de 0 a 1)");
    int c = jg_curva_arg(a, n, 0, "curva");
    if (c < 0 || !jg_num(a, n, 1, &t, "curva")) return valor_nulo();
    return valor_numero(jg_curva(c, t));
}

/* suavizar(desde, hasta, segundos, inicio[, curva]): el valor que toca ahora
   en un cambio de 'desde' a 'hasta' que empezó en 'inicio' (un juego.tiempo())
   y dura 'segundos'; antes de empezar, 'desde', y al acabar, 'hasta' */
static Valor *fn_jg_suavizar(Valor **a, int n) {
    double seg, inicio;
    if (n < 4) return jg_error("ErrorTipo", "suavizar() espera (desde, hasta, segundos, inicio[, curva])");
    if (!jg_num(a, n, 2, &seg, "suavizar") || !jg_num(a, n, 3, &inicio, "suavizar")) return valor_nulo();
    int c = jg_curva_arg(a, n, 4, "suavizar");
    if (c < 0) return valor_nulo();
    double t = seg > 0 ? (jg_reloj() - inicio) / seg : 1;
    return jg_interpolar(a[0], a[1], jg_curva(c, t), "suavizar");
}

/* acercar(actual, objetivo, rapidez): un paso de 'actual' hacia 'objetivo' en
   este fotograma, más largo cuanto más lejos (frena al llegar). Con rapidez 10
   recorre en una décima de segundo unos dos tercios de lo que le falta, vaya
   el juego a los fotogramas que vaya. Para una cámara que sigue al jugador,
   una barra de vida que baja... */
static Valor *fn_jg_acercar(Valor **a, int n) {
    double r;
    if (n < 3) return jg_error("ErrorTipo", "acercar() espera (actual, objetivo, rapidez)");
    if (!jg_num(a, n, 2, &r, "acercar")) return valor_nulo();
    if (r < 0) return jg_error("Error", "acercar(): la rapidez no es negativa");
    return jg_interpolar(a[0], a[1], 1 - exp(-r * _jgs.dt), "acercar");
}

/* ── Partículas ──
   particulas(x, y[, opciones]) lanza un puñado desde (x, y); dibujar_particulas()
   las mueve y las dibuja (llámala cada fotograma, donde quieras que salgan). */
static const char *const JG_OPC_PART[] = {
    "cantidad", "color", "color_final", "velocidad", "direccion", "apertura", "vida",
    "tamaño", "tamano", "tamaño_final", "tamano_final", "gravedad", "mezcla", NULL };

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
                         "color_final, velocidad, direccion, apertura, vida, tamaño, tamaño_final, gravedad y mezcla", k);
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
    int mezcla = 1;
    Valor *mz = jg_opcion_valor(op, "mezcla");
    if (mz) {
        if (mz->tipo == VAL_TEXTO && !strcmp(mz->texto, "sumar")) mezcla = 2;
        else if (mz->tipo == VAL_TEXTO && !strcmp(mz->texto, "multiplicar")) mezcla = 4;
        else if (mz->tipo != VAL_TEXTO || strcmp(mz->texto, "normal"))
            return jg_error("Error", "particulas(): \"mezcla\" es \"normal\", \"sumar\" (chispas y energía que brillan) o \"multiplicar\" (humo que oscurece)");
    }
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
        p->mezcla = (unsigned char)mezcla;
    }
    return valor_nulo();
}

/* dibujar_particulas(): mueve y dibuja las partículas vivas; devuelve cuántas quedan */
static Valor *fn_jg_dibujar_particulas(Valor **a, int n) {
    (void)a; (void)n;
    JG_ABIERTA();
    float dt = (float)_jgs.dt;
    int vivas = 0, modo = 1;
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
        if (p.mezcla != modo) { modo = p.mezcla; _jg.SetBlend(_jgs.render, modo); }
        if (modo == 4)              /* al multiplicar no cuenta la transparencia: se acerca al blanco */
            for (int k = 0; k < 3; k++) c[k] = (unsigned char)(255 - (255 - c[k]) * c[3] / 255);
        _jg.SetDrawColor(_jgs.render, c[0], c[1], c[2], c[3]);
        JgRect r = {(int)floorf(p.x - tam / 2 - (float)_jgs.cam_x), (int)floorf(p.y - tam / 2 - (float)_jgs.cam_y),
                    (int)ceilf(tam), (int)ceilf(tam)};
        _jg.FillRect(_jgs.render, &r);
    }
    _jgs.npart = vivas;
    if (modo != 1) _jg.SetBlend(_jgs.render, 1);
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
    if (f > 0 && _jgs.fase != 1) { _jgs.fase = 1; _jgs.t_fundido = jg_reloj_real(); }
    return valor_nulo();
}

static Valor *fn_jg_escena(Valor **a, int n)        { (void)a; (void)n; return valor_texto(_jgs.escena); }
static Valor *fn_jg_escena_nueva(Valor **a, int n)  { (void)a; (void)n; return valor_booleano(_jgs.escena_nueva); }
static Valor *fn_jg_tiempo_escena(Valor **a, int n) { (void)a; (void)n; return valor_numero(jg_reloj() - _jgs.t_escena); }

/* En presentar(): hace el cambio pendiente y dibuja el fundido */
static void jg_escenas(void) {
    _jgs.escena_nueva = 0;
    double ahora = jg_reloj_real();           /* el fundido no se para con congelar() */
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

/* ── Efectos de sonido generados ──
   Un sintetizador pequeño al estilo de los juegos de 8 bits: una onda cuya
   frecuencia se desliza, con envolvente, arpegio, vibrato y un filtro. Cada
   tipo de efecto tiene sus parámetros y un poco de azar, así dos monedas no
   suenan exactamente igual (salvo que se fije la semilla). */
typedef struct {
    int onda;                       /* 0 cuadrada, 1 sierra, 2 seno, 3 ruido, 4 triangular */
    double f0, f1;                  /* frecuencia al empezar y al acabar (desliza) */
    double ataque, sostener, caida; /* envolvente, en segundos */
    double arpegio_t, arpegio;      /* a los arpegio_t s la frecuencia se multiplica por arpegio */
    double vib_prof, vib_vel;       /* vibrato: profundidad (fracción) y velocidad (Hz) */
    double ciclo;                   /* ciclo de la cuadrada (0,5 = cuadrada pura) */
    double filtro;                  /* paso bajo: 1 sin filtro, menos es más apagado */
} JgSintesis;

static const char *const JG_EFECTOS[] = { "moneda", "salto", "disparo", "explosion", "golpe",
                                          "subir", "clic", "perder", NULL };

/* Azar propio del efecto (para que la semilla no toque matematica.aleatorio) */
static unsigned long long _jg_azar_efecto;
static double jg_azar_ef(void) {
    _jg_azar_efecto = _jg_azar_efecto * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(_jg_azar_efecto >> 11) / 9007199254740992.0;
}
static double jg_entre(double a, double b) { return a + (b - a) * jg_azar_ef(); }

static int jg_preparar_efecto(const char *tipo, JgSintesis *p) {
    memset(p, 0, sizeof *p);
    p->ciclo = 0.5; p->filtro = 1; p->ataque = 0.002;
    if (strcmp(tipo, "moneda") == 0) {
        p->onda = 0; p->f0 = p->f1 = jg_entre(900, 1500);
        p->sostener = jg_entre(0.03, 0.07); p->caida = jg_entre(0.15, 0.3);
        p->arpegio_t = jg_entre(0.04, 0.08); p->arpegio = jg_azar_ef() < 0.5 ? 1.5 : 1.335;
    } else if (strcmp(tipo, "salto") == 0) {
        p->onda = 0; p->ciclo = jg_entre(0.2, 0.45);
        p->f0 = jg_entre(250, 450); p->f1 = p->f0 * jg_entre(1.8, 2.6);
        p->sostener = jg_entre(0.05, 0.12); p->caida = jg_entre(0.1, 0.2);
    } else if (strcmp(tipo, "disparo") == 0) {
        p->onda = jg_azar_ef() < 0.5 ? 0 : 1; p->ciclo = jg_entre(0.2, 0.5);
        p->f0 = jg_entre(800, 1400); p->f1 = jg_entre(120, 250);
        p->sostener = jg_entre(0.02, 0.06); p->caida = jg_entre(0.12, 0.22);
    } else if (strcmp(tipo, "explosion") == 0) {
        p->onda = 3; p->f0 = jg_entre(2500, 4500); p->f1 = jg_entre(300, 700);
        p->sostener = jg_entre(0.05, 0.15); p->caida = jg_entre(0.4, 0.8); p->filtro = jg_entre(0.25, 0.5);
        p->vib_prof = jg_entre(0, 0.2); p->vib_vel = jg_entre(5, 15);
    } else if (strcmp(tipo, "golpe") == 0) {
        p->onda = jg_azar_ef() < 0.6 ? 3 : 0; p->f0 = jg_entre(1500, 3000); p->f1 = jg_entre(200, 500);
        if (p->onda == 0) { p->f0 = jg_entre(250, 450); p->f1 = jg_entre(60, 120); }
        p->sostener = jg_entre(0.01, 0.03); p->caida = jg_entre(0.08, 0.16); p->filtro = jg_entre(0.4, 0.8);
    } else if (strcmp(tipo, "subir") == 0) {
        p->onda = jg_azar_ef() < 0.5 ? 0 : 4; p->f0 = jg_entre(250, 400); p->f1 = p->f0 * jg_entre(2.5, 3.5);
        p->sostener = jg_entre(0.25, 0.4); p->caida = jg_entre(0.15, 0.3);
        p->vib_prof = jg_entre(0.03, 0.08); p->vib_vel = jg_entre(10, 18);
    } else if (strcmp(tipo, "clic") == 0) {
        p->onda = 0; p->f0 = p->f1 = jg_entre(1000, 1600); p->ciclo = jg_entre(0.3, 0.5);
        p->sostener = jg_entre(0.01, 0.02); p->caida = jg_entre(0.02, 0.05);
    } else if (strcmp(tipo, "perder") == 0) {
        p->onda = 1; p->f0 = jg_entre(400, 550); p->f1 = jg_entre(60, 100);
        p->sostener = jg_entre(0.5, 0.7); p->caida = jg_entre(0.2, 0.35);
        p->vib_prof = jg_entre(0.04, 0.08); p->vib_vel = jg_entre(5, 8); p->filtro = 0.6;
    } else return 0;
    return 1;
}

/* Genera el efecto en estéreo de 16 bits a 44100 Hz; devuelve las muestras por canal */
static short *jg_sintetizar(const JgSintesis *p, double tono, double largo, int *muestras) {
    double dur = (p->ataque + p->sostener + p->caida) * largo;
    int total = (int)(dur * JG_FRECUENCIA);
    if (total < 2) total = 2;
    short *buf = malloc((size_t)total * JG_CANALES * sizeof(short));
    if (!buf) return NULL;
    double fase = 0, ruido = 0, filtrada = 0, at = p->ataque * largo, so = p->sostener * largo;
    for (int i = 0; i < total; i++) {
        double t = (double)i / JG_FRECUENCIA, u = t / dur;
        double f = p->f0 * pow(p->f1 / p->f0, u) * tono;               /* desliza de f0 a f1 */
        if (p->arpegio > 0 && t >= p->arpegio_t * largo) f *= p->arpegio;
        if (p->vib_prof > 0) f *= 1 + p->vib_prof * sin(2 * M_PI * p->vib_vel * t);
        double antes = fase;
        fase += f / JG_FRECUENCIA;
        fase -= floor(fase);
        double m;
        switch (p->onda) {
            case 0:  m = fase < p->ciclo ? 1 : -1; break;
            case 1:  m = 2 * fase - 1; break;
            case 2:  m = sin(2 * M_PI * fase); break;
            case 3:  if (fase < antes) ruido = jg_azar_ef() * 2 - 1;      /* nuevo valor en cada ciclo */
                     m = ruido; break;
            default: m = fase < 0.5 ? 4 * fase - 1 : 3 - 4 * fase; break;
        }
        filtrada += (m - filtrada) * p->filtro;
        double env = t < at ? t / at : t < at + so ? 1 : 1 - (t - at - so) / (dur - at - so);
        if (env < 0) env = 0;
        short v = (short)(filtrada * env * env * 14000);
        buf[2 * i] = buf[2 * i + 1] = v;
    }
    *muestras = total;
    return buf;
}

/* Lee tipo y opciones de efecto(tipo[, opciones]) / crear_efecto(...) y genera el sonido */
static short *jg_efecto_args(Valor **a, int n, const char *fn, int *muestras, float *vol, char *clave, size_t max) {
    char msg[300];
    if (n < 1 || a[0]->tipo != VAL_TEXTO) {
        snprintf(msg, sizeof msg, "%s() espera el tipo de efecto: moneda, salto, disparo, explosion, golpe, subir, clic o perder", fn);
        jg_error("ErrorTipo", msg);
        return NULL;
    }
    Valor *op = n > 1 && a[1]->tipo == VAL_DICCIONARIO ? a[1] : NULL;
    if (op)
        for (int i = 0; i < op->diccionario.cantidad; i++) {
            const char *k = op->diccionario.claves[i];
            if (strcmp(k, "volumen") && strcmp(k, "semilla") && strcmp(k, "tono") && strcmp(k, "largo")) {
                snprintf(msg, sizeof msg, "%s(): opción desconocida '%s'. Las opciones son: volumen, tono, largo y semilla", fn, k);
                jg_error("Error", msg);
                return NULL;
            }
        }
    double semilla = -1, tono = 1, largo = 1, v = 0.5;
    jg_opcion(op, "semilla", &semilla);
    jg_opcion(op, "tono", &tono);
    jg_opcion(op, "largo", &largo);
    jg_opcion(op, "volumen", &v);
    if (tono <= 0.05 || tono > 20 || largo <= 0.05 || largo > 10) {
        snprintf(msg, sizeof msg, "%s(): 'tono' y 'largo' multiplican la frecuencia y la duración: usa valores como 0.5, 1 o 2", fn);
        jg_error("Error", msg);
        return NULL;
    }
    *vol = (float)(v < 0 ? 0 : v > 1 ? 1 : v);
    if (semilla < 0 && clave) semilla = 1;                 /* crear_efecto: siempre el mismo sonido */
    if (semilla >= 0) _jg_azar_efecto = (unsigned long long)(semilla * 7919) + 12345;
    else _jg_azar_efecto += 0x9E3779B97F4A7C15ULL;            /* otro distinto cada vez, sin tocar matematica.aleatorio */
    JgSintesis p;
    if (!jg_preparar_efecto(a[0]->texto, &p)) {
        snprintf(msg, sizeof msg, "%s(): no conozco el efecto '%s'. Hay: moneda, salto, disparo, explosion, golpe, subir, clic y perder", fn, a[0]->texto);
        jg_error("Error", msg);
        return NULL;
    }
    if (clave) snprintf(clave, max, "<efecto %s %g %g %g>", a[0]->texto, semilla, tono, largo);
    return jg_sintetizar(&p, tono, largo, muestras);
}

/* efecto(tipo[, opciones]): genera el efecto y lo hace sonar; devuelve la voz */
static Valor *fn_jg_efecto(Valor **a, int n) {
    JG_ABIERTA();
    int muestras;
    float vol;
    short *buf = jg_efecto_args(a, n, "efecto", &muestras, &vol, NULL, 0);
    if (!buf) return valor_nulo();
    return valor_numero(jg_sonar(buf, muestras, vol, 0, buf));
}

/* crear_efecto(tipo[, opciones]): el efecto como sonido, para reproducir() o guardar_sonido() */
static Valor *fn_jg_crear_efecto(Valor **a, int n) {
    JG_ABIERTA();
    int muestras;
    float vol;
    char clave[200];
    short *buf = jg_efecto_args(a, n, "crear_efecto", &muestras, &vol, clave, sizeof clave);
    if (!buf) return valor_nulo();
    int k = -1;
    for (int i = 0; i < _jgs.nsonidos; i++) if (strcmp(_jgs.sonidos[i].ruta, clave) == 0) { k = i; break; }
    if (k >= 0) free(buf);
    else {
        if (_jgs.nsonidos == JG_MAX_SONIDOS) { free(buf); return jg_error("ErrorJuego", "demasiados sonidos cargados"); }
        k = _jgs.nsonidos++;
        _jgs.sonidos[k].datos = buf;
        _jgs.sonidos[k].muestras = muestras;
        _jgs.sonidos[k].ruta = strdup(clave);
        _jgs.sonidos[k].mtime = _jgs.sonidos[k].tam = -1;
    }
    Valor *d = valor_diccionario_crear();
    valor_diccionario_agregar(d, "sonido", valor_numero(k));
    valor_diccionario_agregar(d, "duracion", valor_numero((double)_jgs.sonidos[k].muestras / JG_FRECUENCIA));
    valor_diccionario_agregar(d, "ruta", valor_texto(clave));
    return d;
}

/* guardar_sonido(sonido, ruta): lo guarda como .wav (estéreo, 16 bits, 44100 Hz) */
static Valor *fn_jg_guardar_sonido(Valor **a, int n) {
    JG_ABIERTA();
    JgSonido *so = jg_sonido_arg(a, n, "guardar_sonido");
    if (!so) return valor_nulo();
    if (n < 2 || a[1]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "guardar_sonido() espera (sonido, ruta del .wav)");
    FILE *f = fopen(a[1]->texto, "wb");
    if (!f) return jg_error("Error", "guardar_sonido(): no se pudo crear el archivo");
    unsigned datos = (unsigned)so->muestras * JG_CANALES * 2;
    unsigned char cab[44];
    memcpy(cab, "RIFF", 4);
    unsigned v[] = {36 + datos};
    for (int i = 0; i < 4; i++) cab[4 + i] = (unsigned char)(v[0] >> (8 * i));
    memcpy(cab + 8, "WAVEfmt ", 8);
    const unsigned campos[] = {16, 1 | JG_CANALES << 16, JG_FRECUENCIA, JG_FRECUENCIA * JG_CANALES * 2, (JG_CANALES * 2) | 16 << 16};
    for (int c = 0; c < 5; c++) for (int i = 0; i < 4; i++) cab[16 + 4 * c + i] = (unsigned char)(campos[c] >> (8 * i));
    memcpy(cab + 36, "data", 4);
    for (int i = 0; i < 4; i++) cab[40 + i] = (unsigned char)(datos >> (8 * i));
    int ok = fwrite(cab, 1, 44, f) == 44;
    for (int i = 0; ok && i < so->muestras * JG_CANALES; i++) {
        unsigned char m[2] = {(unsigned char)(so->datos[i] & 0xFF), (unsigned char)((unsigned short)so->datos[i] >> 8)};
        ok = fwrite(m, 1, 2, f) == 2;
    }
    ok = fclose(f) == 0 && ok;
    if (!ok) return jg_error("Error", "guardar_sonido(): no se pudo escribir el archivo");
    return valor_nulo();
}

/* ── Mapas de casillas ──
   Un nivel escrito con texto, un carácter por casilla, para cualquier juego 2D:
   plataformas, vista cenital (mazmorras, laberintos), puzles por casillas... */

/* Pone (o sustituye) d[clave] = v, quedándose con v */
static void jg_dic_poner(Valor *d, const char *clave, Valor *v) {
    int k = valor_diccionario_buscar(d, clave);
    if (k >= 0) { valor_destruir(d->diccionario.valores[k]); d->diccionario.valores[k] = v; }
    else valor_diccionario_agregar(d, clave, v);
}

static JgMapa *jg_mapa_arg(Valor **a, int n, const char *fn) {
    int k = n > 0 ? jg_id_de(a[0], "mapa") : -1;
    if (k < 0 || k >= _jgs.nmapas || !_jgs.mapas[k].celdas) {
        char msg[120];
        snprintf(msg, sizeof msg, "%s() espera un mapa creado con juego.mapa()", fn);
        jg_error("ErrorTipo", msg);
        return NULL;
    }
    return &_jgs.mapas[k];
}

/* Carácter de la casilla (c, f); fuera del mapa, 0 */
static int jg_celda(const JgMapa *m, int c, int f) {
    if (c < 0 || f < 0 || c >= m->cols || f >= m->filas) return 0;
    return (unsigned char)m->celdas[f * m->cols + c];
}

static int jg_es_solida(const JgMapa *m, int c, int f) {
    if (c < 0 || c >= m->cols) return m->bordes >= 1;
    if (f < 0 || f >= m->filas) return m->bordes >= 2;
    return m->solido[jg_celda(m, c, f) & 127];
}

/* mapa(filas, tamaño[, opciones]): opciones "solidos" (texto con los caracteres
   que son pared; "#" por defecto), "plataformas" (se atraviesan desde abajo y
   se pisan desde arriba) y "bordes" ("lados", "todos" o "ninguno") */
static Valor *fn_jg_mapa(Valor **a, int n) {
    if (n < 2 || a[0]->tipo != VAL_LISTA || a[0]->lista.cantidad == 0)
        return jg_error("ErrorTipo", "mapa() espera (lista de filas de texto, tamaño de casilla[, opciones])");
    double tam;
    if (!jg_num(a, n, 1, &tam, "mapa")) return valor_nulo();
    if (tam <= 0) return jg_error("Error", "mapa(): el tamaño de casilla tiene que ser mayor que 0");
    Valor *op = n > 2 && a[2]->tipo == VAL_DICCIONARIO ? a[2] : NULL;
    int filas = a[0]->lista.cantidad, cols = 0;
    for (int f = 0; f < filas; f++) {
        Valor *v = a[0]->lista.elementos[f];
        if (v->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "mapa(): cada fila es un texto");
        for (const unsigned char *t = (const unsigned char *)v->texto; *t; t++)
            if (*t >= 128) {
                char msg[160];
                snprintf(msg, sizeof msg, "mapa(): la fila %d tiene un carácter que no es ASCII; usa letras sin tilde, cifras o signos", f + 1);
                return jg_error("Error", msg);
            }
        int l = (int)strlen(v->texto);
        if (l > cols) cols = l;
    }
    if (cols == 0 || (long)cols * filas > 16 * 1024 * 1024) return jg_error("Error", "mapa(): tamaño no válido");
    int k = -1;
    for (int i = 0; i < _jgs.nmapas; i++) if (!_jgs.mapas[i].celdas) { k = i; break; }
    if (k < 0) {
        if (_jgs.nmapas == JG_MAPAS) return jg_error("ErrorJuego", "demasiados mapas");
        k = _jgs.nmapas++;
    }
    JgMapa *m = &_jgs.mapas[k];
    memset(m, 0, sizeof *m);
    m->cols = cols; m->filas = filas; m->tam = tam; m->bordes = 1;
    m->celdas = malloc((size_t)cols * filas);
    memset(m->celdas, '.', (size_t)cols * filas);
    for (int f = 0; f < filas; f++) {
        const char *t = a[0]->lista.elementos[f]->texto;
        memcpy(m->celdas + (size_t)f * cols, t, strlen(t));
    }
    Valor *v = jg_opcion_valor(op, "solidos");
    const char *sol = v && v->tipo == VAL_TEXTO ? v->texto : "#";
    for (const unsigned char *t = (const unsigned char *)sol; *t; t++) m->solido[*t & 127] = 1;
    v = jg_opcion_valor(op, "plataformas");
    if (v && v->tipo == VAL_TEXTO)
        for (const unsigned char *t = (const unsigned char *)v->texto; *t; t++) m->plataforma[*t & 127] = 1;
    v = jg_opcion_valor(op, "bordes");
    if (v && v->tipo == VAL_TEXTO) {
        if (strcmp(v->texto, "todos") == 0) m->bordes = 2;
        else if (strcmp(v->texto, "ninguno") == 0) m->bordes = 0;
        else if (strcmp(v->texto, "lados") != 0) { free(m->celdas); m->celdas = NULL;
            return jg_error("Error", "mapa(): \"bordes\" es \"lados\", \"todos\" o \"ninguno\""); }
    }
    Valor *d = valor_diccionario_crear();
    valor_diccionario_agregar(d, "mapa", valor_numero(k));
    valor_diccionario_agregar(d, "columnas", valor_numero(cols));
    valor_diccionario_agregar(d, "filas", valor_numero(filas));
    valor_diccionario_agregar(d, "tamaño", valor_numero(tam));
    valor_diccionario_agregar(d, "ancho", valor_numero(cols * tam));
    valor_diccionario_agregar(d, "alto", valor_numero(filas * tam));
    return d;
}

static int jg_celda_args(Valor **a, int n, int i, int *c, int *f, const char *fn) {
    double dc, df;
    if (!jg_num(a, n, i, &dc, fn) || !jg_num(a, n, i + 1, &df, fn)) return 0;
    *c = (int)floor(dc); *f = (int)floor(df);
    return 1;
}

/* casilla(mapa, columna, fila): el carácter que hay ahí ("" fuera del mapa) */
static Valor *fn_jg_casilla(Valor **a, int n) {
    JgMapa *m = jg_mapa_arg(a, n, "casilla");
    int c, f;
    if (!m || !jg_celda_args(a, n, 1, &c, &f, "casilla")) return valor_nulo();
    char t[2] = {(char)jg_celda(m, c, f), 0};
    return valor_texto(t);
}

/* poner_casilla(mapa, columna, fila, carácter) */
static Valor *fn_jg_poner_casilla(Valor **a, int n) {
    JgMapa *m = jg_mapa_arg(a, n, "poner_casilla");
    int c, f;
    if (!m || !jg_celda_args(a, n, 1, &c, &f, "poner_casilla")) return valor_nulo();
    if (n < 4 || a[3]->tipo != VAL_TEXTO || strlen(a[3]->texto) != 1 || (unsigned char)a[3]->texto[0] >= 128)
        return jg_error("ErrorTipo", "poner_casilla() espera (mapa, columna, fila, un carácter ASCII)");
    if (c >= 0 && f >= 0 && c < m->cols && f < m->filas) m->celdas[f * m->cols + c] = a[3]->texto[0];
    return valor_nulo();
}

/* solido(mapa, columna, fila): ¿es pared? (fuera del mapa, según los bordes) */
static Valor *fn_jg_solido(Valor **a, int n) {
    JgMapa *m = jg_mapa_arg(a, n, "solido");
    int c, f;
    if (!m || !jg_celda_args(a, n, 1, &c, &f, "solido")) return valor_nulo();
    return valor_booleano(jg_es_solida(m, c, f));
}

static Valor *jg_par(double x, double y) {
    Valor *l = valor_lista_crear();
    lista_agregar(l, valor_numero(x));
    lista_agregar(l, valor_numero(y));
    return l;
}

/* casilla_en(mapa, x, y): [columna, fila] de la casilla que hay en ese punto */
static Valor *fn_jg_casilla_en(Valor **a, int n) {
    JgMapa *m = jg_mapa_arg(a, n, "casilla_en");
    double x, y;
    if (!m || !jg_num(a, n, 1, &x, "casilla_en") || !jg_num(a, n, 2, &y, "casilla_en")) return valor_nulo();
    return jg_par(floor(x / m->tam), floor(y / m->tam));
}

/* buscar_en_mapa(mapa, caracteres): lista de [columna, fila] de las casillas
   que tienen alguno de esos caracteres, fila a fila */
static Valor *fn_jg_buscar_en_mapa(Valor **a, int n) {
    JgMapa *m = jg_mapa_arg(a, n, "buscar_en_mapa");
    if (!m) return valor_nulo();
    if (n < 2 || a[1]->tipo != VAL_TEXTO) return jg_error("ErrorTipo", "buscar_en_mapa() espera (mapa, caracteres)");
    Valor *l = valor_lista_crear();
    for (int f = 0; f < m->filas; f++)
        for (int c = 0; c < m->cols; c++)
            if (m->celdas[f * m->cols + c] && strchr(a[1]->texto, m->celdas[f * m->cols + c]))
                lista_agregar(l, jg_par(c, f));
    return l;
}

/* ¿Toca algo sólido el rectángulo [x, x+w) × [y, y+h)? */
static int jg_choca_rect(const JgMapa *m, double x, double y, double w, double h) {
    int c0 = (int)floor(x / m->tam), c1 = (int)floor((x + w - 1e-6) / m->tam);
    int f0 = (int)floor(y / m->tam), f1 = (int)floor((y + h - 1e-6) / m->tam);
    for (int f = f0; f <= f1; f++)
        for (int c = c0; c <= c1; c++)
            if (jg_es_solida(m, c, f)) return 1;
    return 0;
}

/* ¿Hay una plataforma bajo los pies, en la fila donde acaba el rectángulo
   después de bajar, que estaba por debajo de los pies antes de bajar? */
static int jg_pisa_plataforma(const JgMapa *m, double x, double w, double pies_antes, double pies, double *tope) {
    int c0 = (int)floor(x / m->tam), c1 = (int)floor((x + w - 1e-6) / m->tam);
    int f = (int)floor((pies - 1e-6) / m->tam);
    double arriba = f * m->tam;
    if (pies_antes > arriba + 1e-6) return 0;
    for (int c = c0; c <= c1; c++)
        if (m->plataforma[jg_celda(m, c, f) & 127]) { *tope = arriba; return 1; }
    return 0;
}

static double jg_num_dic(Valor *d, const char *clave, double defecto, int *falta) {
    int k = valor_diccionario_buscar(d, clave);
    if (k < 0 || d->diccionario.valores[k]->tipo != VAL_NUMERO) { if (falta) *falta = 1; return defecto; }
    return d->diccionario.valores[k]->numero;
}

/* mover_en_mapa(mapa, cosa[, dt]): mueve 'cosa' (un diccionario con x, y, ancho,
   alto, vx, vy) según su velocidad sin atravesar paredes. Al chocar se arrima y
   pone a 0 esa velocidad. Deja en la cosa "en_suelo", "techo" (lógicos) y
   "pared" (-1 a la izquierda, 1 a la derecha, 0 ninguna). Devuelve si chocó. */
static Valor *fn_jg_mover_en_mapa(Valor **a, int n) {
    JgMapa *m = jg_mapa_arg(a, n, "mover_en_mapa");
    if (!m) return valor_nulo();
    if (n < 2 || a[1]->tipo != VAL_DICCIONARIO)
        return jg_error("ErrorTipo", "mover_en_mapa() espera (mapa, diccionario con x, y, ancho, alto, vx, vy[, dt])");
    Valor *d = a[1];
    int falta = 0;
    double x = jg_num_dic(d, "x", 0, &falta), y = jg_num_dic(d, "y", 0, &falta);
    double w = jg_num_dic(d, "ancho", 0, &falta), h = jg_num_dic(d, "alto", 0, &falta);
    if (falta || w <= 0 || h <= 0)
        return jg_error("Error", "mover_en_mapa(): la cosa necesita números en \"x\", \"y\", \"ancho\" y \"alto\"");
    double vx = jg_num_dic(d, "vx", 0, NULL), vy = jg_num_dic(d, "vy", 0, NULL);
    double dt = _jgs.dt;
    if (n > 2 && !jg_num(a, n, 2, &dt, "mover_en_mapa")) return valor_nulo();
    double dx = vx * dt, dy = vy * dt;
    /* en pasos de media casilla como mucho, para no atravesar paredes finas */
    int pasos = (int)ceil(fmax(fabs(dx), fabs(dy)) / (m->tam / 2));
    if (pasos < 1) pasos = 1;
    if (pasos > 1000) pasos = 1000;
    double px = dx / pasos, py = dy / pasos;
    int pared = 0, techo = 0, suelo = 0;
    for (int i = 0; i < pasos; i++) {
        if (px != 0) {
            double nx = x + px;
            if (jg_choca_rect(m, nx, y, w, h)) {
                if (px > 0) { nx = floor((nx + w) / m->tam) * m->tam - w; pared = 1; }
                else        { nx = (floor(nx / m->tam) + 1) * m->tam; pared = -1; }
                if ((px > 0 && nx < x) || (px < 0 && nx > x)) nx = x;   /* ya estaba metido: no lo empuja */
                px = 0; vx = 0;
            }
            x = nx;
        }
        if (py != 0) {
            double ny = y + py, tope;
            if (jg_choca_rect(m, x, ny, w, h)) {
                if (py > 0) { ny = floor((ny + h) / m->tam) * m->tam - h; suelo = 1; }
                else        { ny = (floor(ny / m->tam) + 1) * m->tam; techo = 1; }
                if ((py > 0 && ny < y) || (py < 0 && ny > y)) ny = y;
                py = 0; vy = 0;
            } else if (py > 0 && jg_pisa_plataforma(m, x, w, y + h, ny + h, &tope)) {
                ny = tope - h; suelo = 1; py = 0; vy = 0;
            }
            y = ny;
        }
    }
    /* ¿Está apoyado aunque no se haya movido hacia abajo? */
    if (!suelo && vy >= 0) {
        double tope;
        suelo = jg_choca_rect(m, x, y + 0.5, w, h) || jg_pisa_plataforma(m, x, w, y + h, y + h + 0.5, &tope);
    }
    jg_dic_poner(d, "x", valor_numero(x));
    jg_dic_poner(d, "y", valor_numero(y));
    jg_dic_poner(d, "vx", valor_numero(vx));
    jg_dic_poner(d, "vy", valor_numero(vy));
    jg_dic_poner(d, "en_suelo", valor_booleano(suelo));
    jg_dic_poner(d, "techo", valor_booleano(techo));
    jg_dic_poner(d, "pared", valor_numero(pared));
    return valor_booleano(pared || techo || (suelo && dy > 0));
}

/* El trozo de imagen con que se pinta un carácter en dibujar_mapa():
     imagen               la imagen entera
     [hoja, 5]            el cuadro 5 de una hoja de juego.hoja()
     [hoja, [5, 6, 7]]    animado: va pasando por esos cuadros (0.25 s cada uno)
     [hoja, [5, 6], 0.1]  animado, con los segundos que dura cada cuadro
   Devuelve el número de imagen, o -1 (con el error puesto). */
static int jg_aspecto_imagen(Valor *v, const char *car, JgRect *src) {
    char msg[300];
    int id = jg_id_de(v, "imagen");
    int ox, oy;
    if (id >= 0 && id < _jgs.nimagenes) {
        if (jg_hoja_de(v)) {
            snprintf(msg, sizeof msg, "dibujar_mapa(): \"%s\" es una hoja de tiles; di qué cuadro, por ejemplo [hoja, 3]", car);
            jg_error("ErrorTipo", msg);
            return -1;
        }
        JgRect r = {0, 0, _jgs.imagenes[id].ancho, _jgs.imagenes[id].alto};
        *src = r;
        return id;
    }
    int nl = v->tipo == VAL_LISTA ? v->lista.cantidad : 0;
    Valor *hoja = nl >= 2 ? v->lista.elementos[0] : NULL;
    JgHoja *h = hoja ? jg_hoja_de(hoja) : NULL;
    id = h ? h->imagen : -1;
    if (!h) {
        snprintf(msg, sizeof msg, "dibujar_mapa(): \"%s\" debe ser un color, una imagen o [hoja, cuadro] "
                 "(la hoja se hace con juego.hoja(\"tiles.png\", 16))", car);
        jg_error("ErrorTipo", msg);
        return -1;
    }
    Valor *c = v->lista.elementos[1];
    int cuadro = 0;
    if (c->tipo == VAL_NUMERO && nl == 2) {
        cuadro = (int)floor(c->numero);
    } else if (c->tipo == VAL_LISTA && c->lista.cantidad > 0 && nl <= 3) {
        double paso = 0.25;
        if (nl == 3) {
            if (v->lista.elementos[2]->tipo != VAL_NUMERO || v->lista.elementos[2]->numero <= 0) {
                snprintf(msg, sizeof msg, "dibujar_mapa(): en \"%s\", lo tercero son los segundos de cada cuadro (más de 0)", car);
                jg_error("ErrorTipo", msg);
                return -1;
            }
            paso = v->lista.elementos[2]->numero;
        }
        long i = (long)floor(jg_reloj() / paso) % c->lista.cantidad;
        Valor *e = c->lista.elementos[i];
        for (int j = 0; j < c->lista.cantidad; j++)
            if (c->lista.elementos[j]->tipo != VAL_NUMERO) {
                snprintf(msg, sizeof msg, "dibujar_mapa(): en \"%s\", los cuadros de la animación son números", car);
                jg_error("ErrorTipo", msg);
                return -1;
            }
        cuadro = (int)floor(e->numero);
    } else {
        snprintf(msg, sizeof msg, "dibujar_mapa(): \"%s\" es [hoja, cuadro] o [hoja, [cuadros], segundos]", car);
        jg_error("ErrorTipo", msg);
        return -1;
    }
    jg_hoja_cuadro(h, -1, cuadro, src, &ox, &oy, msg, sizeof msg);
    return id;
}

/* dibujar_mapa(mapa, aspecto): dibuja las casillas que se ven. 'aspecto' dice
   cómo se pinta cada carácter: un color, una imagen (que se ajusta a la
   casilla) o un cuadro de una hoja de tiles, que puede estar animado (ver
   jg_aspecto_imagen). Los caracteres que no aparecen no se dibujan. */
static Valor *fn_jg_dibujar_mapa(Valor **a, int n) {
    JG_ABIERTA();
    JgMapa *m = jg_mapa_arg(a, n, "dibujar_mapa");
    if (!m) return valor_nulo();
    if (n < 2 || a[1]->tipo != VAL_DICCIONARIO)
        return jg_error("ErrorTipo", "dibujar_mapa() espera (mapa, {carácter: color, imagen o [hoja, cuadro]})");
    /* aspecto de cada carácter: color (tipo 1) o trozo de una textura (tipo 2) */
    unsigned char tipo[128] = {0}, color[128][4];
    void *tex[128] = {0};
    JgRect src[128];
    Valor *d = a[1];
    for (int i = 0; i < d->diccionario.cantidad; i++) {
        const char *k = d->diccionario.claves[i];
        if (!k[0] || k[1] || (unsigned char)k[0] >= 128)
            return jg_error("ErrorTipo", "dibujar_mapa(): cada clave es un solo carácter, por ejemplo \"#\"");
        Valor *v = d->diccionario.valores[i];
        int ch = (unsigned char)k[0];
        if (v->tipo == VAL_DICCIONARIO || (v->tipo == VAL_LISTA && v->lista.cantidad > 0 &&
                                           v->lista.elementos[0]->tipo == VAL_DICCIONARIO)) {
            int id = jg_aspecto_imagen(v, k, &src[ch]);
            if (id < 0) return valor_nulo();
            tipo[ch] = 2;
            tex[ch] = _jgs.imagenes[id].tex;
            continue;
        }
        Valor *arg[1] = {v};
        if (!jg_color(arg, 1, 0, color[ch])) return valor_nulo();
        tipo[ch] = 1;
    }
    int c0 = (int)floor(_jgs.cam_x / m->tam), f0 = (int)floor(_jgs.cam_y / m->tam);
    int c1 = (int)floor((_jgs.cam_x + _jgs.ancho) / m->tam), f1 = (int)floor((_jgs.cam_y + _jgs.alto) / m->tam);
    if (c0 < 0) c0 = 0;
    if (f0 < 0) f0 = 0;
    if (c1 >= m->cols) c1 = m->cols - 1;
    if (f1 >= m->filas) f1 = m->filas - 1;
    for (int f = f0; f <= f1; f++)
        for (int c = c0; c <= c1; c++) {
            int ch = (unsigned char)m->celdas[f * m->cols + c] & 127;
            if (!tipo[ch]) continue;
            int x0 = (int)floor(c * m->tam - _jgs.cam_x), y0 = (int)floor(f * m->tam - _jgs.cam_y);
            int x1 = (int)floor((c + 1) * m->tam - _jgs.cam_x), y1 = (int)floor((f + 1) * m->tam - _jgs.cam_y);
            JgRect r = {x0, y0, x1 - x0, y1 - y0};
            if (tipo[ch] == 1) { jg_pintar(color[ch]); _jg.FillRect(_jgs.render, &r); }
            else {
                _jg.TextureAlpha(tex[ch], 255);
                _jg.TextureColor(tex[ch], 255, 255, 255);
                _jg.RenderCopyEx(_jgs.render, tex[ch], &src[ch], &r, 0, NULL, 0);
            }
        }
    return valor_nulo();
}

/* camino(mapa, [c1, f1], [c2, f2][, diagonales]): el camino más corto entre dos
   casillas sin pasar por paredes (A*), como lista de [columna, fila] sin la de
   salida; [] si no hay camino. Para que un enemigo persiga al jugador. */
static int jg_casilla_lista(Valor *v, int *c, int *f) {
    if (v->tipo != VAL_LISTA || v->lista.cantidad != 2 || v->lista.elementos[0]->tipo != VAL_NUMERO ||
        v->lista.elementos[1]->tipo != VAL_NUMERO) return 0;
    *c = (int)floor(v->lista.elementos[0]->numero); *f = (int)floor(v->lista.elementos[1]->numero);
    return 1;
}

static Valor *fn_jg_camino(Valor **a, int n) {
    JgMapa *m = jg_mapa_arg(a, n, "camino");
    if (!m) return valor_nulo();
    int c1, f1, c2, f2;
    if (n < 3 || !jg_casilla_lista(a[1], &c1, &f1) || !jg_casilla_lista(a[2], &c2, &f2))
        return jg_error("ErrorTipo", "camino() espera (mapa, [columna, fila] de salida, [columna, fila] de llegada[, diagonales])");
    int diag = n > 3 && a[3]->tipo == VAL_BOOLEANO && a[3]->booleano;
    Valor *res = valor_lista_crear();
    int W = m->cols, H = m->filas;
    if (c1 < 0 || f1 < 0 || c1 >= W || f1 >= H || c2 < 0 || f2 < 0 || c2 >= W || f2 >= H) return res;
    if (jg_es_solida(m, c2, f2)) return res;
    if (c1 == c2 && f1 == f2) return res;
    size_t N = (size_t)W * H;
    int *g = malloc(N * sizeof(int)), *de = malloc(N * sizeof(int));
    unsigned char *cerrado = calloc(N, 1);
    int *monton = malloc((N * 8 + 16) * 2 * sizeof(int)), nm = 0;   /* montículo de pares (prioridad, casilla) */
    if (!g || !de || !cerrado || !monton) { free(g); free(de); free(cerrado); free(monton); return res; }
    for (size_t i = 0; i < N; i++) { g[i] = INT_MAX; de[i] = -1; }
    #define JG_H(c, f) (diag ? (abs((c) - c2) > abs((f) - f2) ? abs((c) - c2) : abs((f) - f2)) * 10 \
                             : (abs((c) - c2) + abs((f) - f2)) * 10)
    #define JG_EMPUJAR(pr, id) do { int i_ = nm++; monton[2*i_] = (pr); monton[2*i_+1] = (id); \
        while (i_ > 0) { int p_ = (i_ - 1) / 2; if (monton[2*p_] <= monton[2*i_]) break; \
            int t0 = monton[2*p_], t1 = monton[2*p_+1]; monton[2*p_] = monton[2*i_]; monton[2*p_+1] = monton[2*i_+1]; \
            monton[2*i_] = t0; monton[2*i_+1] = t1; i_ = p_; } } while (0)
    int inicio = f1 * W + c1, fin = f2 * W + c2;
    g[inicio] = 0;
    JG_EMPUJAR(JG_H(c1, f1), inicio);
    int hallado = 0;
    while (nm > 0 && nm < (int)(N * 8)) {
        int actual = monton[1];
        nm--;
        monton[0] = monton[2 * nm]; monton[1] = monton[2 * nm + 1];
        for (int i = 0;;) {                                    /* hundir */
            int l = 2 * i + 1, r = l + 1, menor = i;
            if (l < nm && monton[2*l] < monton[2*menor]) menor = l;
            if (r < nm && monton[2*r] < monton[2*menor]) menor = r;
            if (menor == i) break;
            int t0 = monton[2*i], t1 = monton[2*i+1];
            monton[2*i] = monton[2*menor]; monton[2*i+1] = monton[2*menor+1];
            monton[2*menor] = t0; monton[2*menor+1] = t1;
            i = menor;
        }
        if (cerrado[actual]) continue;
        cerrado[actual] = 1;
        if (actual == fin) { hallado = 1; break; }
        int c = actual % W, f = actual / W;
        static const int DC[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DF[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        for (int k = 0; k < (diag ? 8 : 4); k++) {
            int nc = c + DC[k], nf = f + DF[k];
            if (nc < 0 || nf < 0 || nc >= W || nf >= H || jg_es_solida(m, nc, nf)) continue;
            if (k >= 4 && (jg_es_solida(m, c + DC[k], f) || jg_es_solida(m, c, f + DF[k]))) continue;  /* sin cortar esquinas */
            int id = nf * W + nc, coste = g[actual] + (k >= 4 ? 14 : 10);
            if (cerrado[id] || coste >= g[id]) continue;
            g[id] = coste; de[id] = actual;
            JG_EMPUJAR(coste + JG_H(nc, nf), id);
        }
    }
    #undef JG_H
    #undef JG_EMPUJAR
    if (hallado) {
        int largo = 0;
        for (int i = fin; i != inicio; i = de[i]) largo++;
        int *orden = malloc(sizeof(int) * (size_t)largo), k = largo;
        for (int i = fin; i != inicio; i = de[i]) orden[--k] = i;
        for (int i = 0; i < largo; i++) lista_agregar(res, jg_par(orden[i] % W, orden[i] / W));
        free(orden);
    }
    free(g); free(de); free(cerrado); free(monton);
    return res;
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
        {"medir_texto",     fn_jg_medir_texto,     -1},
        {"fuente",          fn_jg_fuente,          -1},
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
        {"colision",        fn_jg_colision,        -1},
        {"curva",           fn_jg_curva,           -1},
        {"suavizar",        fn_jg_suavizar,        -1},
        {"acercar",         fn_jg_acercar,         -1},
        {"caja",            fn_jg_caja,            -1},
        {"imagen",          fn_jg_imagen,           1},
        {"dibujar",         fn_jg_dibujar,         -1},
        {"sprite",          fn_jg_sprite,           2},
        {"hoja",            fn_jg_hoja,            -1},
        {"pisadas",         fn_jg_pisadas,         -1},
        {"animador",        fn_jg_animador,        -1},
        {"animacion",       fn_jg_animacion,       -1},
        {"acabada",         fn_jg_acabada,          1},
        {"golpe",           fn_jg_golpe,           -1},
        {"libre",           fn_jg_libre,            1},
        {"pisada",          fn_jg_pisada,           1},
        {"animar",          fn_jg_animar,          -1},
        {"guardar_imagen",  fn_jg_guardar_imagen,   2},
        {"sonido",          fn_jg_sonido,           1},
        {"reproducir",      fn_jg_reproducir,      -1},
        {"detener",         fn_jg_detener,         -1},
        {"musica",          fn_jg_musica,          -1},
        {"volumen",         fn_jg_volumen,         -1},
        {"camara",          fn_jg_camara,          -1},
        {"temblor",         fn_jg_temblor,         -1},
        {"congelar",        fn_jg_congelar,         1},
        {"velocidad",       fn_jg_velocidad,       -1},
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
        {"combo",           fn_jg_combo,           -1},
        {"mantenida",       fn_jg_mantenida,       -1},
        {"pulsaciones",     fn_jg_pulsaciones,     -1},
        {"direccion",       fn_jg_direccion,        2},
        {"recargado",       fn_jg_recargado,        0},
        {"colision_circulos", fn_jg_colision_circulos, 6},
        {"colision_circulo_rect", fn_jg_colision_circulo_rect, -1},
        {"dentro",          fn_jg_dentro,          -1},
        {"cuadro",          fn_jg_cuadro,          -1},
        {"particulas",      fn_jg_particulas,      -1},
        {"dibujar_particulas", fn_jg_dibujar_particulas, 0},
        {"borrar_particulas", fn_jg_borrar_particulas, 0},
        {"cambiar_escena",  fn_jg_cambiar_escena,  -1},
        {"escena",          fn_jg_escena,           0},
        {"escena_nueva",    fn_jg_escena_nueva,     0},
        {"tiempo_escena",   fn_jg_tiempo_escena,    0},
        {"mapa",            fn_jg_mapa,            -1},
        {"efecto",          fn_jg_efecto,          -1},
        {"crear_efecto",    fn_jg_crear_efecto,    -1},
        {"guardar_sonido",  fn_jg_guardar_sonido,   2},
        {"casilla",         fn_jg_casilla,          3},
        {"poner_casilla",   fn_jg_poner_casilla,    4},
        {"solido",          fn_jg_solido,           3},
        {"casilla_en",      fn_jg_casilla_en,       3},
        {"buscar_en_mapa",  fn_jg_buscar_en_mapa,   2},
        {"mover_en_mapa",   fn_jg_mover_en_mapa,   -1},
        {"dibujar_mapa",    fn_jg_dibujar_mapa,     2},
        {"camino",          fn_jg_camino,          -1},
    };
    registrar_modulo_diccionario(entorno, "juego",
        fns, (int)(sizeof(fns) / sizeof(fns[0])), NULL, NULL, 0);
}
