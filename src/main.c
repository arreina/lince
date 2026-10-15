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
    printf("\n🐆 Lince v0.6\n\n");
    printf("Uso:\n");
    printf("  lince <archivo.lince>                    Ejecuta un programa\n");
    printf("  lince compilar <archivo.lince>           Crea un ejecutable autónomo\n");
    printf("  lince compilar <archivo.lince> -o <bin>  Con nombre personalizado\n");
    printf("  lince compilar <a.lince> --incluir <ruta> Incluye niveles, imágenes u otros archivos (varias veces)\n");
    printf("  lince comprobar <archivo.lince>          Comprueba la sintaxis sin ejecutar\n");
    printf("  lince depurar <archivo.lince>            Depura paso a paso\n");
    printf("  lince lsp                                Servidor de lenguaje para editores\n");
    printf("  lince                                    Abre el REPL interactivo\n");
    printf("  lince instalar <url>                     Instala un paquete\n");
    printf("  lince desinstalar <nombre>               Desinstala un paquete\n");
    printf("  lince listar                             Lista paquetes instalados\n");
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
    interprete_ejecutar(interp, programa);

    interprete_destruir(interp);
    nodo_destruir(programa);
    parser_destruir(parser);
    lexer_destruir(lexer);
    return 0;
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
        int num_extras = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
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

        int resultado = empaquetado_crear(archivo, ruta_bin, extras, num_extras);
        if (resultado == 0)
            printf("✅ Compilado correctamente → %s\n\n", ruta_bin);
        return resultado;
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

    char       *codigo   = leer_archivo(archivo);
    Lexer      *lexer    = lexer_crear(codigo);
    int         cantidad = 0;
    Token      *tokens   = lexer_tokenizar(lexer, &cantidad);
    Parser     *parser   = parser_crear(tokens, cantidad);
    Nodo       *programa = parser_parsear(parser);
    Interprete *interp   = interprete_crear();

    /* Para resolver los 'importar "./x"' junto a este fichero */
    interprete_fijar_archivo(archivo);

    /* Argumentos del programa (los que van después del archivo) */
    interprete_set_args(argc - 2, argv + 2);

    interprete_ejecutar(interp, programa);

    interprete_destruir(interp);
    nodo_destruir(programa);
    parser_destruir(parser);
    lexer_destruir(lexer);
    free(codigo);
    return 0;
}


