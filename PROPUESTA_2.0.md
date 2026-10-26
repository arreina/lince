# Propuesta de cambios para la 2.0

La 2.0 es una limpieza, no un lenguaje nuevo, y cada cambio que rompa lo aprueba el autor uno
por uno (PENDIENTE.md). Esta es la lista para decidir. Con lo aprobado se hace la 1.9:

- **Lo nuevo ya funciona en la 1.9.** Por ejemplo, `juego.fisica.cuerpo` existe a la vez que
  `juego.cuerpo`.
- **Lo viejo avisa** con «obsoleto: usa X» en `lince comprobar`, en el editor y al ejecutar
  (una vez por cosa).
- **`lince migrar` lo cambia solo**, siempre que la columna «migrar» diga que sí.

En la 2.0 lo viejo deja de funcionar.

Cada cambio dice cómo es ahora, cómo quedaría, por qué, si `lince migrar` puede hacerlo solo y
cuántas veces sale en los ejemplos, *La piedra del lince*, las dos demos de lucha y la web (para
hacerse una idea de cuánto código toca). Al final de cada uno va mi recomendación.

---

## A. ¿El módulo `juego` en partes?

Hoy `juego` tiene unas 155 funciones, todas al mismo nivel. El plan decía ordenarlo en partes
(`juego.fisica.cuerpo`, `juego.sonido.reproducir`…). Hay un problema: una parte no puede llamarse
igual que una función que ya existe. `juego.sonido(ruta)` carga un sonido y `juego.mapa(filas)`
crea un mapa, así que esas partes obligarían a escribir `juego.sonido.cargar(ruta)` y
`juego.mapa.crear(filas)`.

**A1. `juego.fisica`**. Las 13 funciones de la física pasan a una parte:

| Ahora | En la 2.0 |
|---|---|
| `juego.fisica(op)` | `juego.fisica.mundo(op)` |
| `juego.cuerpo(m, d)` · `juego.quitar_cuerpo(m, c)` | `juego.fisica.cuerpo(m, d)` · `juego.fisica.quitar(m, c)` |
| `juego.avanzar_fisica(m)` | `juego.fisica.avanzar(m)` |
| `juego.tocando` · `juego.empujar` | `juego.fisica.tocando` · `juego.fisica.empujar` |
| `juego.bisagra` · `juego.cuerda` · `juego.quitar_union` · `juego.puntos_union` | `juego.fisica.bisagra` · `.cuerda` · `.quitar_union` · `.puntos_union` |
| `juego.centro_cuerpo` · `juego.puntos_cuerpo` · `juego.dibujar_cuerpo` | `juego.fisica.centro` · `.puntos` · `.dibujar` |

- **Por qué:** `cuerpo`, `cuerda`, `tocando` o `empujar` no dicen nada sueltos en `juego`, y
  `avanzar_fisica` y `quitar_cuerpo` repiten la palabra.
- **Migrar:** sí. **Usos:** 39.
- **Recomiendo:** sí.

**A2. `juego.mapa`**. Las funciones de los mapas de casillas y de Tiled:

| Ahora | En la 2.0 |
|---|---|
| `juego.mapa(filas, t)` · `juego.mapa_tiled(ruta)` | `juego.mapa.crear(filas, t)` · `juego.mapa.tiled(ruta)` |
| `casilla` · `poner_casilla` · `solido` · `casilla_en` | `juego.mapa.casilla` · `.poner` · `.solido` · `.casilla_en` |
| `buscar_en_mapa` · `mover_en_mapa` · `dibujar_mapa` · `dibujar_tiled` · `camino` | `juego.mapa.buscar` · `.mover` · `.dibujar` · `.dibujar_tiled` · `.camino` |

- **Por qué:** quita el «_en_mapa» y el «_mapa» de los nombres, y lo de los mapas queda junto.
- **Migrar:** sí. **Usos:** 58.
- **Recomiendo:** sí.

**A3. `juego.sonido`**. `reproducir`, `musica`, `volumen`, `detener`, `efecto`, `pitido`…
pasarían a `juego.sonido.reproducir`, etc., y `juego.sonido(ruta)` a `juego.sonido.cargar(ruta)`.

- **Por qué no:** los nombres ya son claros, y es lo que más se usa (261 veces): cada línea
  sería más larga sin ganar nada.
- **Migrar:** sí.
- **Recomiendo:** no.

**A4. El resto** (dibujo, entrada, acciones, cámara, escenas, menús, jugadores…) se queda en
`juego` tal cual. Son lo de todos los días y sus nombres ya son claros.

