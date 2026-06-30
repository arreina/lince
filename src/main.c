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
#include "compilador.h"

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
    printf("\n🐆 Lince v0.5\n\n");
    printf("Uso:\n");
    printf("  lince <archivo.lince>                    Ejecuta un programa\n");
    printf("  lince compilar <archivo.lince>           Compila a binario nativo\n");
    printf("  lince compilar <archivo.lince> -o <bin>  Con nombre personalizado\n");
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

int main(int argc, char *argv[]) {
    /* Sin argumentos — REPL */
    if (argc < 2) {
        repl_iniciar();
        return 0;
    }

    const char *cmd = argv[1];

    /* Subcomando compilar */
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

        /* Nombre del binario — por defecto el nombre del archivo sin .lince */
        char ruta_bin[512];
        char ruta_c[512];
        if (argc >= 5 && strcmp(argv[3], "-o") == 0) {
            strncpy(ruta_bin, argv[4], sizeof(ruta_bin) - 1);
        } else {
            /* Quitar .lince del nombre */
            strncpy(ruta_bin, archivo, sizeof(ruta_bin) - 1);
            char *punto = strrchr(ruta_bin, '.');
            if (punto) *punto = '\0';
        }
        snprintf(ruta_c, sizeof(ruta_c), "%s.c", ruta_bin);

        printf("\n🐆 Lince — Compilador\n\n");
        printf("   Fuente  : %s\n", archivo);
        printf("   Salida  : %s\n", ruta_bin);
        printf("   C gen.  : %s\n\n", ruta_c);

        char       *codigo   = leer_archivo(archivo);
        Lexer      *lexer    = lexer_crear(codigo);
        int         cantidad = 0;
        Token      *tokens   = lexer_tokenizar(lexer, &cantidad);
        Parser     *parser   = parser_crear(tokens, cantidad);
        Nodo       *programa = parser_parsear(parser);

        int resultado = compilador_compilar(programa, ruta_c, ruta_bin);

        if (resultado == 0) {
            printf("✅ Compilado correctamente → %s\n\n", ruta_bin);
        }

        nodo_destruir(programa);
        parser_destruir(parser);
        lexer_destruir(lexer);
        free(codigo);
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

    /* Pasar argumentos al intérprete (sin el nombre del archivo) */
    interprete_set_args(argc - 2, argv + 2);

    /* Pasar argumentos del programa (los que van después del archivo) */
    interprete_set_args(argc - 2, argv + 2);

    interprete_ejecutar(interp, programa);

    interprete_destruir(interp);
    nodo_destruir(programa);
    parser_destruir(parser);
    lexer_destruir(lexer);
    free(codigo);
    return 0;
}


