# 🐆 Lince — Especificación del Lenguaje v0.6

## Filosofía

Lince es un lenguaje de programación **para videojuegos, en español**, implementado en C puro sin dependencias para compilarlo (la ventana, el dibujo, la entrada y el sonido usan SDL2, que se carga al abrir la ventana). Tres pilares:

1. **En español** — la programación no debería requerir saber inglés
2. **Una sola forma** — sin excepciones a las reglas, sin comportamiento implícito
3. **Errores literales** — mensajes claros con número de línea exacto

---

## Palabras clave

| Keyword | Significado |
|---------|-------------|
| `sea` | variable mutable |
| `fijo` | constante |
| `lista` | tipo lista |
| `diccionario` | tipo diccionario |
| `funcion` | función o lambda |
| `generador` | función generadora |
| `producir` | emitir valor desde generador |
| `devolver` | return |
| `si` / `sino si` / `sino` | if / else if / else |
| `para` | for clásico |
| `cada` | usado con `para cada` |
| `mientras` | while |
| `hacer` | do (do-while) |
| `elegir` / `caso` / `otro` | switch / case / default |
| `clase` / `extiende` / `implementa` | class / extends / implements |
| `interfaz` | define una interfaz |
| `enumeracion` | define una enumeración |
| `abstracta` / `abstracto` / `estatico` / `sobrescribe` | modificadores de clase y miembro |
| `privado` / `protegido` / `publico` | visibilidad |
| `obtener` / `fijar` | propiedades |
| `es` | operador de comprobación de tipo |
| `mi` | self / this |
| `padre` | llamada al constructor padre |
| `verdadero` / `falso` | true / false |
| `nulo` | null |
| `escribir` | print |
| `leer` | leer del teclado |
| `val` / `ref` | parámetro por valor / referencia |
| `intentar` / `capturar` / `finalmente` | try / catch / finally |
| `lanzar` | throw |
| `importar` | import |
| `y` / `o` / `no` | and / or / not |

---

## Tipos de dato

| Tipo | Keyword | Ejemplo |
|------|---------|---------|
| Número | `numero` | `3`, `3.14`, `0xff` (hexadecimal), `0b1010` (binario; admite `_`: `0b1111_0000`) |
| Texto | `texto` | `"hola"` |
| Lógico | `logico` | `verdadero`, `falso` |
| Lista | `lista` | `[1, 2, 3]` |
| Diccionario | `diccionario` | `{"clave": valor}` |
| Función | `funcion` | lambdas, funciones como args |
| Sin valor | `nulo` | `nulo` |
| Bytes | — | `bytes.leer("nivel.bin")` (módulo `bytes`) |

---

## Variables y constantes

```lince
sea nombre = "Ana"              # mutable
fijo PI = 3.14159               # constante

sea lista notas = [9, 7, 10]
sea diccionario d = {"nombre": "Ana", "edad": 22}
```

### Un nombre se define una sola vez

Definir dos veces el mismo nombre **en el mismo ámbito** es un error, con
`sea`, `fijo`, `funcion`, `generador`, `clase`, `interfaz` o `enumeracion`:

```lince
sea x = 1
sea x = 2        # ❌ Error: ya hay algo llamado 'x' en este ámbito
x = 2            # ✅ así se cambia el valor
```

Tapar un nombre en un ámbito **interior** sí es legal, y es lo normal:

```lince
sea visible = "fuera"

funcion f(): texto {
    sea visible = "dentro"      # ✅ otro ámbito
    devolver visible
}
```

---

## Operadores

| Operador | Significado |
|----------|-------------|
| `+` | suma / concatenación de texto |
| `-` `*` `/` `%` | resta, multiplicación, división, módulo |
| `++` `--` | incremento / decremento |
| `==` `!=` `<` `>` `<=` `>=` | comparación |
| `y` `o` `no` | lógicos |

---

## Funciones

```lince
funcion nombre(val|ref tipo param, ...): tipo_retorno {
    devolver valor
}
```

### Con valores por defecto

```lince
funcion saludar(val texto nombre, val texto saludo = "Hola"): texto {
    devolver saludo + ", " + nombre
}
saludar("Ana")                # "Hola, Ana"
saludar("Ana", "Buenos días") # "Buenos días, Ana"
```

### Lambdas

```lince
sea doble = funcion(val numero x): numero { devolver x * 2 }
escribir(doble(5))  # 10
```

### Funciones como argumentos

```lince
funcion aplicar(val numero x, val funcion f): numero { devolver f(x) }
escribir(aplicar(5, doble))  # 10
```

### Reglas

- Todo parámetro indica `val` o `ref`
- Todo parámetro tiene tipo declarado
- Toda función declara su tipo de retorno
- `val` — copia; `ref` — referencia (los cambios afectan al original)
- Parámetros con valor por defecto van al final
- Recursión limitada a 500 niveles → `ErrorRecursion` capturable

---

## Funciones integradas

Sin necesidad de `importar`:

```lince
escribir("Hola")           # imprime en pantalla
leer("¿Tu nombre? ")       # lee del teclado

a_numero("3.14")           # texto → número
a_texto(42)                # número → texto
a_logico(1)                # valor → lógico

argumentos()               # lista de args de línea de comandos

rango(5)                   # [0, 1, 2, 3, 4]
rango(1, 6)                # [1, 2, 3, 4, 5]
rango(0, 10, 2)            # [0, 2, 4, 6, 8]
rango(10, 0, -1)           # [10, 9, ..., 1]

mapear(lista, funcion)     # aplica función a cada elemento
filtrar(lista, funcion)    # filtra según condición
reducir(lista, inicial, f) # acumula un valor
```

---

## Control de flujo

### Condicional

```lince
si condicion {
    ...
} sino si condicion {
    ...
} sino {
    ...
}
```

### Elegir / Caso

```lince
elegir expresion {
    caso valor1 { ... }
    caso valor2 { ... }
    otro        { ... }   # opcional
}
```

Funciona con números, textos, lógicos y enumeraciones.

### Bucles

```lince
# For clásico
para (i = 0; i < 10; i++) { ... }

# While
mientras condicion { ... }

# Do-while
hacer { ... } mientras (condicion)

# Para cada — listas, generadores, diccionarios, texto
para cada numero n en lista { escribir(n) }
para cada texto clave en diccionario { escribir(clave) }
para cada texto c en "hola" { escribir(c) }
```

---

## Listas

```lince
sea lista nums = [1, 2, 3]
nums[0]              # acceso — devuelve 1
nums[-1]             # último elemento
nums[0] = 99         # asignación

# Modifican la lista
nums.agregar(4)
nums.eliminar(0)        # por índice
nums.insertar(1, 99)    # índice y valor

# Consultan, sin modificar
nums.longitud()
nums.contiene(3)
nums.posicion(3)        # índice, o -1 si no está

# Devuelven una lista nueva — la original no se toca
nums.ordenar()
nums.invertir()
nums.copiar()
```

`ordenar()` usa el orden natural: números por valor, textos
alfabéticamente. Para otro criterio se le pasa una función comparadora
que devuelve un número negativo si el primer elemento va antes, positivo
si va después y cero si da igual:

