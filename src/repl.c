/*
 * 🐆 Lince — repl.c
 * Bucle interactivo: lee, evalúa e imprime resultados.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "interprete.h"
#include "modulos.h"
#include "plataforma.h"

#define VERSION     "0.1"
#define MAX_LINEA   4096
#define HISTORIAL   64

/* ─────────────────────────────────────────
   COLORES ANSI
───────────────────────────────────────── */
static int usar_colores = 0;

static const char *C(const char *codigo) {
    return usar_colores ? codigo : "";
}

#define RESET    "\033[0m"
#define VERDE    "\033[32m"
#define ROJO     "\033[31m"
#define AMARILLO "\033[33m"
#define CYAN     "\033[36m"
#define GRIS     "\033[90m"
#define NEGRITA  "\033[1m"

/* ─────────────────────────────────────────
   HISTORIAL DE COMANDOS
───────────────────────────────────────── */
static char *historial[HISTORIAL];
static int   hist_cantidad = 0;

static void historial_agregar(const char *linea) {
    if (strlen(linea) == 0) return;
    /* No duplicar la última entrada */
    if (hist_cantidad > 0 &&
        strcmp(historial[hist_cantidad-1], linea) == 0) return;

    if (hist_cantidad >= HISTORIAL) {
        free(historial[0]);
        memmove(historial, historial + 1,
                sizeof(char*) * (HISTORIAL - 1));
        hist_cantidad--;
    }
    historial[hist_cantidad++] = strdup(linea);
}

/* ─────────────────────────────────────────
   COMANDOS ESPECIALES DEL REPL
───────────────────────────────────────── */
static void mostrar_ayuda(void) {
    fprintf(stdout, "\n%s%sComandos del REPL:%s\n", C(CYAN), C(NEGRITA), C(RESET));
    fprintf(stdout, "  %s:ayuda%s        — muestra esta ayuda\n",       C(AMARILLO), C(RESET));
    fprintf(stdout, "  %s:salir%s        — cierra el REPL\n",           C(AMARILLO), C(RESET));
    fprintf(stdout, "  %s:limpiar%s      — limpia la pantalla\n",       C(AMARILLO), C(RESET));
    fprintf(stdout, "  %s:historial%s    — muestra el historial\n",     C(AMARILLO), C(RESET));
    fprintf(stdout, "  %s:modulos%s      — muestra los módulos\n",      C(AMARILLO), C(RESET));
    fprintf(stdout, "  %s:cargar f.lince%s — ejecuta un archivo\n",    C(AMARILLO), C(RESET));
    fprintf(stdout, "\n%s%sEjemplos rápidos:%s\n", C(CYAN), C(NEGRITA), C(RESET));
    fprintf(stdout, "  %s>>%s sea x = 42\n",                    C(GRIS), C(RESET));
    fprintf(stdout, "  %s>>%s escribir(\"Hola, Mundo!\")\n",    C(GRIS), C(RESET));
    fprintf(stdout, "  %s>>%s importar \"matematica\"\n",       C(GRIS), C(RESET));
    fprintf(stdout, "  %s>>%s matematica.raiz(16)\n\n",         C(GRIS), C(RESET));
}

static void mostrar_modulos(void) {
    fprintf(stdout, "\n%s%sMódulos disponibles:%s\n", C(CYAN), C(NEGRITA), C(RESET));
    fprintf(stdout, "  %smatematica%s — raiz, potencia, abs, piso, techo,\n"
                    "               redondear, seno, coseno, tangente,\n"
                    "               logaritmo, maximo, minimo, aleatorio,\n"
                    "               PI, E, TAU\n", C(AMARILLO), C(RESET));
    fprintf(stdout, "  %stexto%s      — dividir, unir, repetir, invertir,\n"
                    "               formato, a_numero, de_numero, a_lista,\n"
                    "               posicion, contar, extraer, es_numero,\n"
                    "               es_letra, es_vacio, empieza_con,\n"
                    "               termina_con, rellenar_izq, rellenar_der,\n"
                    "               centrar\n", C(AMARILLO), C(RESET));
    fprintf(stdout, "  %sarchivos%s   — leer, escribir, agregar, existe\n",
            C(AMARILLO), C(RESET));
    fprintf(stdout, "  %stiempo%s     — ahora, esperar, fecha, hora\n\n",
            C(AMARILLO), C(RESET));
}

