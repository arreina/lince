# 🐆 Probar el equipo

Un programa para comprobar, en un ordenador o un móvil de verdad, que a Lince le
llega todo bien: teclado, ratón, mandos, sonido y pantalla táctil. Es la prueba
que falta para la 1.0.

```
cd examples/probar_equipo
lince probar_equipo.lince                          # Linux o Windows
lince repartir probar_equipo.lince --android       # el .apk, para el móvil
```

Se cambia de página con 1, 2, 3, 4 o Tab, con LB y RB del mando, o tocando las
pestañas de arriba. Para salir, Esc dos veces seguidas (en el móvil, «atrás» dos
veces). Abajo a la izquierda sale siempre lo último que ha llegado.

## Lista de comprobación

Apunta en cada línea en qué equipo lo has probado (por ejemplo «Linux, portátil»,
«Windows 11», «móvil Android 14») y si ha ido bien.

### 1. Teclado y ratón

- [ ] Cada tecla del dibujo se enciende mientras está pulsada (letras, números,
      F1 a F12, flechas, mayúsculas, control, alt, espacio, entrar, borrar, tab, Esc).
- [ ] Al escribir en el recuadro salen bien la **ñ**, las **tildes** (á, é, í, ó, ú),
      la **ü** y los signos **¿** y **¡**, y «borrar» quita la última letra.
- [ ] La posición del ratón sigue al ratón, también a pantalla completa (F11).
- [ ] Los tres botones del ratón se encienden y la rueda cambia el número.

### 2. Mandos

Con cada mando que tengas (por cable y por Bluetooth; Xbox, PlayStation, genéricos):

- [ ] Al conectarlo, sale solo, sin reiniciar el programa; al desconectarlo, desaparece.
- [ ] A, B, X e Y están en su sitio (en un mando de PlayStation: A = ✕, B = ○,
      X = □, Y = △).
- [ ] La cruceta, LB, RB, atrás, guía e inicio se encienden.
- [ ] Las dos palancas mueven su punto, y al soltarlas vuelve al centro.
- [ ] Al pulsar las palancas hacia dentro, se ponen verdes.
- [ ] Los gatillos LT y RT se llenan poco a poco y vuelven a cero.
- [ ] «Vibrar» (o V, o Y en el mando) hace vibrar el mando (si lo admite).
- [ ] Con dos mandos, cada uno sale en su cuadro y no se mezclan.

### 3. Sonido

Con altavoces y con auriculares:

- [ ] El WAV (una moneda) y el OGG corto (un golpe) suenan.
- [ ] La música suena sin cortes ni chasquidos y vuelve a empezar al acabar (dura 25 s).
- [ ] «Por la izquierda» suena solo por la izquierda y «por la derecha», solo por la derecha.
- [ ] «Más agudo» y «más grave» suenan distintos.
- [ ] Los efectos generados suenan, uno distinto cada vez.
- [ ] «Dieciséis sonidos a la vez» suena como un montón de monedas, sin chasquidos.
- [ ] El volumen general sube y baja.
- [ ] Al quitar los auriculares o cambiar de salida de sonido, el sonido sigue.

### 4. Táctil y móvil

En el móvil:

- [ ] Sale apaisado y a pantalla completa, sin las barras del sistema.
- [ ] Cada dedo es un círculo con su número, y con varios dedos a la vez salen todos.
- [ ] La bola de «Inclinación» rueda hacia donde se inclina el móvil.
- [ ] «Vibrar» hace vibrar el móvil.
- [ ] «Atrás» dos veces cierra el programa; una vez sola, no.
- [ ] Al salir a otra app y volver, sigue a pantalla completa y con sonido.
- [ ] Con un mando por Bluetooth conectado al móvil, la página de mandos lo ve.

Si algo falla, apunta el equipo, el sistema y lo que sale en «Lo último».
