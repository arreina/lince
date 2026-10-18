/*
 * 🐆 Lince — main.c
 * Punto de entrada: ejecuta archivos, REPL o gestiona paquetes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "interprete.h"
#include "repl.h"
#include "paquetes.h"
#include "empaquetado.h"
#include "depurador.h"
#include "lsp.h"
#include "modulos.h"
#include "recarga.h"
#include "repartir.h"
#include "apk.h"
#include "nuevo.h"
#include "version.h"

static char *leer_archivo(const char *ruta) {
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr,
            "\n❌ Error:\n"
            "   No se encontró el archivo '%s'.\n"
            "   Comprueba que la ruta sea correcta.\n\n", ruta);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    rewind(f);
    char *buf = malloc(tam + 1);
    size_t leido = fread(buf, 1, tam, f);
    buf[leido] = '\0';
    fclose(f);
    return buf;
}

static void mostrar_ayuda(void) {
    printf("\n🐆 Lince " LINCE_VERSION "\n\n");
    printf("Uso:\n");
    printf("  lince nuevo <nombre>                     Crea un juego nuevo listo para cambiarlo\n");
    printf("  lince <archivo.lince>                    Ejecuta un programa\n");
    printf("  lince compilar <archivo.lince>           Crea un ejecutable autónomo\n");
    printf("  lince compilar <archivo.lince> -o <bin>  Con nombre personalizado\n");
    printf("  lince compilar <a.lince> --incluir <ruta> Incluye niveles u otros archivos (varias veces)\n");
    printf("  lince repartir <juego.lince>             Carpeta y .zip para compartir el juego (con SDL2)\n");
    printf("  lince repartir <juego.lince> --android   El .apk para instalarlo en un móvil Android\n");
    printf("  lince repartir <juego.lince> --aab       El .aab para publicarlo en Google Play\n");
    printf("  lince comprobar <archivo.lince>          Comprueba la sintaxis sin ejecutar\n");
    printf("  lince depurar <archivo.lince>            Depura paso a paso\n");
    printf("  lince lsp                                Servidor de lenguaje para editores\n");
    printf("  lince                                    Abre el REPL interactivo\n");
    printf("  lince instalar <url>                     Instala un paquete\n");
    printf("  lince desinstalar <nombre>               Desinstala un paquete\n");
    printf("  lince listar                             Lista paquetes instalados\n");
    printf("  lince version                            Dice qué versión de Lince es\n");
    printf("  lince ayuda                              Muestra esta ayuda\n\n");
    printf("Ejemplos:\n");
    printf("  lince programa.lince\n");
    printf("  lince compilar programa.lince\n");
    printf("  lince compilar programa.lince -o mi_programa\n");
    printf("  lince instalar https://github.com/usuario/paquete\n\n");
}

/* Ejecuta el programa que lleva dentro un ejecutable creado con 'lince compilar' */
static int ejecutar_empaquetado(int argc, char *argv[]) {
    const char *ruta   = empaquetado_ruta_principal();
    const char *codigo = empaquetado_codigo_principal();
    interprete_fijar_lector(empaquetado_buscar);

    Lexer      *lexer    = lexer_crear(codigo);
    int         cantidad = 0;
    Token      *tokens   = lexer_tokenizar(lexer, &cantidad);
    Parser     *parser   = parser_crear(tokens, cantidad);
    Nodo       *programa = parser_parsear(parser);
    Interprete *interp   = interprete_crear();

    interprete_fijar_archivo(ruta);
    interprete_set_args(argc - 1, argv + 1);   /* todos los argumentos son del programa */
    char mensaje[700] = "";
    if (interprete_ejecutar_recargable(interp, programa, mensaje, sizeof mensaje) != 0) {
        /* Un juego no se cierra sin más: el error se queda en la ventana
           hasta que se cierre (en el móvil no hay otro sitio donde verlo) */
        if (juego_ventana_abierta()) juego_esperar_arreglo(mensaje);
        exit(1);
    }

    interprete_destruir(interp);
    nodo_destruir(programa);
    parser_destruir(parser);
    lexer_destruir(lexer);
    return 0;
}

