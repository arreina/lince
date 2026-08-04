# 🐆 Ejercicios de ejemplo

Ejercicios pequeños y clásicos resueltos en Lince, pensados para ver el
lenguaje en casos concretos. Cada archivo es independiente, no pide nada
por teclado y se ejecuta así:

```sh
make                          # compilar el intérprete
./lince examples/01_fizzbuzz.lince
```

Todos de una vez:

```sh
for f in examples/*.lince; do echo "== $f"; ./lince "$f"; done
```

## Los ejercicios

| Archivo | Ejercicio | Qué enseña |
|---------|-----------|------------|
| `01_fizzbuzz.lince` | FizzBuzz del 1 al 20 | bucle `para`, `si` / `sino si` / `sino`, módulo `%` |
| `02_factorial_y_fibonacci.lince` | Factorial y Fibonacci, recursivos e iterativos | funciones, recursión, acumuladores, `matematica` |
| `03_numeros_primos.lince` | Primos y factorización | funciones que devuelven `logico`, salida temprana, listas |
| `04_palindromos.lince` | Detectar palíndromos | métodos de texto, recorrer texto, índices negativos |
| `05_listas_y_estadisticas.lince` | Mínimo, máximo, media y mediana | listas, `mapear` / `filtrar` / `reducir` |
| `06_diccionarios_contar.lince` | Contar palabras y letras | diccionarios como contador, `.contiene()`, recorrerlos |
| `07_ordenar_y_buscar.lince` | Burbuja, selección, búsqueda binaria | bucles anidados, intercambios, qué se copia y qué se comparte |
| `08_clases_figuras.lince` | Áreas de figuras geométricas | clases, `mi`, herencia con `extiende`, `padre()`, polimorfismo |
| `09_errores.lince` | División segura y validación | `intentar` / `capturar` / `finalmente`, `lanzar` |
| `10_tablas_y_figuras.lince` | Tabla de multiplicar y pirámides | bucles anidados, `texto.repetir()`, generadores propios |

Van de menos a más: los primeros sólo usan bucles y condicionales, y los
últimos ya entran en clases, errores y generadores.

## Dos cosas que conviene saber

Aparecen comentadas dentro de los ejemplos, pero se resumen aquí porque
sorprenden la primera vez:

- **El texto se maneja por bytes, no por caracteres.** Una vocal
  acentuada ocupa dos bytes en UTF-8, así que `"camión".longitud()` da 7
  y al invertir el texto la tilde se rompe. Se ve al final de
  `04_palindromos.lince`.
- **Las listas y los diccionarios se comparten.** Los números, textos y
  lógicos se copian al pasarlos a una función, pero una lista no: si la
  función cambia su contenido, el cambio se ve fuera. Por eso las
  ordenaciones de `07_ordenar_y_buscar.lince` copian la lista antes de
  tocarla.

## Y también

En la carpeta `ejemplos/` hay programas más completos (una agenda, una
tienda, una calculadora). Estos de aquí son más pequeños y cada uno se
centra en una idea.
