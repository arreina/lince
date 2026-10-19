# 🐆 Lince — Especificación del Lenguaje v0.9

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
| `tarea` | función que corre aparte y puede esperar (solo cuenta delante de un nombre y `(`; no es palabra reservada) |
| `devolver` | return |
| `si` / `sino si` / `sino` | if / else if / else |
| `para` | for clásico |
| `cada` | usado con `para cada` |
| `mientras` | while |
| `romper` / `continuar` | break / continue |
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

### Varios nombres de una vez

Con corchetes, cada nombre toma un elemento de una lista; con llaves, el
valor de su clave en un diccionario:

```lince
sea [ancho, alto] = [320, 180]
sea [menor, mayor] = min_max(notas)     # una función que devuelve dos cosas
sea {vida, nombre} = enemigo            # enemigo["vida"], enemigo["nombre"]
fijo [COLUMNAS, FILAS] = [20, 15]
[a, b] = [b, a]                         # sin 'sea': asigna (aquí, intercambia)
```

- En una lista tiene que haber tantos elementos como nombres (si no,
  `ErrorRango`); en un diccionario, cada clave tiene que existir.
- Las listas y diccionarios de dentro se comparten, igual que con `sea x = l[0]`.
- Ojo: `y` y `o` son palabras del lenguaje y no valen como nombres, así que
  `sea [x, y] = pos` no funciona; `sea [px, py] = pos` sí.

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
| `++` `--` | incremento / decremento, como sentencia: `i++`, `mi.vidas--`, `puntos[j]++` |
| `+=` `-=` `*=` `/=` `%=` | operar y guardar: `x += 5` es `x = x + 5` |
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

Se puede llamar a cualquier cosa que dé una función, no solo a un nombre: una lista de
ataques, un diccionario de acciones, lo que devuelve otra función o un campo de un objeto.

```lince
golpes[i](fuerza)
acciones["saltar"]()
sumador(2)(3)
enemigo.al_morir()          # una función guardada en el campo (si la clase no tiene ese método)
```

El `(` va en la misma línea que lo que se llama: uno al principio de la línea siguiente
empieza otra expresión.

Dos funciones son iguales (`==`) si son la misma: la misma declaración, o la misma lambda
guardada en dos sitios. Dos lambdas escritas igual son distintas.

### Asignación compuesta

`+=`, `-=`, `*=`, `/=` y `%=` operan y guardan en un solo paso, en
variables, elementos y campos:

```lince
x += vx * dt            # x = x + vx * dt
puntos *= 2
nombre += "!"           # con texto, concatena
vidas[jugador] -= 1     # en una lista o un diccionario
mi.energia += 10        # en un campo
pos += Vector(1, 0)     # con objetos, usa el método 'sumar' de su clase
```

El índice y el objeto se calculan una sola vez (`l[f()] += 1` llama a `f`
una vez). En un diccionario la clave tiene que existir: `d["nueva"] += 1`
da `ErrorRango`.

### Reglas

- Todo parámetro indica `val` o `ref`
- Todo parámetro tiene tipo declarado; `cualquiera` acepta un valor de cualquier tipo
  (`val cualquiera v`, y también como tipo de retorno), y con `es` se mira cuál es
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

rango(5)                   # 0, 1, 2, 3, 4 (para 'para cada')
rango(1, 6)                # 1, 2, 3, 4, 5
rango(0, 10, 2)            # 0, 2, 4, 6, 8
rango(10, 0, -1)           # 10, 9, ..., 1

esperar()                  # en una tarea: hasta el fotograma siguiente
esperar(0.5)               # hasta que pase medio segundo
esperar(otra_tarea)        # hasta que acabe; da lo que devolvió

