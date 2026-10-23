# Novedades

Lo que trae cada versión de Lince. La CI publica cada versión en GitHub con el
apartado que le toca de este archivo.

## 0.12.0 (en preparación)

- **`examples/probar_equipo`**: un programa para comprobar en un ordenador o un móvil
  de verdad el teclado (también la ñ y las tildes), el ratón, los mandos, el sonido y
  la pantalla táctil, con una lista de comprobación en su `LEEME.md`.
- `dentro()` acepta el punto como una lista `[x, y]`, como la que da `raton()`:
  `juego.dentro(juego.raton(), boton)`.

### Para los editores y el IDE

- **Lo que escribe `escribir` sale en el momento**, en orden con los errores, aunque la salida
  no sea una terminal (antes se quedaba en el búfer hasta el final cuando la leía otro
  programa, como un IDE).
- **Los errores de un fichero importado dicen de cuál son**: `❌ ErrorTipo en línea 3 de
  libs/enemigos.lince: ...`, también los de sintaxis. El error capturado trae `e.archivo`.
- Un `lanzar` sin capturar dice su línea (antes, solo el mensaje).

### Revisión del lenguaje antes de la 1.0

Cosas que, una vez congelado el lenguaje, ya no se podrían cambiar:

- **`==` compara listas y diccionarios por lo que tienen**: `[1, 2] == [1, 2]` es verdadero
  (antes, falso: una lista ni siquiera era igual a sí misma). `contiene()` y `posicion()`
  también. **Arreglado:** `posicion()` daba por iguales todas las listas, diccionarios y
  objetos, así que `enemigos.posicion(e)` daba siempre 0.
- **`ref` funciona**: lo que valga el parámetro al acabar la función pasa a la variable que
  se le dio (`intercambiar(ref a, ref b)`). Antes `ref` hacía lo mismo que `val`.
- **`val` es opcional**: `funcion doble(numero x)` es lo mismo que `funcion doble(val numero x)`.
- En un `si`, **una lista, un diccionario o unos bytes vacíos cuentan como falso**, como `""`
  y `0` (antes, verdadero).
- **Arreglado:** `x % 0` da `ErrorMatematico`, como `x / 0` (antes daba `nan`).

## 0.11.0

`y` y `o` como nombres (para las coordenadas de un juego), `lista + lista`, `a_texto()` de cualquier valor, y en el móvil el botón «atrás» como Esc y los diálogos lejos del mando de la pantalla.

### Descargas

| Archivo | Qué es |
|---|---|
| `lince-0.11.0-linux.tar.gz` | Lince para Linux (x86_64). Para abrir ventanas hace falta SDL2: `sudo apt install libsdl2-2.0-0` |
| `lince-0.11.0-windows.zip` | Lince para Windows (x86_64), con SDL2 dentro |
| `lince-android.apk` | La plantilla de Android que usan `lince repartir --android` y `--aab` (ya va dentro de los dos anteriores) |
| `lince-language-0.11.0.vsix` | La extensión de VS Code: `code --install-extension lince-language-0.11.0.vsix` |

### El lenguaje

- **`y` y `o` ya se pueden usar como nombres.** Solo son el operador entre dos valores; en
  cualquier otro sitio son una variable más, así que en un juego se puede escribir
  `sea y = 10` y `si x > 0 y y > 0`. Lo que ya estaba escrito sigue igual.
- `lista + lista` da una lista nueva con las dos seguidas (también `lista += [...]`).
- `a_texto()` convierte también listas, diccionarios y lo demás, como lo escribe `escribir`
  (antes daba error).

### En el móvil

Salió probando La Piedra del Lince en el emulador de Android 16:

- **El botón «atrás» cuenta como Esc**: abre la pausa, vuelve en un menú o sale, según lo que
  haga el juego con Esc (antes cerraba el juego siempre). Y desde Android 16 ni siquiera le
  llegaba al juego: el sistema cerraba la app.