static void mostrar_historial(void) {
    if (hist_cantidad == 0) {
        fprintf(stdout, "%s  (historial vacío)\n%s", C(GRIS), C(RESET));
        return;
    }
    fprintf(stdout, "\n%s%sHistorial:%s\n", C(CYAN), C(NEGRITA), C(RESET));
    for (int i = 0; i < hist_cantidad; i++)
        fprintf(stdout, "  %s%2d%s  %s\n", C(GRIS), i + 1, C(RESET), historial[i]);
    fprintf(stdout, "\n");
}

/* ─────────────────────────────────────────
   AUTODETECCIÓN DE BLOQUE MULTILÍNEA
   Detecta si la línea abre llaves sin cerrar
───────────────────────────────────────── */
static int contar_llaves(const char *linea) {
    int balance = 0;
    int en_texto = 0;
    for (int i = 0; linea[i]; i++) {
        if (linea[i] == '"') { en_texto = !en_texto; continue; }
        if (en_texto) continue;
        if (linea[i] == '{') balance++;
        if (linea[i] == '}') balance--;
        if (linea[i] == '#') break; /* comentario */
    }
    return balance;
}

/* ─────────────────────────────────────────
   EVALUACIÓN DE UNA LÍNEA
───────────────────────────────────────── */
static void evaluar(const char *codigo, Interprete *interp) {
    /* Suprimir errores de compilación para mostrarlos bonito */
    Lexer  *lexer  = lexer_crear(codigo);
    int     cant   = 0;
    Token  *tokens = lexer_tokenizar(lexer, &cant);
    Parser *parser = parser_crear(tokens, cant);
    Nodo   *ast    = parser_parsear(parser);

    if (!hay_error) {
        /* Ejecutar y mostrar resultado si es una expresión */
        Nodo *prog = ast;
        if (prog->bloque.cantidad == 1) {
            Nodo *sent = prog->bloque.sentencias[0];
            /* Si es una expresión pura (no declaración ni control),
               mostramos su valor */
            int es_expr = (sent->tipo == NODO_NUMERO     ||
                           sent->tipo == NODO_TEXTO      ||
                           sent->tipo == NODO_BOOLEANO   ||
                           sent->tipo == NODO_NULO       ||
                           sent->tipo == NODO_IDENTIFICADOR ||
                           sent->tipo == NODO_BINARIO    ||
                           sent->tipo == NODO_LLAMADA    ||
                           sent->tipo == NODO_METODO     ||
                           sent->tipo == NODO_INDICE     ||
                           sent->tipo == NODO_ACCESO     ||
                           sent->tipo == NODO_INSTANCIA);
            if (es_expr) {
                Valor *v = ejecutar_nodo(sent, interp->global);
                if (v && v->tipo != VAL_NULO) {
                    char *s = valor_a_texto_repl(v);
                    fprintf(stdout, "%s=> %s%s\n", C(VERDE), C(RESET), s);
                    free(s);
                }
                valor_destruir(v);
                goto fin;
            }
        }
        interprete_ejecutar(interp, ast);
    }

fin:
    if (hay_error && valor_error) {
        if (valor_error->error->linea > 0)
            fprintf(stdout, "%s❌ %s en línea %d: %s%s\n",
                   C(ROJO),
                   valor_error->error->tipo,
                   valor_error->error->linea,
                   valor_error->error->mensaje,
                   C(RESET));
        else
            fprintf(stdout, "%s❌ %s: %s%s\n",
                   C(ROJO),
                   valor_error->error->tipo,
                   valor_error->error->mensaje,
                   C(RESET));
        valor_destruir(valor_error);
        valor_error = NULL;
        hay_error   = 0;
    }

    nodo_destruir(ast);
    parser_destruir(parser);
    lexer_destruir(lexer);
}

/* ─────────────────────────────────────────
   LECTURA DE LÍNEA CON HISTORIAL
   Soporta flechas arriba/abajo, izq/der,
   inicio/fin, borrado — en Linux/Mac/Windows
───────────────────────────────────────── */
#ifdef LINCE_WINDOWS
#include <conio.h>
static void terminal_raw(int activo) { (void)activo; }
static int  leer_tecla(void) { return _getch(); }
#else
#include <termios.h>
#include <unistd.h>
static struct termios term_orig;
static void terminal_raw(int activo) {
    if (activo) {
        struct termios raw = term_orig;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    } else {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_orig);
    }
}
static int leer_tecla(void) { return getchar(); }
#endif

