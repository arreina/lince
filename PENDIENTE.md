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

**La 2.0 está publicada (2026-10-27): el lenguaje queda congelado otra vez**, como pasó con la 1.0 (2026-10-25). Desde ahora el lenguaje solo se amplía (lo que se añade no rompe juegos ya escritos) y el trabajo pasa al IDE (lince-motor, en su propia conversación). Lo que hubo que hacer:

- [x] **El lenguaje no cambia de forma que rompa juegos ya escritos.** Desde la 1.0 se cumple ese compromiso.
- [x] **Probado en real** por el autor con `examples/probar_equipo`: Linux y Android, con teclado, ratón, sonido y un mando de PS5 conectado al PC; todo bien. **Sin probar en un equipo real: Windows** (se probó con wine y en la CI de Windows) **y los mandos de Xbox y genéricos**. Si sale algún fallo ahí, se arregla como cualquier otro. Subir un juego a Google Play queda para después de la 1.0.
- [x] **Algún juego completo hecho con Lince**, no solo los ejemplos, para que salga lo que falta: `juegos/la_piedra_del_lince`. Probado con wine (Windows) y en el emulador de Android 16.
- [x] **El manual lo cubre todo:** el del lenguaje (con los módulos `matematica`, `texto`, `json`, `bytes` y `bits` y las herramientas) y una página para el módulo `juego` (`docs/juego.html`) con un ejemplo y su captura real para cada tema. La lista completa de funciones y opciones sigue en la especificación.

## Camino a la 2.0

Plan aprobado por el autor el 2026-10-25. Las 1.x **solo añaden**: ningún juego escrito deja de
funcionar. Lo que haya que cambiar de verdad se marca como obsoleto durante las 1.x y se quita en
la 2.0, con una herramienta que actualiza los juegos sola. **Cada versión nueva se le avisa a
lince-motor** para que el IDE la incorpore (resaltado, LSP, autocompletado, ayudas).

### 1.2 — Lo pequeño que quedó pendiente del lenguaje (publicada)
- [x] Comparar textos con `<`, `>`, `<=`, `>=` en orden alfabético de verdad (tildes junto a su
  letra, la ñ entre la n y la o, sin distinguir mayúsculas salvo para desempatar). `ordenar()`
  usa el mismo orden, que es lo que ya decía la especificación.
- [x] `"a" * 3` → `"aaa"` y `[0] * 10` (también al revés: `3 * "a"`).
- [x] Números como claves de diccionario: `{1: "uno"}`, `d[3] = x`; `claves()` y `para cada`
  las devuelven como números.
- [x] `a_numero(" 3 ")`: acepta espacios alrededor.
- [x] `x ?? 0` (si `x` es nulo, vale lo de la derecha) y `jugador?.arma` / `jugador?.atacar()`
  (nulo en vez de error si `jugador` es nulo).
- [x] `texto.decimales(3.5, 2)` → «3.50» y `texto.miles(12000)` → «12.000».

### 1.3 — Tipos opcionales y avisos sin ejecutar (publicada)
Lince ya llevaba tipos en los parámetros y en el retorno, delante del nombre; se siguió esa forma
(`sea numero vida`) en vez de la `vida: numero` del primer borrador, para no tener dos maneras de
escribir lo mismo. Y en vez de un `lince revisar` nuevo, los avisos los da el `lince comprobar` de
siempre, que es el que ya usa el editor.
- [x] Tipo delante del nombre en `sea` / `fijo` y en los campos de clase: `sea numero vida = 3`.
  Al ejecutar se comprueba el valor inicial; los cambios posteriores, sin ejecutar.
- [x] `lince comprobar` (y el editor) avisan sin ejecutar: nombres que no existen (con «¿querías
  decir…?»), lo usado antes de definirse, variables sin usar, código que no se ejecuta, tipos que
  no casan, llamadas con los argumentos que no son, campos y métodos que la clase no tiene,
  miembros que no tiene un módulo, un alias o una enumeración, y dar otro valor a un `fijo`
  (`src/revisor.c`).
- [x] El alias de `importar ... como` vale también como tipo: `x es geo.Punto`, `extiende geo.Punto`,
  `implementa geo.Forma`, parámetros y variables.

### 1.4 — Herramientas (publicada)
`lince probar` no existía: se hizo entero (las pruebas son los archivos `prueba_*.lince` y
`*_prueba.lince`), con la cobertura dentro.
- [x] `lince formatear` (estilo único; el editor formatea igual y la extensión de VS Code lo hace
  al guardar). Comprueba token a token que el programa no cambia (`src/formatear.c`).
