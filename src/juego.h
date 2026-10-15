/* ─────────────────────────────────────────
   MÓDULO: juego  (se incluye desde modulos.c)
   Ventana 2D, dibujo, teclado, ratón y sonido sencillos. Usa SDL2 cargada en
   tiempo de ejecución (dlopen / LoadLibrary): Lince no la necesita para
   compilarse, y si no está instalada solo falla 'importar "juego"' al abrir.
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
    return NULL;
}
#define JG_ABRIR_LIB()   jg_abrir_lib()
#define JG_SIMBOLO(h, n) dlsym(h, n)
#endif

typedef struct { int x, y, w, h; } JgRect;

static struct {
    int  (*Init)(unsigned);
    void (*Quit)(void);
    const char *(*GetError)(void);
    void *(*CreateWindow)(const char *, int, int, int, int, unsigned);
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
    int  (*QueueAudio)(unsigned, const void *, unsigned);
    unsigned (*QueuedAudio)(unsigned);
} _jg;

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
} _jgs;

static int jg_tecla_codigo(const char *nombre);
static int jg_guardar_bmp(const char *ruta);

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
    JG_FN(CreateWindow, "SDL_CreateWindow")
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
    JG_FN(QueueAudio, "SDL_QueueAudio")
    JG_FN(QueuedAudio, "SDL_GetQueuedAudioSize")
    return 1;
}

static double jg_ahora(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
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
    if (_jg.Init(0x20 | 0x10) != 0 && _jg.Init(0x20) != 0) {   /* vídeo (+ audio si se puede) */
        char msg[400];
        snprintf(msg, sizeof(msg), "no se pudo iniciar SDL2: %s", _jg.GetError());
        return jg_error("ErrorJuego", msg);
    }
    _jgs.abierta = 1;
    _jgs.ventana = _jg.CreateWindow(a[0]->texto, 0x2FFF0000, 0x2FFF0000, (int)w, (int)h, 0);
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
    memset(_jgs.teclas, 0, sizeof(_jgs.teclas));
    memset(_jgs.previas, 0, sizeof(_jgs.previas));
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
    JgRect r = {(int)floor(v[0]), (int)floor(v[1]), (int)ceil(v[2]), (int)ceil(v[3])};
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
    _jg.DrawLine(_jgs.render, (int)v[0], (int)v[1], (int)v[2], (int)v[3]);
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
    int cx = (int)v[0], cy = (int)v[1], r = (int)v[2];
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
    if (_jgs.total & 1) {
        for (int i = 0; i < _jgs.npulsar; i++) _jgs.teclas[_jgs.pulsar[i]] = 1;
        if (_jgs.hay_clic) { _jgs.rx = _jgs.clic_x; _jgs.ry = _jgs.clic_y; _jgs.raton |= 1; }
    } else if (_jgs.hay_clic) { _jgs.rx = _jgs.clic_x; _jgs.ry = _jgs.clic_y; }
    return valor_nulo();
}

/* Guarda la imagen actual de la ventana como BMP; 1 si va bien */
static int jg_guardar_bmp(const char *ruta) {
    size_t tam = (size_t)_jgs.ancho * _jgs.alto * 4;
    unsigned char *px = malloc(tam);
    if (!px) return 0;
    int ok = 0;
    if (_jg.ReadPixels(_jgs.render, NULL, 0x16362004, px, _jgs.ancho * 4) == 0) {
        void *sup = _jg.SurfaceFrom(px, _jgs.ancho, _jgs.alto, 32, _jgs.ancho * 4,
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

/* pitido(frecuencia, milisegundos[, volumen]): onda cuadrada suavizada, no bloquea */
static Valor *fn_jg_pitido(Valor **a, int n) {
    JG_ABIERTA();
    double f, ms, vol = 0.25;
    if (!jg_num(a, n, 0, &f, "pitido") || !jg_num(a, n, 1, &ms, "pitido")) return valor_nulo();
    if (n > 2 && !jg_num(a, n, 2, &vol, "pitido")) return valor_nulo();
    if (f < 20 || f > 20000 || ms <= 0 || ms > 5000) return jg_error("Error", "pitido(): frecuencia 20-20000 Hz y duración hasta 5000 ms");
    if (vol < 0) vol = 0;
    if (vol > 1) vol = 1;
    if (!_jgs.audio) {
        struct { int freq; unsigned short formato; unsigned char canales, silencio; unsigned short muestras, pad;
                 unsigned tam; void *callback, *datos; } want, have;
        memset(&want, 0, sizeof(want));
        want.freq = 44100; want.formato = 0x8010; want.canales = 1; want.muestras = 1024;   /* S16 */
        _jgs.audio = _jg.OpenAudio(NULL, 0, &want, &have, 0);
        if (!_jgs.audio) return valor_nulo();                 /* sin audio: se ignora en silencio */
        if (have.formato != 0x8010 || have.canales != 1 || have.freq != 44100) {
            _jg.CloseAudio(_jgs.audio); _jgs.audio = 0;
            return valor_nulo();
        }
        _jg.PauseAudio(_jgs.audio, 0);
    }
    if (_jg.QueuedAudio(_jgs.audio) > 44100 * 2) return valor_nulo();   /* ya hay >1 s en cola */
    int total = (int)(44100 * ms / 1000);
    short *buf = malloc((size_t)total * sizeof(short));
    if (!buf) return valor_nulo();
    int fade = total < 441 ? total / 2 : 441;                           /* 10 ms de rampa: sin chasquidos */
    for (int i = 0; i < total; i++) {
        double fase = fmod(i * f / 44100.0, 1.0);
        double s = fase < 0.5 ? 1.0 : -1.0;
        double env = i < fade ? (double)i / fade : (total - i) < fade ? (double)(total - i) / fade : 1.0;
        buf[i] = (short)(s * env * vol * 12000);
    }
    _jg.QueueAudio(_jgs.audio, buf, (unsigned)(total * sizeof(short)));
    free(buf);
    return valor_nulo();
}

/* aleatorio utilidades: colision_rectangulos(x1,y1,w1,h1, x2,y2,w2,h2) */
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
    };
    registrar_modulo_diccionario(entorno, "juego",
        fns, (int)(sizeof(fns) / sizeof(fns[0])), NULL, NULL, 0);
}