```lince
# De mayor a menor
nums.ordenar(funcion(val numero a, val numero b): numero {
    devolver b - a
})
```

La ordenación es estable: los elementos equivalentes conservan el orden
que tenían.

La regla general de los métodos de lista: los que **añaden o quitan**
modifican la lista, y los que **transforman** devuelven una copia nueva.
Conviene recordarlo porque las listas se pasan compartidas a las
funciones, así que `copiar()` es la forma de trabajar sin tocar la
original.

---

## Diccionarios

```lince
sea diccionario d = {"nombre": "Ana", "edad": 22}
d["nombre"]
d["ciudad"] = "Madrid"
d.contiene("nombre")
d.longitud()
d.eliminar("edad")
```

---

## Texto — métodos

```lince
s.longitud()
s.mayusculas()
s.minusculas()
s.recortar()            # sin espacios, tabuladores ni saltos de línea en los extremos
s.contiene("algo")
s.reemplazar("a", "b")
s[0]                    # carácter por índice
s[-1]                   # último carácter
```

El texto se guarda en UTF-8 y se trabaja **por caracteres, no por
bytes**, que es lo que se espera escribiendo en español:

```lince
sea palabra = "niño"
palabra.longitud()      # 4, no 5
palabra[2]              # "ñ"
palabra.mayusculas()    # "NIÑO"
```

Las vocales acentuadas y la ñ ocupan dos bytes, pero cuentan como una
sola letra en `longitud()`, al indexar, al recorrer con `para cada` y en
los anchos de `rellenar_izq`, `rellenar_der` y `centrar`. El cambio de
mayúsculas y minúsculas también las respeta: `á` ↔ `Á`, `ñ` ↔ `Ñ`.

Eso sí, una letra con tilde es una letra **distinta** de la misma sin
tilde: `"á" != "a"`. Por eso `"Dábale arroz a la zorra el abad"` no sale
palíndromo tal cual — para eso habría que quitar los acentos primero.

---

## Clases

```lince
clase Persona {
    funcion crear(val texto nombre, val numero edad): nulo {
        mi.nombre = nombre
        mi.edad   = edad
    }
    funcion saludar(): texto {
        devolver "Hola, soy " + mi.nombre
    }
}

sea p = Persona("Ana", 22)
escribir(p.saludar())
```

### Herencia

```lince
clase Empleado extiende Persona {
    funcion crear(val texto nombre, val numero edad, val texto empresa): nulo {
        padre(nombre, edad)
        mi.empresa = empresa
    }
    funcion saludar(): texto {
        devolver "Hola, trabajo en " + mi.empresa
    }
}
```

### Reglas

- `crear` es el constructor — siempre devuelve `nulo`
- `mi` representa la instancia actual
- `padre(args)` llama al constructor del padre
- Los métodos del padre se heredan y pueden sobreescribirse
- Se puede heredar en cadena

### Campos declarados

Además de crearlos con `mi.x = ...`, una clase puede declarar sus campos con
`sea` / `fijo`, con valor inicial opcional. Se inicializan antes de `crear`.

```lince
clase Cuenta {
    sea titular = "anónimo"
    fijo MONEDA = "EUR"        # constante: no se puede reasignar
    sea saldo                  # sin valor inicial: empieza en nulo
}
```

Los campos que no se declaran siguen funcionando, y son públicos.

### Visibilidad

Delante de un método o de un campo se escribe `publico` (por defecto),
`privado` o `protegido`:

```lince
clase Cuenta {
    privado sea saldo = 0
    protegido funcion auditar(): nulo { ... }
    privado funcion crear(): nulo { ... }      # constructor privado
}
```

- **`privado`**: sólo lo ve el código escrito dentro de la propia clase.
- **`protegido`**: lo ve también el código de sus subclases.
- Saltarse la visibilidad lanza un **`ErrorAcceso`**, capturable como cualquier error.
- La visibilidad es de **clase**, no de instancia: un método puede leer el
  campo privado de *otro* objeto de su misma clase.
- Una lambda definida dentro de un método conserva su visibilidad, aunque se
  llame más tarde desde fuera.
- Un constructor privado sólo se puede llamar desde la propia clase: sirve
  para el patrón *singleton* o para obligar a usar un método estático.

### Miembros estáticos

`estatico` los hace de la clase y no de cada objeto. Se usan con el nombre de
la clase:

```lince
clase Contador {
    estatico sea cuenta = 0
    estatico fijo LIMITE = 100
    estatico funcion subir(): numero {
        Contador.cuenta = Contador.cuenta + 1
        devolver Contador.cuenta
    }
}
escribir(Contador.subir())
```

- Un método estático no tiene `mi`.
- Las subclases heredan los estáticos: `SubContador.subir()` funciona.
- Un método estático se llama con `Clase.metodo()`, no sobre un objeto.

### Clases y métodos abstractos

```lince
abstracta clase Figura {
    abstracto funcion area(): numero          # sin cuerpo
    funcion describir(): texto { devolver "área " + mi.area() }
}
```

- Una clase `abstracta` no se puede instanciar.
- Un método `abstracto` no tiene cuerpo, y toda subclase **concreta** está
  obligada a implementarlo (error al declararla si no lo hace).

### `sobrescribe`

`sobrescribe funcion area()` declara que el método reemplaza uno del padre.
Es opcional, pero si el padre no tiene ese método, Lince da error al declarar
la clase: así una errata en el nombre no crea un método nuevo en silencio.

### Propiedades

`obtener` y `fijar` dan acceso con sintaxis de campo y permiten validar:

```lince
clase Persona {
    privado sea _edad = 0
    obtener edad(): numero { devolver mi._edad }
    fijar edad(val numero v): nulo {
        si v < 0 { lanzar Error("edad negativa") }
        mi._edad = v
    }
}
sea p = Persona()
p.edad = 30            # llama a 'fijar edad'
escribir(p.edad)       # llama a 'obtener edad'
```

Una propiedad con `obtener` y sin `fijar` es de sólo lectura. No pongas a
un campo el mismo nombre que a una propiedad: el campo gana.

### Operadores y `a_texto`

Una clase decide qué significan los operadores con métodos de estos nombres
(los de un argumento):

| Operador | Método |
|----------|--------|
| `+` `-` `*` `/` `%` | `sumar` `restar` `multiplicar` `dividir` `modulo` |
| `==` / `!=` | `igual` (sin él, dos referencias al mismo objeto son iguales) |
| `<` | `menor` |
| `>` | `mayor`, o derivado de `menor` e `igual` |
| `<=` | `menor_igual`, o `menor` o `igual` |
| `>=` | `mayor_igual`, o no `menor` |

El método `a_texto()` (sin argumentos) decide cómo se escribe el objeto con
`escribir` y al concatenarlo con un texto.

### Comprobar el tipo: `es`

```lince
si figura es Cuadrado { ... }       # clase (o subclase)
si figura es Forma { ... }          # interfaz implementada
si x es numero { ... }              # numero, texto, logico, lista, diccionario, nulo, funcion
si error es ErrorTipo { ... }
```

Se escribe `no (x es Tipo)` para negarlo: `no x es Tipo` se lee como `(no x) es Tipo`.

---

## Interfaces