- **Recomiendo:** dejarlo así.

---

## B. Formas duplicadas de hacer lo mismo en `juego`

**B1. `reproducir` con volumen y bucle sueltos.**

| Ahora | En la 2.0 |
|---|---|
| `reproducir(s, 0.5, verdadero)` | `reproducir(s, {"volumen": 0.5, "bucle": verdadero})` |

- **Por qué:** dos formas para lo mismo. La de opciones es la que vale para todo lo nuevo
  (`lado`, `tono`, `grupo`, `fundido`).
- **Migrar:** sí. **Usos:** 56.
- **Recomiendo:** sí.

**B2. `musica` con el volumen suelto.**

| Ahora | En la 2.0 |
|---|---|
| `musica(ruta, 0.5)` | `musica(ruta, {"volumen": 0.5})` |

- **Por qué:** lo mismo que B1.
- **Migrar:** sí. **Usos:** 11.
- **Recomiendo:** sí.

**B3. `texto` con escala y color sueltos, o con la fuente tal cual.**

| Ahora | En la 2.0 |
|---|---|
| `texto(t, x, y, 2, [255, 0, 0])` | `texto(t, x, y, {"escala": 2, "color": [255, 0, 0]})` |
| `texto(t, x, y, LETRA)` | `texto(t, x, y, {"fuente": LETRA})` |

- **Por qué:** hoy hay tres formas de llamarla.
- **Migrar:** sí. **Usos:** 59.
- **Recomiendo:** sí.

**B4. `ancho_texto` es `medir_texto(t)[0]`.**

| Ahora | En la 2.0 |
|---|---|
| `ancho_texto(t, op)` | `medir_texto(t, op)[0]` |

- **Por qué:** `medir_texto` ya da el ancho y el alto.
- **Migrar:** sí. **Usos:** 38.
- **Recomiendo:** sí, aunque es la más discutible: `ancho_texto` se lee muy bien, sobre todo
  para centrar. Si prefieres quedártela, no pasa nada.

**B5. Una sola `colision` para todo.**

| Ahora | En la 2.0 |
|---|---|
| `colision(x1, y1, w1, h1, x2, y2, w2, h2)` | `colision([x1, y1, w1, h1], [x2, y2, w2, h2])` |
| `colision_circulos(x1, y1, r1, x2, y2, r2)` | `colision({"x": x1, "y": y1, "radio": r1}, {...})` |
| `colision_circulo_rect(cx, cy, r, x, y, w, h)` | `colision({"x": cx, "y": cy, "radio": r}, [x, y, w, h])` |
| `dentro(px, py, x, y, w, h)` | `colision([px, py], [x, y, w, h])` |

`colision(a, b)` aceptaría cajas `[x, y, ancho, alto]`, puntos `[x, y]`, diccionarios con
`x, y, ancho, alto` (un cuerpo, un personaje) y círculos (`x, y, radio`).

- **Por qué:** hoy son cuatro funciones con 6, 7 u 8 números sueltos que es fácil desordenar,
  y `_rect` es la única abreviatura de todo el módulo.
- **Migrar:** sí. **Usos:** 28.
- **Recomiendo:** sí.

**B6. `posicion_camara()` es `camara_actual()` sin el zoom ni el giro.**

- **Migrar:** sí, pero queda feo: `[juego.camara_actual()["x"], juego.camara_actual()["y"]]`.
- **Usos:** 0.
- **Recomiendo:** quitarla, porque nadie la usa.

---

## C. Nombres que no siguen la misma regla

**C1. Las partidas guardadas.**

| Ahora | En la 2.0 |
|---|---|
| `guardar(nombre, datos)` · `cargar(nombre)` · `olvidar(nombre)` | `guardar_partida` · `cargar_partida` · `olvidar_partida` |

- **Por qué:** `juego.guardar` puede ser cualquier cosa, y al lado están `guardar_imagen` y
  `guardar_sonido`, que escriben archivos.
- **Migrar:** sí. **Usos:** 18.
- **Recomiendo:** sí.

**C2. Para quitar cosas, siempre `quitar_`.**

| Ahora | En la 2.0 |
|---|---|
| `borrar_particulas()` | `quitar_particulas()` |

- **Por qué:** todo lo demás es `quitar_cuerpo`, `quitar_union`, `quitar_jugador`. `olvidar_`
  se queda para lo guardado y para `olvidar_jugadores`.
- **Migrar:** sí. **Usos:** 4.
- **Recomiendo:** sí.

