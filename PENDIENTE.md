# Pendiente

Lo que queda por hacer en Lince y lo que ya se ha decidido, para no tener que
repensarlo cada vez. Las ideas para el editor visual van en
[IDEAS_EDITOR.md](IDEAS_EDITOR.md), no aquí.

## Versión 0.8

- [x] **Google Play:** `lince repartir --aab` hace el `.aab` firmado que pide Google Play, en C como el `.apk`. Probado con bundletool y en un emulador con Android 16; falta subir uno de verdad a Play Console (lo hace el autor, cuando tenga cuenta; no hace falta para la 1.0).
- [x] **Partidas guardadas:** `juego.guardar`, `juego.cargar` (con valores por defecto), `juego.olvidar` y `juego.carpeta_guardado`, en la carpeta de datos del usuario de cada sistema.
- [x] **Menús y diálogos, en Lince:** `juego.menu` y `juego.dialogo` (con quién habla, retrato y respuestas), con teclado, mando, ratón o dedo; `examples/12_aventura.lince`.

## Versión 0.9

- [x] **Mapas de Tiled:** `juego.mapa_tiled` (.tmx y .tmj, tilesets .tsx y .tsj) y `juego.dibujar_tiled`. Quedan fuera los mapas isométricos y hexagonales, los infinitos, los tilesets de imágenes sueltas y las capas comprimidas con zstd.
- [x] **Física sencilla, en Lince:** `juego.fisica`, `juego.cuerpo`, `juego.avanzar_fisica`, `juego.tocando`, `juego.empujar` y `juego.quitar_cuerpo`; `examples/13_fisica.lince`. Sin giros (los cuerpos no rotan) y con choques caja contra caja o círculo contra círculo (un círculo contra una caja cuenta como caja).
- [x] **Controles a gusto:** `juego.menu_controles()` (se guarda y se aplica solo), `juego.restablecer_controles()` y `juego.entrada_pulsada()`.

## Camino a la 1.0

La 1.0 será la primera versión estable: hasta entonces se sigue con 0.9, 0.10, 0.11... **Cuando la 1.0 esté terminada y probada, el lenguaje se queda ahí y se pasa a trabajar en el IDE** (lince-motor, en su propia conversación). Para llegar a ella:

- [ ] **El lenguaje no cambia de forma que rompa juegos ya escritos.** Desde la 1.0 se cumple ese compromiso.
- [ ] **Probado en real** (lo hace el autor): sonido, mandos (también por Bluetooth), teclado y ratón en equipos de verdad. Con `examples/probar_equipo` y la lista de su `LEEME.md`. Subir un juego a Google Play queda para después de la 1.0.
- [x] **Algún juego completo hecho con Lince**, no solo los ejemplos, para que salga lo que falta: `juegos/la_piedra_del_lince`. Probado con wine (Windows) y en el emulador de Android 16; falta en un móvil y un Windows de verdad.
- [x] **El manual lo cubre todo:** el del lenguaje (con los módulos `matematica`, `texto`, `json`, `bytes` y `bits` y las herramientas) y una página para el módulo `juego` (`docs/juego.html`) con un ejemplo y su captura real para cada tema. La lista completa de funciones y opciones sigue en la especificación.

## Lenguaje

- [x] `mapear`, `filtrar` y `reducir` aceptan también generadores y bytes (salió escribiendo el manual).

## Biblioteca de juego en Lince

La lógica de juego reutilizable va escrita en Lince, en `src/lince/juego.lince`, encima de primitivas pequeñas en C: es lo que aporta Lince.

Nada pendiente ahora mismo.

## Rendimiento

- [ ] **Camino B, una máquina virtual de bytecode:** solo si un juego de verdad lo necesita.
  - El camino A (cachés y atajos) ya se hizo y dio entre un 2 y un 30 %.
  - En los juegos probados, casi todo el tiempo del fotograma es de SDL, no de Lince.

## Para probar en real (lo hace el autor)

- [ ] Sonido, mando, teclado y ratón en un equipo de verdad.

