# Novedades

Lo que trae cada versión de Lince. La CI publica cada versión en GitHub con el
apartado que le toca de este archivo.

## 0.10.0 (en preparación)

### El lenguaje

- `mapear`, `filtrar` y `reducir` aceptan también generadores (`filtrar(rango(10), ...)`) y bytes.

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
