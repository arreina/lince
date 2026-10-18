#!/bin/bash
# 🐆 Lince para Android: hace el .apk de un juego.
#
#   android/construir.sh juego.lince [-o juego.apk] [--nombre "Mi juego"]
#                        [--id org.lince.mijuego] [--version 1] [--incluir carpeta]...
#   android/construir.sh --plantilla [-o lince-android.apk]
#
# La plantilla es un .apk de Lince sin juego: con ella, 'lince repartir
# --android juego.lince' hace el .apk sin nada de lo de abajo (ver src/apk.c).
#
# Necesita, fuera del repositorio (por defecto en ~/lince-android, o donde diga
# LINCE_ANDROID): jdk/ (Java 17), sdk/ (con platforms, build-tools y ndk) y
# SDL2/ (las fuentes de SDL2). Ver android/LEEME.md.
set -e
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
A="${LINCE_ANDROID:-$HOME/lince-android}"
mkdir -p "$A"
# Java y el SDK: los de $A si están; si no, los del sistema (como en la CI)
if [ -d "$A/jdk" ]; then export JAVA_HOME="$A/jdk"; fi
[ -n "$JAVA_HOME" ] && export PATH="$JAVA_HOME/bin:$PATH"
SDK="$A/sdk"
[ -d "$SDK" ] || SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$SDK}}"
NDK="$(ls -d "$SDK"/ndk/* 2>/dev/null | sort -V | tail -1)"
BT="$(ls -d "$SDK"/build-tools/* 2>/dev/null | sort -V | tail -1)"
PLAT="$(ls -d "$SDK"/platforms/android-* 2>/dev/null | sort -V | tail -1)"
SDL="${SDL_RUTA:-$A/SDL2}"
for falta in "$(command -v javac || echo javac)" "$NDK/ndk-build" "$BT/aapt2" "$PLAT/android.jar" "$SDL/Android.mk"; do
    [ -e "$falta" ] || { echo "❌ Falta $falta (ver android/LEEME.md)"; exit 1; }
done

JUEGO="" ; SALIDA="" ; NOMBRE="" ; ID="" ; VERSION=1 ; EXTRAS=() ; PLANTILLA=0
while [ $# -gt 0 ]; do
    case "$1" in
        -o) SALIDA="$2"; shift 2 ;;
        --nombre) NOMBRE="$2"; shift 2 ;;
        --id) ID="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        --incluir) EXTRAS+=(--incluir "$2"); shift 2 ;;
        --plantilla) PLANTILLA=1; shift ;;
        *) JUEGO="$1"; shift ;;
    esac
done
if [ "$PLANTILLA" = 1 ]; then          # lo que apk.c sabe cambiar
    JUEGO="plantilla"; ID="org.lince.plantilla"; NOMBRE="LINCE_NOMBRE"; SALIDA="${SALIDA:-lince-android.apk}"
fi
[ -n "$JUEGO" ] || { echo "Uso: android/construir.sh juego.lince [-o juego.apk] [--nombre \"Mi juego\"] [--id org.lince.mijuego]"; exit 1; }
BASE="$(basename "$JUEGO" .lince)"
SALIDA="${SALIDA:-$BASE.apk}"
NOMBRE="${NOMBRE:-$BASE}"
if [ -z "$ID" ]; then            # 01_pong.lince → org.lince.pong (cada parte empieza por letra)
    PARTE="$(echo "$BASE" | tr -cd 'a-zA-Z0-9' | tr 'A-Z' 'a-z' | sed 's/^[0-9]*//')"
    ID="org.lince.${PARTE:-juego}"
fi
echo "$ID" | grep -Eq '^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$' || { echo "❌ El --id es como org.lince.mijuego"; exit 1; }

LINCE="$RAIZ/lince"
make -s -C "$RAIZ" > /dev/null

