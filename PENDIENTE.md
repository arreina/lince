# Pendiente

Lo que queda por hacer en Lince y lo que ya se ha decidido, para no tener que
repensarlo cada vez. Las ideas para el editor visual van en
[IDEAS_EDITOR.md](IDEAS_EDITOR.md), no aquí.

## Lenguaje

- [ ] **Enumeraciones de verdad.**
  - Ahora sus valores son textos (`"Color.Rojo"`).
  - Faltaría:
    - que tengan valores propios;
    - recorrerlas con `para cada`;
    - que `elegir` avise de un caso que falta: añadir `Estado.Agarrado` y que Lince diga dónde se ha olvidado.
- [ ] **`mi.x++` y `lista[i]++`** dan «Sentencia no reconocida». `i++` solo vale con una variable; hoy hay que escribir `mi.x += 1`.
- [ ] **Guardar un generador en una variable.**
  - `sea g = contar(5)` deja `g` en `nulo`, porque copiar un generador da `nulo`.
  - Solo se puede recorrer directamente: `para cada x en contar(5)`.
  - Si se arregla, el generador se recorrerá una sola vez.
- [ ] **Tareas sin nombre**, como `tarea() { ... }` igual que las lambdas. Hoy una tarea se declara siempre con nombre (`tarea nombre() { }`), también dentro de una clase.
- [ ] **El tipo `tarea` en los parámetros** (`val tarea t`). Hoy se usa `val cualquiera t`, y `t es tarea` para comprobarlo.

## Biblioteca de juego en Lince

La lógica de juego reutilizable va escrita en Lince, en `src/lince/juego.lince`, encima de primitivas pequeñas en C: es lo que aporta Lince.

- [ ] **Escenas en Lince.** Ahora que hay tareas, el fundido podría ser una tarea que dibuja cada fotograma, en vez de código C dentro de `presentar()`.
- [ ] **Animador en Lince.** Está atado a las hojas y a la recarga en caliente de C; habría que separar qué parte es lógica y cuál es dibujo.

## Rendimiento

- [ ] **Camino B, una máquina virtual de bytecode:** solo si un juego de verdad lo necesita.
  - El camino A (cachés y atajos) ya se hizo y dio entre un 2 y un 30 %.
  - En los juegos probados, casi todo el tiempo del fotograma es de SDL, no de Lince.

## Para probar en real (lo hace el autor)

- [ ] Sonido, mando, teclado y ratón en un equipo de verdad.

## Plataformas

- [ ] **Android** (en estudio): que un juego de Lince se pueda instalar en un móvil.

## Decisiones ya tomadas

- Lince es un lenguaje para videojuegos, no de propósito general. Los módulos de servidor, bases de datos, red, etc. se quitaron (siguen en el historial).
- **Este repositorio es solo el lenguaje.** El editor visual será lince-motor, un proyecto aparte.
- **`lince compilar` empaqueta el intérprete con el programa.** No hay un compilador a C aparte.
- **Funciona en Linux y en Windows.** macOS no se va a soportar.
- **Bibliotecas de terceros:** stb_image, stb_vorbis y stb_truetype van tal cual en `src/terceros`. Lince no podría escribirlas con la velocidad necesaria.
- **Los recursos descargados no entran en el repositorio:** imágenes, sonidos y fuentes con licencia ajena. Las demos que los usan viven fuera.
- **Una hoja de sprites es una sola imagen con todos los fotogramas**, no un archivo por fotograma.
- **Los sonidos realistas los pone quien hace el juego**, en archivos. Lince solo genera los de 8 bits (`juego.efecto`).
- **Palabras reservadas que no pueden ser nombres:** entre otras, `y`, `o`, `no`, `lista`, `mi`, `caso`, `otro`, `romper` y `continuar`.
  - Por eso no vale `sea [x, y] = ...`.
  - `en`, `cualquiera`, `tarea`, `es`, `como`, `obtener`, `fijar`, etc. solo cuentan en su sitio.
- **Escenas y animador se quedan en C de momento** (ver arriba).
