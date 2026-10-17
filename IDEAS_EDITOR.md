# Ideas para el editor (lince-motor)

Este repositorio es solo el lenguaje. Aquí se apuntan las cosas que irían bien
en un editor visual, que será **lince-motor** (por rediseñar). Cada idea dice
en qué parte del lenguaje se apoyaría: el editor no debería hacer nada que el
lenguaje no pueda hacer también escribiendo código.

## Hojas de sprites

- **Ver cómo se ha dividido una hoja** (`juego.hoja`): la imagen original al
  lado de la rejilla resultante, con cada figura numerada y su fila. Ahora
  mismo solo se puede comprobar guardando la hoja con `guardar_imagen` y
  mirándola fuera.
- **Quitar cuadros con un clic** en vez de escribir `{"quitar": [0, 1, 2]}`.
- **Ajustar el fondo y la escala viendo el resultado**: qué color se ha tomado
  como fondo, qué se ha quitado (los calcetines blancos se quedan) y cómo queda
  reducida. Hoy la tolerancia del fondo es fija (`JG_TOLERANCIA_FONDO`); un
  control para cambiarla sería una opción nueva del lenguaje.
- **Elegir entre "sola", "por número" y "por tamaño"** viendo la rejilla
  encima de la imagen.
- **Recargar al guardar**: si la hoja se regenera fuera (por ejemplo en
  Ludo.AI), el juego ya la rehace solo; el editor podría avisar de que cambió
  de forma (otro número de cuadros) y de qué animaciones afecta.

## Animaciones

- **Editor de animaciones del animador** (`juego.animador`): nombre, fila,
  cuadros (arrastrándolos a una tira), cuadros por segundo, repetir y
  siguiente, con la animación en bucle al lado. Que genere el diccionario.
- **Ver las cajas de choque** (`juego.caja`) de cada cuadro encima de la
  animación, y ajustar el `margen` viéndolo; en el juego en marcha, una tecla
  para enseñarlas todas (hoy se dibujan a mano con `contorno`).
- **Diagrama de transiciones**: qué animación va después de cuál
  (`"siguiente"`).
- **Línea de tiempo con las pisadas** (`juego.pisadas`): las detectadas
  marcadas sobre los cuadros, para corregirlas a mano (`"pisadas": [...]`) y
  oírlas con el sonido de pasos elegido.
- **Probar `juego.animar`** sobre un dibujo: todos los tipos a la vez, con
  controles de intensidad, cuadros, vista, y para las piernas `piernas` y
  `centro` marcados con el ratón sobre el dibujo. Botón para guardar la hoja
  (`guardar_imagen`) y retocarla.

## Dibujo

- **Editor de píxeles para `juego.sprite`**: que lea y escriba la lista de
  filas de texto y la paleta del código, para seguir sin archivos.
- **Paleta de colores** compartida entre sprites.
- **Probar fuentes** (`juego.fuente`): escribir un texto y verlo con cada
  fuente y tamaño; para una fuente dibujada en una imagen, ver qué letra ha
  tocado a cada carácter y avisar si sobra o falta alguna.

## Movimiento

- **Ver las curvas** de `juego.suavizar` (`"rebote"`, `"elastico"`...)
  moviendo una bola, y elegir la que va con un clic.

## Mapas

- **Editor de mapas de casillas** (`juego.mapa`): pintar el nivel con los
  tiles de una hoja y que escriba las filas de texto; marcar qué caracteres son
  sólidos o plataformas; ver los tiles animados (`[hoja, [cuadros], segundos]`).
- **Ver el camino de `juego.camino`** entre dos casillas, para probar
  enemigos.
- **Elegir qué tile es cada carácter** (el aspecto de `dibujar_mapa`) con un
  clic en la hoja.

## Sonido

- **Probador de `juego.efecto`**: cada tipo con controles de tono, largo,
  volumen y semilla, y botón para guardarlo (`guardar_sonido`).
- **Asignar sonidos a las pisadas** y a otros momentos de una animación.

## Probar y enseñar el juego

- **Grabar un vídeo con sonido** del juego: lo que hoy se hace a mano con
  `LINCE_JUEGO_FOTOS`, `LINCE_JUEGO_AUDIO` y gstreamer, con un guion de
  entradas (qué teclas y cuándo) que se pueda grabar jugando.
- **Errores en el editor**: el mismo aviso que sale en la ventana del juego
  (recarga en caliente), con un enlace a la línea.
- **Vista de partículas**: ajustar las opciones de `juego.particulas` viendo
  el resultado.
- **Escenas**: lista de escenas del juego y saltar a una para probarla.
