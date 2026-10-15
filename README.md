<div align="center">

# 🐆 Lince

**Un lenguaje para hacer videojuegos, en español**

*Perspicaz · Preciso · Para todos*

---

[![Licencia MIT](https://img.shields.io/badge/licencia-MIT-6A2D72.svg)](LICENSE)
[![Versión](https://img.shields.io/badge/versión-0.5-F1BF00.svg)]()
[![Tests](https://img.shields.io/badge/tests-31%2F31-C60B1E.svg)]()
[![Plataformas](https://img.shields.io/badge/plataformas-Linux%20·%20Mac%20·%20Windows-6A2D72.svg)]()

[🌐 lince-lang.github.io](https://arreina.github.io/lince)

</div>

---

Lince es un lenguaje de programación **para videojuegos**, **escrito en C puro** y **sin dependencias para compilarlo**, diseñado para que crear juegos en español sea natural, claro y accesible. La ventana, el dibujo, el teclado y el sonido vienen incluidos (SDL2, que se carga al abrir la ventana).

```lince
importar "juego"

juego.abrir("Mi primer juego", 800, 600)
sea x = 100

mientras juego.abierto() {
    si juego.tecla("derecha") { x = x + 300 * juego.dt() }
    si juego.tecla("izquierda") { x = x - 300 * juego.dt() }

    juego.limpiar([20, 20, 30])
    juego.circulo(x, 300, 20, "#ffc850")
    juego.texto("Flechas para mover", 10, 10, 3)
    juego.presentar()          # muestra, espera a 60 fps y lee el teclado
}
```

Un juego completo, un Pong para dos jugadores, está en [`examples/juegos/pong.lince`](examples/juegos/pong.lince).

## ¿Por qué Lince?

- **En español** — la programación no debería requerir saber inglés
- **Hecho para juegos** — ventana, dibujo 2D, texto, teclado, ratón, sonido y colisiones en un solo módulo (`juego`)
- **Sin dependencias para compilar** — un solo binario, compilado con `gcc` y `make` (SDL2 se carga al abrir la ventana)
- **Errores claros** — mensajes literales con número de línea exacto
- **Ejecutables autónomos** — `lince compilar` empaqueta tu programa en un único ejecutable, sin instalar nada en destino

## Instalación

### Linux y Mac

```bash
git clone https://github.com/arreina/lince.git
cd lince
make
sudo make install   # opcional
```

### Windows (MSYS2/MinGW)

```bash
pacman -S mingw-w64-x86_64-gcc make
git clone https://github.com/arreina/lince.git
cd lince && make
```

## Uso

```bash
lince programa.lince              # ejecutar
lince compilar programa.lince     # crear un ejecutable autónomo
lince depurar programa.lince      # depurar paso a paso
lince comprobar programa.lince    # sólo comprobar la sintaxis
lince lsp                         # servidor de lenguaje para tu editor
lince                             # REPL interactivo
lince instalar https://github.com/usuario/paquete
lince ayuda
```

## El lenguaje

### Variables y tipos

```lince
sea nombre = "Ana"
fijo PI = 3.14159
sea lista notas = [9, 7, 10]
sea diccionario config = {"debug": verdadero, "version": 2}
```

### Funciones y closures

```lince
funcion factorial(val numero n): numero {
    si n <= 1 { devolver 1 }
    devolver n * factorial(n - 1)
}

# Valores por defecto
funcion saludar(val texto nombre, val texto saludo = "Hola"): texto {
    devolver saludo + ", " + nombre
}

# Closures
funcion multiplicador(val numero factor): funcion {
    devolver funcion(val numero x): numero { devolver x * factor }
}
sea doble = multiplicador(2)
escribir(doble(5))   # 10
```

### Clases, herencia e interfaces

```lince
interfaz Forma {
    funcion area(): numero
}

clase Rectangulo implementa Forma {
    funcion crear(val numero ancho, val numero alto): nulo {
        mi.ancho = ancho
        mi.alto  = alto
    }
    funcion area(): numero { devolver mi.ancho * mi.alto }
}

clase Cuadrado extiende Rectangulo {
    funcion crear(val numero lado): nulo {
        padre(lado, lado)
    }
}
```

### Generadores

```lince
generador pares(val numero hasta): numero {
    para cada numero n en rango(hasta) {
        si n % 2 == 0 { producir n }
    }
}

para cada numero n en pares(10) {
    escribir(n)   # 0, 2, 4, 6, 8
}
```

### Enumeraciones

```lince
enumeracion Color { Rojo Verde Azul }

sea c = Color.Rojo
elegir c {
    caso Color.Rojo  { escribir("rojo") }
    caso Color.Verde { escribir("verde") }
    otro             { escribir("otro") }
}
```

### Manejo de errores

```lince
intentar {
    sea datos = json.parsear(archivos.leer("partida.json"))
    escribir(datos["version"])
} capturar (ErrorRango e) {
    escribir("Clave no encontrada: " + e.mensaje)
} capturar (error e) {
    escribir("Error: " + e.mensaje)
} finalmente {
    escribir("Listo")
}
```

### Módulos

```lince
importar "matematica"
importar "archivos"
importar "json"

escribir(matematica.raiz(144))                          # 12
archivos.escribir("partida.json", json.texto({"nivel": 3}))
```

**Módulos disponibles:** `juego` (ventana, dibujo, entrada, sonido) · `matematica` · `texto` · `archivos` · `tiempo` · `sistema` · `json` (partidas guardadas) · `pruebas`

## Ejecutables autónomos

```bash
lince compilar programa.lince
./programa
```

`lince compilar` crea un único ejecutable con tu programa y los archivos que importa dentro (con `--incluir` también imágenes, niveles o sonidos): se puede copiar a otra máquina sin instalar Lince; solo hace falta SDL2 si el juego abre una ventana. Soporta el 100 % del lenguaje y tarda milisegundos, porque el ejecutable lleva el propio intérprete.

**Rendimiento** (`make bench`, en segundos): `fib(30)` 0,3 · bucle de 5 millones de iteraciones 0,5 · diccionario de 50.000 claves 0,04.

## Tests

```bash
make test                              # tests del intérprete
./lince tests/runner_compilador.lince  # tests de los ejecutables
```

```
✅ 31/31 tests del intérprete pasaron
✅ 15/15 tests del compilador pasaron
```

## Soporte para editores

| Editor | Resaltado | Snippets |
|--------|-----------|----------|
| VSCode | ✅ | ✅ |
| Sublime Text | ✅ | ✅ |
| Notepad++ | ✅ | — |

Instrucciones en [`editores/README.md`](editores/README.md).

## Estructura del proyecto

```
lince/
├── src/                    # código fuente en C
│   ├── main.c              # CLI y subcomandos
│   ├── lexer.c/h           # análisis léxico
│   ├── parser.c/h          # análisis sintáctico
│   ├── interprete.c/h      # evaluación
│   ├── empaquetado.c/h     # lince compilar: crea ejecutables autónomos
│   ├── modulos.c/h         # módulos estándar
│   ├── repl.c/h            # REPL con historial
│   └── paquetes.c/h        # gestor de paquetes
├── tests/
│   ├── runner.lince               # tests del intérprete
│   ├── runner_compilador.lince    # tests de los ejecutables
│   ├── lince/                     # tests del intérprete
│   └── compilador/                # tests del compilador
├── editores/               # soporte para editores
├── ESPECIFICACION.md       # especificación completa
└── Makefile
```

## Licencia

MIT — libre para usar, modificar y distribuir.

---

<div align="center">

*🐆 Lince — ágil, preciso, para hacer juegos*

</div>