/* Ejecuta un programa .lince con recarga en caliente: si es un juego y se
   guarda un cambio mientras corre, juego.presentar() lo corta y aquí se
   vuelve a lanzar el código nuevo con la misma ventana, conservando las
   variables 'sea' del nivel superior (o desde cero, con F5). Si el programa
   se para por un error con la ventana abierta, el error se muestra en ella
   hasta que se guarde un arreglo. Sin ventana, todo es como siempre. */
static void avisar(int linea, const char *mensaje) {
    fprintf(stderr, "⚠️  Aviso en línea %d: %s\n", linea, mensaje);
}

static int ejecutar_recargable(const char *archivo) {
    recarga_iniciar(archivo);
    Interprete *anterior = NULL;
    for (;;) {
        /* El AST no se libera: las funciones conservadas de una versión
           anterior lo siguen usando. */
        char       *codigo   = leer_archivo(archivo);
        Lexer      *lexer    = lexer_crear(codigo);
        int         cantidad = 0;
        Token      *tokens   = lexer_tokenizar(lexer, &cantidad);
        Parser     *parser   = parser_crear(tokens, cantidad);
        Nodo       *programa = parser_parsear(parser);
        Interprete *interp   = interprete_crear();
        lsp_conocer_importados(parser, codigo, archivo);
        parser_avisos(parser, avisar);

        /* Para resolver los 'importar "./x"' junto a este fichero */
        interprete_fijar_archivo(archivo);
        interprete_conservar(anterior ? anterior->global : NULL, interp);
        char mensaje[700] = "";
        int r = interprete_ejecutar_recargable(interp, programa, mensaje, sizeof mensaje);
        interprete_conservar(NULL, NULL);
        if (r == 1) {
            /* Error: con la ventana abierta se espera a que se arregle */
            if (!juego_ventana_abierta() || !juego_esperar_arreglo(mensaje)) exit(1);
            r = 2;
        }
        if (r == 0) {
            interprete_destruir(interp);
            if (anterior) interprete_destruir(anterior);
            return 0;
        }
        /* Recargar: el que acaba de correr pasa a ser «el anterior» */
        if (anterior) interprete_destruir(anterior);
        anterior = interp;
        if (recarga_completa()) { interprete_destruir(anterior); anterior = NULL; }
        interprete_olvidar_importados();
        juego_preparar_recarga(anterior != NULL);
        printf("🔄 Recargado %s%s\n", archivo, anterior ? "" : " (desde cero)");
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    /* ¿Es un ejecutable con un programa dentro? */
    if (empaquetado_cargar(argv[0]))
        return ejecutar_empaquetado(argc, argv);

    /* Sin argumentos — REPL */
    if (argc < 2) {
        repl_iniciar();
        return 0;
    }

    const char *cmd = argv[1];

    /* Servidor de lenguaje para editores: 'lince lsp' */
    if (strcmp(cmd, "lsp") == 0) return lsp_servir();

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "versión") == 0 || strcmp(cmd, "--version") == 0) {
        printf("Lince %s\n", LINCE_VERSION);
        return 0;
    }

    /* Depurador interactivo: 'lince depurar archivo.lince [args]' */
    if (strcmp(cmd, "depurar") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\n❌ Uso: lince depurar <archivo.lince> [argumentos]\n\n");
            return 1;
        }
        return depurador_ejecutar(argv[2], argc - 3, argv + 3);
    }

    /* Comprobar la sintaxis sin ejecutar: 'lince comprobar archivo.lince' */
    if (strcmp(cmd, "comprobar") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\n❌ Uso: lince comprobar <archivo.lince>\n\n");
            return 1;
        }
        char   *codigo = leer_archivo(argv[2]);
        /* El editor puede pasar la ruta de verdad si comprueba una copia */
        lsp_fijar_ruta(argc > 3 ? argv[3] : argv[2]);
        lsp_analizar(codigo);            /* termina el proceso si hay un error */
        printf("✅ Sintaxis correcta: %s\n", argv[2]);
        return 0;
    }

    /* Subcomando compilar: empaqueta el programa en un ejecutable */
    if (strcmp(cmd, "compilar") == 0) {
        if (argc < 3) {
            fprintf(stderr,
                "\n❌ Uso: lince compilar <archivo.lince> [-o nombre]\n\n");
            return 1;
        }
        const char *archivo = argv[2];
        const char *ext = strrchr(archivo, '.');
        if (!ext || strcmp(ext, ".lince") != 0) {
            fprintf(stderr,
                "\n❌ El archivo '%s' no tiene la extensión '.lince'.\n\n", archivo);
            return 1;
        }

        /* Opciones: -o nombre, --incluir ruta (varias) */
        char ruta_bin[512] = "";
        const char *extras[64];
        int num_extras = 0, solo_datos = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--datos") == 0) {
                solo_datos = 1;          /* sin el intérprete: lo que va dentro de un .apk */
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                snprintf(ruta_bin, sizeof(ruta_bin), "%s", argv[++i]);
            } else if (strcmp(argv[i], "--incluir") == 0 && i + 1 < argc) {
                if (num_extras < 64) extras[num_extras++] = argv[++i];
                else i++;
            } else {
                fprintf(stderr, "\n❌ Opción desconocida: %s\n"
                    "   Uso: lince compilar <archivo.lince> [-o nombre] [--incluir carpeta_o_archivo]...\n\n", argv[i]);
                return 1;
            }
        }
        if (!ruta_bin[0]) {
            /* por defecto el nombre del archivo sin .lince */
            snprintf(ruta_bin, sizeof(ruta_bin), "%s", archivo);
            char *punto = strrchr(ruta_bin, '.');
            if (punto) *punto = '\0';
        }
