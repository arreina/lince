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

## Una cosa que conviene saber

Aparece comentada dentro de los ejemplos, pero se resume aquí porque
sorprende la primera vez:

- **Las listas y los diccionarios se comparten.** Los números, textos y
  lógicos se copian al pasarlos a una función, pero una lista no: si la
  función cambia su contenido, el cambio se ve fuera. Para eso está
  `copiar()`, que usa `07_ordenar_y_buscar.lince` antes de ordenar.

El texto, en cambio, se cuenta por caracteres y no por bytes, así que
`"niño".longitud()` da 4 y `"el niño".mayusculas()` da `EL NIÑO`. Una
tilde sí cuenta como letra distinta: `"á" != "a"`, cosa que se ve al
final de `04_palindromos.lince`.

## Y también

En la carpeta `ejemplos/` hay programas más completos (una agenda, una
tienda, una calculadora). Estos de aquí son más pequeños y cada uno se
centra en una idea.
