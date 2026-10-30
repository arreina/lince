# Bichito — prototipo gris de «La clase»

Prototipo del capítulo 3 del documento de diseño. Es un **prototipo gris**: sirve
para probar el bucle jugable, no el aspecto. Todo son cajas, el protagonista es un
muñeco de píxeles y el único arte que se respeta son las gafas.

```
./lince juegos/bichito_prototipo/la_clase.lince
```

Teclas: `←` `→` andar · `↓` quedarse quieto · `Z` mirar · `1` intensidad suave · `ESC` salir.

## Para qué sirve

El documento deja tres preguntas abiertas (sección 13) y este prototipo existe para
contestarlas jugando, no discutiendo:

- **¿Qué hace el jugador minuto a minuto?** Aquí: andar, acercarse, pararse, mirar.
  Nada más. Si eso aburre, el capítulo necesita otro verbo.
- **¿Hace falta saltar?** Este prototipo no tiene salto a propósito. Se juega y se ve
  si se echa de menos.
- **¿La pausa se siente como algo que ayuda?** Es lo que más conviene probar: está
  puesta de forma que alivia sin resolver.

## Cómo está montado el sonido

Cada cosa que suena en el aula (reloj, ventana, zumbido del fluorescente, voces,
lápiz) está **en un sitio** del aula. Lo que se oye depende de dónde esté Bichito:
el volumen y el lado salen de la distancia, en cada fotograma.

**Suena de verdad, y sin un solo archivo de audio.** Cada capa es un
`juego.crear_tono()` en bucle: el zumbido es una cuadrada de 100 Hz, las voces son
ruido amortiguado (un murmullo, no palabras), el reloj una cuadrada aguda. Cuando
haya sonido grabado, lo único que cambia es una línea por capa: donde pone
`crear_tono(...)` pondrá `juego.sonido("voces.ogg")`. Todo lo demás sigue igual.

El recurso central de la sección 9 del documento —«la mezcla puede amortiguar el
conjunto y, al mismo tiempo, hacer que ciertos detalles se perciban con gran
nitidez»— es `juego.amortiguar()` más la opción `"nitido"`. Amortiguar **no es
bajar el volumen**: se van los agudos y queda el cuerpo del sonido, como oír el
aula desde el pasillo. Aquí el aula se amortigua cuando se acumulan estímulos, al
pararse y al mirar el patrón; y el reloj y el zumbido, una vez se han hecho su
sitio, dejan de pasar por el filtro y se quedan nítidos encima de todo lo demás.
Eso es exactamente el paso 2.

El panel de arriba a la izquierda enseña la mezcla; eso es un andamio para poder
probar, en el juego de verdad no se vería, se oiría.

Los seis pasos de la sección 7 del documento:

| Paso | Cómo está hecho |
|---|---|
| 1 Exploración | el aula mide tres pantallas y al empezar está tranquila |
| 2 Atención | el reloj y el zumbido suben solos durante los primeros 35 segundos: no suenan más fuerte, es que Bichito deja de poder no oírlos |
| 3 Acumulación | al fondo se juntan voces + zumbido + lápiz, y la imagen se cierra por los bordes |
| 4 Pausa | `↓` baja todas las capas pero **ninguna llega a cero** |
| 5 Patrón | `Z` cerca de un pupitre alto: un rato de calma, no abre ninguna puerta |
| 6 Salida | la puerta está abierta desde el primer segundo |

## Dos decisiones que conviene discutir

**No hay barra de estrés.** El documento avisa de que un número de saturación se
parece demasiado a gestionar una enfermedad. Aquí no se mide nada: lo que cambia es
qué se oye y cuánto se ve. El cierre por los bordes es un estrechamiento de la
atención, no un efecto de pánico — nada tiembla ni parpadea.

**El puzle de alinear pupitres no está.** El propio documento pide revisarlo para que
no sugiera que el niño tiene que ordenar el mundo para merecer avanzar. En su sitio
hay un patrón que se puede *mirar*: uno de cada tres pupitres es más alto, y fijarse
en ese ritmo da un rato de concentración agradable que no desbloquea nada. Es una
propuesta, no una decisión.

Además, lo que pide la sección 6:

- **La autorregulación no es una llave mágica.** Quedarse quieto se nota, pero el
  aula sigue ahí. Se ve mirando el círculo de las voces: encoge, no desaparece.
- **No se castiga sentirse desbordado.** No hay forma de perder ni de hacerlo mal.
- **Intensidad ajustable sin penalización.** `1` baja el cierre y los círculos sin
  cambiar nada más de la escena.

## Lo que no es

No tiene arte final, ni música, ni la hermana, ni el aula de verdad. Las capas de
sonido son tonos generados: tienen la forma correcta y se mueven como deben, pero
un zumbido sintético no es el zumbido grabado de un fluorescente. Eso llega con los
archivos de audio, y no hace falta tocar la lógica para meterlos.

Cuando Bichito pase de prototipo a proyecto, esta carpeta se va a su propio
repositorio. Aquí está de momento porque sirve de banco de pruebas del lenguaje.

## Probarlo sin pantalla

Se puede capturar sin abrir ventana, que es como se han revisado los seis pasos:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
LINCE_JUEGO_FOTOGRAMAS=760 LINCE_JUEGO_GUION=2:derecha:750 \
LINCE_JUEGO_CAPTURA=/tmp/voces.bmp ./lince juegos/bichito_prototipo/la_clase.lince
```

| Momento | Guion |
|---|---|
| arranque tranquilo | `LINCE_JUEGO_FOTOGRAMAS=20` |
| acumulación al fondo | `2:derecha:750` con 760 fotogramas |
| pausa en medio del ruido | `2:derecha:750,756:abajo:400` con 980 |
| mirar el patrón | `2:derecha:360,370:z:260` con 640 |
| salir por la puerta | `2:derecha:1120` con 1180 |

Con `LINCE_JUEGO_AUDIO=mezcla.wav` se guarda además el sonido sincronizado con esos
fotogramas, que es como se ha comprobado que la mezcla hace lo que dice.