**C3. El ángulo de la cámara se llama `"angulo"`, como en todo lo demás.**

| Ahora | En la 2.0 |
|---|---|
| `camara(x, y, {"giro": 15})` | `camara(x, y, {"angulo": 15})` |

- **Por qué:** en `dibujar` y en la física el ángulo es `angulo`; en la cámara es `giro`, y en
  la física `vgiro` es otra cosa (la velocidad de giro).
- **Migrar:** sí, cuando las opciones están escritas en la llamada. Si van en una variable,
  avisa y no lo cambia.
- **Usos:** 9.
- **Recomiendo:** sí.

**C4. `clic(n)` frente a `boton_raton(n)` y `boton_raton_soltado(n)`.**

Por la regla de `tecla` / `tecla_pulsada` / `tecla_soltada`, `clic` debería llamarse
`boton_raton_pulsado`. Pero `clic` se entiende a la primera.

- **Recomiendo:** dejarlo así.

---

## D. El lenguaje

**D1. Quitar `val`.**

| Ahora | En la 2.0 |
|---|---|
| `funcion f(val numero x)` | `funcion f(numero x)` (`ref` se queda) |

- **Por qué:** desde la 1.0 no poner nada ya es lo mismo que `val`. Es ruido en cada función,
  y lo primero que se pregunta quien empieza.
- **Migrar:** sí; solo quita la palabra. **Usos:** 352.
- **Recomiendo:** sí.

**D2. Lo que se hace con un texto, como método del texto.**

Hoy unas cosas son métodos (`t.longitud()`, `t.mayusculas()`, `t.contiene("a")`) y otras van
en el módulo `texto` (`texto.posicion(t, "a")`, `texto.dividir(t, ",")`,
`texto.extraer(t, 0, 3)`). Las listas, en cambio, lo tienen todo como métodos
(`lista.posicion(x)`).

| Ahora | En la 2.0 |
|---|---|
| `texto.posicion(t, x)` · `texto.dividir(t, ",")` · `texto.extraer(t, a, b)` … | `t.posicion(x)` · `t.dividir(",")` · `t.extraer(a, b)` … |

El módulo `texto` se queda con lo que no parte de un texto: `texto.unir(lista, "-")`,
`texto.decimales(n, 2)`, `texto.miles(n)`, `texto.formato(...)`.

- **Por qué:** hoy hay que acordarse de qué va de cada forma.
- **Migrar:** sí. **Usos:** 1 en los juegos (dentro de Lince, algunos más).
- **Recomiendo:** sí.

**D3. Funciones repetidas del módulo `texto`.**

| Ahora | Es lo mismo que |
|---|---|
| `texto.a_numero(t)` | `a_numero(t)` |
| `texto.de_numero(n)` | `a_texto(n)` |
| `texto.repetir(t, 3)` | `t * 3` |
| `texto.es_vacio(t)` | `t == ""` |

- **Migrar:** sí. **Usos:** 0.
- **Recomiendo:** quitarlas.

**D4. Las anotaciones de tipo se comprueban también al ejecutar.**

Hoy `sea numero x = "hola"` solo da un aviso en `lince comprobar` y en el editor. En la 2.0
sería un `ErrorTipo` al ejecutar, cuando se pone la anotación. Sin anotación todo sigue igual.

- **Por qué:** estaba en el plan; una anotación que no se cumple es un error que nadie ve.
- **Migrar:** no se puede arreglar solo. La 1.9 avisaría al ejecutar («en la 2.0 esto será un
  error») para encontrarlos todos antes.
- **Recomiendo:** sí.

---

## Lo que no propongo tocar

- `pitido` y `efecto`: hacen cosas distintas.
- `efecto` y `crear_efecto`: uno suena y el otro da el sonido para guardarlo.
- `cuadro()` y el `animador`: el primero es la forma sencilla de animar y sigue siendo útil.
- `mapear`, `filtrar` y `reducir` como funciones: valen también para generadores y bytes, no
  solo para listas.
- Los menús (`menu`, `menu_controles`, `menu_idiomas`, `menu_jugadores`): ya siguen la misma
  regla.

---

## Decidido (el autor, 2026-10-26)

- **Sí:** A1, A2, B1, B2, B3, B5, B6, C1, C2, C3, D1, D2, D3 y D4.
- **No:**
  - A3: el sonido sigue en `juego`.
  - B4: `ancho_texto` se queda.
  - C4: `clic` se queda.
- **Sin cambios:** A4 (el resto de `juego`, como está).
