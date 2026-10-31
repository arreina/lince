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
#include "escritorio.h"
#include "apk.h"
#include "nuevo.h"
#include "formatear.h"
#include "documentar.h"
#include "probar.h"
#include "migrar.h"
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
    printf("  lince repartir <juego.lince>             Carpeta y .zip (para itch.io), y el instalador de Windows\n");
    printf("                                           o el archivo único de Linux, con su icono y su nombre\n");
    printf("  lince repartir <juego.lince> --android   El .apk para instalarlo en un móvil Android\n");
    printf("  lince repartir <juego.lince> --aab       El .aab para publicarlo en Google Play\n");
    printf("  lince comprobar <archivo.lince>          Sin ejecutar: la sintaxis, y avisos de descuidos\n");
    printf("  lince probar [carpeta] [--aceptar]       Las pruebas (prueba_*.lince) y su cobertura\n");
    printf("  lince formatear <archivo.lince>...       Deja el código con el estilo de Lince\n");
    printf("  lince migrar [--comprobar] <archivo o carpeta>...  Pasa el código a la forma de la 2.0\n");
    printf("  lince perfil <archivo.lince>             Lo ejecuta y dice en qué se va el tiempo\n");
    printf("  lince documentar <archivo.lince> [-o a.md] La ayuda de sus comentarios, en Markdown\n");
    printf("  lince depurar <archivo.lince>            Depura paso a paso\n");
    printf("  lince lsp                                Servidor de lenguaje para editores\n");
    printf("  lince                                    Abre el REPL interactivo\n");
    printf("  lince instalar <url>                     Instala un paquete\n");
    printf("  lince instalar <url>@<versión>           Lo instala para este juego, con esa versión fija\n");
    printf("  lince instalar                           Instala los paquetes de lince.paquetes\n");
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
    int al_salir = 0;
    if (escritorio_arrancar(argc, argv, &al_salir)) return al_salir;   /* el instalador, o --desinstalar */
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
    interprete_fijar_principal(ruta);
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
        interprete_fijar_principal(archivo);
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

    /* Compilar y repartir cuentan lo que van haciendo y, si algo falla, lo
       dicen por stderr: sin búfer, para que con una tubería salga en orden */
    if (strcmp(cmd, "compilar") == 0 || strcmp(cmd, "repartir") == 0)
        setvbuf(stdout, NULL, _IONBF, 0);

    /* Servidor de lenguaje para editores: 'lince lsp' */
    if (strcmp(cmd, "lsp") == 0) return lsp_servir();

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "versión") == 0 || strcmp(cmd, "--version") == 0) {
        printf("Lince %s\n", LINCE_VERSION);
        return 0;
    }

    /* Depurador interactivo: 'lince depurar [--ide] archivo.lince [args]' */
    if (strcmp(cmd, "depurar") == 0) {
        int ide = argc >= 3 && strcmp(argv[2], "--ide") == 0;
        if (argc < 3 + ide) {
            fprintf(stderr, "\n❌ Uso: lince depurar [--ide] <archivo.lince> [argumentos]\n\n");
            return 1;
        }
        return depurador_ejecutar(argv[2 + ide], argc - 3 - ide, argv + 3 + ide, ide);
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

    /* 'lince migrar [--comprobar] archivos o carpetas': lo que la 2.0 cambia, a
       su forma nueva. Con --comprobar dice qué cambiaría y sale con 1 si hay
       algo (para la integración continua), sin tocar nada. */
    if (strcmp(cmd, "migrar") == 0) {
        int comprobar = 0, mostrar = 0, rutas = 0, pendientes = 0, migrados = 0, total = 0, a_mano = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--comprobar") == 0) comprobar = 1;
            else if (strcmp(argv[i], "--mostrar") == 0) mostrar = 1;
            else rutas++;
        }
        if (mostrar && rutas == 1) {        /* el resultado, a la salida (también trozos sueltos, para la documentación) */
            for (int i = 2; i < argc; i++) {
                if (argv[i][0] == '-' && argv[i][1] == '-') continue;
                char *codigo = leer_archivo(argv[i]);
                char *nuevo = migrar_codigo(codigo, NULL);
                fputs(nuevo, stdout);
                free(nuevo);
                free(codigo);
            }
            return 0;
        }
        if (!rutas) {
            fprintf(stderr, "\n❌ Uso: lince migrar [--comprobar | --mostrar] <archivo o carpeta>...\n"
                            "   Pasa el código de la 1.x a la forma de la 2.0. Con --comprobar\n"
                            "   solo dice qué cambiaría, sin tocar nada.\n\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] == '-') continue;
            char **archivos = NULL;
            int n = migrar_buscar(argv[i], &archivos);
            for (int k = 0; k < n; k++) {
                char *codigo = leer_archivo(archivos[k]);
                total++;
                int linea = 0;
                char msg[1024];
                MigrarInforme inf;
                char *nuevo = migrar_codigo(codigo, &inf);
                int cambia = strcmp(nuevo, codigo) != 0;
                /* Lo migrado tiene que ser Lince de la 2.0 (lo de la 1.x, con 'val', no
                   lo es: por eso se mira después). Si no lo es, ¿ya estaba roto antes? */
                if (lsp_comprobar_texto(nuevo, &linea, msg, sizeof msg)) {
                    int roto = cambia ? lsp_comprobar_texto(codigo, &linea, msg, sizeof msg) : 1;
                    if (!roto) {
                        fprintf(stderr, "\n❌ No sé migrar '%s' sin romperlo (la línea %d quedaría mal): lo dejo como estaba.\n"
                                        "   (Es un fallo de Lince: cuéntalo con el archivo, por favor.)\n\n", archivos[k], linea);
                        return 1;
                    }
                    printf("⏭️  %s: tiene un error de sintaxis en la línea %d; arréglalo antes de migrarlo.\n", archivos[k], linea);
                    migrar_informe_liberar(&inf);
                    free(nuevo);
                    free(codigo);
                    continue;
                }
                if (cambia || inf.nmano) {
                    printf("%s %s", comprobar ? "📄" : cambia ? "✨" : "📄", archivos[k]);
                    if (cambia) printf(comprobar ? " (%d cambios por hacer)" : " (%d cambios)", inf.ncambios);
                    printf("\n");
                    int vals = 0;
                    for (int c = 0; c < inf.ncambios; c++) {
                        if (!strcmp(inf.cambios[c].que, "val")) { vals++; continue; }
                        if (comprobar) printf("   línea %d: %s\n", inf.cambios[c].linea, inf.cambios[c].que);
                    }
                    if (vals && comprobar) printf("   'val' (ya no hace falta): %d %s\n", vals, vals == 1 ? "vez" : "veces");
                    for (int c = 0; c < inf.nmano; c++)
                        printf("   ✋ línea %d, a mano: %s\n", inf.mano[c].linea, inf.mano[c].que);
                    a_mano += inf.nmano;
                }
                if (cambia) {
                    pendientes++;
                    if (!comprobar) {
                        FILE *f = fopen(archivos[k], "wb");
                        if (!f || fwrite(nuevo, 1, strlen(nuevo), f) != strlen(nuevo)) {
                            fprintf(stderr, "\n❌ No pude escribir '%s'.\n\n", archivos[k]);
                            return 1;
                        }
                        fclose(f);
                        migrados++;
                    }
                }
                migrar_informe_liberar(&inf);
                free(nuevo);
                free(codigo);
                free(archivos[k]);
            }
            free(archivos);
        }
        if (comprobar) printf("%s %d de %d archivos tienen algo que cambiar para la 2.0%s\n", pendientes ? "✏️ " : "✅",
                              pendientes, total, a_mano ? "; lo marcado con ✋ hay que mirarlo a mano" : "");
        else printf("%s %d de %d archivos migrados a la forma de la 2.0%s\n", migrados ? "✨" : "✅", migrados, total,
                    a_mano ? "; lo marcado con ✋ hay que mirarlo a mano" : "");
        return comprobar && pendientes ? 1 : 0;
    }

    /* 'lince formatear a.lince b.lince…': el estilo de siempre. Con --comprobar
       no cambia nada y sale con 1 si algo no lo tiene; con --mostrar escribe
       el resultado en la salida en vez de en el archivo (para el editor). */
    if (strcmp(cmd, "formatear") == 0) {
        int comprobar = 0, mostrar = 0, archivos = 0, distintos = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--comprobar") == 0) comprobar = 1;
            else if (strcmp(argv[i], "--mostrar") == 0) mostrar = 1;
            else archivos++;
        }
        if (!archivos) {
            fprintf(stderr, "\n❌ Uso: lince formatear [--comprobar | --mostrar] <archivo.lince>...\n\n");
            return 1;
        }
        for (int i = 2; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] == '-') continue;
            char *codigo = leer_archivo(argv[i]);
            {   /* con un error de sintaxis no se toca: lo dice y termina */
                Lexer *lx = lexer_crear(codigo);
                int nt = 0;
                Token *tk = lexer_tokenizar(lx, &nt);
                Parser *ps = parser_crear(tk, nt);
                nodo_destruir(parser_parsear(ps));
                parser_destruir(ps);
                lexer_destruir(lx);
            }
            char *nuevo = formatear_codigo(codigo);
            if (!nuevo) {
                fprintf(stderr, "\n❌ No sé formatear '%s' sin cambiar el programa: lo dejo como estaba.\n"
                                "   (Es un fallo de Lince: cuéntalo con el archivo, por favor.)\n\n", argv[i]);
                return 1;
            }
            int igual = strcmp(nuevo, codigo) == 0;
            if (mostrar) fputs(nuevo, stdout);
            else if (comprobar) {
                if (!igual) { printf("✏️  Sin formatear: %s\n", argv[i]); distintos++; }
            } else if (!igual) {
                FILE *f = fopen(argv[i], "wb");
                if (!f || fwrite(nuevo, 1, strlen(nuevo), f) != strlen(nuevo)) {
                    fprintf(stderr, "\n❌ No pude escribir '%s'.\n\n", argv[i]);
                    return 1;
                }
                fclose(f);
                printf("✨ Formateado: %s\n", argv[i]);
            } else printf("✅ Ya tenía el estilo: %s\n", argv[i]);
            free(nuevo);
            free(codigo);
        }
        return comprobar && distintos ? 1 : 0;
    }

    /* 'lince perfil juego.lince [argumentos]': lo ejecuta midiendo en qué se
       va el tiempo, y al terminar lo cuenta (ver interprete.c) */
    if (strcmp(cmd, "perfil") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\n❌ Uso: lince perfil <archivo.lince> [argumentos]\n\n");
            return 1;
        }
        interprete_set_args(argc - 3, argv + 3);
        interprete_perfil_activar(argv[2]);
        return ejecutar_recargable(argv[2]);
    }

    /* 'lince probar [carpetas o archivos]': las pruebas y cuánto código recorren */
    if (strcmp(cmd, "probar") == 0) return probar_ejecutar(argc - 2, argv + 2);

    /* 'lince documentar libreria.lince [-o AYUDA.md]': la ayuda de sus comentarios */
    if (strcmp(cmd, "documentar") == 0) {
        const char *archivo = NULL, *salida = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) salida = argv[++i];
            else archivo = argv[i];
        }
        if (!archivo) {
            fprintf(stderr, "\n❌ Uso: lince documentar <archivo.lince> [-o ayuda.md]\n\n");
            return 1;
        }
        char *codigo = leer_archivo(archivo);
        const char *base = strrchr(archivo, '/');
