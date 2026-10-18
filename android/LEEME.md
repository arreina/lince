# 🐆 Lince en Android

Un juego de Lince se convierte en un `.apk` que se instala en un móvil Android
(de 64 bits, Android 7.0 o más nuevo):

```bash
lince repartir mi_juego.lince --android                         # mi_juego.apk
lince repartir mi_juego.lince --android -o juego.apk --nombre "Mi juego" --id org.lince.mijuego
```

No hace falta instalar nada más: Lince parte de una **plantilla**
(`lince-android.apk`, un `.apk` de Lince sin juego), le mete el juego, le pone
el nombre del paquete y de la app y lo firma él mismo (`src/apk.c` y
`src/firmar.c`: SHA-256, RSA y el esquema de firma v2 de Android escritos en
C). La plantilla se busca junto al ejecutable de Lince, en `~/.lince/` o donde
diga `LINCE_ANDROID_PLANTILLA`; viene con cada versión de Lince (la hace la CI)
o se hace con `android/construir.sh --plantilla`.

La primera vez se crea una llave en `~/.lince/android-llave.txt` y con ella se
firman todos tus juegos. **Guárdala**: para instalar una versión nueva de un
juego encima de la anterior, tiene que estar firmada con la misma llave.

## Hacer el .apk con el SDK

`android/construir.sh` hace lo mismo con las herramientas de Android (y es lo
que hace la plantilla):

```bash
android/construir.sh examples/11_movil.lince -o movil.apk --nombre "Lince en el móvil"
android/construir.sh --plantilla -o lince-android.apk
```

El `.apk` lleva:

- **libmain.so**: Lince entero, compilado con el NDK. En Android, en vez de
  `main()` se llama a `SDL_main()` (al final de `src/main.c`), que prepara el
  arranque (`src/android.c`) y ejecuta el juego.
- **libSDL2.so** y la parte en Java de SDL2 (`SDLActivity`), compiladas desde
  las fuentes de SDL2.
- **assets/juego.paq**: el juego empaquetado, lo mismo que `lince compilar`
  pega a un ejecutable pero sin el intérprete (`lince compilar juego.lince
  --datos`): el programa, lo que importa y las imágenes, sonidos y fuentes que
  nombra.
- Un icono de lince hecho con `android/icono.lince`.

## Lo que hace falta (fuera del repositorio)

En `~/lince-android`, o donde diga `LINCE_ANDROID`:

| Carpeta | Qué es |
|---|---|
| `jdk/` | Java 17 (por ejemplo Temurin) |
| `sdk/` | el SDK de Android con `platforms;android-34`, `build-tools;34.0.0` y `ndk;26.3.11579264` |
| `SDL2/` | las fuentes de SDL2 (2.30.9), tal cual (o `SDL_RUTA`) |

`sdkmanager --sdk_root=... "platforms;android-34" "build-tools;34.0.0" "ndk;26.3.11579264"`
las instala (hay que aceptar la licencia del SDK). La primera vez se compilan
Lince y SDL2 para el móvil y la parte en Java; luego se reutilizan (en
`obra/`). El `.apk` se firma con una llave de pruebas que se crea ahí mismo
(`lince-pruebas.keystore`): vale para instalarlo, no para publicarlo.

## Instalarlo

Copia el `.apk` al móvil y ábrelo (hay que permitir instalar apps de
«orígenes desconocidos»), o con el móvil conectado por USB y la depuración
activada: `adb install movil.apk`. Lo que escribe el juego (`escribir`, los
errores) se ve con `adb logcat -s lince`.

## En el móvil

- **Pantalla**: el juego ocupa toda la pantalla, escalado sin deformarse (con
  bandas si hace falta). Si es más ancho que alto, el móvil se pone de lado.
- **Mando en la pantalla**: si el juego usa acciones con `"mando:..."`, sale solo
  un mando como el de los juegos de móvil: una palanca que aparece donde se
  apoya el pulgar izquierdo y, a la derecha, los botones que usan las acciones
  (A, B, X, Y, e inicio arriba). Un juego hecho para teclado y mando se juega
  en el móvil sin cambiar nada. Se ajusta o se quita con `juego.mando_tactil()`.
- **Mandos Bluetooth**: funcionan como en el ordenador (los lee SDL2), también
  si se conectan con el juego en marcha. Mientras haya uno conectado, el mando
  de la pantalla se esconde.
- **Táctil**: `juego.toques()` da los dedos que hay en la pantalla (un toque
  rápido cuenta al menos un fotograma), y una acción puede tener botones propios
  en la pantalla: `"pantalla:x,y,ancho,alto"`. El primer dedo también mueve el
  ratón (`juego.raton()`, `juego.clic()`). `juego.tactil()` es verdadero.
- **Vibrar e inclinar**: `juego.vibrar(segundos)` hace vibrar el móvil (y los
  mandos), y `juego.inclinacion()` dice hacia dónde está inclinado; una acción
  puede usarlo con `"inclinacion:x-"`, `"inclinacion:x+"`...
- **Atrás**: el botón «atrás» del móvil cierra el juego (`juego.abierto()` pasa
  a falso).
- **Archivos**: lo que el juego escribe (récords, partidas) va a su carpeta
  privada del móvil.
- **Errores**: si el juego se para por un error, el error se queda en la
  pantalla hasta que se pulse «atrás».
- **Pausa**: al salir de la app el juego se queda parado, y sigue al volver.
- **Tareas**: el cambio de pila de las tareas está escrito a mano para arm64 y
  x86_64 (`src/corrutina.c`): Android no trae `swapcontext`.
