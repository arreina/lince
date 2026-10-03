# 🐆 Lince — Especificación del Lenguaje v0.6

## Filosofía

Lince es un lenguaje de programación de **propósito general en español**, implementado en C puro sin dependencias externas. Tres pilares:

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
| Número | `numero` | `3`, `3.14` |
| Texto | `texto` | `"hola"` |
| Lógico | `logico` | `verdadero`, `falso` |
| Lista | `lista` | `[1, 2, 3]` |
| Diccionario | `diccionario` | `{"clave": valor}` |
| Función | `funcion` | lambdas, funciones como args |
| Sin valor | `nulo` | `nulo` |

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
s.recortar()
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
importar "red"
importar "expresiones"
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
tiempo.ahora()     # timestamp Unix
tiempo.fecha()     # "2025-01-15"
tiempo.hora()      # "14:30:00"
tiempo.esperar(500) # pausa 500ms
```

### `sistema`

```lince
sistema.ejecutar("ls -la")
sistema.existe("archivo.txt")
sistema.salir(0)
```

### `json`

```lince
sea datos = json.parsear("{\"nombre\": \"Ana\"}")
escribir(datos["nombre"])
escribir(json.texto({"clave": "valor"}))
```

### `red`

```lince
sea r = red.obtener("https://api.com/datos")
sea r = red.enviar("https://api.com/crear", "{\"nombre\": \"Ana\"}")
sea r = red.actualizar("https://api.com/usuario/1", cuerpo)
sea r = red.eliminar("https://api.com/usuario/1")

# Con cabeceras
sea r = red.obtener("https://api.com", {"Authorization": "Bearer token"})

# La respuesta es un diccionario
r["codigo"]   # 200, 404...
r["cuerpo"]   # texto de la respuesta
r["ok"]       # verdadero si codigo < 400
```

Requiere `curl` instalado. En Windows usa el subsistema de curl del sistema.

### `expresiones`

```lince
expresiones.coincidir("hola123", "[0-9]+")         # verdadero
expresiones.buscar("precio: 42 euros", "[0-9]+")   # "42"
sea nums = expresiones.todos("a1 b2 c3", "[0-9]+") # ["1","2","3"]
expresiones.reemplazar("hola mundo", "mundo", "Lince")  # "hola Lince"
```

En Linux/Mac usa regex POSIX completo. En Windows usa búsqueda de subcadena.

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

## Compilador a binario nativo

```bash
lince compilar programa.lince           # genera ./programa
lince compilar programa.lince -o nombre # nombre personalizado
```

Transpila a C y compila con `gcc -O2`. Requiere `gcc` instalado.

### Soporte del compilador

| Característica | Compilador |
|----------------|-----------|
| Variables, constantes, tipos | ✅ |
| Funciones, recursión, defaults | ✅ |
| Lambdas y closures | ✅ |
| Listas y diccionarios | ✅ |
| Para, mientras, hacer, para cada | ✅ |
| Si/sino, elegir/caso | ✅ |
| Clases, herencia, padre() | ✅ |
| Interfaces | ✅ |
| Enumeraciones | ✅ |
| intentar/capturar/finalmente | ✅ |
| Generadores y rango() | ✅ |
| Todos los módulos estándar | ✅ |

### Rendimiento

```
Fibonacci(30) — intérprete:       ~4000ms
Fibonacci(30) — binario nativo:    ~600ms
Mejora: ~6.7x
```

---

## REPL interactivo

```bash
lince          # abre el REPL
lince prog.lince               # ejecuta archivo
lince prog.lince arg1 arg2     # con argumentos
lince compilar prog.lince      # compila a binario
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
make test                              # 22 tests del intérprete
./lince tests/runner_compilador.lince  # 8 tests del compilador
```

**Tests del intérprete (22):** `test_variables`, `test_operadores`, `test_condicionales`, `test_bucles`, `test_funciones`, `test_listas`, `test_diccionarios`, `test_texto`, `test_clases`, `test_errores`, `test_modulos`, `test_json`, `test_sistema`, `test_enumeraciones`, `test_interfaces`, `test_closures`, `test_recursion`, `test_casos_limite`, `test_generadores`, `test_matematica`, `test_archivos`, `test_expresiones`

**Tests del compilador (8):** `test_basico`, `test_listas_dics`, `test_funciones`, `test_clases`, `test_closures`, `test_generadores`, `test_errores`, `test_modulos`

---

## Compatibilidad

| Plataforma | Estado | Compilar con |
|------------|--------|--------------|
| Linux | ✅ Nativo | `make` |
| Mac | ✅ Nativo | `make` |
| Windows (MSYS2/MinGW) | ✅ | `make` |
| Windows (WSL) | ✅ | `make` |

---

## Cómo compilar

```bash
make              # compilar
make test         # 22 tests del intérprete
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
│   ├── compilador.c/h  # compilador a binario nativo
│   ├── repl.c/h        # REPL interactivo con historial
│   ├── paquetes.c/h    # gestor de paquetes
│   └── plataforma.h    # compatibilidad multiplataforma
├── tests/
│   ├── runner.lince           # runner de tests del intérprete
│   ├── runner_compilador.lince # runner de tests del compilador
│   ├── lince/                 # 22 tests del intérprete
│   └── compilador/            # 8 tests del compilador
├── editores/           # soporte VSCode, Sublime, Notepad++
├── ejemplos/           # programas de ejemplo
├── ESPECIFICACION.md
├── README.md
├── LICENSE
└── Makefile
```

---

*🐆 Lince v0.5 — ágil, preciso, para todos.*