- [x] `lince documentar`: la ayuda sacada de los comentarios de encima de cada cosa; el editor la
  enseña al pasar el ratón (`src/documentar.c`, `textDocument/hover`).
- [x] `lince probar` con cobertura: qué sentencias han ejecutado las pruebas y qué líneas no.
- [x] `lince perfil`: el tiempo de cada fotograma en Lince, en los módulos (dibujar…) y en mostrar y
  esperar, y las funciones que más tardan.
- [x] Paquetes con versión fija (`url@v1.2`) en `paquetes/` del juego y `lince.paquetes` con la
  huella SHA-256; `lince instalar` sin nada los baja tal cual.

### 1.5 — Gráficos y texto en pantalla (publicada)
En C solo lo que toca el dibujo (las capas, y el zoom, el giro y la zona de la cámara); lo demás
está en `juego.lince`.
- [x] Capas de dibujo con orden (`"capa": 3`, `juego.capa(n)`).
- [x] Cámara con zoom y giro; varias cámaras con `zona` (pantalla partida); `a_mundo` y `a_pantalla`.
- [x] Medir texto ya estaba (`medir_texto`, `"ancho"`); se añade `texto_caja` (alto, «...», lo que
  falta para la página siguiente, alineación vertical).
- [x] Traducciones: `idiomas/<código>.txt`, `traducir`, `idioma`, `idiomas`, `menu_idiomas`; los menús
  y diálogos de Lince salen traducidos.
- [x] Tiled: capas con zstd y mapas infinitos.
- [ ] Tiled isométricos y hexagonales: solo si algún juego los pide.

### 1.6 — Física y sonido
- [x] Cuerpos que giran, círculo contra caja de verdad, polígonos, uniones sencillas (cuerda, bisagra).
  En Lince (Box2D Lite: impulsos con arranque en caliente); en C solo `poligono()` para dibujar.
- [x] Grupos de volumen (música, efectos, voces), sonido a izquierda o derecha según la
  posición, paso suave entre músicas. En C lo del mezclador (grupos, fundidos, dos músicas a la
  vez); el sonido con sitio, en Lince.
- [ ] Con muchos cuerpos que giran (unos 40 o más) la física pasa de 6 ms por fotograma: se mira en
  la 1.7 (rendimiento), si algún juego lo necesita.

### 1.7 — Rendimiento (solo si hace falta)
- [ ] La máquina virtual de bytecode («camino B», ver Rendimiento), si `lince perfil` demuestra que Lince es el cuello de botella.
- Revisado el 2026-10-26: **no hace falta por ahora**.
  - Con `lince perfil`, jugando 600 fotogramas, el código Lince ocupa como mucho 0,21 ms por fotograma en los 13 ejemplos, *La piedra del lince* y las demos de lucha.
  - Con la tarjeta gráfica, la demo de lucha tarda 1,9 ms por fotograma, casi todo de SDL. Los 15 ms que salían en las pruebas eran por dibujar sin tarjeta gráfica.
  - Lo único que se encontró fue un tirón de 90 ms en el primer destello (`"blanco"`), arreglado en la 1.6.1.
  - Donde más pesa Lince es en la física con muchos cuerpos que giran (36 cajas, 6 ms).

### 1.8 — Varios jugadores en el mismo equipo
- [x] Cada mando, un jugador: «pulsa un botón para unirte», controles por jugador; con la pantalla partida de la 1.5.
  En Lince (`unirse`, `menu_jugadores`, copias de las acciones por jugador); en C, que cada mando
  conserve su número y el jugador en `accion`/`direccion`.
- [ ] Probarlo con varios mandos de verdad, conectando y desconectando (lo hace el autor).

### 1.9 — Preparar la 2.0
- [x] Lo nuevo de la 2.0 (ver abajo) ya funciona, y lo que va a cambiar avisa «obsoleto: usa X»
  (al ejecutar, en `lince comprobar` y en el editor).
- [x] `lince migrar --comprobar` (qué cambiaría) y `lince migrar` (lo cambia solo). Los ejemplos,
  el juego, la web y las pruebas ya están migrados con él.
- [x] Última versión que añade cosas; después, solo arreglos.

