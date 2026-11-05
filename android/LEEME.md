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

`--version N` pone el número de versión (1 si no se dice). Para que un móvil
acepte una versión nueva encima de la anterior, el número tiene que ser mayor.

## Publicarlo en Google Play

Google Play no quiere un `.apk` sino un `.aab` (Android App Bundle), con el que
Google hace después el `.apk` justo para cada móvil:

```bash
lince repartir mi_juego.lince --aab --nombre "Mi juego" --id org.tunombre.mijuego --version 1 --version-nombre "1.0"
```

Hace `mi_juego.aab`, firmado con tu llave (la firma JAR, la que pide Google
Play), que se sube en [Play Console](https://play.google.com/console), y la
carpeta `mi_juego-google-play/` con lo que pide la ficha de la tienda:

- `icono-512.png`: el icono de la tienda.
- `grafico-destacado.png`: el de 1024 × 500 (una captura con el icono; mejor si
  luego haces uno tuyo).
- `captura-1.png`, `captura-2.png`...: capturas del juego jugando solo, sin
  ventana, en los fotogramas de `--capturas` (60, 180 y 300: a 1, 3 y 5
  segundos) y con lo que diga `--guion` pulsado (`--guion "30:espacio"`, como
  el de `juego.jugar`). Agrandadas por un número entero y sin transparencia, como
  las pide Google.
- `certificado-de-subida.pem`: el certificado de tu llave, por si Google lo pide.
- `LEEME.txt`: lo demás que pregunta Play Console, paso a paso.

El icono del juego (`--icono`, o `icono.png` junto al juego) va también en el
`.apk` y el `.aab`: el que se ve en el móvil. Hay que
tener cuenta de desarrollador (25 $, una sola vez), y Google pide probar la app
con un grupo de personas antes de publicarla. Para tener en cuenta:

- **El `--id` no se puede cambiar** después de publicar: elige uno tuyo
  (`org.tunombre.mijuego`), no `org.lince...`.
- **Cada `.aab` que subas, con `--version` más alto** que el anterior. Lo que
  ve el jugador en la tienda es `--version-nombre` (`"1.0"`, `"1.1"`...).
- **Tu llave es la «llave de subida»**: Google firma el juego que llega a los
  móviles con la suya (la firma de apps de Google Play). Si pierdes tu llave,
  Google te deja cambiarla; si la pierdes sin haber publicado nada, no pasa nada.
- El juego apunta a Android 16 (API 36) y sus bibliotecas sirven en móviles con
  páginas de memoria de 16 KB: lo que Google Play pide hoy.

En el `.aab` van las mismas piezas que en el `.apk`, colocadas como pide
Google, y el manifiesto y los recursos en su formato (protobuf). Esas dos
piezas vienen hechas en la plantilla, en `aab/` (las hace bundletool en
`android/construir.sh --plantilla`), y Lince solo cambia en ellas el nombre,
el paquete y la versión. Para probar un `.aab` en un móvil conectado por USB:
`java -jar bundletool.jar build-apks --bundle=mi_juego.aab --output=mi_juego.apks --connected-device`
y `java -jar bundletool.jar install-apks --apks=mi_juego.apks`.

## Anuncios recompensados y compras

Los módulos `anuncios` y `compras` (ver la especificación) usan bibliotecas
de Google: la de anuncios (Google Mobile Ads y el consentimiento, UMP) y la
de compras (Google Play Billing). Solo van en el `.apk` y el `.aab` de los
juegos que importan esos módulos, con sus permisos (`INTERNET`, `AD_ID`,
`com.android.vending.BILLING`...): un juego que no los importa queda igual
que siempre.

**Anuncios.** En [AdMob](https://admob.google.com) se crea la app y un bloque
de anuncios **recompensados**: sus identificadores (`ca-app-pub-...~...` y
`ca-app-pub-.../...`) van en `anuncios.configurar({"app": ..., "bloque": ...})`.
`lince repartir` pone el de la app en el manifiesto (lo busca en el código; o
`--anuncios-app ca-app-pub-...~...`). Mientras se prueba, deja `"pruebas"`
como está (verdadero): salen los anuncios de prueba de Google, que se pueden
tocar sin que te cierren la cuenta de AdMob. Pon `"pruebas": falso` solo en la
versión que publiques. En Europa hay que pedir el consentimiento: llama a
`anuncios.pedir_consentimiento()` al empezar y configura el mensaje de
consentimiento (GDPR) en AdMob, en «Privacidad y mensajes». En la ficha de
Play Console: «Contiene anuncios», y en «Seguridad de los datos», que la app
comparte el identificador de publicidad con Google para anuncios.

**Compras.** En Play Console, en «Monetizar → Productos → Productos
integrados en la aplicación», se crea cada producto (su id es el que va en
`compras.consultar_productos` y `compras.comprar`). Antes hay que subir un
`.aab` del juego que importe `compras` (Google no deja crear productos sin
él). Para probar sin pagar, añade tu cuenta como *license tester*
(«Configuración → Pruebas de licencias») e instala el juego desde la pista de
pruebas interna: las compras salen como de prueba. En la ficha, «Contiene
compras en la aplicación».

**Probarlo sin móvil.** `LINCE_ANUNCIOS=completar` (o `cerrar`, `fallar`,
`sin_conexion`, `no_disponible`) y `LINCE_COMPRAS="comprar=comprado;precio=3,99 €"`
simulan los dos en el ordenador y en `juego.jugar()`. Lo que dice Google
cuando un anuncio no carga se ve con `adb logcat -s lince`.

**Cómo va dentro.** `android/construir.sh --plantilla` hace con Gradle
(`android/servicios/`, que descarga Gradle y las bibliotecas la primera vez)
tres variantes de la app: «anuncios», «compras» y las dos. De cada una guarda
en la plantilla, en `variantes/`, lo que cambia: el manifiesto unido, los
recursos y el código (`classes*.dex`, con la parte en Java:
`android/servicios/anuncios/org/lince/Anuncios.java` y
`android/servicios/compras/org/lince/Compras.java`). `lince repartir` elige la
variante según lo que importa el juego y cambia en el manifiesto lo que
empieza por el paquete de la plantilla (los proveedores de las bibliotecas
de Google llevan el paquete del juego, para que no choquen dos juegos en el
mismo móvil). Con eso la plantilla pasa de unos 2,5 MB a unos 15 MB.

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
| `sdk/` | el SDK de Android con `platforms;android-36`, `build-tools;34.0.0` y `ndk;26.3.11579264` |
| `SDL2/` | las fuentes de SDL2 (2.30.9), tal cual (o `SDL_RUTA`) |
| `descargas/bundletool.jar` | bundletool (o `BUNDLETOOL`), para la plantilla: si no está, `construir.sh --plantilla` lo descarga |
| `gradle-8.14.3/` | Gradle, para las variantes con anuncios y compras: si no está, `construir.sh --plantilla` lo descarga (y Gradle, las bibliotecas de Google) |

`sdkmanager --sdk_root=... "platforms;android-36" "build-tools;34.0.0" "ndk;26.3.11579264"`
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
  rápido cuenta al menos un fotograma, en el mismo sitio que uno largo), y una
  acción puede tener botones propios en la pantalla: `"pantalla:x,y,ancho,alto"`.
  El primer dedo también mueve el ratón (`juego.raton()`, `juego.clic()`, que
  tampoco pierde los toques rápidos). `juego.deslizamiento()` reconoce los
  gestos de deslizar el dedo. `juego.tactil()` es verdadero.
- **Conexión**: `sistema.conexion()` dice si hay Internet. Para eso Android
  pide el permiso `ACCESS_NETWORK_STATE` (de los normales: no sale ninguna
  pregunta al jugador), que solo llevan los juegos que la llaman. La plantilla
  trae esa variante del manifiesto en `variantes/red/`, y `lince repartir` la
  elige al ver el juego (`elegir_variante` en `src/apk.c`).
- **Vibrar e inclinar**: `juego.vibrar(segundos)` hace vibrar el móvil (y los
  mandos), y `juego.inclinacion()` dice hacia dónde está inclinado; una acción
  puede usarlo con `"inclinacion:x-"`, `"inclinacion:x+"`...
- **Atrás**: el botón «atrás» del móvil cierra el juego (`juego.abierto()` pasa
  a falso).
- **Archivos**: lo que el juego escribe va a su carpeta privada del móvil, y
  las partidas guardadas (`juego.guardar`) a `partidas/` dentro de ella.
- **Errores**: si el juego se para por un error, el error se queda en la
  pantalla hasta que se pulse «atrás».
- **Pausa**: al salir de la app el juego se queda parado, y sigue al volver.
- **Tareas**: el cambio de pila de las tareas está escrito a mano para arm64 y
  x86_64 (`src/corrutina.c`): Android no trae `swapcontext`.