#ifdef _WIN32
        const char *b2 = strrchr(archivo, '\\');
        if (!base || (b2 && b2 > base)) base = b2;
#endif
        char titulo[256];
        snprintf(titulo, sizeof titulo, "%s", base ? base + 1 : archivo);
        char *punto = strrchr(titulo, '.');
        if (punto && strcmp(punto, ".lince") == 0) *punto = '\0';
        char *md = documentar_markdown(codigo, titulo);
        if (salida) {
            FILE *f = fopen(salida, "wb");
            if (!f) { fprintf(stderr, "\n❌ No pude escribir '%s'.\n\n", salida); return 1; }
            fputs(md, f);
            fclose(f);
            printf("📖 Ayuda de %s → %s\n", archivo, salida);
        } else fputs(md, stdout);
        free(md);
        free(codigo);
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
                          "                       [--nombre \"Mi juego\"] [--icono icono.png] [--para windows|linux]\n"
                          "        lince repartir <juego.lince> --android [--aab] [-o juego.apk] [--nombre \"Mi juego\"]\n"
                          "                       [--id org.lince.mijuego] [--version 1]\n\n";
        if (argc < 3) { fprintf(stderr, "\n❌ Falta el juego.\n%s", uso); return 1; }
        const char *ext = strrchr(argv[2], '.');
        if (!ext || strcmp(ext, ".lince") != 0) {
            fprintf(stderr, "\n❌ El archivo '%s' no tiene la extensión '.lince'.\n\n", argv[2]);
            return 1;
        }
        const char *carpeta = NULL, *nombre = NULL, *id = NULL, *icono = NULL, *para = NULL;
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
            else if (strcmp(argv[i], "--icono") == 0 && i + 1 < argc) icono = argv[++i];
            else if (strcmp(argv[i], "--para") == 0 && i + 1 < argc) {
                para = argv[++i];
                if (strcmp(para, "windows") != 0 && strcmp(para, "linux") != 0) {
                    fprintf(stderr, "\n❌ --para es windows o linux (para Android, --android).\n\n");
                    return 1;
                }
            }
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
        if (nombre && !nombre[0]) { fprintf(stderr, "\n❌ --nombre no puede ir vacío.\n\n"); return 1; }
        RepartirOpciones op = {carpeta, nombre, icono, para};
        return repartir_crear(argv[2], &op, extras, num_extras);
    }

    /* Subcomandos del gestor de paquetes */
    if (strcmp(cmd, "instalar") == 0) {
        if (argc < 3) return paquetes_instalar_proyecto();   /* los de lince.paquetes */
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