/* Lee una línea con edición y navegación de historial */
static int leer_linea(char *buf, int max, const char *prompt) {
    #ifndef LINCE_WINDOWS
    if (!isatty(STDIN_FILENO)) {
        /* stdin no es terminal — lectura simple */
        fprintf(stdout, "%s", prompt); fflush(stdout);
        if (!fgets(buf, max, stdin)) return 0;
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        return 1;
    }
    tcgetattr(STDIN_FILENO, &term_orig);
    #endif

    fprintf(stdout, "%s", prompt); fflush(stdout);

    char tmp[4096];
    int  pos     = 0;   /* posición del cursor en tmp */
    int  len     = 0;   /* longitud actual de tmp */
    int  hist_pos = hist_cantidad; /* navegación historial */
    tmp[0] = '\0';

    terminal_raw(1);

    while (1) {
        int c = leer_tecla();

        /* Enter */
        if (c == '\n' || c == '\r') {
            fprintf(stdout, "\n");
            break;
        }

        /* Ctrl+C / Ctrl+D */
        if (c == 3 || c == 4) {
            tmp[0] = '\0'; len = 0;
            fprintf(stdout, "\n");
            if (c == 4 && len == 0) {
                terminal_raw(0);
                buf[0] = '\0';
                return 0; /* EOF */
            }
            break;
        }

        /* Backspace */
        if (c == 127 || c == 8) {
            if (pos > 0) {
                memmove(tmp + pos - 1, tmp + pos, len - pos);
                pos--; len--;
                tmp[len] = '\0';
                /* Redibujar */
                fprintf(stdout, "\r%s%*s\r%s", prompt, len+1, "", prompt);
                fwrite(tmp, 1, len, stdout);
                /* Reposicionar cursor */
                if (pos < len) {
                    fprintf(stdout, "\033[%dD", len - pos);
                }
                fflush(stdout);
            }
            continue;
        }

        /* Secuencias de escape (flechas, etc.) */
        if (c == 27) {
            int c2 = leer_tecla();
            if (c2 == '[') {
                int c3 = leer_tecla();

                /* Flecha arriba — historial anterior */
                if (c3 == 'A') {
                    if (hist_pos > 0) {
                        hist_pos--;
                        strncpy(tmp, historial[hist_pos], max - 1);
                        len = pos = strlen(tmp);
                        fprintf(stdout, "\r%s%-*s\r%s%s",
                            prompt, len+1, "", prompt, tmp);
                        fflush(stdout);
                    }
                    continue;
                }

                /* Flecha abajo — historial siguiente */
                if (c3 == 'B') {
                    if (hist_pos < hist_cantidad) {
                        hist_pos++;
                        if (hist_pos == hist_cantidad) {
                            tmp[0] = '\0'; len = pos = 0;
                        } else {
                            strncpy(tmp, historial[hist_pos], max - 1);
                            len = pos = strlen(tmp);
                        }
                        fprintf(stdout, "\r%s%-*s\r%s%s",
                            prompt, len+1, "", prompt, tmp);
                        fflush(stdout);
                    }
                    continue;
                }

                /* Flecha izquierda */
                if (c3 == 'D') {
                    if (pos > 0) { pos--; fprintf(stdout, "\033[1D"); fflush(stdout); }
                    continue;
                }

                /* Flecha derecha */
                if (c3 == 'C') {
                    if (pos < len) { pos++; fprintf(stdout, "\033[1C"); fflush(stdout); }
                    continue;
                }

                /* Inicio (Home) */
                if (c3 == 'H' || c3 == '1') {
                    if (c3 == '1') leer_tecla(); /* consumir ~ */
                    if (pos > 0) { fprintf(stdout, "\033[%dD", pos); pos = 0; fflush(stdout); }
                    continue;
                }

                /* Fin (End) */
                if (c3 == 'F' || c3 == '4') {
                    if (c3 == '4') leer_tecla(); /* consumir ~ */
                    if (pos < len) { fprintf(stdout, "\033[%dC", len - pos); pos = len; fflush(stdout); }
                    continue;
                }

                /* Supr (Delete) */
                if (c3 == '3') {
                    leer_tecla(); /* consumir ~ */
                    if (pos < len) {
                        memmove(tmp + pos, tmp + pos + 1, len - pos);
                        len--; tmp[len] = '\0';
                        fprintf(stdout, "\r%s%*s\r%s", prompt, len+2, "", prompt);
                        fwrite(tmp, 1, len, stdout);
                        if (pos < len) fprintf(stdout, "\033[%dD", len - pos);
                        fflush(stdout);
                    }
                    continue;
                }
            }
            continue;
        }

        /* Carácter normal imprimible */
        if (c >= 32 && len < max - 1) {
            memmove(tmp + pos + 1, tmp + pos, len - pos);
            tmp[pos] = (char)c;
            pos++; len++;
            tmp[len] = '\0';

            /* Redibujar desde posición actual */
            fwrite(tmp + pos - 1, 1, len - pos + 1, stdout);
            if (pos < len) fprintf(stdout, "\033[%dD", len - pos);
            fflush(stdout);
        }
    }

    terminal_raw(0);
    strncpy(buf, tmp, max - 1);
    buf[max - 1] = '\0';
    return 1;
}
void repl_iniciar(void) {
    usar_colores = lince_colores_disponibles();

    printf("%s%s\n🐆 Lince v" VERSION "%s"
           " — Escribe %s:ayuda%s para ver los comandos\n\n",
           C(CYAN), C(NEGRITA), C(RESET), C(AMARILLO), C(RESET));

    Interprete *interp = interprete_crear();
    char        linea[MAX_LINEA];
    char        bloque[MAX_LINEA * 16];
    int         balance = 0;
    bloque[0] = '\0';

    while (1) {
        /* Construir prompt */
        char prompt[128];
        if (balance == 0)
            snprintf(prompt, sizeof(prompt), "%s>> %s", C(CYAN), C(RESET));
        else
            snprintf(prompt, sizeof(prompt), "%s.. %s", C(GRIS), C(RESET));

        /* Leer línea con historial interactivo */
        if (!leer_linea(linea, sizeof(linea), prompt)) {
            fprintf(stdout, "%s¡Hasta luego!%s\n\n", C(GRIS), C(RESET));
            break;
        }

        size_t len = strlen(linea);

        /* Ignorar líneas vacías fuera de bloque */
        if (len == 0 && balance == 0) continue;

        /* ── Comandos especiales ── */
        if (balance == 0) {
            if (strcmp(linea, ":salir") == 0 ||
                strcmp(linea, ":exit")  == 0 ||
                strcmp(linea, ":q")     == 0) {
                fprintf(stdout, "%s¡Hasta luego!%s\n\n", C(GRIS), C(RESET));
                break;
            }
            if (strcmp(linea, ":ayuda") == 0 ||
                strcmp(linea, ":help")  == 0) {
                mostrar_ayuda();
                continue;
            }
            if (strcmp(linea, ":limpiar") == 0 ||
                strcmp(linea, ":clear")   == 0) {
                lince_limpiar_pantalla();
                continue;
            }
            if (strcmp(linea, ":historial") == 0) {
                mostrar_historial();
                continue;
            }
            if (strcmp(linea, ":modulos") == 0) {
                mostrar_modulos();
                continue;
            }
            if (strncmp(linea, ":cargar ", 8) == 0) {
                const char *archivo = linea + 8;
                FILE *f = fopen(archivo, "r");
                if (!f) {
                    fprintf(stdout, "%s❌ No se encontró '%s'\n%s", C(ROJO), archivo, C(RESET));
                    continue;
                }
                fseek(f, 0, SEEK_END);
                long tam = ftell(f);
                rewind(f);
                char *buf = malloc(tam + 1);
                size_t leido = fread(buf, 1, tam, f);
                buf[leido] = '\0';
                fclose(f);
                fprintf(stdout, "%sCargando '%s'...\n%s", C(GRIS), archivo, C(RESET));
                evaluar(buf, interp);
                free(buf);
                continue;
            }
        }

        /* ── Acumulación de bloques multilínea ── */
        historial_agregar(linea);
        balance += contar_llaves(linea);

        if (balance == 0 && strlen(bloque) == 0) {
            /* Línea simple — evaluar directamente */
            evaluar(linea, interp);
        } else {
            /* Acumular en bloque */
            strncat(bloque, linea, sizeof(bloque) - strlen(bloque) - 2);
            strncat(bloque, "\n",  sizeof(bloque) - strlen(bloque) - 1);

            if (balance <= 0) {
                /* Bloque completo — evaluar */
                balance = 0;
                evaluar(bloque, interp);
                bloque[0] = '\0';
            }
        }
    }

    interprete_destruir(interp);
}