## Plataformas

- [ ] **Consolas: Nintendo Switch 1 y 2 y PlayStation 5.** Más adelante se verá cómo hacerlo. Lo que se sabe:
  - Hace falta registrarse como desarrollador en Nintendo y en Sony, que lo aprueban ellos, y usar sus kits de desarrollo.
  - Sus herramientas y SDK van con un acuerdo de confidencialidad, así que el código para cada consola no podría ir en este repositorio público. Tendría que ir aparte, en privado, como lince-motor.
  - SDL2 tiene versiones para estas consolas, pero solo se dan a desarrolladores aprobados.
  - Lince ya está en C sin dependencias y sus tareas tienen el cambio de pila escrito a mano para arm64 (Switch) y x86_64 (PS5), lo que ayudará.

- [ ] **Android**: hecho lo básico (`lince repartir --android`, `android/construir.sh`, táctil, mando en la pantalla, botones,
  «atrás», archivos en su carpeta, tareas en arm64). Falta:
  - **Probarlo con un mando Bluetooth** en el móvil (lo hace el autor): que se lea y
    que el mando de la pantalla se esconda al conectarlo. Ya probado en un móvil de
    verdad sin mando, en el emulador y las tareas en arm64 con `qemu-aarch64`.

## Decisiones ya tomadas

- **Versiones:** el número está en `src/version.h` y lo nuevo de cada una en `NOVEDADES.md`. Al subir la etiqueta `vX.Y.Z`, la CI publica la versión con Lince para Linux y Windows, la plantilla de Android y la extensión de VS Code.

- Lince es un lenguaje para videojuegos, no de propósito general. Los módulos de servidor, bases de datos, red, etc. se quitaron (siguen en el historial).
- **Este repositorio es solo el lenguaje.** El editor visual será lince-motor, un proyecto aparte.
- **`lince compilar` empaqueta el intérprete con el programa.** No hay un compilador a C aparte.
- **Funciona en Linux y en Windows.** macOS no se va a soportar.
- **No hay versión para el navegador** (WebAssembly): el autor no le ve sentido. Los juegos van al ordenador (Linux y Windows) y al móvil (Android).
- **Bibliotecas de terceros:** stb_image, stb_vorbis y stb_truetype van en `src/terceros`. Lince no podría escribirlas con la velocidad necesaria. Se tocan lo mínimo: a stb_vorbis se le arreglaron fallos con archivos estropeados que encontró `make fuzz` (cada cambio, marcado con «Lince:»).
- **Recursos de terceros (imágenes, sonidos, fuentes) sí, si su licencia deja usarlos y repartirlos:** mejor CC0 (dominio público, como los de Kenney); con CC-BY u otras que piden citar, en un `CREDITOS.md` al lado. Cada recurso va con su licencia y de dónde viene. Lo que no tenga licencia clara no entra.
- **Una hoja de sprites es una sola imagen con todos los fotogramas**, no un archivo por fotograma.
- **Los sonidos realistas los pone quien hace el juego**, en archivos. Lince solo genera los de 8 bits (`juego.efecto`).
- **Palabras reservadas que no pueden ser nombres:** entre otras, `y`, `o`, `no`, `lista`, `mi`, `caso`, `otro`, `romper` y `continuar`.
  - Por eso no vale `sea [x, y] = ...`.
  - `en`, `cualquiera`, `tarea`, `es`, `como`, `obtener`, `fijar`, etc. solo cuentan en su sitio.
- **Ya están en Lince** los combos, `mantenida`, `pulsaciones`, las curvas, `suavizar`, `acercar`, las escenas y el animador. `presentar()` llama en cada fotograma a `_presentar()` de la biblioteca; `dibujar()` y `caja()` llaman a `_cuadro()` con un animador; las variables de la biblioteca se conservan al recargar en caliente.
- **En C se queda** lo que habla con la pantalla, el teclado o el altavoz, o mira píxeles: dibujar, las hojas (dividirlas, la caja de lo que se ve, las pisadas), los sonidos, las fuentes, los mapas.
