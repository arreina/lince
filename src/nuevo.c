/*
 * 🐆 Lince — nuevo.c
 * 'lince nuevo mi_juego': crea la carpeta mi_juego con un juego pequeño y
 * completo (portada, partida y fin; teclado y mando) listo para cambiarlo,
 * para no empezar nunca con un archivo en blanco.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include "nuevo.h"

#ifdef _WIN32
#include <direct.h>
#define CREAR_DIR(r) _mkdir(r)
#define SEP "\\"
#else
#define CREAR_DIR(r) mkdir(r, 0755)
#define SEP "/"
#endif

/* @TITULO@, @TITULO_MAYUS@ y @ARCHIVO@ se sustituyen al crear el juego.
   tests/juego/smoke.sh crea uno y lo juega solo, así que no se puede romper
   sin que la CI lo note. */
static const char PLANTILLA[] =
    "# 🐆 @TITULO@ — hecho con Lince\n"
    "#\n"
    "# Ejecútalo con:   lince @ARCHIVO@\n"
    "# Mientras corre, cambia algo y guarda: se actualiza solo (F5 empieza de cero).\n"
    "# Para compartirlo: lince repartir @ARCHIVO@\n"
    "#\n"
    "# Ahora mismo es un juego pequeño para empezar: recoge todas las estrellas que\n"
    "# puedas en 30 segundos. Cámbialo todo lo que quieras.\n"
    "importar \"juego\"\n"
    "importar \"matematica\"\n"
    "\n"
    "fijo ANCHO = 640\n"
    "fijo ALTO = 480\n"
    "fijo VELOCIDAD = 260               # píxeles por segundo\n"
    "fijo DURACION = 30                 # segundos de partida\n"
    "\n"
    "juego.abrir(\"@TITULO@\", ANCHO, ALTO)\n"
    "\n"
    "# Controles: cada acción vale con el teclado y con el mando a la vez\n"
    "juego.definir_accion(\"izquierda\", [\"izquierda\", \"a\", \"mando:izquierda\", \"mando:izq_x-\"])\n"
    "juego.definir_accion(\"derecha\", [\"derecha\", \"d\", \"mando:derecha\", \"mando:izq_x+\"])\n"
    "juego.definir_accion(\"arriba\", [\"arriba\", \"w\", \"mando:arriba\", \"mando:izq_y-\"])\n"
    "juego.definir_accion(\"abajo\", [\"abajo\", \"s\", \"mando:abajo\", \"mando:izq_y+\"])\n"
    "juego.definir_accion(\"aceptar\", [\"espacio\", \"entrar\", \"mando:a\"])\n"
    "\n"
    "# Dibujos hechos con texto: cada letra es un píxel del color que le des\n"
    "fijo PROTAGONISTA = juego.sprite([\n"
    "    \"..XXXX..\",\n"
    "    \".XXXXXX.\",\n"
    "    \"XXOXXOXX\",\n"
    "    \"XXXXXXXX\",\n"
    "    \"XXOXXOXX\",\n"
    "    \"XXXOOXXX\",\n"
    "    \".XXXXXX.\",\n"
    "    \"..XXXX..\"\n"
    "], {\"X\": [255, 170, 60], \"O\": [40, 30, 30]})\n"
    "fijo ESTRELLA = juego.sprite([\n"
    "    \"...Y...\",\n"
    "    \"..YYY..\",\n"
    "    \"YYYYYYY\",\n"
    "    \".YYYYY.\",\n"
    "    \".YY.YY.\"\n"
    "], {\"Y\": [255, 215, 80]})\n"
    "\n"
    "# ── Estado de la partida ─────────────────────────────────────────────\n"
    "sea pos_x = ANCHO / 2              # dónde está el protagonista\n"
    "sea pos_y = ALTO / 2\n"
    "sea estrella = [0, 0]\n"
    "sea puntos = 0\n"
    "sea record = 0\n"
    "\n"
    "funcion nueva_estrella(): nulo {\n"
    "    estrella = [matematica.aleatorio(40, ANCHO - 40), matematica.aleatorio(60, ALTO - 40)]\n"
    "}\n"
    "\n"
    "funcion empezar(): nulo {\n"
    "    pos_x = ANCHO / 2\n"
    "    pos_y = ALTO / 2\n"
    "    puntos = 0\n"
    "    nueva_estrella()\n"
    "}\n"
    "\n"
    "# ── Escenas: portada, partida y fin ──────────────────────────────────\n"
    "funcion portada(): nulo {\n"
    "    juego.limpiar([24, 28, 52])\n"
    "    sea t = \"@TITULO_MAYUS@\"\n"
    "    juego.texto(t, (ANCHO - juego.ancho_texto(t, 5)) / 2, 150, 5, [255, 215, 80])\n"
    "    juego.dibujar(PROTAGONISTA, ANCHO / 2, 260, {\"escala\": 6, \"centrado\": verdadero})\n"
    "    si juego.cuadro(2, 2) == 0 {\n"
    "        sea p = \"PULSA ESPACIO\"\n"
    "        juego.texto(p, (ANCHO - juego.ancho_texto(p, 3)) / 2, 340, 3, [255, 255, 255])\n"
    "    }\n"
    "    si juego.accion_pulsada(\"aceptar\") { juego.cambiar_escena(\"partida\", 0.5) }\n"
    "}\n"
    "\n"
    "funcion partida(val numero dt): nulo {\n"
    "    si juego.escena_nueva() { empezar() }\n"
    "    pos_x = matematica.limitar(pos_x + juego.direccion(\"izquierda\", \"derecha\") * VELOCIDAD * dt, 16, ANCHO - 16)\n"
    "    pos_y = matematica.limitar(pos_y + juego.direccion(\"arriba\", \"abajo\") * VELOCIDAD * dt, 56, ALTO - 16)\n"
    "    si juego.colision_circulos(pos_x, pos_y, 16, estrella[0], estrella[1], 12) {\n"
    "        puntos = puntos + 1\n"
    "        juego.pitido(880, 60, 0.2)\n"
    "        juego.particulas(estrella[0], estrella[1], {\"cantidad\": 20, \"color\": [255, 215, 80], \"velocidad\": [60, 180]})\n"
    "        nueva_estrella()\n"
    "    }\n"
    "    sea queda = DURACION - juego.tiempo_escena()\n"
    "    si queda <= 0 {\n"
    "        record = matematica.maximo(record, puntos)\n"
    "        juego.cambiar_escena(\"fin\", 0.5)\n"
    "    }\n"
    "\n"
    "    juego.limpiar([24, 28, 52])\n"
    "    juego.dibujar(ESTRELLA, estrella[0], estrella[1], {\"escala\": 4, \"centrado\": verdadero})\n"
    "    juego.dibujar(PROTAGONISTA, pos_x, pos_y, {\"escala\": 4, \"centrado\": verdadero})\n"
    "    juego.dibujar_particulas()\n"
    "    juego.texto(\"PUNTOS \" + a_texto(puntos), 12, 12, 3, [255, 255, 255])\n"
    "    sea t = \"TIEMPO \" + a_texto(matematica.techo(matematica.maximo(0, queda)))\n"
    "    juego.texto(t, ANCHO - juego.ancho_texto(t, 3) - 12, 12, 3, [255, 255, 255])\n"
    "}\n"
    "\n"
    "funcion fin(): nulo {\n"
    "    juego.limpiar([24, 28, 52])\n"
    "    sea t = \"SE ACABO EL TIEMPO\"\n"
    "    juego.texto(t, (ANCHO - juego.ancho_texto(t, 4)) / 2, 160, 4, [255, 215, 80])\n"
    "    sea r = a_texto(puntos) + \" ESTRELLAS - RECORD \" + a_texto(record)\n"
    "    juego.texto(r, (ANCHO - juego.ancho_texto(r, 2)) / 2, 230, 2, [255, 255, 255])\n"
    "    sea p = \"ESPACIO PARA JUGAR OTRA VEZ\"\n"
    "    juego.texto(p, (ANCHO - juego.ancho_texto(p, 2)) / 2, 280, 2, [200, 200, 220])\n"
    "    si juego.accion_pulsada(\"aceptar\") { juego.cambiar_escena(\"partida\", 0.5) }\n"
    "}\n"
    "\n"
    "# Al recargar en caliente se sigue donde estabas, en vez de volver a la portada\n"
    "si no juego.recargado() { juego.cambiar_escena(\"portada\") }\n"
    "\n"
    "# ── El bucle del juego: unas 60 vueltas por segundo ──────────────────\n"
    "mientras juego.abierto() {\n"
    "    si juego.tecla_pulsada(\"escape\") { juego.salir() }\n"
    "    sea escena = juego.escena()\n"
    "    si escena == \"portada\" {\n"
    "        portada()\n"
    "    } sino si escena == \"partida\" {\n"
    "        partida(juego.dt())\n"
    "    } sino {\n"
    "        fin()\n"
    "    }\n"
    "    juego.presentar()\n"
    "}\n";