```lince
interfaz Forma {
    funcion area(): numero
    funcion describir(): texto
}

clase Rectangulo implementa Forma {
    funcion crear(val numero ancho, val numero alto): nulo {
        mi.ancho = ancho
        mi.alto  = alto
    }
    funcion area(): numero { devolver mi.ancho * mi.alto }
    funcion describir(): texto { devolver "Rectángulo" }
}
```

### Múltiples interfaces

```lince
clase Cuadrado implementa Forma, Serializable { ... }
```

### Reglas

- Una interfaz declara firmas sin implementación
- Si falta algún método, Lince lanza un error al definir la clase
- Una clase puede `extiende` y `implementa` a la vez
- Permiten polimorfismo sin herencia

### Métodos por defecto

Una interfaz puede dar el cuerpo de un método. La clase que la implementa lo
recibe si no tiene uno propio (ni heredado), lo que permite compartir código
entre clases sin herencia múltiple:

```lince
interfaz Saludable {
    funcion nombre(): texto
    funcion saludar(): texto { devolver "hola " + mi.nombre() }
}
```

---

## Enumeraciones

```lince
enumeracion Color { Rojo Verde Azul }

sea c = Color.Rojo
escribir(c)            # "Color.Rojo"

si c == Color.Rojo { escribir("Es rojo") }

elegir c {
    caso Color.Rojo  { escribir("rojo") }
    caso Color.Verde { escribir("verde") }
    otro             { escribir("otro") }
}
```

Los valores se representan como texto `"Nombre.Valor"`, son constantes y funcionan en `elegir/caso`.

---

## Generadores

```lince
generador contar(val numero hasta): numero {
    sea i = 0
    mientras i < hasta {
        producir i
        i = i + 1
    }
}

para cada numero n en contar(5) {
    escribir(n)   # 0, 1, 2, 3, 4
}
```

### `rango()` — generador integrado

```lince
rango(5)          # 0, 1, 2, 3, 4
rango(1, 6)       # 1, 2, 3, 4, 5
rango(0, 10, 2)   # 0, 2, 4, 6, 8
rango(10, 0, -1)  # 10, 9, ..., 1
```

### Reglas

- `generador` declara una función generadora en vez de `funcion`
- `producir` emite un valor y continúa
- Los generadores se usan con `para cada`
- Límite de 10.000 valores por generador

---

## Manejo de errores

```lince
intentar {
    lanzar ErrorRango("fuera de rango")
} capturar (ErrorRango e) {
    escribir("Rango: " + e.mensaje)
} capturar (error e) {
    escribir("General: " + e.mensaje)
} finalmente {
    escribir("Siempre se ejecuta")
}
```

### Tipos de error predefinidos

| Tipo | Cuándo |
|------|--------|
| `Error` | error genérico |
| `ErrorTipo` | tipo de dato incorrecto |
| `ErrorRango` | índice o valor fuera de rango |
| `ErrorNulo` | valor nulo inesperado |
| `ErrorMatematico` | división por cero, etc. |
| `ErrorArgumento` | argumentos incorrectos |
| `ErrorRecursion` | recursión demasiado profunda |
| `ErrorJuego` | fallo del módulo `juego` (sin SDL2, sin pantalla…) |

### Todos los errores se pueden capturar

Lo que el lenguaje detecta por su cuenta llega igual que un `lanzar`: un método
con los argumentos mal, un índice fuera de rango, un campo o método que no
existe, llamar a algo que no es una función, asignar a una constante… Cada uno
trae su tipo (`ErrorArgumento`, `ErrorRango`, `Error`…) y se captura con
`intentar/capturar`. Ninguno cierra el programa por su cuenta.

Un error que nadie captura termina el programa y dice **la línea**:

```
❌ ErrorArgumento en línea 5: 'agregar' necesita exactamente 1 argumento.
```

En el REPL el error se muestra y la sesión sigue. Si hay varios errores en
cadena, se informa del primero, que es la causa, y no de los que vienen detrás
como consecuencia suya.

El objeto error tiene: `e.mensaje`, `e.tipo`, `e.linea`

---

## Módulos estándar

```lince
importar "matematica"
importar "texto"
importar "archivos"
importar "tiempo"
importar "sistema"
importar "json"
importar "pruebas"
importar "juego"
```

### `matematica`

```lince
matematica.abs(-5)            # 5
matematica.piso(3.9)          # 3
matematica.techo(3.1)         # 4
matematica.redondear(3.5)     # 4
matematica.truncar(3.9)       # 3
matematica.es_entero(4.0)     # verdadero
matematica.raiz(25)           # 5
matematica.potencia(2, 8)     # 256
matematica.log2(8)            # 3
matematica.log10(1000)        # 3
matematica.logaritmo(1)       # 0
matematica.seno(0)            # 0
matematica.coseno(0)          # 1
matematica.tangente(0)        # 0
matematica.maximo(3, 7)       # 7
matematica.minimo(3, 7)       # 3
matematica.factorial(5)       # 120
matematica.combinaciones(5,2) # 10
matematica.aleatorio()        # 0.0 .. 1.0
matematica.aleatorio(1, 10)   # entero entre 1 y 10
matematica.PI                 # 3.14159...
matematica.E                  # 2.71828...
matematica.TAU                # 6.28318...
```

### `texto`

```lince
texto.dividir("a,b,c", ",")   # ["a", "b", "c"]
texto.unir(["a","b"], "-")    # "a-b"
texto.invertir("hola")        # "aloh"
texto.posicion("hola", "ol")  # 1
```

### `archivos`

```lince
archivos.leer("datos.txt")
archivos.escribir("out.txt", "contenido")
archivos.agregar("log.txt", "línea")
archivos.existe("archivo.txt")
archivos.eliminar("archivo.txt")
archivos.copiar("a.txt", "b.txt")
archivos.mover("viejo.txt", "nuevo.txt")
archivos.es_directorio("carpeta")
archivos.crear_directorio("carpeta")
archivos.listar("carpeta")        # lista de nombres
```

### `tiempo`

```lince
tiempo.ahora()     # segundos desde 1970, con decimales (sirve para medir tiempos)
tiempo.fecha()     # "2025-01-15"
tiempo.hora()      # "14:30:00"
tiempo.esperar(500) # pausa 500ms
```

### `sistema`

```lince
sistema.ejecutar("ls -la")             # devuelve lo que escribe el comando
sistema.proceso("make")                # {"salida", "error", "codigo", "ok"}
sistema.existe("archivo.txt")
sistema.salir(0)
```

### `json`

```lince
sea datos = json.parsear("{\"nombre\": \"Ana\"}")
escribir(datos["nombre"])
escribir(json.texto({"clave": "valor"}))
```

### Fechas (`tiempo`)

```lince
importar "tiempo"
sea t = tiempo.crear_utc(2024, 2, 29, 13, 5, 9)         # marca de tiempo (segundos desde 1970)
tiempo.formatear_utc(t, "%d/%m/%Y %H:%M")                # "29/02/2024 13:05"
tiempo.partes_utc(t)                                     # {"anio":..., "mes":..., "dia":..., "hora":..., "minuto":..., "segundo":..., "dia_semana":..., "dia_anio":...}
tiempo.analizar_utc("2024-02-29", "%Y-%m-%d")            # marca de tiempo, o nulo si no encaja
```

