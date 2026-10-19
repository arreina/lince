# 🐆 La Piedra del Lince

Una aventura de vista cenital hecha con Lince, para Linux, Windows y Android.

Grum, el cíclope, ha robado la Piedra del Lince, la que protege el pueblo de
Valleclaro. La sabia Ura te da la espada de tu abuelo: cruza el Bosque Viejo,
encuentra la llave de la fortaleza, vence a Grum y devuelve la piedra.

## Jugar

```
cd juegos/la_piedra_del_lince
lince la_piedra_del_lince.lince
```

| | Teclado | Mando |
|---|---|---|
| Andar | flechas o WASD | cruceta o palanca |
| Hablar, abrir, espadazo | espacio, Z o entrar | A |
| Pausa (y cambiar los controles) | Esc o P | inicio |

En el móvil sale un mando en la pantalla. La partida se guarda sola al pasar de
un sitio a otro, y también en el pozo del pueblo.

Para repartirlo: `lince compilar la_piedra_del_lince.lince` (un ejecutable con
todo dentro) o `lince repartir la_piedra_del_lince.lince` (Windows, Linux y Android).

## Qué tiene

- Tres mapas de **Tiled** (`mapas/*.tmx`): el pueblo, el bosque y la fortaleza. Se
  abren y se cambian en [Tiled](https://www.mapeditor.org); lo que se mueve
  (gente, enemigos, cofres, arbustos, salidas) son objetos de la capa «Cosas».
  `hacer_mapas.py` los hizo la primera vez; si los retocas en Tiled, no lo
  vuelvas a ejecutar.
- Diálogos con retrato y respuestas, una tienda, carteles, cofres y una partida
  guardada (`juego.dialogo`, `juego.guardar`).
- Babosas, murciélagos, arañas y fantasmas, cada uno con su manera de moverse, y
  Grum, que lanza rocas y llama a sus murciélagos cuando se enfada.
- Arbustos que se cortan, monedas y corazones que caen, una reja que se abre al
  vaciar la sala.
- Portada, pausa, controles a gusto del jugador (`juego.menu_controles`), derrota
  y créditos.

Los gráficos, la música, los sonidos y la letra son de terceros con licencias
libres (Kenney, cynicmusic, hornpipe2, Press Start 2P): ver [CREDITOS.md](CREDITOS.md).