# 1. Lince y SDL2 para el móvil (se rehacen solo si cambian)
OBRA="$A/obra"
echo "🔧 Compilando Lince y SDL2 para Android..."
"$NDK/ndk-build" -s -j"$(nproc)" NDK_PROJECT_PATH="$OBRA/ndk" NDK_LIBS_OUT="$OBRA/lib" \
    APP_BUILD_SCRIPT="$RAIZ/android/jni/Android.mk" NDK_APPLICATION_MK="$RAIZ/android/jni/Application.mk" \
    SDL_RUTA="$SDL" > "$OBRA.log" 2>&1 || { tail -30 "$OBRA.log"; echo "❌ Falló la compilación para Android"; exit 1; }

# 2. La parte en Java: la de SDL y la actividad de Lince (se rehace si cambia)
if [ ! -f "$OBRA/dex/classes.dex" ] || [ "$RAIZ/android/java/org/lince/LinceActivity.java" -nt "$OBRA/dex/classes.dex" ]; then
    echo "🔧 Compilando la parte en Java..."
    rm -rf "$OBRA/clases" "$OBRA/dex"; mkdir -p "$OBRA/clases" "$OBRA/dex"
    javac -nowarn --release 8 -classpath "$PLAT/android.jar" -d "$OBRA/clases" \
        $(find "$SDL/android-project/app/src/main/java" "$RAIZ/android/java" -name '*.java') 2> "$OBRA/javac.log" ||
        { cat "$OBRA/javac.log"; exit 1; }
    "$BT/d8" --min-api 21 --lib "$PLAT/android.jar" --output "$OBRA/dex" $(find "$OBRA/clases" -name '*.class')
fi

# 3. El juego, el icono y el manifiesto
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/assets" "$T/res/drawable"
if [ "$PLANTILLA" = 1 ]; then
    : > "$T/assets/.vacio"              # sin juego: lo pone 'lince repartir --android'
else
    "$LINCE" compilar "$JUEGO" --datos -o "$T/assets/juego.paq" "${EXTRAS[@]}" > "$T/compilar.log" || { cat "$T/compilar.log"; exit 1; }
fi
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$LINCE" "$RAIZ/android/icono.lince" "$T/res/drawable/icono.png" > /dev/null
NOMBRE_XML="$(printf '%s' "$NOMBRE" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/"/\&quot;/g')"
sed -e "s|@ID@|$ID|g" -e "s|@VERSION@|$VERSION|g" -e "s|@NOMBRE@|$NOMBRE_XML|g" \
    "$RAIZ/android/AndroidManifest.xml" > "$T/AndroidManifest.xml"

# 4. El .apk: recursos, código, bibliotecas, alinear y firmar
"$BT/aapt2" compile --dir "$T/res" -o "$T/res.zip"
"$BT/aapt2" link -o "$T/base.apk" -I "$PLAT/android.jar" --manifest "$T/AndroidManifest.xml" \
    -A "$T/assets" "$T/res.zip"
cp "$OBRA/dex/classes.dex" "$T/"
mkdir -p "$T/lib"
cp -r "$OBRA/lib/"* "$T/lib/"
(cd "$T" && zip -q -r base.apk classes.dex lib)
"$BT/zipalign" -f -p 4 "$T/base.apk" "$T/alineado.apk"
LLAVE="$A/lince-pruebas.keystore"           # una llave de pruebas, solo de esta máquina
[ -f "$LLAVE" ] || keytool -genkeypair -keystore "$LLAVE" -alias lince -keyalg RSA -keysize 2048 \
    -validity 10000 -storepass lince-pruebas -keypass lince-pruebas -dname "CN=Lince pruebas" > /dev/null 2>&1
"$BT/apksigner" sign --ks "$LLAVE" --ks-pass pass:lince-pruebas --key-pass pass:lince-pruebas \
    --out "$SALIDA" "$T/alineado.apk"
rm -f "$SALIDA.idsig"
echo "✅ $SALIDA ($(du -h "$SALIDA" | cut -f1)) — $NOMBRE, $ID"