`crear`, `formatear`, `partes` y `analizar` usan la hora local; las versiones
`_utc` no dependen de la zona horaria. `dia_semana` va de 0 (domingo) a 6. Los
formatos son los de `strftime`; `analizar` entiende `%Y %y %m %d %H %M %S`.

### `juego`

Ventana 2D con dibujo, imágenes, teclado, ratón, mando y sonido, para hacer
videojuegos. Usa SDL2, que se carga al abrir la ventana (no hace falta para compilar
Lince). Las imágenes y el OGG los lee el propio Lince. En Debian/Ubuntu:
`sudo apt install libsdl2-2.0-0`; en Windows, `SDL2.dll` junto al juego (`lince repartir`
la pone). El bucle lo escribes tú:

```lince
importar "juego"
juego.abrir("Mi juego", 800, 600)
sea x = 100
mientras juego.abierto() {
    si juego.tecla("derecha") { x = x + 300 * juego.dt() }     # dt: segundos del último fotograma
    juego.limpiar([20, 20, 30])
    juego.circulo(x, 300, 20, "#ffc850")
    juego.texto("Hola", 10, 10, 3)
    juego.presentar()                                          # muestra, espera a 60 fps y lee la entrada
}
```

**Ventana y bucle**

| Función | Qué hace |
|---|---|
| `abrir(título, ancho, alto)` / `cerrar()` / `salir()` | abre la ventana / la destruye / pide terminar el bucle (`abierto()` pasa a falso, como al pulsar la X) |
| `abierto()` · `titulo(t)` · `ancho()` · `alto()` | estado y tamaño |
| `presentar()` | muestra lo dibujado, espera al siguiente fotograma y actualiza la entrada y `dt()` |
| `dt()` · `fps()` · `tiempo()` | segundos del último fotograma · fotogramas por segundo · segundos desde que se abrió |
| `pantalla_completa([sí/no])` | sin argumento alterna; el juego se escala sin deformarse y el ratón sigue en coordenadas del juego |

**Dibujo**

| Función | Qué hace |
|---|---|
| `limpiar([color])` | rellena toda la ventana |
| `rectangulo(x, y, ancho, alto[, color])` · `contorno(...)` | rectángulo relleno / solo borde |
| `circulo(x, y, radio[, color])` · `linea(x1, y1, x2, y2[, color])` | |
| `texto(t, x, y[, escala[, color]])` · `ancho_texto(t[, escala])` | fuente de mapa de bits incluida (mayúsculas, cifras, signos; las minúsculas salen como mayúsculas; tildes y ñ se marcan) |
| `camara(x, y)` · `camara()` | desplaza todo lo que se dibuja (para mundos más grandes que la pantalla) / vuelve a (0, 0), por ejemplo para el marcador |
| `captura(ruta)` | guarda la ventana como `.bmp` |

**Imágenes y sprites**

| Función | Qué hace |
|---|---|
| `imagen(ruta)` | carga un PNG, JPG, BMP, GIF o TGA; devuelve `{"imagen", "ancho", "alto", "ruta"}`. Cargar la misma ruta dos veces no repite el trabajo |
| `sprite(filas, colores)` | crea una imagen a partir de texto, un carácter por píxel: `juego.sprite([".X.", "XXX"], {"X": [255, 0, 0]})`; los caracteres sin color son transparentes |
| `hoja(imagen o ruta, ancho[, alto][, opciones])` | una imagen partida en cuadros iguales: los de una animación o los tiles de un mapa. Se cuentan de izquierda a derecha y de arriba abajo desde 0 (un número fuera de rango da la vuelta: −1 es el último). Opciones: `separacion` (píxeles entre cuadros) y `margen` (alrededor), como en muchas hojas descargadas. Devuelve la imagen con `"celda"`, `"columnas"` y `"cuadros"`. También vale una imagen hecha con `sprite()` |
| `animar(imagen o ruta, tipo[, opciones])` | genera una animación a partir de un solo dibujo y la devuelve como hoja (ver abajo) |
| `guardar_imagen(imagen, ruta)` | guarda una imagen (un sprite, una hoja generada...) como `.png`, por ejemplo para retocarla en un editor |
| `dibujar(imagen, x, y[, opciones])` | dibuja la imagen; `opciones` es un diccionario con cualquiera de: `escala`, `ancho`, `alto`, `angulo` (grados), `espejo`, `voltear`, `centrado` (x, y es el centro), `alfa` (0–255), `color` (tinte), `recorte` (`[x, y, ancho, alto]`) y `cuadro` (el número de cuadro de una hoja; con una imagen normal, indica también `celda`: `[ancho, alto]` de cada cuadro) |

**Entrada**

| Función | Qué hace |
|---|---|
| `tecla(nombre)` · `tecla_pulsada(n)` · `tecla_soltada(n)` | mantenida / pulsada en este fotograma / soltada en este fotograma |
| `raton()` · `boton_raton(n)` · `clic(n)` · `rueda()` | `[x, y]` · botón mantenido (1 izquierdo, 2 central, 3 derecho) · clic de este fotograma · giro de la rueda |
| `mandos()` · `boton(nombre[, mando])` · `boton_pulsado(nombre[, mando])` · `eje(nombre[, mando])` | mandos conectados (hasta 4; `mando` va de 1 a 4) · botones `a`, `b`, `x`, `y`, `lb`, `rb`, `inicio`, `atras`, `guia`, `arriba`, `abajo`, `izquierda`, `derecha`, `stick_izq`, `stick_der` · ejes `izq_x`, `izq_y`, `der_x`, `der_y` (−1 a 1) y gatillos `lt`, `rt` (0 a 1) |
| `cursor(visible)` · `capturar_raton(si)` · `movimiento_raton()` | muestra u oculta la flecha · encierra y oculta el ratón para apuntar (devuelve si se pudo) · `[dx, dy]` movido desde el fotograma anterior |
| `editar(texto[, maximo])` · `texto_escrito()` | el texto con lo tecleado añadido y «borrar» aplicado (tildes y ñ incluidas) · lo tecleado en este fotograma |

**Acciones.** Un nombre para varias teclas, botones y ejes, así el juego funciona igual con teclado, ratón y mando:

```lince
juego.definir_accion("saltar", ["espacio", "w", "mando:a", "raton:izquierdo"])
juego.definir_accion("izquierda", ["izquierda", "a", "mando:izquierda", "mando:izq_x-"])
juego.definir_accion("derecha", ["derecha", "d", "mando:derecha", "mando:izq_x+"])

si juego.accion_pulsada("saltar") { vy = -500 }
x = x + juego.direccion("izquierda", "derecha") * 200 * juego.dt()   # gradual con el stick
```

| Función | Qué hace |
|---|---|
| `definir_accion(nombre, entradas)` | crea o sustituye la acción. Entradas: una tecla (`"espacio"`), `"mando:a"` (botón de cualquier mando), `"mando2:a"` (del mando 2), `"mando:izq_x-"` / `"mando:izq_x+"` (eje hacia un lado), `"raton:izquierdo"` / `"raton:central"` / `"raton:derecho"` |
| `accion(n)` · `accion_pulsada(n)` · `accion_soltada(n)` | mantenida / empieza en este fotograma / termina en este fotograma |
| `direccion(negativa, positiva)` | de −1 a 1: entero con teclas, gradual con el stick |

