# 🐆 Minijuegos de ejemplo

Diez juegos pequeños y completos, cada uno pensado para enseñar una parte de
Lince. Se juegan así (necesitan SDL2; en Debian/Ubuntu: `sudo apt install libsdl2-2.0-0`):

```sh
make                                  # compilar el intérprete
./lince examples/01_pong.lince
```

Y se reparten como un único ejecutable:

```sh
./lince compilar examples/02_serpiente.lince -o serpiente
./serpiente
```

| Juego | Cómo se juega | Qué enseña |
|-------|---------------|------------|
| `01_pong.lince` | W / S y flechas; 1 o 2 jugadores | el bucle `mientras juego.abierto()`, `dt()`, teclado, colisiones, texto y sonido |
| `02_serpiente.lince` | flechas o WASD | listas de listas, tiempo acumulado con `dt`, funciones con variables globales, **escribir tu nombre** para el récord (`juego.editar`), que **se guarda** para la próxima vez (`juego.guardar` y `juego.cargar`) |
| `03_rompeladrillos.lince` | flechas o A / D, espacio lanza | lista de diccionarios, rebotes y colisiones con rectángulos, estados de juego |
| `04_pajaro.lince` | espacio salta | gravedad, obstáculos que se crean y descartan, **guardar el récord** con `archivos` y `json` |
| `05_esquiva.lince` | flechas o A / D | **clases, herencia y polimorfismo** (`Objeto` → `Meteorito` y `Estrella`), `es` |
| `06_memoria.lince` | ratón | el ratón, una máquina de estados y temporizadores sin bloquear el bucle |
| `07_tres_en_raya.lince` | ratón | **recursión** (minimax): la máquina nunca pierde |
| `08_plataformas.lince` | flechas / A-D y espacio, o **mando** | nivel escrito como texto, **sprites dibujados con texto** y animados (`juego.sprite`, hoja de sprites), gravedad y un **mapa de casillas** (`juego.mapa`, `juego.mover_en_mapa`), **cámara** con marcador fijo, **acciones** (cada control definido una vez para teclado y mando), **partículas**, **escenas** con fundido (portada y nivel) y animación con `juego.cuadro` |
| `09_mazmorra.lince` | flechas o WASD, o **mando** | un juego de **vista cenital**: el mismo mapa de casillas sin gravedad, casillas que cambian (`juego.poner_casilla`) y **enemigos que te persiguen** esquivando las paredes (`juego.camino`) |
| `10_paseo.lince` | flechas o A / D, Mayús corre, o **mando** | **animaciones generadas** a partir de un solo dibujo (`juego.animar`: andar, correr y respirar), la hoja generada en pantalla, **animación tradicional** con una hoja de sprites que se divide sola (`juego.hoja`, los pájaros), pasos con sonido y un paisaje que se repite con varias capas de profundidad |
| `11_movil.lince` | botones en la pantalla (o el ratón), flechas y espacio | para probar Lince **en un móvil Android** (`android/construir.sh`): `juego.toques()` con un círculo por dedo, **botones táctiles** con `"pantalla:..."` en las acciones, inclinar el móvil para moverse (`"inclinacion:x-"`), vibrar al saltar, una tarea que hace parpadear una estrella y un archivo que cuenta cuántas veces se ha abierto |

| `12_aventura.lince` | flechas o WASD, espacio para hablar, Esc para la pausa | un **menú** de portada y otro de pausa (`juego.menu`) y **diálogos** con el texto saliendo letra a letra, quién habla y respuestas para elegir (`juego.dialogo`, `juego.respuesta`); todo con teclado, mando, ratón o dedo |

| `13_fisica.lince` | flechas o A/D, espacio para saltar, R para empezar otra vez | **física sencilla** (`juego.fisica`, `juego.cuerpo`, `juego.avanzar_fisica`): cajas que se empujan según su masa y se apilan, pelotas que rebotan, un ascensor que lleva lo que tiene encima y monedas que solo avisan al tocarlas (`juego.tocando`) |

Todos terminan con **Esc** (en la aventura, Esc abre la pausa, que tiene «Salir»).

## Probar el equipo

`probar_equipo/` no es un juego: es para comprobar en un ordenador o un móvil de verdad
que a Lince le llega todo bien, con una página para el teclado y el ratón (también la ñ
y las tildes al escribir), otra para los mandos (hasta cuatro, con sus palancas,
gatillos y vibración), otra para el sonido (WAV, OGG, música, izquierda y derecha,
volumen) y otra para la pantalla táctil y la inclinación. Su `LEEME.md` trae la lista
de lo que hay que mirar. Se abre con `cd examples/probar_equipo` y
`lince probar_equipo.lince`, o en el móvil con `lince repartir probar_equipo.lince --android`.

## Prueba la recarga en caliente

Abre `08_plataformas.lince` con `./lince examples/08_plataformas.lince`, avanza un
poco y, sin cerrarlo, cambia en el editor `fijo GRAVEDAD = 1500` por `700`, o el color
de las monedas, y guarda: el juego se actualiza al momento y el lince sigue donde
estaba. **F5** empieza desde cero. Si guardas algo con un error, sale en la ventana.

## Probarlos sin pantalla

Para los tests, el módulo `juego` tiene un modo automático controlado con
variables de entorno: `LINCE_JUEGO_FOTOGRAMAS=N` cierra la ventana tras N
fotogramas (sin esperas), `LINCE_JUEGO_PULSAR=espacio,a` pulsa esas teclas en
los fotogramas impares, `LINCE_JUEGO_CLIC=x,y` hace clic ahí,
`LINCE_JUEGO_ESCRIBIR=Ana` teclea ese texto cuando el juego lo pide (`<` borra) y
`LINCE_JUEGO_CAPTURA=salida.bmp` guarda el último fotograma. Junto con
`SDL_VIDEODRIVER=dummy` no hace falta ni monitor (`make test-ejemplos`).