### 2.0 — Limpieza, no un lenguaje nuevo
Cada cambio que rompa, aprobado por el autor uno por uno. **Aprobados el 2026-10-26 y hechos en la 2.0.0**; el
detalle de cada uno está en PROPUESTA_2.0.md:
- [x] A1 `juego.fisica.*` (mundo, cuerpo, avanzar, quitar, tocando, empujar, bisagra, cuerda…).
- [x] A2 `juego.mapa.*` (crear, tiled, casilla, poner, solido, casilla_en, buscar, mover, dibujar, dibujar_tiled, camino).
- [x] B1 `reproducir`, B2 `musica` y B3 `texto`, solo con opciones (sin volumen, bucle, escala, color ni fuente sueltos).
- [x] B5 una sola `colision(a, b)` (cajas, puntos, círculos y cuerpos); fuera `colision_circulos`, `colision_circulo_rect` y `dentro`.
- [x] B6 fuera `posicion_camara()`.
- [x] C1 `guardar_partida` / `cargar_partida` / `olvidar_partida`; C2 `quitar_particulas`; C3 `"angulo"` en la cámara.
- [x] D1 fuera `val`; D2 lo de un texto, como métodos (`t.dividir(",")`); D3 fuera `texto.a_numero`, `de_numero`, `repetir` y `es_vacio`.
- [x] D4 las anotaciones de tipo, comprobadas también al ejecutar.
- No: el sonido en partes (A3), quitar `ancho_texto` (B4) ni cambiar `clic` (C4).
- [x] La 2.x se congela igual que la 1.x: desde la 2.0.0 solo se añade lo que no rompe juegos ya escritos.
- [ ] Probar `lince migrar` con lince-motor y con los juegos de otros, y pasar a la 2.0 las demos de lucha (el autor).

## Camino a la 3.0

Decidido por el autor el 2026-10-27. Cada 2.x solo añade: nada de lo que funciona en la 2.0 se
rompe. El orden puede cambiar si un juego de verdad (o lince-motor) pide antes otra cosa.

### 2.1 — Pantalla de opciones
- [ ] `juego.menu_opciones()` hecha: volumen de música, efectos y voces, pantalla completa,
  controles (`menu_controles`) e idioma (`menu_idiomas`), que se guarda sola y se aplica al
  abrir el juego. Que el juego pueda añadir sus propias opciones.

### 2.2 — Accesibilidad
- [ ] En las opciones: texto más grande, colores para daltonismo (paletas seguras y, si se puede,
  un filtro), subtítulos de los sonidos (`reproducir(s, {"subtitulo": "Puerta"})`) y quitar el
  temblor de la cámara y los destellos.

### 2.3 — Luces y sombras 2D
- [ ] Oscurecer la escena y poner luces con su color, radio y forma (antorchas, linternas, el
  brillo de un disparo), que respeten la cámara y las capas.

### 2.4 — Enemigos con cabeza
- [ ] Máquinas de estados (patrullar, perseguir, atacar, huir) y comportamientos de movimiento
  (seguir, huir, esquivar, rodear, en grupo), escritos en Lince.

### 2.5 — Pruebas de juegos
- [ ] Que `lince probar` juegue solo: guion de entradas (teclas, mando, toques), comprobar
  píxeles y capturas, y comparar con la vez anterior para ver qué ha cambiado.

### 2.6 — Paquetes de escritorio
- [ ] `lince repartir`: instalador de Windows, AppImage de Linux y un `.zip` listo para itch.io,
  con el icono y el nombre del juego.

### 2.7 — Google Play de verdad
- [ ] `lince repartir --aab` deja el juego listo para subir: iconos en todos los tamaños,
  versión, firma de subida y lo que pide la ficha de la tienda. Probado subiendo uno de prueba
  (hace falta la cuenta de desarrollador de Google Play del autor).

### 2.8 — Preparar la 3.0 (solo si hay limpieza)
- [ ] Como la 1.9: lo que vaya a cambiar avisa y `lince migrar` lo cambia solo.

### 3.0 — Otra limpieza, solo si hace falta
- [ ] Lo que haya quedado mal en la 2.x se va apuntando aquí. Si al llegar hay algo, propuesta,
  el autor aprueba cada cambio y `lince migrar`, como en la 2.0. Si no hay nada, no hay 3.0.

Fuera del plan (decisión del autor): juego en red (se quitó a propósito), consolas (depende de
que Nintendo y Sony aprueben; iría en un repositorio privado), macOS y navegador (descartados).
Solo si un juego lo pide: la máquina virtual (ver Rendimiento) y los mapas isométricos y
hexagonales de Tiled.

## Lenguaje

**Revisión antes de congelar en la 1.0** (decidido por el autor el 2026-10-23):