Para escribir un nombre (por ejemplo para un récord): `nombre = juego.editar(nombre, 10)` en cada fotograma y `juego.tecla_pulsada("entrar")` para terminar.

**Sonido**

| Función | Qué hace |
|---|---|
| `sonido(ruta)` | carga un `.wav` o un `.ogg` (Ogg Vorbis, mucho más pequeño: mejor para la música); devuelve `{"sonido", "duracion", "ruta"}` |
| `reproducir(sonido[, volumen[, bucle]])` | lo hace sonar (se mezclan hasta 32 a la vez); devuelve el número de la voz |
| `detener([voz])` | para esa voz o, sin argumento, todo |
| `musica(ruta o sonido[, volumen])` · `musica(nulo)` | suena en bucle y sustituye a la anterior / la para. Con la ruta de un `.ogg` (`juego.musica("tema.ogg")`) se lee poco a poco mientras suena: una canción de 3 minutos ocupa unos cientos de KB en vez de unos 30 MB. Si ya suena esa misma, solo cambia el volumen (no vuelve a empezar al recargar en caliente) |
| `volumen([v])` | volumen general de 0 a 1 |
| `pitido(frecuencia, ms[, volumen])` | tono sencillo, sin archivos |
| `efecto(tipo[, opciones])` | genera y hace sonar un efecto al estilo de los juegos de 8 bits, sin archivos: `"moneda"`, `"salto"`, `"disparo"`, `"explosion"`, `"golpe"`, `"subir"`, `"clic"` o `"perder"`. Cada vez suena un poco distinto. Opciones: `volumen` (0,5), `tono` y `largo` (multiplican la frecuencia y la duración: `{"tono": 1.5}` más agudo) y `semilla` (siempre el mismo sonido) |
| `crear_efecto(tipo[, opciones])` | el efecto como sonido, para `reproducir()` o `guardar_sonido()`; sin `semilla` usa la 1 |
| `guardar_sonido(sonido, ruta)` | guarda un sonido como `.wav` |

**Colisiones y animación**

| Función | Qué hace |
|---|---|
| `colision(x1, y1, w1, h1, x2, y2, w2, h2)` | ¿se solapan dos rectángulos? |
| `colision_circulos(x1, y1, r1, x2, y2, r2)` · `colision_circulo_rect(cx, cy, r, x, y, w, h)` | ¿se tocan dos círculos? · ¿toca el círculo al rectángulo? |
| `dentro(px, py, x, y, w, h)` | ¿está el punto dentro del rectángulo? (botones, clics) |
| `cuadro(cuadros, por_segundo[, desde[, repetir]])` | el cuadro de una animación que toca ahora: de 0 a `cuadros` − 1, o el elemento que toca si `cuadros` es una lista (`[0, 1, 2, 1]`). `desde` es el `tiempo()` en que empezó; con `repetir` falso se queda en el último |

```lince
fijo HEROE = juego.hoja("heroe.png", 16)                            # cuadros de 16×16
juego.dibujar(HEROE, x, y, {"cuadro": juego.cuadro(4, 8)})          # 4 cuadros, 8 por segundo
```

**Animaciones generadas.** Con un solo dibujo del personaje (quieto y, para andar, de perfil
mirando a la derecha), `animar()` hace los cuadros de una animación:

```lince
fijo ANDAR = juego.animar("pj.png", "andar")            # o un sprite hecho con texto
juego.dibujar(ANDAR, x, y, {"cuadro": juego.cuadro(ANDAR["cuadros"], 10), "centrado": verdadero})
juego.guardar_imagen(ANDAR, "pj_andar.png")             # para retocarla a mano, si quieres
```

| Tipo | Qué hace |
|---|---|
| `"andar"` · `"correr"` | mueve las piernas: busca las filas de abajo con hueco en medio (las dos piernas), las balancea por turnos y levanta el pie de la que avanza, mientras el cuerpo baja un poco al apoyar los dos pies. Si no encuentra piernas separadas usa el 30 % de abajo del dibujo. Para un dibujo de frente, `{"vista": "frente"}`: los pies suben por turnos, sin avanzar |
| `"respirar"` | se estira y se encoge un poco: para cuando está quieto |
| `"saltar"` | se agacha, sale estirado y vuelve a su forma (la subida la haces tú) |
| `"golpe"` | destello blanco y sacudida, al recibir daño |
| `"morir"` | da un saltito y cae de lado |
| `"flotar"` · `"girar"` | sube y baja · da una vuelta entera |

Opciones: `cuadros` (por defecto 8; 6 en correr y saltar, 4 en golpe), `intensidad` (1 es lo
normal; 2, el doble de movimiento), `vista` (`"lado"` o `"frente"`), y si no acierta con las piernas,
`piernas` (cuántas filas de abajo son las piernas) y `centro` (la columna que las separa).

Funciona mejor con pixel art pequeño (de 16 a 48 píxeles de alto) y con las piernas separadas
por al menos un píxel transparente. Cada cuadro es algo mayor que el dibujo (lo que asoma al
moverse; la hoja dice cuánto en `"relleno"`, `[x, y]` por cada lado) y el dibujo queda en el centro,
así que con `"centrado"` se dibuja en el mismo sitio que el original. Si la imagen cambia en el
disco con el juego en marcha, sus animaciones se rehacen solas. Ejemplo: `10_paseo`.

**Mapas de casillas.** Un nivel escrito con texto, un carácter por casilla, para cualquier juego 2D:
plataformas, vista cenital (mazmorras, laberintos, RPG), puzles por casillas...

```lince
fijo NIVEL = juego.mapa([
    "##########",
    "#P...*...#",
    "#..####..#",
    "##########"
], 32)
sea yo = {"x": 40, "y": 40, "ancho": 20, "alto": 20, "vx": 0, "vy": 0}
# en cada fotograma:
yo["vx"] = juego.direccion("izquierda", "derecha") * 150
yo["vy"] = juego.direccion("arriba", "abajo") * 150      # o, en un juego de plataformas, gravedad
juego.mover_en_mapa(NIVEL, yo)                            # se mueve sin atravesar las paredes
juego.dibujar_mapa(NIVEL, {"#": [80, 70, 100]})
```

