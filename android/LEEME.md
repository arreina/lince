# 🐆 Lince en Android

Un juego de Lince se convierte en un `.apk` que se instala en un móvil Android
(de 64 bits: arm64, o x86_64 en el emulador):

```bash
android/construir.sh examples/11_movil.lince -o movil.apk --nombre "Lince en el móvil"
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
- **Táctil**: `juego.toques()` da los dedos que hay en la pantalla, y una
  acción puede tener botones en la pantalla: `"pantalla:x,y,ancho,alto"`. El
  primer dedo también mueve el ratón (`juego.raton()`, `juego.clic()`).
  `juego.tactil()` es verdadero.
- **Atrás**: el botón «atrás» del móvil cierra el juego (`juego.abierto()` pasa
  a falso).
- **Archivos**: lo que el juego escribe (récords, partidas) va a su carpeta
  privada del móvil.
- **Errores**: si el juego se para por un error, el error se queda en la
  pantalla hasta que se pulse «atrás».
- **Pausa**: al salir de la app el juego se queda parado, y sigue al volver.
- **Tareas**: el cambio de pila de las tareas está escrito a mano para arm64 y
  x86_64 (`src/corrutina.c`): Android no trae `swapcontext`.