mapear(lista, funcion)     # aplica función a cada elemento
filtrar(lista, funcion)    # filtra según condición
reducir(lista, inicial, f) # acumula un valor
```

`mapear`, `filtrar` y `reducir` aceptan una lista, un generador (también `rango()`) o unos
bytes, y devuelven una lista (o el valor acumulado): `filtrar(rango(100), es_primo)`. Un
generador se recorre entero antes; si da más de un millón de valores (uno que no acaba nunca)
es un error, y lo que hay que hacer es recorrerlo con `para cada` y `romper`.

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

### `si` que da un valor

Con una sola expresión en cada rama, un `si` también vale como valor, y
`sino` es obligatorio (si no, no habría valor cuando la condición falla):

```lince
sea lado = si vel < 0 { -1 } sino { 1 }
sea nota = si n >= 9 { "sobresaliente" } sino si n >= 5 { "aprobado" } sino { "suspenso" }
juego.texto("{puntos} {si puntos == 1 { "punto" } sino { "puntos" }}", 10, 10)
```

Solo se evalúa la rama que toca. Cada rama lleva una expresión, no
sentencias; para más, el `si` de siempre.

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

El incremento del `para` clásico también puede ser `i += 2`, `i -= 5`,
`i *= 2`...: `para (i = 0; i < 100; i += 10) { ... }`.

### `romper` y `continuar`

`romper` sale del bucle en el que está; `continuar` se salta lo que queda de
la vuelta y pasa a la siguiente (en el `para` clásico, después del
incremento; en `hacer`, después de mirar la condición):

```lince
para cada enemigo e en enemigos {
    si e["muerto"] { continuar }      # este no cuenta
    si colision(jugador, e) {
        vidas -= 1
        romper                        # con un golpe por fotograma basta
    }
}
```

- Con bucles uno dentro de otro, solo sale del de **dentro**.
- Dentro de un `elegir`, sale del bucle que lo rodea (no hay que "romper"
  cada `caso`, que nunca siguen de uno a otro).
- Un `finalmente` se ejecuta igual, aunque se salga con `romper`.
- Fuera de un bucle es un error de sintaxis. Una función definida dentro de
  un bucle tampoco puede romperlo: el bucle es de quien la llama.

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

## Texto con valores dentro

Entre llaves, dentro de un texto, va cualquier expresión, y su valor se
escribe en ese sitio tal como lo haría `escribir`:

```lince
escribir("Vidas: {vidas}")
escribir("{nombre} tiene {puntos * 10} puntos")
juego.texto("Nivel {nivel + 1} de {niveles.longitud()}", 10, 10)
escribir("posición {pos}")      # un objeto se escribe con su a_texto()
```

Es lo mismo que sumar trozos (`"Vidas: " + vidas`), pero se lee mejor y no
hace falta `a_texto`.

- Para escribir una llave tal cual se pone `\{`: `"\{\"x\": {x}}"` da
  `{"x": 5}`. La `}` sola no hace falta escaparla.
- Unas llaves vacías, `{}`, se quedan como están (`texto.formato` las usa).
- Si lo de dentro no es una expresión (por ejemplo, JSON escrito sin `\{`),
  el error lo dice y recuerda lo de `\{`.

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

### Con valor, y recorrerlas

```lince
enumeracion Direccion {
    Arriba = [0, -1]
    Abajo = [0, 1],
    Izquierda = [-1, 0], Derecha = [1, 0]      # espacios, saltos de línea o comas
}

sea d = Direccion.Izquierda
escribir(d.nombre)     # "Izquierda"
escribir(d.indice)     # 2: su posición
escribir(d.valor)      # [-1, 0]; sin '=', el valor es su posición

para cada Direccion dir en Direccion {
    escribir(dir.nombre + ": " + dir.valor)
}
```

### Reglas

- Los valores son constantes: ni se cambian ni se añaden. `Estado.Volando` es un error que
  dice qué valores tiene `Estado`
- Cada valor se escribe como su texto, `"Estado.Quieto"` (en `escribir`, al sumarlo a un
  texto, dentro de `"{e}"` y en `json.texto`), y es igual a otro valor igual y a ese texto:
  `e == "Estado.Quieto"`. Así un valor guardado se sigue reconociendo tras recargar el
  programa en caliente
- `e es Estado` dice si `e` es un valor de esa enumeración. Un parámetro puede declararse
  `val Estado e`, y `val texto e` también lo acepta
- Como clave de un diccionario cuenta su texto: `veces[Estado.Quieto] = 1` es la clave
  `"Estado.Quieto"`
- **Aviso de `elegir`**: si un `elegir` sin `otro` tiene todos sus casos de la misma
  enumeración (del mismo archivo o de uno importado con `importar "./..."`, también con
  `como`) pero le faltan
  valores, Lince avisa al empezar,
  con `lince comprobar` y en el editor. Lo normal es que se haya añadido un valor y se haya
  olvidado ahí:

  ```
  ⚠️  Aviso en línea 12: este 'elegir' no trata Estado.Saltando ni Estado.Golpeando: añádelos, o pon 'otro { }' si no hace falta.
  ```

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

Los números se calculan según se recorren: `rango(1000000)` no ocupa memoria.

### Reglas

- `generador` declara una función generadora en vez de `funcion`
- `producir` da un valor a quien recorre el generador, y el generador se queda parado
  ahí hasta que le pida el siguiente: los valores salen de uno en uno, según se usan
- Los generadores se usan con `para cada`. Guardado en una variable (`sea g = contar(10)`),
  un generador sigue por donde iba: si un bucle sale con `romper`, el siguiente `para cada`
  sobre `g` continúa con el valor siguiente, y cuando acaba ya no da más. `rango()` se puede
  recorrer las veces que se quiera
- No tienen límite de valores: uno puede no acabar nunca (`mientras verdadero { producir n }`).
  Si el bucle sale antes (`romper`, `devolver` o un error) y el generador no está guardado en
  ninguna variable, se para y se ejecutan sus `finalmente`; uno guardado se para cuando se
  suelta (`g = nulo`)
- Dentro de un generador no se puede `esperar()`: quien espera es quien lo recorre

---

## Tareas

Una tarea es código que puede pararse a esperar sin parar el resto del programa: las
cosas que pasan a lo largo del tiempo se escriben en orden, como se piensan, en vez de
con variables que recuerden por qué paso se va.

```lince
tarea ataque_jefe() {
    jefe.animar("cargar")
    esperar(0.8)                 # solo se para esta tarea: el juego sigue
    para (i = 0; i < 3; i++) {
        disparar(jefe.x, jefe.y)
        esperar(0.2)
    }
    mientras no jefe.en_suelo { esperar() }
}

ataque_jefe()                    # empieza ya, y sigue sola en cada fotograma
```

- `tarea nombre(parámetros) { ... }` se declara como una función; el tipo de retorno
  (`: tipo`) es opcional. Llamarla la **empieza**: corre hasta su primer `esperar` y la
  llamada devuelve la tarea
- `esperar()` la deja aparcada en esa línea hasta el fotograma siguiente;
  `esperar(segundos)`, hasta que pase ese tiempo; `esperar(otra_tarea)`, hasta que la otra
  acabe, y da lo que devolvió con `devolver`. Después sigue justo donde estaba, con sus
  variables
- Se puede esperar en cualquier función a la que llame la tarea, y así repartir una
  tarea larga en trozos:

  ```lince
  funcion caminar_hasta(ref Enemigo e, val numero x): nulo {
      mientras e.x < x {
          e.x += 60 * juego.dt()
          esperar()
      }
  }
  tarea patrulla(ref Enemigo e) {
      mientras verdadero {
          caminar_hasta(e, 300)
          esperar(1)
          caminar_hasta(e, 500)
      }
  }
  ```
- Puede haber muchas a la vez (una por enemigo, una para el diálogo...): cada una va a su
  ritmo, en el orden en que empezaron
- Un método también puede ser una tarea, con `mi`: `tarea patrullar() { ... }` dentro de la
  clase, y `enemigo.patrullar()` la empieza (`crear` no puede serlo)
- Una tarea **sin nombre** se escribe como una lambda, y se empieza al llamarla:
  `sea parpadeo = tarea(val numero veces) { ... }` y luego `parpadeo(3)`
- En un parámetro, `val tarea t` solo acepta una tarea
- `t.terminada()` dice si ha acabado; `t.parar()` la para: se deshace desde donde esperaba,
  ejecutando sus `finalmente` (un `capturar` no lo captura). `t es tarea` comprueba el tipo
- Un error dentro de una tarea, antes de su primer `esperar`, le llega a quien la llamó;
  después, a quien la hace avanzar (en un juego, `juego.presentar()`), con la línea de la
  tarea
- **En un juego** avanzan solas una vez por fotograma, al llamar a `juego.presentar()`
  y antes de mostrarlo, así que lo que dibujen sale en ese fotograma. `esperar(segundos)`
  cuenta el tiempo del juego: se para con `congelar()` y va más despacio con
  `velocidad()`. Fuera de una tarea no se puede `esperar` (el bucle del juego no se puede
  parar)
- **Sin juego**, `esperar` fuera de una tarea es lo que las hace avanzar, a 60 vueltas por
  segundo y con el reloj de verdad: `esperar(t)` espera a que acabe `t`
- Las que quedan al acabar el programa, o al recargarlo, se paran (con sus `finalmente`)
- Por dentro, cada tarea tiene su propia pila: pasar de una a otra cuesta alrededor de un
  microsegundo, y mil tareas esperando ocupan unos 16 MB

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
matematica.grados(matematica.PI)  # 180 (juego mide los ángulos en grados)
matematica.radianes(90)       # 1.5707... (seno y coseno van en radianes)
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
tiempo.esperar(0.5) # para el programa medio segundo (en un juego, mejor una tarea con esperar())
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
sea datos = json.parsear("\{\"nombre\": \"Ana\"}")   # \{: la llave tal cual
escribir(datos["nombre"])
escribir(json.texto({"clave": "valor"}))
escribir(json.texto({"clave": [1, 2]}, verdadero))   # en varias líneas, con sangría
```

`json.parsear` da un error si el texto no es JSON válido. Los números se escriben con las cifras
justas para leerlos después iguales.

Lo normal es leer el JSON de un archivo (`json.parsear(archivos.leer("datos.json"))`);
escrito dentro de un texto, cada `{` lleva delante `\` para que no se tome por
un valor metido en el texto (ver «Texto con valores dentro»).

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
la pone).

Casi todo `juego` está en C, porque habla con la pantalla, el teclado, el mando y el altavoz o
trabaja píxel a píxel. Lo que no lo necesita está escrito en Lince, en
[`src/lince/juego.lince`](src/lince/juego.lince), y va dentro del ejecutable: `combo`,
`mantenida`, `pulsaciones`, `curva`, `suavizar`, `acercar`, las escenas y el animador (`animador`,
`animacion`, `acabada`, `pisada`, `golpe` y `libre`; `dibujar()` y `caja()` le preguntan qué
cuadro toca). Para quien lo usa no hay
diferencia; quien quiera una versión distinta puede leerla y copiarla. Un error dentro de una de
esas funciones se cuenta en la línea del programa que la llamó.

Reglas de todo el módulo:

- **Los tiempos, en segundos** (`0.5` es medio segundo), y los ángulos en grados.
- **Las opciones van en un diccionario**, y una opción mal escrita es un error que dice cuáles
  hay: `dibujar(): no conozco la opción 'escla' (hay "escala", "ancho"...)`.
- **Los errores:** `ErrorTipo` si un argumento no es del tipo que toca, `ErrorArgumento` si su
  valor (o una opción) no vale, y `ErrorJuego` si falla algo de fuera: la ventana sin abrir,
  un archivo que no está o no se puede escribir, sin SDL2... `capturar (error e)` los
  captura todos.

El bucle lo escribes tú:

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
| `abrir(título, ancho, alto[, {"escala": n}])` / `cerrar()` / `salir()` | abre la ventana / la destruye / pide terminar el bucle (`abierto()` pasa a falso, como al pulsar la X). Con `escala`, la ventana es `n` veces mayor y cada píxel del juego se ve como un cuadrado de n × n: `abrir("Pelea", 320, 180, {"escala": 3})` para un juego de píxeles grandes, como los de consola portátil; todo se sigue midiendo en 320 × 180 |
| `abierto()` · `titulo(t)` · `ancho()` · `alto()` | estado y tamaño |
| `presentar()` | hace avanzar las [tareas](#tareas), muestra lo dibujado, espera al siguiente fotograma y actualiza la entrada y `dt()` |
| `dt()` · `fps()` · `tiempo()` | segundos del último fotograma · fotogramas por segundo · segundos desde que se abrió. Son tiempo de juego: se paran con `congelar()` y van más despacio con `velocidad()` (ver **Peleas**) |
| `tiempo_real()` | segundos desde que se abrió, sin pararse con `congelar()` ni cambiar con `velocidad()`: para lo que tiene que seguir moviéndose durante un parón (un menú, el fundido de las escenas) |
| `pantalla_completa([sí/no])` | sin argumento alterna; el juego se escala sin deformarse y el ratón sigue en coordenadas del juego |

**Dibujo**

| Función | Qué hace |
|---|---|
| `limpiar([color])` | rellena toda la ventana |
| `cubrir(color)` | pinta toda la ventana encima de lo dibujado, sin moverse con la cámara ni con el temblor: con alfa la oscurece o la tiñe (`juego.cubrir([0, 0, 0, 160])` en una pausa) |
| `rectangulo(x, y, ancho, alto[, color])` · `contorno(...)` | rectángulo relleno / solo borde |
| `circulo(x, y, radio[, color])` · `linea(x1, y1, x2, y2[, color])` | |
| `texto(t, x, y[, escala[, color]])` · `ancho_texto(t[, escala])` | fuente de mapa de bits incluida (mayúsculas, cifras, signos; las minúsculas salen como mayúsculas; tildes y ñ se marcan) |
| `texto(t, x, y, opciones)` · `ancho_texto(t, opciones)` · `medir_texto(t[, opciones])` | con otra fuente, alineado o partido en líneas (ver **Letras**, abajo). `medir_texto` da `[ancho, alto]` |
| `posicion_camara()` | `[x, y]` de la cámara (sin el temblor) |
| `camara(x, y)` · `camara()` | desplaza todo lo que se dibuja (para mundos más grandes que la pantalla) / vuelve a (0, 0), por ejemplo para el marcador, que tampoco tiembla con `temblor()` |
| `captura(ruta)` | guarda la ventana como `.bmp` |

**Letras.** Además de la fuente incluida, `fuente()` carga una fuente TrueType u OpenType
(`.ttf`, `.otf`, como las de Google Fonts) a un tamaño, o hace una con letras dibujadas en una
imagen (pixel art):

```lince
fijo LETRA = juego.fuente("redondita.ttf", 28)         # 28 píxeles de alto
fijo PIXEL = juego.fuente("letras.png", "ABCDEFGHIJKLMNÑOPQRSTUVWXYZ0123456789.,!?")

juego.texto("¡Hola, Ñandú!", 20, 20, LETRA)
juego.texto("PUNTOS: 120", 400, 20, {"fuente": PIXEL, "escala": 3, "color": [255, 220, 0], "alinear": "centro"})
juego.texto(dialogo, 40, 300, {"fuente": LETRA, "ancho": 560})     # parte las líneas por los espacios
```

| Función | Qué hace |
|---|---|
| `fuente(ruta, tamaño)` | una fuente `.ttf` u `.otf` a ese tamaño en píxeles. Las letras se dibujan una vez al cargarla, así que escribir es tan rápido como dibujar sprites. Trae el español entero (tildes, ñ, ü, ¿, ¡), comillas, guiones, puntos suspensivos y el euro, y junta los pares de letras que la fuente diga ("AV") |
| `fuente(imagen, caracteres[, opciones])` | una fuente hecha con las letras dibujadas en una imagen (o en su ruta), en el orden de `caracteres`. Cada letra se separa de la siguiente por columnas transparentes y cada fila de letras por filas transparentes. Si una letra tiene partes separadas (`"`, `%`), o es de ancho fijo, `{"celda": [ancho, alto]}` la parte en cuadros iguales. `separacion`: píxeles entre letras (1; 0 con `celda`); `espacio`: el ancho del espacio |

Con un diccionario de opciones en lugar de la escala, `texto()` admite `fuente`, `escala`, `color`
(con alfa: `[255, 255, 255, 128]`), `alinear` (`"izquierda"`, `"centro"` o `"derecha"`: `x` es
entonces el centro o el borde derecho) y `ancho` (parte en líneas para no pasar de ese ancho; los
saltos de línea `\n` también cuentan). También se le puede pasar la fuente tal cual, como en la
primera línea. Si a la fuente le falta una letra usa la más parecida: la mayúscula si no tiene
minúsculas, la letra sin tilde, o `?`. Los archivos de fuentes van dentro del juego al hacer
`lince compilar` o `lince repartir`, como las imágenes.

**Imágenes y sprites**

| Función | Qué hace |
|---|---|
| `imagen(ruta[, opciones])` | carga un PNG, JPG, BMP, GIF o TGA; devuelve `{"imagen", "ancho", "alto", "ruta"}`. Cargar la misma ruta dos veces no repite el trabajo. Opciones para imágenes descargadas: `fondo` (`"auto"`: el color de las esquinas, si la imagen no tiene transparencia; o un color, `[255, 255, 255]`) lo vuelve transparente, y `escala` (de 0 a 1) la reduce al cargarla, que queda mucho mejor que reducirla al dibujar. `colores` cambia colores: cada par `[de, a]` cambia el color `de` por `a` y también sus sombras y brillos (los de un tono parecido), conservando cuánto más claros u oscuros eran; así se le cambia la ropa a un personaje con un solo color, como el segundo jugador de un juego de lucha: `{"colores": [[[213, 89, 106], [70, 200, 120]]]}` (hasta 16 pares; los grises y los colores de otro tono no se tocan) |
| `sprite(filas, colores)` | crea una imagen a partir de texto, un carácter por píxel: `juego.sprite([".X.", "XXX"], {"X": [255, 0, 0]})`; los caracteres sin color son transparentes |
| `hoja(imagen o ruta[, cómo dividirla])` | una imagen con varios cuadros (los fotogramas de una animación, los tiles de un mapa) partida en esos cuadros; ver **Hojas de sprites** más abajo. También vale una imagen hecha con `sprite()` |
| `animar(imagen o ruta, tipo[, opciones])` | genera una animación a partir de un solo dibujo y la devuelve como hoja (ver abajo) |
| `guardar_imagen(imagen, ruta)` | guarda una imagen (un sprite, una hoja generada...) como `.png`, por ejemplo para retocarla en un editor |
| `dibujar(imagen, x, y[, opciones])` | dibuja la imagen; `opciones` es un diccionario con cualquiera de: `escala`, `ancho`, `alto`, `angulo` (grados), `espejo`, `voltear`, `centrado` (x, y es el centro), `alfa` (0–255), `color` (tinte), `blanco` (de 0 a 1: lo tiñe de blanco, el destello de cuando a alguien le dan), `mezcla` (`"sumar"`: lo dibujado se suma a lo de debajo y brilla, para energía, fuego y luces; `"multiplicar"`: oscurece, para sombras), `recorte` (`[x, y, ancho, alto]`), `cuadro` (el número de cuadro de una hoja; con una imagen normal, indica también `celda`: `[ancho, alto]` de cada cuadro) y `fila` (en una hoja con una animación por fila, cuenta `cuadro` dentro de esa fila) |

**Entrada**

| Función | Qué hace |
|---|---|
| `tecla(nombre)` · `tecla_pulsada(n)` · `tecla_soltada(n)` | mantenida / pulsada en este fotograma / soltada en este fotograma |
| `raton()` · `boton_raton(n)` · `clic(n)` · `boton_raton_soltado(n)` · `rueda()` | `[x, y]` · botón mantenido (1 izquierdo, 2 central, 3 derecho) · clic de este fotograma · soltado en este fotograma · giro de la rueda |
| `mandos()` · `boton(nombre[, mando])` · `boton_pulsado(nombre[, mando])` · `boton_soltado(nombre[, mando])` · `eje(nombre[, mando])` | mandos conectados (hasta 4; `mando` va de 1 a 4) · botones `a`, `b`, `x`, `y`, `lb`, `rb`, `inicio`, `atras`, `guia`, `arriba`, `abajo`, `izquierda`, `derecha`, `stick_izq`, `stick_der` · ejes `izq_x`, `izq_y`, `der_x`, `der_y` (−1 a 1) y gatillos `lt`, `rt` (0 a 1) |
| `cursor(visible)` · `capturar_raton(si)` · `movimiento_raton()` | muestra u oculta la flecha · encierra y oculta el ratón para apuntar (devuelve si se pudo) · `[dx, dy]` movido desde el fotograma anterior |
| `mando_tactil([opciones])` · `mando_tactil(falso)` | el mando de la pantalla táctil, como el de los juegos de móvil: a la izquierda una palanca que aparece donde se apoya el pulgar, y a la derecha los botones. **Sale solo** en un móvil (o con pantalla táctil) si alguna acción usa el mando, con los botones que usan (`"mando:a"`, `"mando:x"`...; `inicio` va arriba) y la palanca si usan la palanca izquierda o la cruceta; **se esconde** mientras haya un mando de verdad conectado (por Bluetooth o cable) y vuelve si se desconecta. Funciona como un mando de verdad (se suma al mando 1), así que un juego hecho para teclado y mando se juega en el móvil sin cambiar nada; la palanca también hace de cruceta. `mando_tactil(opciones)` lo ajusta: `botones`, `palanca`, `alfa` (120), `tamaño` (1) y `siempre` (verdadero: también en el ordenador, con el ratón, y aunque haya mando); `mando_tactil(falso)` lo quita, para un juego con controles propios. Se dibuja encima de todo, sin moverse con la cámara. Está escrito en Lince |
| `vibrar(segundos[, fuerza])` | hace vibrar el móvil y los mandos conectados (fuerza de 0 a 1): un golpe, una explosión. Donde no se puede, no hace nada |
| `inclinacion()` | `[x, y]` de -1 a 1: hacia dónde rodaría una bola sobre la pantalla del móvil (x a la derecha, y hacia abajo), según cómo se incline; `[0, 0]` sin acelerómetro (en el ordenador) |
| `acciones()` | `{nombre: [entradas]}`: las acciones definidas, con sus entradas como se escribieron (para enseñar los controles en un menú) |
| `mando_virtual(botones, ejes)` | para hacer un mando propio: deja pulsados esos botones (`["a"]`) y esos ejes (`{"izq_x": -0.5}`) en el mando 1 hasta que se cambien |
| `toques()` · `tactil()` | los dedos que hay en la pantalla táctil: una lista de `{"id", "x", "y", "nuevo"}` en coordenadas del juego (`nuevo`: empezó a tocar en este fotograma; `id` sigue al mismo dedo mientras no se levante). Sin pantalla táctil, el botón izquierdo del ratón cuenta como un dedo, para probarlo en el ordenador · ¿hay pantalla táctil? (en un móvil, siempre) |
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
| `definir_accion(nombre, entradas)` | crea o sustituye la acción. Entradas: una tecla (`"espacio"`), `"mando:a"` (botón de cualquier mando), `"mando2:a"` (del mando 2), `"mando:izq_x-"` / `"mando:izq_x+"` (eje hacia un lado), `"raton:izquierdo"` / `"raton:central"` / `"raton:derecho"`, `"pantalla:x,y,ancho,alto"` (un botón en la pantalla táctil: cuenta mientras haya un dedo dentro de ese rectángulo; el juego lo dibuja como quiera), `"inclinacion:x-"` / `"inclinacion:x+"` / `"inclinacion:y-"` / `"inclinacion:y+"` (inclinar el móvil hacia ese lado: gradual, entera con media inclinación, y nada si apenas se inclina) |
| `accion(n)` · `accion_pulsada(n)` · `accion_soltada(n)` | mantenida / empieza en este fotograma / termina en este fotograma |
| `direccion(negativa, positiva)` | de −1 a 1: entero con teclas, gradual con el stick |

Para escribir un nombre (por ejemplo para un récord): `nombre = juego.editar(nombre, 10)` en cada fotograma y `juego.tecla_pulsada("entrar")` para terminar.

**Sonido**

| Función | Qué hace |
|---|---|
| `sonido(ruta)` | carga un `.wav` o un `.ogg` (Ogg Vorbis, mucho más pequeño: mejor para la música); devuelve `{"sonido", "duracion", "ruta"}` |
| `reproducir(sonido[, volumen[, bucle]])` · `reproducir(sonido, opciones)` | lo hace sonar (se mezclan hasta 32 a la vez); devuelve el número de la voz. Opciones: `volumen` (0 a 1), `bucle`, `lado` (−1 izquierda, 0 en medio, 1 derecha: que suene de donde pasa) y `tono` (1 normal, 2 una octava más agudo y el doble de rápido, 0,5 más grave; variarlo un poco, de 0,9 a 1,1, hace que un golpe repetido no suene siempre igual) |
| `detener([voz])` | para esa voz o, sin argumento, todo |
| `musica(ruta o sonido[, volumen])` · `musica(nulo)` | suena en bucle y sustituye a la anterior / la para. Con la ruta de un `.ogg` (`juego.musica("tema.ogg")`) se lee poco a poco mientras suena: una canción de 3 minutos ocupa unos cientos de KB en vez de unos 30 MB. Si ya suena esa misma, solo cambia el volumen (no vuelve a empezar al recargar en caliente) |
| `volumen([v])` | volumen general de 0 a 1 |
| `pitido(frecuencia, segundos[, volumen])` | tono sencillo, sin archivos: `pitido(440, 0.1)` |
| `efecto(tipo[, opciones])` | genera y hace sonar un efecto al estilo de los juegos de 8 bits, sin archivos: `"moneda"`, `"salto"`, `"disparo"`, `"explosion"`, `"golpe"`, `"subir"`, `"clic"` o `"perder"`. Cada vez suena un poco distinto. Opciones: `volumen` (0,5), `tono` y `largo` (multiplican la frecuencia y la duración: `{"tono": 1.5}` más agudo) y `semilla` (siempre el mismo sonido) |
| `crear_efecto(tipo[, opciones])` | el efecto como sonido, para `reproducir()` o `guardar_sonido()`; sin `semilla` usa la 1 |
| `guardar_sonido(sonido, ruta)` | guarda un sonido como `.wav` |

**Colisiones y animación**

| Función | Qué hace |
|---|---|
| `colision(x1, y1, w1, h1, x2, y2, w2, h2)` · `colision(caja1, caja2)` | ¿se solapan dos rectángulos? |
| `colision_circulos(x1, y1, r1, x2, y2, r2)` · `colision_circulo_rect(cx, cy, r, x, y, w, h)` | ¿se tocan dos círculos? · ¿toca el círculo al rectángulo? |
| `dentro(px, py, x, y, w, h)` | ¿está el punto dentro del rectángulo? (botones, clics) |
| `caja(cosa, x, y[, opciones])` | el rectángulo `[x, y, ancho, alto]` de lo que se ve de verdad al dibujar `cosa` (una imagen, una hoja o un animador) con esas mismas `x`, `y` y opciones, sin el hueco transparente de alrededor (ver abajo) |

En `colision`, `colision_circulos`, `colision_circulo_rect` y `dentro`, cada rectángulo puede ir en cuatro
números o en una lista `[x, y, ancho, alto]`, como las que da `caja()`, o en un diccionario con
`x`, `y`, `ancho` y `alto` (lo que mueve `mover_en_mapa`, un cuerpo de la física): `juego.colision(yo,
enemigo)`. Cada círculo va en tres números o en `[x, y, radio]`.
| `cuadro(cuadros, por_segundo[, desde[, repetir]])` | el cuadro de una animación que toca ahora: de 0 a `cuadros` − 1, o el elemento que toca si `cuadros` es una lista (`[0, 1, 2, 1]`). `desde` es el `tiempo()` en que empezó; con `repetir` falso se queda en el último |

**Movimiento suave.** Para que las cosas no aparezcan de golpe: un menú que entra desde un lado,
una moneda que vuela al marcador, un título que cae y rebota, una cámara que sigue al jugador sin
tirones.

```lince
sea inicio = juego.tiempo()                                   # cuando empieza el movimiento
# en cada fotograma:
sea y = juego.suavizar(-80, 120, 0.8, inicio, "rebote")       # cae de -80 a 120 en 0,8 s y rebota
juego.texto("¡NIVEL 2!", 400, y, {"fuente": LETRA, "alinear": "centro"})
camara_x = juego.acercar(camara_x, jugador_x - 400, 6)        # la cámara va detrás, frenando al llegar
```

| Función | Qué hace |
|---|---|
| `suavizar(desde, hasta, segundos, inicio[, curva])` | el valor que toca ahora en un cambio de `desde` a `hasta` que empezó en `inicio` (un `tiempo()`) y dura `segundos`. Antes de empezar da `desde`, y al acabar, `hasta`. Valen números o listas de números del mismo largo: `[x, y]`, un color... |
| `acercar(actual, objetivo, rapidez)` | un paso de `actual` hacia `objetivo` en este fotograma, más largo cuanto más lejos, así que frena al llegar. Con rapidez 10 recorre en una décima de segundo unos dos tercios de lo que le falta, vaya el juego a los fotogramas que vaya. También con listas |
| `curva(nombre, t)` | cuánto del camino se ha hecho en el momento `t` (de 0 a 1), para usar las curvas en cualquier cosa |

Las curvas: `"lineal"` (igual todo el rato), `"suave"` (arranca y frena poco a poco; la que se usa si no
se dice), `"entrar"` (arranca despacio), `"salir"` (frena al final), `"rebote"` (rebota al llegar, como
una pelota), `"elastico"` (se pasa y vibra, como un muelle) y `"atras"` (se pasa un poco y vuelve).

**Hojas de sprites.** La manera tradicional de animar: todos los fotogramas dibujados en una sola
imagen, uno al lado del otro (y, si hay varias animaciones, una por fila). `hoja()` la divide
en cuadros, que se numeran desde 0 de izquierda a derecha y de arriba abajo:

```lince
fijo HEROE = juego.hoja("heroe.png")                                # se divide sola
juego.dibujar(HEROE, x, y, {"cuadro": juego.cuadro(HEROE["cuadros"], 8)})   # 8 cuadros por segundo
```

| Cómo dividirla | Qué hace |
|---|---|
| `hoja("heroe.png")` | sola: busca las figuras (cada trozo de dibujo separado de los demás por fondo), las agrupa en filas y las ordena de izquierda a derecha. Pueden medir distinto y estar desordenadas: se copian a una hoja nueva en rejilla, cada una centrada en su celda y, dentro de su fila, a la misma altura que en la original (los pies alineados, y un salto sigue en el aire). Los trocitos sueltos (una gota, una chispa) se unen a la figura más cercana. Si la imagen no tiene transparencia (un JPG con fondo blanco), quita el fondo sola |
| `hoja("heroe.png", {"columnas": 6, "filas": 2})` | en cuadros iguales, diciendo cuántos hay (`filas` es 1 si no se dice). Para hojas en rejilla, sobre todo si un fotograma toca al de al lado o el personaje cambia de sitio dentro de su cuadro |
| `hoja("tiles.png", 16)` · `hoja("tiles.png", 16, 24)` | en cuadros iguales, diciendo lo que mide cada uno (lo normal en los tiles) |

Las dos últimas admiten `separacion` (píxeles entre cuadros) y `margen` (alrededor), como en muchas
hojas descargadas: `hoja("tiles.png", 16, {"separacion": 1})`. Las tres admiten `fondo`, `escala` y `colores`,
como `imagen()`: para una hoja enorme, `hoja("explorador.jpg", {"escala": 0.15})` (los tamaños de la
última se dan en píxeles de la imagen original). Quitar el fondo rellena desde los bordes de la
imagen, así lo que es del mismo color pero está dentro del dibujo (ojos, calcetines blancos) se
queda. Un número de cuadro fuera de rango
da la vuelta (−1 es el último). La hoja devuelta lleva `"cuadros"`, `"celda"` (`[ancho, alto]` de
cada cuadro), `"columnas"`, `"filas"` y `"por_fila"` (cuántos cuadros hay en cada fila).

Para no usar algunos cuadros (uno repetido, o unos del principio en que el personaje está
girándose), `quitar`: los que quedan se vuelven a numerar seguidos desde 0, así el resto del juego
no tiene que saber nada. También vale con una hoja ya hecha: `hoja(OTRA, {"quitar": [5]})`.

```lince
fijo CORRE = juego.hoja("corre.png", {"quitar": [0, 1, 2]})
```

Si cada fila es una animación, `fila` elige cuál y `cuadro` cuenta dentro de ella (y da la
vuelta al llegar al final de la fila):

```lince
fijo PJ = juego.hoja("personaje.png")                   # fila 0: andar, fila 1: saltar...
sea fila = 0
si saltando { fila = 1 }
juego.dibujar(PJ, x, y, {"fila": fila, "cuadro": juego.cuadro(PJ["por_fila"][fila], 10)})
```

**Pisadas.** Para que los pasos suenen justo cuando el pie toca el suelo, `pisadas(hoja[, fila])`
mira los cuadros y devuelve en cuáles hay una pisada: cuando aterriza un pie (tras ir por el aire,
o al lado del otro) o cuando el pie de delante aparece más adelante que en el cuadro anterior
(andando, con los dos pies casi siempre en el suelo). Supone que el personaje mira a la derecha y
que la animación da la vuelta (el último cuadro va antes del primero). Con un tercer argumento,
`pisadas(hoja, fila, cuadros)`, mira esa secuencia (una lista de cuadros de la fila, o cuántos
desde el 0) y da las posiciones dentro de ella; `fila` puede ser `nulo` para toda la hoja. Se
calcula una vez por hoja y secuencia. `cuadros_de(hoja[, fila])` dice cuántos cuadros con dibujo
tiene esa fila (en una rejilla no cuentan las celdas vacías del final). Los sonidos los pones tú:

```lince
fijo PASOS = juego.pisadas(CORRE)                     # por ejemplo [1, 7, 13, 19, 26, 31]
fijo PASO = juego.sonido("paso.wav")
# en cada fotograma, cuando cambia el cuadro:
si cuadro != anterior y PASOS.contiene(cuadro) { juego.reproducir(PASO) }
```

La otra manera de tener una animación es generarla a partir de un solo dibujo con `animar()`
(abajo), que también da una hoja y se dibuja igual. Así se puede empezar con una generada y
cambiarla después por la dibujada a mano sin tocar el resto del juego.

**Recarga en caliente de las hojas.** Si cambia en el disco la imagen de una hoja con el juego
en marcha (la retocas en un editor, o la vuelves a generar), la hoja se rehace sola: se vuelve a
quitar el fondo, a reducir y a dividir como se pidió. Todo lo que la usa ve los cuadros nuevos al
momento, también los animadores guardados en variables. Si cambió de forma (otro número de
cuadros, otro tamaño de celda), el programa se vuelve a ejecutar como al guardar el código,
conservando las variables, para que `["cuadros"]`, `["celda"]` y lo demás se pongan al día.

**Animador.** Lleva por ti qué animación toca, desde cuándo, si se repite o se queda en el último
cuadro, cuál va después y cuándo pisa un pie. Todo su estado va dentro, así que no hay que
llevar tiempos ni números de cuadro:

```lince
fijo NINO = juego.hoja("explorador.png")
sea pj = juego.animador(NINO, {
    "andar":  {"fila": 0, "por_segundo": 10},
    "saltar": {"fila": 2, "por_segundo": 12, "repetir": falso, "siguiente": "quieto"},
    "quieto": {"fila": 2, "cuadros": [0]}
})
fijo PASO = juego.sonido("paso.wav")

# en cada fotograma:
si juego.accion_pulsada("saltar") {
    juego.animacion(pj, "saltar")                   # al acabar pasa sola a "quieto"
} sino si pj["actual"] != "saltar" {
    si andando {
        juego.animacion(pj, "andar")                # si ya estaba andando, sigue por donde iba
    } sino {
        juego.animacion(pj, "quieto")
    }
}
si juego.pisada(pj) { juego.reproducir(PASO) }       # un pie acaba de tocar el suelo
juego.dibujar(pj, x, y, {"centrado": verdadero, "espejo": mira_izquierda})
```

| Función | Qué hace |
|---|---|
| `animador(hoja, animaciones[, inicial])` | crea el animador. Cada animación tiene un nombre y un diccionario con: `fila` (de la hoja; si no se dice, todos los cuadros), `cuadros` (cuáles y en qué orden, contando dentro de la fila: `[0, 1, 2, 1]`), `por_segundo` (10), `repetir` (verdadero; falso: se queda en el último cuadro), `siguiente` (la que va después de una que no se repite), `pisadas` (verdadero, falso o la lista de cuadros con pisada; si no se dice, se calculan en las que se repiten), `golpes` y `cancelar` (ver **Peleas**) y `hoja` (si esa animación está en otra hoja, por ejemplo una de `animar()`). Empieza con `inicial`, o con la primera. Sin `cuadros`, una animación de una hoja en rejilla no cuenta las celdas vacías del final de su fila (una fila de 8 celdas con 6 dibujos es una animación de 6 cuadros) |
| `animacion(animador, nombre[, desde_el_principio])` | pone esa animación. Si ya estaba puesta sigue por donde iba (se puede llamar en cada fotograma), salvo que se pida `desde_el_principio` |
| `dibujar(animador, x, y[, opciones])` | dibuja el cuadro que toca, con las mismas opciones que una imagen |
| `pisada(animador)` | ¿ha tocado un pie el suelo desde la última vez que se preguntó? Llámala una vez por fotograma |
| `acabada(animador)` | ¿ha llegado al final una animación que no se repite? |
| `golpe(animador, x, y[, opciones])` · `libre(animador)` | la caja con la que golpea ahora (o `nulo`) · ¿se puede pasar ya a otra animación? Ver **Peleas** |

**Choques con lo que se ve.** En una hoja cada cuadro es una celda del mismo tamaño, y la figura
no la llena: en una hoja descargada o hecha con IA suele haber mucho hueco transparente alrededor,
y cambia de un cuadro a otro (las piernas abiertas son más anchas). Chocar con la celda es chocar
con el aire. `caja()` da el rectángulo de lo que se ve en el cuadro que toca, colocado como lo pone
`dibujar()` con las mismas opciones (`escala`, `ancho`, `alto`, `centrado`, `espejo`, `voltear`, y
`cuadro` y `fila` con una hoja; con un animador, el cuadro que toca ahora). Se calcula al dividir la
hoja, así que no cuesta nada pedirla en cada fotograma. `margen` la encoge por cada lado, en píxeles
de pantalla (o la agranda si es negativo), para que un choque de refilón no cuente. El ángulo no se
tiene en cuenta. Con `"trozo": [x, y, ancho, alto]`, en vez de lo que se ve coloca ese rectángulo
de la celda (medido con el personaje mirando a la derecha): es lo que hace `golpe()`.

```lince
fijo OPCIONES = {"escala": 2, "centrado": verdadero}
juego.dibujar(pj, x, y, OPCIONES)
si juego.colision(juego.caja(pj, x, y, OPCIONES), juego.caja(PINCHO, px, py)) { vidas = vidas - 1 }
```

Para moverse por un mapa de casillas suele ir mejor un rectángulo fijo (el ancho del cuerpo y la
altura de pie): si la caja cambiara con cada cuadro, al andar el personaje se engancharía en las
paredes. `caja()` es para los golpes, los pinchos, las monedas y los enemigos.

**Peleas.** En un juego de lucha o un beat'em up no basta con saber dónde está el cuerpo: hay que
saber con qué golpea cada cuadro (el puño estirado, la espada, el pie) y en cuáles no golpea
nada. Cada animación del animador puede llevar `"golpes"`: una lista de `[cuadro, x, y, ancho,
alto]`, la caja que golpea en ese cuadro (contado en la secuencia de la animación, como
`pisadas`), medida dentro de la celda con el personaje mirando a la derecha. `golpe()` la da
colocada como la pone `dibujar()` con las mismas opciones (también girada con `espejo`), o `nulo`
si en el cuadro que toca no golpea. Con `"cancelar": n`, desde ese cuadro se puede pasar a otra
animación sin esperar a que acabe: así se encadenan los golpes de un combo. `libre()` dice si ya
se puede (también si la animación se repite o ya acabó).

```lince
sea pj = juego.animador(LUCHADOR, {
    "quieto":  {"fila": 0},
    "golpe1":  {"fila": 3, "cuadros": [0, 1, 1], "repetir": falso, "siguiente": "quieto",
                "golpes": [[1, 38, 24, 12, 12]], "cancelar": 2},
    "golpe2":  {"fila": 3, "cuadros": [2, 3, 3], "repetir": falso, "siguiente": "quieto",
                "golpes": [[1, 36, 26, 12, 12]]}
})
# en cada fotograma:
si juego.libre(pj) y juego.combo(["golpe"], 0.25) {
    si pj["actual"] == "golpe1" { juego.animacion(pj, "golpe2", verdadero) } sino { juego.animacion(pj, "golpe1", verdadero) }
    ataque = ataque + 1                                # cada ataque golpea una vez a cada enemigo
}
sea g = juego.golpe(pj, x, y, {"espejo": mira < 0})
si g != nulo {
    para cada diccionario e en enemigos {
        si e["golpeado_por"] != ataque y juego.colision(g, cuerpo(e)) {
            e["golpeado_por"] = ataque
            e["vida"] = e["vida"] - 1
            e["destello"] = juego.tiempo()           # y al dibujarlo: {"blanco": ...}
            juego.congelar(0.05)                     # el parón que hace que el golpe pese
            juego.temblor(2)
        }
    }
}
```

| Función | Qué hace |
|---|---|
| `golpe(animador, x, y[, opciones])` | la caja `[x, y, ancho, alto]` con la que golpea ahora, o `nulo`. Si en el mismo cuadro hay varias, las junta. Una animación que no se repite deja de golpear al acabar |
| `libre(animador)` | ¿se puede cambiar ya de animación? (se repite, ha acabado o ha llegado a su `cancelar`) |
| `combo(acciones[, segundos])` | ¿se han pulsado esas acciones en ese orden en los últimos `segundos` (0,5)? Puede haber otras pulsaciones en medio. Al dar verdadero, esas pulsaciones se gastan y ya no valen para otro combo, así que se pregunta antes por los largos: `["abajo", "adelante", "golpe"]` antes que `["golpe"]`. Con una sola acción sirve para no perder una pulsación que llega un poco antes de que el personaje pueda atacar: `combo(["golpe"], 0.25)` |
| `congelar(segundos)` | para el juego ese tiempo (de verdad, no del juego): `dt()` da 0, y las animaciones, las partículas, `suavizar()` y `tiempo()` se quedan quietos. Unas centésimas al acertar un golpe (0,05 uno flojo, 0,1 uno fuerte) es lo que hace que se note. Los fundidos de las escenas y el temblor siguen |
| `velocidad([v])` | lo rápido que pasa el tiempo del juego: 1 normal, 0,3 cámara lenta (el golpe final), 0 pausa. Sin nada, dice la que hay |
| `temblor(fuerza[, segundos])` | la cámara tiembla hasta `fuerza` píxeles y se calma en `segundos` (0,3). Lo que se dibuja después de `camara()` (el marcador) no tiembla |
| `mantenida(acción[, margen])` | cuántos segundos lleva pulsada (0 si no lo está). Con `margen`, si se soltó hace menos de esos segundos dice cuánto estuvo pulsada: para los golpes cargados, como «mantener atrás un segundo y luego adelante y patada» (al pulsar adelante, atrás ya se ha soltado): `mantenida("atras", 0.3) >= 1 y combo(["adelante", "patada"], 0.3)`. Se puede volver a definir la acción sin perder la carga |
| `pulsaciones(acción[, segundos])` | cuántas veces se ha pulsado en los últimos `segundos` (0,5), también las que ya ha gastado `combo()`: para los golpes de machacar un botón (`pulsaciones("puño", 0.6) >= 4`) o para soltarse de un agarre |

`combo()` mira las acciones de `definir_accion()`. «Adelante» depende de hacia dónde mire el
personaje: `combo(["abajo", "derecha", "golpe"])` si mira a la derecha. Los combos no pierden
pulsaciones durante `congelar()`: se miden en tiempo de verdad.

`combo`, `mantenida` y `pulsaciones` están escritas en Lince, encima de tres funciones de C que
también se pueden usar para hacer otras parecidas:

| Función | Qué hace |
|---|---|
| `historial(segundos[, acciones])` | las pulsaciones de los últimos `segundos` (solo las de esas acciones, si se dicen), de la más antigua a la última. Cada una es `{"accion", "hace", "id", "gastada"}`: `hace` son los segundos que han pasado, en tiempo de verdad. Da `nulo` si alguna de las acciones no está definida |
| `gastar(ids)` | marca esas pulsaciones (por su `id`) como gastadas, como hace `combo()` |
| `estado_accion(acción)` | `{"pulsada", "mantenida", "soltada_hace", "duro"}`: si está pulsada, cuántos segundos lleva, hace cuánto se soltó (-1 si nunca) y cuánto duró pulsada la última vez. `nulo` si no está definida |

La inteligencia de los enemigos se escribe en Lince, normalmente como una máquina de estados: un
`"estado"` en el diccionario de cada uno (acercarse, atacar, esperar, recibir, por los aires, en el
suelo...) y lo que hace en cada uno, con `tiempo()` para saber cuánto lleva. Un truco de los
beat'em up: que solo ataque uno a la vez y los demás ronden a cierta distancia, y que cada ataque
empiece con un amago de unas décimas para que se pueda esquivar.

Se pueden leer `pj["actual"]` (el nombre de la animación) y `pj["cuadro"]` (el cuadro de la hoja
que se dibujó). Ejemplo: `10_paseo`, con las tres animaciones generadas por `animar()`.

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
disco con el juego en marcha, sus animaciones se rehacen solas. En las de andar y correr,
`pisadas()` y el animador saben exactamente en qué cuadros apoya el pie (los hizo Lince).
Ejemplo: `10_paseo`, que usa
las dos maneras (el personaje, generado; los pájaros, con una hoja dibujada a mano que se divide sola).

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

**Mapas de Tiled.** Los niveles también se pueden dibujar con [Tiled](https://www.mapeditor.org), el
editor de mapas gratuito más usado, y cargarlos tal cual:

```lince
fijo NIVEL = juego.mapa_tiled("niveles/uno.tmx")
para cada diccionario cosa en NIVEL["objetos"] {          # los objetos que pusiste en Tiled
    si cosa["tipo"] == "jugador" {
        yo["x"] = cosa["x"]
        yo["y"] = cosa["y"]
    }
}
# en cada fotograma:
juego.mover_en_mapa(NIVEL, yo)                            # choca con lo que es sólido
juego.dibujar_tiled(NIVEL, ["Fondo", "Suelo"])            # lo de detrás del personaje
juego.dibujar(personaje, yo["x"], yo["y"])
juego.dibujar_tiled(NIVEL, "Delante")                     # lo de delante
```

| Función | Qué hace |
|---|---|
| `mapa_tiled(ruta[, opciones])` | lee un mapa de Tiled: `.tmx` (lo que Tiled guarda por defecto) o `.tmj` (JSON), con sus tilesets dentro o aparte (`.tsx`, `.tsj`) y las capas en CSV o en base64 (sin comprimir, con zlib o con gzip). Devuelve un mapa como los de `mapa()`, así que `mover_en_mapa`, `casilla`, `solido`, `buscar_en_mapa` y `camino` funcionan igual, con `"#"` en las casillas sólidas, `"="` en las plataformas y `"."` en las libres. Lleva además `"capas"` (los nombres de las capas que se dibujan), `"objetos"` (cada uno con `nombre`, `tipo`, `x`, `y`, `ancho`, `alto`, `rotacion`, `forma`: `"rectangulo"`, `"punto"`, `"elipse"`, `"poligono"`, `"linea"` o `"tile"`, `puntos` en los polígonos y las líneas, `capa` y `propiedades`) y `"propiedades"` (las del mapa). Opciones: `"solidos"` y `"plataformas"` (nombres de capas) y `"bordes"` (como en `mapa()`) |
| `dibujar_tiled(mapa[, capas])` | dibuja las capas visibles, o las nombradas (un nombre o una lista), con la cámara, la opacidad, el desplazamiento y el paralaje de cada capa, los tiles volteados y girados y los tiles animados. Las capas de imagen también se dibujan |

Qué es sólido se dice en Tiled de tres maneras: una capa que se llama **Colisiones** (o Sólidos o
Paredes), una capa o un tile del tileset con la propiedad `solido` (lógica, verdadera), o la
opción `"solidos"`. Con la propiedad o la opción `plataforma`, se atraviesan desde abajo y se
pisan desde arriba. Una capa de colisiones se puede dejar oculta en Tiled: sirve para chocar y
no se dibuja. Lince entiende los mapas ortogonales (rejilla normal) con casillas cuadradas y
tilesets de una sola imagen (también con margen, separación y un color transparente). Los mapas
y lo que usan (tilesets e imágenes) van dentro del juego al hacer `lince compilar` o `lince
repartir`, como las imágenes.

**Física sencilla.** Cosas que caen, rebotan, se empujan y chocan entre ellas y con un mapa, sin
girar: cajas, pelotas, plataformas que se mueven y el propio personaje. Está escrita en Lince.

```lince
sea mundo = juego.fisica({"gravedad": 900, "mapa": NIVEL})
sea yo = juego.cuerpo(mundo, {"x": 40, "y": 300, "ancho": 22, "alto": 30, "friccion": 0})
sea caja = juego.cuerpo(mundo, {"x": 200, "y": 300, "ancho": 30, "alto": 30, "masa": 2})
sea pelota = juego.cuerpo(mundo, {"x": 300, "y": 50, "radio": 10, "rebote": 0.8})
juego.cuerpo(mundo, {"x": 400, "y": 200, "radio": 8, "sensor": verdadero, "gravedad": 0, "tipo": "moneda"})
# en cada fotograma:
yo["vx"] = juego.direccion("izquierda", "derecha") * 200
si juego.accion_pulsada("saltar") y yo["en_suelo"] { yo["vy"] = -500 }
juego.avanzar_fisica(mundo)
para cada diccionario t en juego.tocando(mundo, yo) {
    si t["tipo"] == "moneda" { juego.quitar_cuerpo(mundo, t) }
}
```

| Función | Qué hace |
|---|---|
| `fisica([opciones])` | un mundo: `gravedad` (900 hacia abajo, o `[x, y]`), `mapa` (un mapa de `mapa()` o `mapa_tiled()` con el que chocar) y `pasos` (120: cuántas veces por segundo se calcula, para que nada atraviese nada) |
| `cuerpo(mundo, datos)` | añade un cuerpo y lo devuelve: el mismo diccionario, con lo que falte puesto. `x`, `y` (la esquina), `ancho` y `alto`, o `radio` (un círculo); `vx`, `vy`; `masa` (1: el más pesado empuja más y lo empujan menos); `rebote` (0 a 1); `friccion` (0,5: cómo frena en el suelo); `gravedad` (1; 0 para que flote); `fijo` (no lo mueven ni la gravedad ni los choques: suelos, muros y, con `vx` o `vy`, plataformas y ascensores, que llevan lo que tengan encima); `sensor` (no choca, solo cuenta lo que toca: monedas, zonas). Puede llevar más datos tuyos (`"tipo": "caja"`) |
| `avanzar_fisica(mundo[, dt])` | mueve todo lo de un fotograma: gravedad, mapa y choques. Deja en cada cuerpo `en_suelo` (apoyado en el mapa o en otro cuerpo), y `techo` y `pared` si hay mapa |
| `tocando(mundo, cuerpo)` · `empujar(cuerpo, fx, fy)` · `quitar_cuerpo(mundo, cuerpo)` | los cuerpos que tocaba en el último `avanzar_fisica` · un golpe que cambia su velocidad según su masa · lo quita del mundo |

Los cuerpos son diccionarios normales: para moverlos se cambian `vx` y `vy` (o `x` e `y`), y se
dibujan como cada uno quiera. `examples/13_fisica.lince` lo usa todo.

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
(`[0.4, 0.8]`), `tamaño` (4) y `tamaño_final`, `gravedad` (0) y `mezcla` (`"sumar"`: brillan, como
las chispas de un golpe o la estela de una bola de energía; `"multiplicar"`: oscurecen lo de debajo,
como el humo, y al apagarse se aclaran en vez de volverse transparentes). Una opción mal escrita da error.

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

**Menús y diálogos.** Se manejan igual con teclado (flechas o WASD, entrar o espacio, escape), con
mando (cruceta o palanca, A, B) y con el ratón o el dedo (señalar y tocar), y se dibujan sin
moverse con la cámara. Están escritos en Lince.

```lince
# en cada fotograma de la portada:
sea eleccion = juego.menu(["Jugar", "Opciones", "Salir"])
si eleccion == "Jugar" { juego.cambiar_escena("partida", 0.6) }

# al hablar con alguien:
juego.dialogo([
    {"nombre": "Lince", "texto": "Llevo días viendo luces raras entre los árboles."},
    {"nombre": "Lince", "texto": "¿Me acompañas?", "respuestas": ["Sí", "No"]}
])
# y en la partida:
si no juego.dialogo_abierto() { ...mover al personaje... }
si juego.respuesta() == "Sí" { ... }
```

| Función | Qué hace |
|---|---|
| `menu(textos[, opciones])` | dibuja los textos para elegir, uno debajo de otro, y devuelve el elegido en el fotograma en que se elige; si no, `nulo`. Se llama en cada fotograma mientras se vea, y vuelve a la primera opción si deja de verse un rato. Opciones: `x`, `y` (el centro de la primera opción; por defecto, el menú centrado en la pantalla), `escala` (3), `fuente`, `color`, `color_elegido`, `separacion`, `atras` (lo que devuelve con escape o B, como `"Continuar"` en una pausa), `sonido_mover` y `sonido_elegir` |
| `dialogo(paginas[, opciones])` | abre una caja de diálogo abajo de la pantalla con el texto saliendo letra a letra. `paginas` es una página o una lista; una página es un texto o un diccionario con `texto`, `nombre` (quién habla, en una pestaña encima), `retrato` (una imagen a la izquierda, o `[hoja, cuadro]`: un cuadro de una hoja) y `respuestas` (textos para elegir). Aceptar (entrar, espacio, A o tocar) termina de escribir la página o pasa a la siguiente; una página que no cabe se parte sola. Opciones: `escala` (2), `fuente`, `color`, `fondo`, `borde`, `velocidad` (letras por segundo, 40), `lineas` (3), `arriba` (verdadero: la caja arriba) y `sonido` (suena con las letras) |
| `dialogo_abierto()` · `respuesta()` · `cerrar_dialogo()` | si hay un diálogo abierto (también en el fotograma en que se cierra, para que la tecla que lo cierra no lo vuelva a abrir) · la última respuesta elegida, o `nulo` · lo cierra |

El juego sigue viendo las teclas mientras hay un menú o un diálogo: lo normal es no mover al
personaje mientras `dialogo_abierto()`, como en el ejemplo. `examples/12_aventura.lince` lo usa todo.

**Controles a gusto.** El jugador puede cambiar las teclas y los botones de cada acción:

```lince
# en una pantalla de opciones, en cada fotograma:
si juego.menu_controles() == "volver" { ...salir de las opciones... }
```

| Función | Qué hace |
|---|---|
| `menu_controles([opciones])` | enseña cada acción con sus teclas y sus botones; se elige una y se pulsa lo nuevo (Esc deja como estaba). Una tecla nueva sustituye a las teclas que tenía la acción, un botón de mando a sus botones y una dirección de la palanca a sus direcciones; si otra acción ya la usaba, se intercambian. Lo cambiado **se guarda solo** y se aplica solo la próxima vez que el juego defina esa acción con `definir_accion`, sin hacer nada más. Tiene «Restablecer» y «Volver»; devuelve `"volver"` al salir (también con Esc o B). Opciones: `acciones` (cuáles y en qué orden), `nombres` (`{acción: cómo se enseña}`), `escala` (2; baja sola si no cabe), `fuente`, `color` y `color_elegido` |
| `restablecer_controles()` | vuelven los controles que definió el juego |
| `entrada_pulsada()` | lo que se acaba de pulsar (tecla, botón del ratón, botón o dirección de la palanca de un mando), escrito como en `definir_accion` (`"espacio"`, `"mando:a"`, `"mando:izq_x-"`), o `nulo`: para hacer un menú de controles propio |

**Partidas guardadas.** Récords, opciones y avances que siguen ahí al volver a abrir el juego:

```lince
sea opciones = juego.cargar("opciones", {"musica": 0.8, "idioma": "es"})
sea partida = juego.cargar("partida")           # nulo si no hay nada guardado
juego.guardar("partida", {"nivel": 3, "vidas": 2, "llaves": ["roja"]})
juego.olvidar("partida")                         # empezar de cero
```

| Función | Qué hace |
|---|---|
| `guardar(nombre, datos)` | guarda `datos` (números, textos, lógicos, nulo, listas y diccionarios) con ese nombre; lo de antes con el mismo nombre se cambia. Si el juego se cierra a medias, lo guardado antes sigue entero |
| `cargar(nombre[, por_defecto])` | lo guardado con ese nombre, o `por_defecto` (`nulo` si no se da) si no hay nada. Si los dos son diccionarios, lo que falte en lo guardado se toma de `por_defecto`: así una opción nueva de una versión posterior del juego tiene valor desde el principio |
| `olvidar(nombre)` | borra lo guardado; verdadero si había algo |
| `carpeta_guardado()` | la carpeta donde se guarda |

Cada nombre es un archivo JSON en una carpeta del usuario para cada juego, que se llama como el programa
(`serpiente.lince` → `serpiente`): `~/.local/share/lince/serpiente/` en Linux,
`%APPDATA%\Lince\serpiente\` en Windows y la carpeta privada de la app en Android. No hay que
pensar en rutas, y un juego hecho con `lince compilar` o `lince repartir` guarda en el mismo sitio.
Los nombres no llevan `/ \ : * ? " < > |` ni empiezan por punto. `LINCE_JUEGO_GUARDADO` cambia la
carpeta (para las pruebas).

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
`LINCE_JUEGO_CLIC=x,y` pulsan en los fotogramas impares; `LINCE_JUEGO_GUION=30:derecha:20,50:z`
mantiene derecha desde el fotograma 30 durante 20 y pulsa z en el 50 (un guion de teclas, para
probar combos); `LINCE_JUEGO_ESCRIBIR=texto` teclea
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
- El ejecutable es para el sistema en el que se crea: para Windows, se reparte desde
  Windows (o con el `lince.exe` de `make windows` bajo Wine).

### Android

`lince repartir juego.lince --android [-o juego.apk] [--nombre "Mi juego"] [--id org.lince.mijuego] [--version 1]`
hace un `.apk` para instalar el juego en un móvil Android (64 bits, Android 7.0 o más nuevo) sin
instalar nada más: parte de la plantilla `lince-android.apk` (junto al ejecutable, en `~/.lince/`
o en `LINCE_ANDROID_PLANTILLA`), le mete el juego y lo firma con una llave que se crea la primera
vez en `~/.lince/android-llave.txt` (guárdala: las actualizaciones de un juego tienen que ir
firmadas con la misma). Ver [`android/LEEME.md`](android/LEEME.md); allí está también
`android/construir.sh`, que hace el `.apk` o la plantilla con Java, el SDK y el NDK de Android. En el móvil el juego ocupa la pantalla
(de lado si es más ancho que alto), se controla con `toques()` y con botones `"pantalla:..."`
en las acciones, el botón «atrás» lo cierra y lo que escribe va a su carpeta privada.
`examples/11_movil.lince` sirve para probarlo.

`lince repartir juego.lince --aab [--nombre ...] [--id ...] [--version N]` hace en su lugar el `.aab`
que pide Google Play, firmado con la misma llave (que hace de llave de subida). Cada `.aab` que se
sube lleva un `--version` más alto que el anterior. El juego apunta a Android 16 (API 36) y sus
bibliotecas sirven con páginas de memoria de 16 KB, como pide Google Play.

`lince compilar juego.lince --datos -o juego.paq` empaqueta solo el programa y sus archivos,
sin el intérprete: es lo que va dentro del `.apk`.

### Rendimiento

El ejecutable va a la velocidad del intérprete. Mediciones con los programas de
`bench/` (`make bench`), en segundos: `fib(30)` 0,28 · bucle de 5 millones de
iteraciones 0,35 · diccionario de 50.000 claves 0,03 · 576.000 píxeles con
cuentas 0,3. Una vuelta de bucle cuesta unos 30 ns; una llamada a función,
unos 70.

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
│   ├── corrutina.c/h   # una pila para cada tarea (ucontext / fibras de Windows)
│   ├── modulos.c/h     # módulos estándar
│   ├── empaquetado.c/h # lince compilar: ejecutables autónomos
│   ├── repartir.c/h    # lince repartir: carpeta y .zip con el juego y SDL2
│   ├── recarga.c/h     # recarga en caliente de los juegos
│   ├── imagen.c/h      # PNG, JPG... (stb_image)
│   ├── ogg.c/h         # Ogg Vorbis (stb_vorbis)
│   ├── letra.c/h       # fuentes .ttf y .otf (stb_truetype)
│   ├── juego.h         # módulo juego (SDL2 cargada al ejecutar)
│   ├── terceros/       # stb_image, stb_vorbis y stb_truetype, tal cual
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
