# Pendiente

Lo que queda por hacer en Lince y lo que ya se ha decidido, para no tener que
repensarlo cada vez. Las ideas para el editor visual van en
[IDEAS_EDITOR.md](IDEAS_EDITOR.md), no aquí.

## Versión 0.8

- [x] **Google Play:** `lince repartir --aab` hace el `.aab` firmado que pide Google Play, en C como el `.apk`. Probado con bundletool y en un emulador con Android 16; falta subir uno de verdad a Play Console (lo hace el autor, cuando tenga cuenta).
- [x] **Partidas guardadas:** `juego.guardar`, `juego.cargar` (con valores por defecto), `juego.olvidar` y `juego.carpeta_guardado`, en la carpeta de datos del usuario de cada sistema.
- [x] **Menús y diálogos, en Lince:** `juego.menu` y `juego.dialogo` (con quién habla, retrato y respuestas), con teclado, mando, ratón o dedo; `examples/12_aventura.lince`.

## Versión 0.9

- [ ] **Mapas de Tiled:** `juego.mapa_tiled("nivel.tmj")` carga los niveles hechos con Tiled (capas de tiles, objetos como enemigos o salidas, y colisiones) y se dibujan y se usan con lo que ya hay.
- [ ] **Física sencilla, en Lince:** cuerpos con gravedad, rebotes, empujes y choques entre ellos y con el mapa (cajas que caen, pelotas, plataformas que se mueven), encima de las colisiones que ya hay.
- [ ] **Controles a gusto:** el jugador cambia las teclas y botones de cada acción desde un menú del juego, y se guarda (con los menús y las partidas guardadas de la 0.8).

## Lenguaje

Nada pendiente ahora mismo: lo que vaya saliendo se apunta aquí.

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
- **Bibliotecas de terceros:** stb_image, stb_vorbis y stb_truetype van tal cual en `src/terceros`. Lince no podría escribirlas con la velocidad necesaria.
- **Los recursos descargados no entran en el repositorio:** imágenes, sonidos y fuentes con licencia ajena. Las demos que los usan viven fuera.
- **Una hoja de sprites es una sola imagen con todos los fotogramas**, no un archivo por fotograma.
- **Los sonidos realistas los pone quien hace el juego**, en archivos. Lince solo genera los de 8 bits (`juego.efecto`).
- **Palabras reservadas que no pueden ser nombres:** entre otras, `y`, `o`, `no`, `lista`, `mi`, `caso`, `otro`, `romper` y `continuar`.
  - Por eso no vale `sea [x, y] = ...`.
  - `en`, `cualquiera`, `tarea`, `es`, `como`, `obtener`, `fijar`, etc. solo cuentan en su sitio.
- **Ya están en Lince** los combos, `mantenida`, `pulsaciones`, las curvas, `suavizar`, `acercar`, las escenas y el animador. `presentar()` llama en cada fotograma a `_presentar()` de la biblioteca; `dibujar()` y `caja()` llaman a `_cuadro()` con un animador; las variables de la biblioteca se conservan al recargar en caliente.
- **En C se queda** lo que habla con la pantalla, el teclado o el altavoz, o mira píxeles: dibujar, las hojas (dividirlas, la caja de lo que se ve, las pisadas), los sonidos, las fuentes, los mapas.