| Función | Qué hace |
|---|---|
| `mapa(filas, tamaño[, opciones])` | crea el mapa (un carácter ASCII por casilla; `tamaño` en píxeles). Opciones: `"solidos"` (los caracteres que son pared; `"#"` por defecto), `"plataformas"` (se atraviesan desde abajo y se pisan desde arriba, como en los juegos de plataformas) y `"bordes"`: fuera del mapa es pared por los `"lados"` (por defecto: se puede caer por un hoyo), por `"todos"` o por `"ninguno"`. Devuelve `{"mapa", "columnas", "filas", "tamaño", "ancho", "alto"}` |
| `mover_en_mapa(mapa, cosa[, dt])` | mueve `cosa` (un diccionario con `x`, `y`, `ancho`, `alto`, `vx`, `vy`) según su velocidad sin atravesar paredes: al chocar se arrima y esa velocidad pasa a 0. Deja en `cosa` `"en_suelo"` y `"techo"` (lógicos) y `"pared"` (−1 izquierda, 1 derecha, 0). La gravedad, si la hay, la sumas tú a `vy` |
| `casilla(mapa, c, f)` · `poner_casilla(mapa, c, f, carácter)` | lee o cambia una casilla (coger una gema, abrir una puerta, romper un bloque); fuera del mapa `casilla` da `""` |
| `solido(mapa, c, f)` · `casilla_en(mapa, x, y)` | ¿es pared? · `[columna, fila]` de un punto en píxeles |
| `buscar_en_mapa(mapa, caracteres)` | lista de `[columna, fila]` con esos caracteres: dónde empieza el jugador, dónde están las monedas o los enemigos |
| `camino(mapa, [c1, f1], [c2, f2][, diagonales])` | el camino más corto sin atravesar paredes, como lista de `[columna, fila]` sin la de salida (`[]` si no hay). Para enemigos que persiguen |
| `dibujar_mapa(mapa, aspecto)` | dibuja las casillas que se ven (respeta la cámara); `aspecto` da a cada carácter un color, una imagen o un tile de una hoja (ver abajo). Los caracteres que no aparecen no se dibujan |

**Tiles.** Lo normal es dibujar los niveles con una hoja de tiles (un PNG con todos los tiles en
rejilla, hecho con cualquier editor o descargado) y decir qué tile toca a cada carácter:

```lince
fijo TILES = juego.hoja("tiles.png", 16)            # tiles de 16×16 (se ajustan al tamaño de la casilla)
juego.dibujar_mapa(NIVEL, {
    "#": [TILES, 0],                               # el tile 0
    ".": [TILES, 5],
    "~": [TILES, [8, 9, 10, 9]],                   # animado: agua (0,25 s cada cuadro)
    "S": [TILES, [12, 13], 0.1]                    # animado, 0,1 s cada cuadro
})
```

El aspecto de un carácter se puede cambiar en cualquier momento (una puerta que se abre, un
interruptor): es un diccionario normal. `09_mazmorra` hace su hoja con `sprite()`, sin archivos.

Ejemplos: `08_plataformas` (con gravedad y plataformas) y `09_mazmorra` (vista cenital con tiles,
fantasmas que te persiguen).

**Partículas.** Chispas, polvo, explosiones: `particulas(x, y[, opciones])` lanza un puñado y
`dibujar_particulas()` las mueve y las dibuja (llámala en cada fotograma, después de lo que deban
tapar; respeta la cámara y devuelve cuántas quedan). `borrar_particulas()` las quita todas.

```lince
juego.particulas(x, y, {"cantidad": 30, "color": [255, 200, 50], "velocidad": [50, 200], "gravedad": 400})
```

Opciones (todas opcionales; las que admiten `[mínimo, máximo]` toman un valor al azar en ese
rango para cada partícula): `cantidad` (20), `color` (blanco), `color_final` (el mismo
transparente: se desvanecen), `velocidad` en píxeles por segundo (`[60, 180]`), `direccion` en
grados (0 = derecha, −90 = arriba) y `apertura` (360: en todas direcciones), `vida` en segundos
(`[0.4, 0.8]`), `tamaño` (4) y `tamaño_final`, y `gravedad` (0). Una opción mal escrita da error.

**Escenas.** Un nombre para cada parte del juego (portada, partida, fin...):

```lince
juego.cambiar_escena("portada")
mientras juego.abierto() {
    si juego.escena() == "portada" { portada() } sino { partida() }
    juego.presentar()
}
# dentro de portada(): si juego.tecla_pulsada("espacio") { juego.cambiar_escena("partida", 0.6) }
```

| Función | Qué hace |
|---|---|
| `cambiar_escena(nombre[, fundido])` | cambia de escena en el siguiente `presentar()` (así un fotograma no mezcla dos escenas; la primera empieza en el acto). Con `fundido` en segundos, la pantalla se oscurece, cambia y se aclara |
| `escena()` · `escena_nueva()` · `tiempo_escena()` | la escena actual · verdadero en su primer fotograma (para prepararla) · segundos desde que empezó |

En `matematica` hay además `distancia`, `angulo`, `interpolar`, `limitar`, `signo`, `radianes`,
`grados`, `semilla` (misma semilla, misma secuencia de `aleatorio`: partidas repetibles) y, para
vectores (listas `[x, y]`), `longitud(x, y)`, `normalizar(x, y)`, `vector(angulo, largo)` (el
contrario de `angulo`), `rotar(x, y, grados)` y `acercar(actual, objetivo, paso)` (avanza sin
pasarse: frenar, perseguir, cámaras suaves).

**Recarga en caliente.** Con `lince juego.lince` el juego se vigila mientras corre: al guardar el
programa (o un archivo que importa) se vuelve a ejecutar con el código nuevo **sin cerrar la
ventana**, y las imágenes y sonidos cargados que cambien se recargan solos.

- Las variables `sea` del nivel superior **conservan su valor** (el jugador sigue donde estaba);
  las constantes `fijo`, las funciones y las clases salen del código nuevo, y los objetos
  conservados pasan a usar la clase nueva. Para que un cambio de valor se note al guardar
  (gravedad, colores, un sprite), decláralo con `fijo`.
- `juego.recargado()` es verdadero cuando el programa se ha recargado así; sirve para no
  reiniciar la partida: `si no juego.recargado() { empezar() }`.
- **F5** lo vuelve a empezar desde cero, sin conservar nada.
- Si lo guardado tiene un error de sintaxis, el juego sigue con el código anterior y el error
  sale abajo, en la ventana. Si el programa se para por un error, el error ocupa la ventana
  hasta que se guarde el arreglo.

Los ejecutables de `lince compilar` no se recargan.

Las imágenes y los sonidos se leen también desde dentro de un ejecutable de `lince compilar`
o `lince repartir`: los que el código nombra con su ruta (`"img/lince.png"`) van dentro solos.

Los colores son `[r, g, b]`, `[r, g, b, a]` (0–255) o texto `"#rrggbb"` / `"#rrggbbaa"`; si
se omite es blanco. Teclas: `"a"`–`"z"`, `"0"`–`"9"`, `"arriba"`, `"abajo"`, `"izquierda"`,
`"derecha"`, `"espacio"`, `"entrar"`, `"escape"`, `"borrar"`, `"tab"`, `"mayus"`, `"ctrl"`,
`"alt"`, `"f1"`–`"f12"` (y los nombres de SDL). Los fallos son de tipo `ErrorJuego`
(sin SDL2, sin pantalla) o `Error`. Ejemplos completos: los nueve minijuegos de `examples/`.

Para pruebas automáticas (sin pantalla, con `SDL_VIDEODRIVER=dummy`): `LINCE_JUEGO_FOTOGRAMAS=N`
cierra el juego tras N fotogramas de 1/60 s; `LINCE_JUEGO_PULSAR=espacio,derecha` y
`LINCE_JUEGO_CLIC=x,y` pulsan en los fotogramas impares; `LINCE_JUEGO_ESCRIBIR=texto` teclea
(`<` borra); `LINCE_JUEGO_CAPTURA=ruta.bmp` guarda el último fotograma y
`LINCE_JUEGO_FOTOS=carpeta` todos; `LINCE_JUEGO_AUDIO=ruta.wav` guarda el sonido sincronizado con
esos fotogramas (para hacer un vídeo con sonido) y `LINCE_JUEGO_SEMILLA=n` hace las partidas repetibles.

