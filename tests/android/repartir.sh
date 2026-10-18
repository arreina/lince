#!/bin/bash
# 'lince repartir --android' con la plantilla (lince-android.apk junto a Lince,
# o LINCE_ANDROID_PLANTILLA): hace el .apk de un ejemplo y comprueba que lleva
# el juego, el manifiesto cambiado y la firma v2 (con apksigner si lo hay), y
# el .aab para Google Play (con jarsigner y bundletool si los hay). Se salta si
# no hay plantilla: se hace con android/construir.sh --plantilla.
LINCE="$(cd "$(dirname "${1:-./lince}")" && pwd)/$(basename "${1:-./lince}")"
RAIZ="$(cd "$(dirname "$0")/../.." && pwd)"
echo "🐆 Android (lince repartir --android)"
PL="${LINCE_ANDROID_PLANTILLA:-$(dirname "$LINCE")/lince-android.apk}"
if [ ! -f "$PL" ]; then echo "  ⏭  no hay plantilla de Android: se omite"; exit 0; fi
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
fallos=0
ok() { echo "  ✅ $1"; }
mal() { echo "  ❌ $1"; fallos=$((fallos+1)); }
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --android -o "$T/pong.apk" > "$T/salida.txt" 2>&1
[ $? -eq 0 ] && ok "se hace el .apk" || { mal "no se hizo el .apk"; cat "$T/salida.txt"; }
grep -qF "pong, org.lince.pong" "$T/salida.txt" && ok "nombre y paquete por defecto: pong, org.lince.pong" || mal "nombre o paquete por defecto"
[ -f "$T/.lince/android-llave.txt" ] && ok "crea la llave en ~/.lince" || mal "no crea la llave"
unzip -l "$T/pong.apk" 2>/dev/null | grep -q "assets/juego.paq" && ok "lleva el juego" || mal "no lleva assets/juego.paq"
unzip -l "$T/pong.apk" 2>/dev/null | grep -q "META-INF/" && mal "lleva la firma vieja de la plantilla" || ok "sin la firma de la plantilla"
unzip -l "$T/pong.apk" 2>/dev/null | grep -q " aab/" && mal "lleva lo del .aab" || ok "sin lo del .aab"
grep -qa "APK Sig Block 42" "$T/pong.apk" && ok "lleva el bloque de firma v2" || mal "sin bloque de firma v2"
cp "$T/.lince/android-llave.txt" "$T/llave1"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --android -o "$T/pong2.apk" --nombre "Mi Pong" --id org.ejemplo.pong > "$T/salida2.txt" 2>&1
cmp -s "$T/llave1" "$T/.lince/android-llave.txt" && ok "la segunda vez usa la misma llave" || mal "la llave ha cambiado"
grep -qF "Mi Pong, org.ejemplo.pong" "$T/salida2.txt" && ok "--nombre e --id" || mal "--nombre o --id"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --android --id Mal.id > "$T/salida3.txt" 2>&1
grep -qF "El --id es como" "$T/salida3.txt" && ok "error de un --id mal escrito" || mal "falta el error del --id"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --android --version 0 > "$T/salida4.txt" 2>&1
grep -qF -- "--version es un número" "$T/salida4.txt" && ok "error de un --version mal escrito" || mal "falta el error de --version"

# El .aab de Google Play
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --aab --version 12 -o "$T/pong.aab" > "$T/salida5.txt" 2>&1
[ $? -eq 0 ] && ok "se hace el .aab" || { mal "no se hizo el .aab"; cat "$T/salida5.txt"; }
grep -qF "org.lince.pong, versión 12" "$T/salida5.txt" && ok "con la versión de --version" || mal "sin la versión"
unzip -l "$T/pong.aab" > "$T/aab.txt" 2>/dev/null
for f in BundleConfig.pb base/manifest/AndroidManifest.xml base/resources.pb base/dex/classes.dex \
         base/lib/arm64-v8a/libmain.so base/assets/juego.paq META-INF/MANIFEST.MF META-INF/LINCE.SF META-INF/LINCE.RSA; do
    grep -q " $f\$" "$T/aab.txt" || mal "al .aab le falta $f"
done
grep -qE " (AndroidManifest.xml|resources.arsc|aab/.*)\$" "$T/aab.txt" && mal "el .aab lleva lo del .apk" || ok "el .aab tiene lo suyo y la firma JAR"
unzip -p "$T/pong.aab" base/manifest/AndroidManifest.xml | grep -qa "org.lince.pong" && ok "manifiesto del .aab con el paquete" || mal "el manifiesto del .aab no tiene el paquete"
if command -v jarsigner > /dev/null; then
    jarsigner -verify "$T/pong.aab" 2>&1 | grep -q "jar verified" && ok "jarsigner: la firma del .aab es válida" || mal "jarsigner no da por buena la firma del .aab"
else
    echo "  ⏭  sin jarsigner: no se comprueba la firma del .aab"
fi
BUNDLETOOL="${BUNDLETOOL:-$HOME/lince-android/descargas/bundletool.jar}"
if [ -f "$BUNDLETOOL" ] && command -v java > /dev/null; then
    java -jar "$BUNDLETOOL" validate --bundle="$T/pong.aab" > /dev/null 2>&1 && ok "bundletool: el .aab es válido" || mal "bundletool no da por bueno el .aab"
    java -jar "$BUNDLETOOL" dump manifest --bundle="$T/pong.aab" 2>/dev/null | grep -qF 'android:versionCode="12"' \
        && ok "bundletool: versionCode 12" || mal "bundletool no ve el versionCode"
else
    echo "  ⏭  sin bundletool: no se comprueba el .aab con él"
fi

APKSIGNER="$(ls -d "${ANDROID_SDK_ROOT:-$HOME/lince-android/sdk}"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)"
if [ -n "$APKSIGNER" ] && command -v java > /dev/null; then
    "$APKSIGNER" verify "$T/pong.apk" && ok "apksigner: la firma es válida" || mal "apksigner no la da por buena"
else
    echo "  ⏭  sin apksigner: no se comprueba la firma con él"
fi
[ "$fallos" -eq 0 ] && echo "✅ Android: todo correcto" || exit 1