#ifdef _WIN32
        {   /* en Windows un ejecutable necesita la extensión .exe */
            size_t lr = strlen(ruta_bin);
            if (lr < 4 || _stricmp(ruta_bin + lr - 4, ".exe") != 0)
                strncat(ruta_bin, ".exe", sizeof(ruta_bin) - lr - 1);
        }
#endif

        printf("\n🐆 Lince — Compilador\n\n");
        printf("   Fuente  : %s\n", archivo);
        printf("   Salida  : %s\n\n", ruta_bin);

        int resultado = solo_datos ? empaquetado_crear_datos(archivo, ruta_bin, extras, num_extras)
                                   : empaquetado_crear(archivo, ruta_bin, extras, num_extras);
        if (resultado == 0)
            printf("✅ Compilado correctamente → %s\n\n", ruta_bin);
        return resultado;
    }

    /* Juego nuevo a partir de una plantilla */
    if (strcmp(cmd, "nuevo") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\n❌ Uso: lince nuevo <nombre>   (por ejemplo: lince nuevo mi_juego)\n\n");
            return 1;
        }
        return nuevo_crear(argv[2]);
    }

    /* Repartir: carpeta y .zip con el juego y SDL2, listos para compartir */
    if (strcmp(cmd, "repartir") == 0) {
        const char *uso = "   Uso: lince repartir <juego.lince> [-o carpeta] [--incluir carpeta_o_archivo]...\n"
                          "        lince repartir <juego.lince> --android [--aab] [-o juego.apk] [--nombre \"Mi juego\"]\n"
                          "                       [--id org.lince.mijuego] [--version 1]\n\n";
        if (argc < 3) { fprintf(stderr, "\n❌ Falta el juego.\n%s", uso); return 1; }
        const char *ext = strrchr(argv[2], '.');
        if (!ext || strcmp(ext, ".lince") != 0) {
            fprintf(stderr, "\n❌ El archivo '%s' no tiene la extensión '.lince'.\n\n", argv[2]);
            return 1;
        }
        const char *carpeta = NULL, *nombre = NULL, *id = NULL;
        const char *extras[64];
        int num_extras = 0, android = 0, aab = 0, version = 1;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) carpeta = argv[++i];
            else if (strcmp(argv[i], "--android") == 0) android = 1;
            else if (strcmp(argv[i], "--aab") == 0) android = aab = 1;
            else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) {
                char *fin;
                long v = strtol(argv[++i], &fin, 10);
                if (*fin || v < 1 || v > 2100000000) {
                    fprintf(stderr, "\n❌ --version es un número entero, 1 o más (súbelo cada vez que publiques el juego).\n\n");
                    return 1;
                }
                version = (int)v;
            }
            else if (strcmp(argv[i], "--nombre") == 0 && i + 1 < argc) nombre = argv[++i];
            else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) id = argv[++i];
            else if (strcmp(argv[i], "--incluir") == 0 && i + 1 < argc) {
                if (num_extras < 64) extras[num_extras++] = argv[++i];
                else i++;
            } else {
                fprintf(stderr, "\n❌ Opción desconocida: %s\n%s", argv[i], uso);
                return 1;
            }
        }
        if (android) {
            /* un .apk para Android (o el .aab de Google Play), sin Java ni el SDK (ver apk.c) */
            char base[256], apk[512], id_auto[300];
            const char *b = strrchr(argv[2], '/');
#ifdef _WIN32
            const char *b2 = strrchr(argv[2], '\\');
            if (!b || (b2 && b2 > b)) b = b2;
#endif
            snprintf(base, sizeof base, "%s", b ? b + 1 : argv[2]);
            base[strlen(base) - 6] = '\0';                      /* sin ".lince" */
            if (!carpeta) { snprintf(apk, sizeof apk, "%s.%s", base, aab ? "aab" : "apk"); carpeta = apk; }
            if (!id) {                         /* 01_pong → org.lince.pong: cada parte empieza por letra */
                size_t k = 0;
                char parte[200];
                for (const char *c = base; *c && k < sizeof parte - 1; c++)
                    if ((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z'))
                        parte[k++] = (char)(*c >= 'A' && *c <= 'Z' ? *c + 32 : *c);
                parte[k] = '\0';
                const char *p = parte;
                while (*p >= '0' && *p <= '9') p++;
                snprintf(id_auto, sizeof id_auto, "org.lince.%s", *p ? p : "juego");
                id = id_auto;
            }
            int bien = id[0] >= 'a' && id[0] <= 'z' && strchr(id, '.') != NULL;
            for (const char *c = id; *c && bien; c++)
                bien = (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' ||
                       (*c == '.' && c[1] >= 'a' && c[1] <= 'z');
            if (!bien) { fprintf(stderr, "\n❌ El --id es como org.lince.mijuego (minúsculas, cada parte empieza por letra).\n\n"); return 1; }
            const char *nombre_auto = base;                /* 01_pong → pong */
            while ((*nombre_auto >= '0' && *nombre_auto <= '9') || *nombre_auto == '_') nombre_auto++;
            return apk_crear(argv[2], carpeta, nombre ? nombre : (*nombre_auto ? nombre_auto : base), id, version, aab,
                             extras, num_extras);
        }
        return repartir_crear(argv[2], carpeta, extras, num_extras);
    }

    /* Subcomandos del gestor de paquetes */
    if (strcmp(cmd, "instalar") == 0) {
        if (argc < 3) {
            fprintf(stderr,
                "\n❌ Uso: lince instalar <url>\n"
                "   Ejemplo: lince instalar https://github.com/usuario/paquete\n\n");
            return 1;
        }
        return paquetes_instalar(argv[2]);
    }

    if (strcmp(cmd, "desinstalar") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\n❌ Uso: lince desinstalar <nombre>\n\n");
            return 1;
        }
        return paquetes_desinstalar(argv[2]);
    }

    if (strcmp(cmd, "listar") == 0) {
        return paquetes_listar();
    }

    if (strcmp(cmd, "ayuda") == 0 || strcmp(cmd, "--ayuda") == 0 ||
        strcmp(cmd, "-a") == 0    || strcmp(cmd, "--help") == 0) {
        mostrar_ayuda();
        return 0;
    }

    /* Ejecutar archivo .lince */
    const char *archivo = argv[1];
    const char *ext = strrchr(archivo, '.');
    if (!ext || strcmp(ext, ".lince") != 0) {
        fprintf(stderr,
            "\n❌ Error:\n"
            "   El archivo '%s' no tiene la extensión '.lince'.\n"
            "   Usa 'lince ayuda' para ver los comandos disponibles.\n\n",
            archivo);
        return 1;
    }

    /* Argumentos del programa (los que van después del archivo) */
    interprete_set_args(argc - 2, argv + 2);
    return ejecutar_recargable(archivo);
}



#ifdef __ANDROID__
#include <unistd.h>
/* En Android, SDLActivity arranca el juego llamando a SDL_main (ver android.c) */
int android_preparar(void);
__attribute__((visibility("default"))) int SDL_main(int argc, char *argv[]) {
    if (!android_preparar()) return 1;
    /* Al acabar se cierra el proceso: si no, Android lo guarda y al volver a
       abrir la app Lince empezaría con lo que dejó el juego anterior */
    int r = ejecutar_empaquetado(argc, argv);
    fflush(NULL);
    _exit(r);       /* sin destructores: otros hilos de Android aún los usan */
}
#endif