/* Escribe 'texto' en 'f' cambiando cada 'marca' por su valor */
static void sustituir(FILE *f, const char *texto, const char *const marcas[][2], int n) {
    for (const char *p = texto; *p; ) {
        int hecho = 0;
        if (*p == '@')
            for (int i = 0; i < n; i++) {
                size_t l = strlen(marcas[i][0]);
                if (strncmp(p, marcas[i][0], l) == 0) { fputs(marcas[i][1], f); p += l; hecho = 1; break; }
            }
        if (!hecho) fputc(*p++, f);
    }
}

int nuevo_crear(const char *nombre) {
    /* el nombre es el de la carpeta y el archivo: letras, cifras, _ y - */
    size_t l = strlen(nombre);
    int valido = l > 0 && l < 100;
    for (const unsigned char *c = (const unsigned char *)nombre; *c && valido; c++)
        if (!isalnum(*c) && *c != '_' && *c != '-') valido = 0;
    if (!valido) {
        fprintf(stderr, "\n❌ '%s' no sirve como nombre: usa letras sin tilde, cifras, '_' o '-'\n"
                        "   (por ejemplo: lince nuevo mi_juego)\n\n", nombre);
        return 1;
    }
    struct stat st;
    if (stat(nombre, &st) == 0) {
        fprintf(stderr, "\n❌ Ya existe '%s': elige otro nombre o bórralo antes.\n\n", nombre);
        return 1;
    }
    if (CREAR_DIR(nombre) != 0) {
        fprintf(stderr, "\n❌ No se pudo crear la carpeta '%s'.\n\n", nombre);
        return 1;
    }
    /* «mi_juego» → título «Mi juego», «MI JUEGO» y archivo «mi_juego.lince» */
    char titulo[128], mayus[128], archivo[128], ruta[300];
    snprintf(titulo, sizeof titulo, "%s", nombre);
    for (char *c = titulo; *c; c++) if (*c == '_' || *c == '-') *c = ' ';
    titulo[0] = (char)toupper((unsigned char)titulo[0]);
    for (size_t i = 0; i <= strlen(titulo); i++) mayus[i] = (char)toupper((unsigned char)titulo[i]);
    snprintf(archivo, sizeof archivo, "%s.lince", nombre);
    snprintf(ruta, sizeof ruta, "%s" SEP "%s", nombre, archivo);
    FILE *f = fopen(ruta, "wb");
    if (!f) { fprintf(stderr, "\n❌ No se pudo crear '%s'.\n\n", ruta); return 1; }
    const char *const marcas[][2] = { {"@TITULO_MAYUS@", mayus}, {"@TITULO@", titulo}, {"@ARCHIVO@", archivo} };
    sustituir(f, PLANTILLA, marcas, 3);
    fclose(f);

    printf("\n🐆 Juego nuevo: %s\n\n", ruta);
    printf("   cd %s\n", nombre);
    printf("   lince %s        juégalo\n\n", archivo);
    printf("   Con el juego abierto, cambia algo en %s y guarda: se actualiza solo.\n", archivo);
    printf("   Para compartirlo: lince repartir %s\n", archivo);
    printf("   Tutorial: https://arreina.github.io/lince/tutorial.html\n\n");
    return 0;
}