- [x] `==` compara listas y diccionarios **por contenido** (también `contiene()` y `posicion()`); los objetos, como ahora (el mismo objeto o su método `igual`). Arreglar de paso `posicion()`, que hoy da por iguales todas las listas, diccionarios y objetos (`enemigos.posicion(e)` da siempre 0).
- [x] `ref` **que funcione**: lo que se asigna a un parámetro `ref` cambia la variable que se pasó. Hoy `val` y `ref` hacen lo mismo. Las listas y diccionarios se comparten siempre; corregir en la especificación «`val` — copia».
- [x] `val` **opcional** (sin poner nada = `val`).
- [x] En un `si`, **una lista o un diccionario vacíos cuentan como falso**, como `""`, `0` y `nulo`.
- [x] `x % 0` da `ErrorMatematico`, como `x / 0` (antes daba `nan`).
- [x] **Indexar un texto escrito tal cual** (`"hola"[0]`), y de paso sus métodos
  (`"hola".mayusculas()`): estaba apuntado para después de la 1.0, pero entró en la
  0.14.0. Es aditivo y no rompe nada. Queda pendiente el resto de esa lista.
- Para después de la 1.0 (no rompen nada): comparar textos con `<`, `"a" * 3`,
  números como claves de diccionario, `a_numero(" 3 ")` con espacios. Van en la 1.2
  (ver «Camino a la 2.0»).

- [x] `mapear`, `filtrar` y `reducir` aceptan también generadores y bytes (salió escribiendo el manual).

- [x] **Todo error es capturable, también en los métodos** (0.14.0): `"x".contiene(5)`,
  `d.contiene(5)` y `d.eliminar(5)` reventaban el proceso (leían la unión como `char*`);
  ahora dan `ErrorTipo`. Auditados con argumentos del tipo equivocado todos los métodos y
  todas las funciones de los módulos: no quedaba ningún otro.

- [x] **Una línea que empieza por `[` es otra sentencia** (0.14.0), no un índice de la
  anterior: `sea otra = a` y debajo `[a, b] = [b, a]` fallaba, porque la rama del índice de
  `postfijo()` no exigía la misma línea como ya hacía la de la llamada.

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
- **Palabras reservadas que no pueden ser nombres:** entre otras, `no`, `lista`, `mi`, `caso`,
  `otro`, `romper` y `continuar`.
  - `y` y `o` **sí valen** como nombres desde la 0.11.0: son el operador sólo entre dos
    valores. También en la desestructuración, así que `sea [x, y] = pos` funciona.
  - `en`, `cualquiera`, `tarea`, `es`, `como`, `obtener`, `fijar`, etc. solo cuentan en su sitio.
- **Ya están en Lince** los combos, `mantenida`, `pulsaciones`, las curvas, `suavizar`, `acercar`, las escenas y el animador. `presentar()` llama en cada fotograma a `_presentar()` de la biblioteca; `dibujar()` y `caja()` llaman a `_cuadro()` con un animador; las variables de la biblioteca se conservan al recargar en caliente.
- **En C se queda** lo que habla con la pantalla, el teclado o el altavoz, o mira píxeles: dibujar, las hojas (dividirlas, la caja de lo que se ve, las pisadas), los sonidos, las fuentes, los mapas.

## Encontrado repasando la especificación (0.14.0)

- [x] La especificación decía que las clases de una librería **no** se podían instanciar a
  través del alias de `importar ... como`, y sí se puede (`geo.Punto(1, 2)` funciona). El
  límite que queda de verdad es otro: el alias no vale donde se espera un *tipo*
  (`x es geo.Punto`, `clase Mia extiende geo.Punto` son errores de sintaxis). Corregido el
  texto.
- [x] **Repasada la especificación entera contra el código** (0.14.0), sección por sección.
  Salieron dos fallos que cerraban el programa —`lanzar e` dentro de un `capturar`, y
  `elegir` casando con el caso equivocado cuando los tipos no coincidían— y cuatro cosas
  que el documento decía y no eran ciertas (`sea [x, y]`, las clases por el alias, el campo
  que «gana» a la propiedad, y que `capturar` es obligatorio). Todo corregido y con test de
  regresión. Lo que no se pudo comprobar aquí: el módulo `juego` con ventana (sin pantalla
  en el contenedor; sí lo de `hoja()`, `imagen()` y `guardar_imagen()` sin ventana) y
  `make bench` (le falta `/usr/bin/time`), aunque los tiempos medidos a mano cuadran con
  los de la especificación.
