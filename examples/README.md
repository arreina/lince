# 🐆 Minijuegos de ejemplo

Ocho juegos pequeños y completos, cada uno pensado para enseñar una parte de
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
| `02_serpiente.lince` | flechas o WASD | listas de listas, tiempo acumulado con `dt`, funciones con variables globales, **escribir tu nombre** para el récord (`juego.editar`) |
| `03_rompeladrillos.lince` | flechas o A / D, espacio lanza | lista de diccionarios, rebotes y colisiones con rectángulos, estados de juego |
| `04_pajaro.lince` | espacio salta | gravedad, obstáculos que se crean y descartan, **guardar el récord** con `archivos` y `json` |
| `05_esquiva.lince` | flechas o A / D | **clases, herencia y polimorfismo** (`Objeto` → `Meteorito` y `Estrella`), `es` |
| `06_memoria.lince` | ratón | el ratón, una máquina de estados y temporizadores sin bloquear el bucle |
| `07_tres_en_raya.lince` | ratón | **recursión** (minimax): la máquina nunca pierde |
| `08_plataformas.lince` | flechas / A-D y espacio, o **mando** | nivel escrito como texto, **sprites dibujados con texto** y animados (`juego.sprite`, hoja de sprites), gravedad y colisiones con el mapa, **cámara** con marcador fijo, **acciones** (cada control definido una vez para teclado y mando), **partículas**, **escenas** con fundido (portada y nivel) y animación con `juego.cuadro` |

Todos terminan con **Esc**.

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