- Mientras se ve el mando de la pantalla, los diálogos van arriba, porque abajo los tapaban
  la palanca y los botones (si el juego dice `arriba`, manda lo que diga).

## 0.10.0

Un juego completo hecho con Lince (La Piedra del Lince), la revisión del módulo `juego` antes de la 1.0 y generadores en `mapear`, `filtrar` y `reducir`.

### Descargas

| Archivo | Qué es |
|---|---|
| `lince-0.10.0-linux.tar.gz` | Lince para Linux (x86_64). Para abrir ventanas hace falta SDL2: `sudo apt install libsdl2-2.0-0` |
| `lince-0.10.0-windows.zip` | Lince para Windows (x86_64), con SDL2 dentro |
| `lince-android.apk` | La plantilla de Android que usan `lince repartir --android` y `--aab` (ya va dentro de los dos anteriores) |
| `lince-language-0.10.0.vsix` | La extensión de VS Code: `code --install-extension lince-language-0.10.0.vsix` |

### El lenguaje

- `mapear`, `filtrar` y `reducir` aceptan también generadores (`filtrar(rango(10), ...)`) y bytes.

### Revisión del módulo `juego` antes de la 1.0

Cambios que pueden obligar a tocar un juego:

- `juego.pitido(frecuencia, segundos)` y `tiempo.esperar(segundos)` van en **segundos**, como todo lo demás (antes, milisegundos): `pitido(440, 100)` pasa a `pitido(440, 0.1)`.
- El tamaño del mando táctil es `"tamaño"` (antes `"tamano"`), como en las partículas, que ya no aceptan `"tamano"`.
- **Toda opción mal escrita da error**, también en `dibujar`, `caja` y `mapa`, que antes la ignoraban, y siempre con el mismo mensaje: `no conozco la opción 'x' (hay ...)`.
- **Los errores del módulo, con su tipo:** `ErrorTipo`, `ErrorArgumento` (un valor o una opción que no valen) y `ErrorJuego` (la ventana sin abrir, un archivo...). Si capturabas `Error`, captura `error` (todos) o el tipo concreto.
- Los menús, diálogos y controles hablan de «opciones», como el resto: `menu(textos[, opciones])`.

Cosas nuevas:

- `juego.boton_soltado()` (mando) y `juego.boton_raton_soltado()`, como `tecla_soltada()`.
- `colision_circulos` y `colision_circulo_rect` aceptan círculos como `[x, y, radio]`.

### Un juego completo: La Piedra del Lince

`juegos/la_piedra_del_lince` es una aventura entera hecha con Lince: un pueblo, un bosque
y una fortaleza (mapas de Tiled), diálogos, tienda, cofres, cuatro clases de enemigos, un
jefe, partida guardada, pausa con controles y créditos. Los gráficos, la música y los
sonidos son de terceros con licencias libres (Kenney, OpenGameArt). Al hacerlo salieron:

- **Arreglado:** los menús y los diálogos no hacían caso a la palanca ni a la cruceta del
  mando de la pantalla del móvil (ni a `mando_virtual`): se leían antes de sumarlo. Ahora
  se leen después.
- El `retrato` de un diálogo puede ser un cuadro de una hoja: `[hoja, cuadro]`.
- `colision`, `colision_circulo_rect` y `dentro` aceptan como rectángulo un diccionario con
  `x`, `y`, `ancho` y `alto` (lo que mueve `mover_en_mapa`): `juego.colision(yo, enemigo)`.

## 0.9.0

Mapas de Tiled, física sencilla, controles a gusto del jugador y un manual completo.

### Descargas

| Archivo | Qué es |
|---|---|
| `lince-0.9.0-linux.tar.gz` | Lince para Linux (x86_64). Para abrir ventanas hace falta SDL2: `sudo apt install libsdl2-2.0-0` |
| `lince-0.9.0-windows.zip` | Lince para Windows (x86_64), con SDL2 dentro |
| `lince-android.apk` | La plantilla de Android que usan `lince repartir --android` y `--aab` (ya va dentro de los dos anteriores) |
| `lince-language-0.9.0.vsix` | La extensión de VS Code: `code --install-extension lince-language-0.9.0.vsix` |