### `bytes`

Datos binarios: leer y escribir archivos byte a byte (formatos de imagen, niveles
guardados en binario, partidas...). Unos bytes se usan como una lista de números de 0 a
255: `b[i]`, `b[i] = 0xff`, `b.longitud()`, `para cada numero x en b`, `==` compara el
contenido y `x es bytes` dice si lo son. `escribir(b)` los muestra en hexadecimal.

```lince
importar "bytes"
sea b = bytes.leer("imagen.bmp")
si b.parte(0, 2).a_texto() == "BM" {
    escribir("ancho: " + b.leer_u32(18) + ", alto: " + b.leer_u32(22))
}
```

| Función o método | Qué hace |
|---|---|
| `bytes.crear(n[, valor])` · `bytes.desde_lista([1, 2])` · `bytes.desde_texto(t)` | bytes nuevos (a 0 o a `valor`) · desde una lista · los de un texto en UTF-8 |
| `bytes.leer(ruta)` · `bytes.guardar(ruta, b)` | el archivo entero (también desde dentro de un ejecutable de `lince compilar`) · escribe el archivo |
| `bytes.unir([b1, b2, ...])` | unos detrás de otros |
| `b.agregar(n)` · `b.parte(desde[, hasta])` · `b.a_lista()` · `b.a_texto()` | añade uno al final · copia de un trozo (los negativos cuentan desde el final) · lista de números · texto (UTF-8) |
| `b.leer_u8(pos)` · `leer_i8` · `leer_u16` · `leer_i16` · `leer_u32` · `leer_i32` | entero sin signo (`u`) o con signo (`i`) de 1, 2 o 4 bytes en esa posición. Por defecto el byte de menos valor va primero (como en WAV y BMP); con un último argumento `verdadero`, el de más valor (como en PNG): `b.leer_u32(16, verdadero)` |
| `b.escribir_u8(pos, valor)` · ... · `b.escribir_i32(pos, valor[, verdadero])` | lo mismo para escribir; da error si el valor no cabe |

### `bits`

Operaciones bit a bit sobre enteros de 32 bits (de 0 a 4294967295; los negativos se
toman en complemento a dos). Los operadores de otros lenguajes (`&`, `|`, `^`, `<<`...)
aquí son funciones con nombre:

| Función | Qué hace | Equivale a |
|---|---|---|
| `ambos(a, b)` · `alguno(a, b)` · `solo_uno(a, b)` | 1 donde los dos tienen 1 · donde alguno · donde solo uno | AND `&` · OR `\|` · XOR `^` |
| `invertir(a)` | cambia todos los bits | NOT `~` |
| `izquierda(a, n)` · `derecha(a, n)` | desplaza `n` bits | `<<` · `>>` |
| `leer(a, pos)` · `poner(a, pos, 0 o 1)` | un bit (0 es el de menos valor) · el número con ese bit cambiado | |
| `contar(a)` | cuántos bits valen 1 | |
| `hex(a[, cifras])` · `binario(a[, cifras])` | como texto: `bits.hex(255)` → `"ff"`, `bits.binario(5, 8)` → `"00000101"` | |
| `de_hex(t)` · `de_binario(t)` | del texto al número | |

```lince
importar "bits"
fijo SALTA = 0b0001
fijo DISPARA = 0b0010
sea estado = bits.alguno(SALTA, DISPARA)          # 0b0011
si bits.ambos(estado, DISPARA) != 0 { escribir("dispara") }
```

### `pruebas`

```lince
importar "pruebas"
pruebas.suite("suma")                          # título de un grupo
pruebas.afirmar("2+2 es 4", 2 + 2 == 4)        # condición verdadera
pruebas.igual("listas", [1, 2], [1, 2])        # compara valores (listas y diccionarios incluidos)
pruebas.lanza("divide por 0", funcion(): nulo { sea x = 1 / 0 })  # la función debe dar error
pruebas.resumen()                              # total; sale con código 1 si algo falló
```

---

## Importar archivos y paquetes

```lince
importar "matematica"           # módulo estándar
importar "./utilidades"         # archivo local relativo
importar "paquete:mi-paquete"   # paquete instalado
```

Un archivo local se busca **junto al archivo que lo importa**, no en el
directorio desde el que se lanza el programa. Así una librería se puede
repartir en varios archivos y usarse desde cualquier sitio:

```
lib/colisiones.lince      importar "./rectangulos"   → lib/rectangulos.lince
app/juego.lince           importar "../lib/colisiones"
```

Importar dos veces el mismo archivo no hace nada la segunda vez, así que dos
librerías pueden depender de una tercera sin que se cargue dos veces.

Lo importado se define en el ámbito actual. Como un nombre no se puede
definir dos veces, dos librerías que compartan el nombre de una función dan
error en lugar de pisarse en silencio.

### Con espacio de nombres: `como`

```lince
importar "./colisiones" como col
importar "./geometria"  como geo

escribir(col.colisionan(a, b))
escribir(geo.PI)
```

Con `como`, el archivo se ejecuta en su propio ámbito y lo que define se
alcanza sólo a través del alias: nada se vuelca donde se importa. Así dos
librerías pueden tener funciones del mismo nombre sin estorbarse, y se ve de
un vistazo de dónde sale cada cosa.

Las funciones de la librería se ven entre ellas y leen sus propias
constantes, porque comparten ese ámbito. El alias ocupa un nombre como
cualquier otra definición, así que repetirlo da error.

`como` no es palabra reservada: se reconoce sólo después de un `importar`, y
sigue valiendo como nombre de variable o de parámetro.

**Límite actual:** las clases de una librería no se pueden instanciar a
través del alias (`geo.Punto(1, 2)`). Importa el archivo sin `como` para
usarlas.

---

## Gestor de paquetes

```bash
lince instalar https://github.com/usuario/paquete
lince instalar https://ejemplo.com/utilidades.lince
lince desinstalar nombre-paquete
lince listar
lince ayuda
```

Los paquetes se instalan en `~/.lince/paquetes/`.

---

## Ejecutables (`lince compilar`)

```bash
lince compilar programa.lince           # genera ./programa
lince compilar programa.lince -o nombre # nombre personalizado
```

Crea un único ejecutable autónomo: una copia del intérprete de Lince con el
programa y todos los archivos que importa metidos dentro (`importar "./x"`, con
o sin `como`, y `importar "paquete:x"`). En la máquina de destino no hace falta
instalar Lince, ni `gcc`, ni llevar los archivos originales. Los argumentos que
se le pasan al ejecutable llegan al programa con `argumentos()`.

Como el ejecutable lleva el mismo intérprete, soporta el **100 % del lenguaje**
(POO completa, `es`, generadores, todos los módulos...) y se
comporta exactamente igual que `lince programa.lince`. Compilar es instantáneo.

