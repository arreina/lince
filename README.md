<div align="center">

# 🐆 Lince

**Un lenguaje de programación de propósito general en español**

*Perspicaz · Preciso · Para todos*

---

[![Licencia MIT](https://img.shields.io/badge/licencia-MIT-6A2D72.svg)](LICENSE)
[![Versión](https://img.shields.io/badge/versión-0.5-F1BF00.svg)]()
[![Tests](https://img.shields.io/badge/tests-30%2F30-C60B1E.svg)]()
[![Plataformas](https://img.shields.io/badge/plataformas-Linux%20·%20Mac%20·%20Windows-6A2D72.svg)]()

[🌐 lince-lang.github.io](https://arreina.github.io/lince)

</div>

---

Lince es un lenguaje de programación **escrito en C puro**, **sin dependencias externas**, diseñado para que programar en español sea natural, claro y accesible.

```lince
# El clásico hola mundo
escribir("¡Hola, Mundo!")

# Con clases y tipos
clase Persona {
    funcion crear(val texto nombre, val numero edad): nulo {
        mi.nombre = nombre
        mi.edad   = edad
    }
    funcion saludar(): texto {
        devolver "Hola, soy " + mi.nombre + " y tengo " + mi.edad + " años"
    }
}

sea lista personas = [Persona("Ana", 22), Persona("Luis", 30)]
para cada elemento p en personas {
    escribir(p.saludar())
}
```

## ¿Por qué Lince?

- **En español** — la programación no debería requerir saber inglés
- **Propósito general** — scripts, herramientas, automatización, aprendizaje
- **Sin dependencias** — un solo binario, compilado con `gcc` y `make`
- **Errores claros** — mensajes literales con número de línea exacto
- **Compilador incluido** — genera binarios nativos ~6.7x más rápidos

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
lince compilar programa.lince     # compilar a binario nativo
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
    sea datos = json.parsear(archivos.leer("config.json"))
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
importar "red"
importar "expresiones"

escribir(matematica.factorial(10))          # 3628800
archivos.escribir("log.txt", "inicio")
sea r = red.obtener("https://api.com/datos")
sea nums = expresiones.todos("a1 b2 c3", "[0-9]+")
```

**Módulos disponibles:** `matematica` · `texto` · `archivos` · `tiempo` · `sistema` · `json` · `red` · `expresiones`

## Compilador a binario nativo

```bash
lince compilar programa.lince
./programa
```

El compilador transpila a C y usa `gcc -O2`. Soporta el 100% del lenguaje incluyendo closures, generadores, clases, interfaces, enumeraciones y todos los módulos.

**Rendimiento** (Fibonacci 30):

| Modo | Tiempo |
|------|--------|
| Intérprete | ~4000ms |
| Binario nativo | ~600ms |
| **Mejora** | **~6.7x** |

## Tests

```bash
make test                              # 22 tests del intérprete
./lince tests/runner_compilador.lince  # 8 tests del compilador
```

```
✅ 22/22 tests del intérprete pasaron
✅  8/8  tests del compilador pasaron
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
│   ├── compilador.c/h      # compilador a binario nativo
│   ├── modulos.c/h         # módulos estándar
│   ├── repl.c/h            # REPL con historial
│   └── paquetes.c/h        # gestor de paquetes
├── tests/
│   ├── runner.lince               # 22 tests del intérprete
│   ├── runner_compilador.lince    # 8 tests del compilador
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

*🐆 Lince v0.5 — ágil, preciso, para todos*

</div>