### Mapas de Tiled

- `juego.mapa_tiled("nivel.tmx")` lee los mapas hechos con [Tiled](https://www.mapeditor.org): `.tmx` y `.tmj`, con sus tilesets (`.tsx`, `.tsj`), capas en CSV o base64 (con zlib o gzip), objetos y propiedades. Da un mapa de Lince normal, así que `mover_en_mapa`, `camino` y las demás funciones sirven igual.
- `juego.dibujar_tiled(mapa[, capas])` lo dibuja con los tiles animados, volteados y girados, la opacidad, el desplazamiento y el paralaje de cada capa.
- Lo sólido y las plataformas se marcan en Tiled: una capa «Colisiones» o la propiedad `solido` en una capa o en un tile.
- `lince compilar` y `lince repartir` meten dentro el mapa, sus tilesets y sus imágenes.

### Física sencilla

- `juego.fisica`, `juego.cuerpo` y `juego.avanzar_fisica`: cuerpos con gravedad, masa, rebote y fricción que chocan entre ellos y con el mapa, plataformas y ascensores que llevan lo que tienen encima, y sensores (`juego.tocando`) para monedas y zonas. Está escrita en Lince.
- Ejemplo nuevo: `examples/13_fisica.lince`.

### Documentación

- Una página nueva para el módulo `juego` ([arreina.github.io/lince/juego.html](https://arreina.github.io/lince/juego.html)), con un ejemplo para cada tema y la captura real de lo que dibuja.
- El manual del lenguaje, completo: las funciones que ya vienen, los módulos `matematica`, `texto`, `json`, `bytes` y `bits`, y las herramientas (compilar, depurar, el REPL, paquetes y editores).

### Más seguro

- `make fuzz` prueba los lectores (JSON, Tiled, imágenes, OGG y el de Lince) con miles de archivos estropeados al azar, también en la CI. Encontró y se arreglaron fallos en el lector de JSON y en el de OGG, que con un archivo estropeado podía cerrar el juego.

### Controles a gusto

- `juego.menu_controles()` deja al jugador cambiar las teclas y botones de cada acción. Lo cambiado se guarda y se aplica solo la próxima vez, sin que el juego haga nada más.
- `juego.restablecer_controles()` y `juego.entrada_pulsada()` (lo que se acaba de pulsar, para hacer un menú propio).
- La aventura (`examples/12_aventura.lince`) tiene «Controles» en la pausa.
- `juego.cargar()` ya no crea la carpeta de las partidas si no existe.

## 0.8.0

Publicar en Google Play, partidas guardadas, y menús y diálogos.

### Descargas

| Archivo | Qué es |
|---|---|
| `lince-0.8.0-linux.tar.gz` | Lince para Linux (x86_64). Para abrir ventanas hace falta SDL2: `sudo apt install libsdl2-2.0-0` |
| `lince-0.8.0-windows.zip` | Lince para Windows (x86_64), con SDL2 dentro |
| `lince-android.apk` | La plantilla de Android que usan `lince repartir --android` y `--aab` (ya va dentro de los dos anteriores) |
| `lince-language-0.8.0.vsix` | La extensión de VS Code: `code --install-extension lince-language-0.8.0.vsix` |

### Google Play

- `lince repartir juego.lince --aab` hace el `.aab` firmado que pide Google Play, sin Java ni el SDK de Android. La plantilla de Android de esta versión lleva lo necesario para hacerlo (la de la 0.7.0 no sirve para el `.aab`).
- `--version N` pone el número de versión del `.apk` o del `.aab`. Cada versión nueva lleva un número mayor.
- Los juegos para Android apuntan a Android 16 (API 36) y sus bibliotecas sirven en móviles con páginas de memoria de 16 KB, como pide Google Play.

### Partidas guardadas

- `juego.guardar("partida", datos)`, `juego.cargar("partida"[, por_defecto])` y `juego.olvidar("partida")` guardan récords, opciones y avances en la carpeta de datos del usuario (también en Android). Con un diccionario por defecto, las opciones nuevas de una versión posterior del juego toman su valor solas.
- La serpiente (`examples/02_serpiente.lince`) guarda el récord.
- `json` se ha rehecho: textos de cualquier largo, `\u00f1` y emojis, decimales que se leen iguales, claves con comillas, `json.texto(valor, verdadero)` en varias líneas y un error si el texto no es JSON válido.

### Menús y diálogos

- `juego.menu(["Jugar", "Salir"])` dibuja un menú y devuelve la opción elegida. Se maneja con teclado, mando, ratón o dedo, y tiene `atras` para las pausas.
- `juego.dialogo(paginas)` abre una caja de diálogo con el texto saliendo letra a letra, quién habla, un retrato y respuestas para elegir (`juego.respuesta()`). Las páginas largas se parten solas.
- Están escritos en Lince, en la biblioteca del módulo `juego`.
- Ejemplo nuevo: `examples/12_aventura.lince`.

## 0.7.0

La primera versión publicada. Lince pasa a ser un lenguaje para hacer videojuegos.

### Descargas

| Archivo | Qué es |
|---|---|
| `lince-0.7.0-linux.tar.gz` | Lince para Linux (x86_64). Para abrir ventanas hace falta SDL2: `sudo apt install libsdl2-2.0-0` |
| `lince-0.7.0-windows.zip` | Lince para Windows (x86_64), con SDL2 dentro |
| `lince-android.apk` | La plantilla de Android que usa `lince repartir --android` (ya va dentro de los dos anteriores) |
| `lince-language-0.7.0.vsix` | La extensión de VS Code: `code --install-extension lince-language-0.7.0.vsix` |

Se descomprime y se usa: `lince examples/01_pong.lince`.

### Videojuegos

- El módulo `juego`: ventana, dibujo, texto con fuentes `.ttf`, imágenes y hojas de sprites, sonido (WAV, OGG en streaming y efectos de 8 bits generados), teclado, ratón, mandos, cámara y mapas de casillas con caminos.
- Acciones que juntan teclado, ratón, mando y pantalla táctil, con combos, `mantenida` y `pulsaciones`.
- Animador, escenas con fundido, partículas y curvas. Están escritos en Lince, dentro del ejecutable.
- Recarga en caliente: al guardar un cambio, el juego sigue con el código nuevo y sin perder el estado.
- `lince nuevo mi_juego` crea un juego pequeño y completo para empezar a cambiarlo.
- Siete minijuegos en `examples/` y el tutorial [«Tu primer juego»](https://arreina.github.io/lince/tutorial.html).

### El lenguaje

- Tareas: `tarea parpadear() { ... esperar(0.5) ... }` es código que espera sin parar el juego.
- Generadores que se pueden guardar en una variable.
- Enumeraciones con valor propio, que se pueden recorrer, y avisos cuando a un `elegir` le falta algún caso.
- `romper`, `continuar`, `+=`, `++` (también `mi.vidas--`), `sea [a, b] = lista` y llamadas a cualquier expresión que dé una función.
- Clases con visibilidad, estáticos, abstractas, propiedades y operadores.

### Compartir un juego

- `lince compilar` hace un ejecutable con el juego dentro.
- `lince repartir` hace una carpeta y un `.zip` con el juego, sus imágenes y sonidos, y SDL2, para Linux o Windows.
- `lince repartir --android` hace el `.apk` para instalarlo en un móvil Android, sin Java ni el SDK de Android. Lleva un mando en la pantalla que sale solo, usa los mandos que se conecten (por cable o Bluetooth) y tiene vibración e inclinación.

### Herramientas

- Servidor de lenguaje (`lince lsp`) y extensión de VS Code, con avisos, autocompletado e ir a la definición.
- Depurador paso a paso (`lince depurar`).
- `lince version`.

Funciona en Linux y en Windows. macOS no está soportado.