Las imágenes y los sonidos (`.png`, `.jpg`, `.bmp`, `.gif`, `.tga`, `.wav`, `.ogg`) que
el código nombra con un texto (`juego.imagen("img/lince.png")`) van dentro solos; si
alguno no existe al compilar, avisa con un ⚠. Lo demás (niveles, rutas que se construyen
al ejecutar como `"nivel" + n + ".txt"`) se incluye con `--incluir`, que se puede repetir
y acepta archivos o carpetas:

```bash
lince compilar juego.lince -o mi_juego --incluir niveles --incluir datos/semilla.json
```

Con eso `archivos.leer("niveles/uno.json")` lee del propio ejecutable, con la
ruta relativa tal como se indicó.

Notas:
- SDL2 (que usa `juego`) sigue haciendo falta en la máquina donde se ejecute:
  `lince repartir` (abajo) la lleva junto al juego.
- Un `.txt` o `.json` no se incluye solo, aunque el código lo nombre: puede ser un
  archivo que el juego escribe (récords) y la copia de dentro taparía la del disco.
- Un `importar` que no se puede resolver al compilar avisa con un ⚠ y el
  ejecutable fallará al llegar a esa línea, como lo haría el intérprete.
- Los archivos se leen al compilar: si cambian después, hay que volver a compilar.

### Compartir un juego (`lince repartir`)

```bash
lince repartir juego.lince                   # crea juego-linux/ y juego-linux.zip
lince repartir juego.lince -o carpeta --incluir niveles
```

Prepara un juego para pasárselo a alguien: una carpeta (`juego-linux` o `juego-windows`)
con el ejecutable (como el de `lince compilar`, con sus imágenes y sonidos dentro), SDL2
y un `LEEME.txt`, y un `.zip` con esa carpeta. Quien lo reciba lo descomprime y juega.

- **Windows**: lleva `SDL2.dll` y las DLL de las que dependa (si las hay en su carpeta).
- **Linux**: lleva `libSDL2-2.0.so.0`. El juego usa la SDL2 del sistema si está
  instalada y, si no, la de su carpeta.
- **macOS**: sin probar; SDL2 no se incluye (`brew install sdl2`).
- El ejecutable es para el sistema en el que se crea: para Windows, se reparte desde
  Windows (o con el `lince.exe` de `make windows` bajo Wine).

### Rendimiento

El ejecutable va a la velocidad del intérprete. Mediciones con los programas de
`bench/` (`make bench`), en segundos: `fib(30)` 0,3 · bucle de 5 millones de
iteraciones 0,5 · diccionario de 50.000 claves 0,04.

---

## Depurador

```bash
lince depurar programa.lince [argumentos]
```

Ejecuta el programa parándose en la primera línea. Comandos:

| Comando | Qué hace |
|---------|----------|
| `s` / `paso` | ejecuta la siguiente línea, entrando en las funciones |
| `n` / `siguiente` | ejecuta la siguiente línea sin entrar en las funciones |
| `o` / `salir` | sigue hasta salir de la función actual |
| `c` / `continuar` | sigue hasta el siguiente punto de parada (o el final) |
| `b 12`, `b lib.lince:5` | pone un punto de parada; `bs` los lista y `d n` borra el n-ésimo |
| `l`, `l 20` | muestra el código alrededor de la línea actual o de la indicada |
| `p expr` | evalúa una expresión en el ámbito actual (`p x * 2`, o `p x = 5` para cambiarla) |
| `v` / `g` | variables locales / globales |
| `bt` | pila de llamadas |
| `q` | termina el programa |

Si no hay más entrada (stdin cerrado), el programa sigue hasta el final.

---

## REPL interactivo

```bash
lince          # abre el REPL
lince prog.lince               # ejecuta archivo
lince prog.lince arg1 arg2     # con argumentos
lince nuevo mi_juego           # carpeta con un juego pequeño listo para cambiar
lince compilar prog.lince      # crea un ejecutable autónomo
lince repartir juego.lince     # carpeta y .zip para compartir el juego
lince depurar prog.lince       # depurador paso a paso
```

### Comandos del REPL

| Comando | Descripción |
|---------|-------------|
| `:ayuda` | muestra la ayuda |
| `:salir` | cierra el REPL |
| `:limpiar` | limpia la pantalla |
| `:historial` | muestra el historial |
| `:modulos` | lista módulos disponibles |
| `:cargar arch.lince` | ejecuta un archivo |

Atajos: ↑/↓ historial, ←/→ cursor, Inicio/Fin, Supr.

---

## Tests

```bash
make test                              # tests del intérprete
./lince tests/runner_compilador.lince  # tests de los ejecutables empaquetados
```

**Tests del intérprete:** los programas de `tests/lince/`, cada uno con su salida esperada (`.esperado`).

**Tests de los ejecutables:** programas de `tests/compilador/` que se empaquetan, se ejecutan y se comparan con su salida esperada, más `probar_empaquetado.sh` (argumentos, código de salida, archivos importados).

---

## Compatibilidad

| Plataforma | Estado | Compilar con |
|------------|--------|--------------|
| Linux | ✅ Probado en la CI | `make` (para jugar: `libsdl2-2.0-0`) |
| Windows (MSYS2 UCRT64) | ✅ Probado en la CI | `make` (para jugar: `SDL2.dll` junto al ejecutable) |
| Windows desde Linux | ✅ | `make windows` (compilación cruzada con MinGW) |
| Mac | ⚠️ Debería funcionar, sin probar | `make` |

---

## Cómo compilar

```bash
make              # compilar
make test         # tests del intérprete
make clean        # limpiar binarios
make install      # instalar en el sistema
```

---

## Estructura del proyecto

```
lince/
├── src/
│   ├── main.c          # punto de entrada y subcomandos CLI
│   ├── lexer.c/h       # análisis léxico
│   ├── parser.c/h      # análisis sintáctico (AST)
│   ├── interprete.c/h  # ejecución del AST
│   ├── modulos.c/h     # módulos estándar
│   ├── empaquetado.c/h # lince compilar: ejecutables autónomos
│   ├── repartir.c/h    # lince repartir: carpeta y .zip con el juego y SDL2
│   ├── recarga.c/h     # recarga en caliente de los juegos
│   ├── imagen.c/h      # PNG, JPG... (stb_image)
│   ├── ogg.c/h         # Ogg Vorbis (stb_vorbis)
│   ├── juego.h         # módulo juego (SDL2 cargada al ejecutar)
│   ├── terceros/       # stb_image y stb_vorbis, tal cual
│   ├── repl.c/h        # REPL interactivo con historial
│   ├── paquetes.c/h    # gestor de paquetes
│   └── plataforma.h    # compatibilidad multiplataforma
├── tests/
│   ├── runner.lince           # runner de tests del intérprete
│   ├── runner_compilador.lince # runner de tests de los ejecutables
│   ├── lince/                 # tests del intérprete
│   └── compilador/            # tests de los ejecutables empaquetados
├── editores/           # soporte VSCode, Sublime, Notepad++
├── examples/           # minijuegos de ejemplo
├── ESPECIFICACION.md
├── README.md
├── LICENSE
└── Makefile
```

---

*🐆 Lince — ágil, preciso, para hacer juegos.*
