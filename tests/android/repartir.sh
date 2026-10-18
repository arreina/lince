#!/bin/bash
# 'lince repartir --android' con la plantilla (lince-android.apk junto a Lince,
# o LINCE_ANDROID_PLANTILLA): hace el .apk de un ejemplo y comprueba que lleva
# el juego, el manifiesto cambiado y la firma v2 (con apksigner si lo hay). Se
# salta si no hay plantilla: se hace con android/construir.sh --plantilla.
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
grep -qa "APK Sig Block 42" "$T/pong.apk" && ok "lleva el bloque de firma v2" || mal "sin bloque de firma v2"
cp "$T/.lince/android-llave.txt" "$T/llave1"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --android -o "$T/pong2.apk" --nombre "Mi Pong" --id org.ejemplo.pong > "$T/salida2.txt" 2>&1
cmp -s "$T/llave1" "$T/.lince/android-llave.txt" && ok "la segunda vez usa la misma llave" || mal "la llave ha cambiado"
grep -qF "Mi Pong, org.ejemplo.pong" "$T/salida2.txt" && ok "--nombre e --id" || mal "--nombre o --id"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --android --id Mal.id > "$T/salida3.txt" 2>&1
grep -qF "El --id es como" "$T/salida3.txt" && ok "error de un --id mal escrito" || mal "falta el error del --id"
APKSIGNER="$(ls -d "${ANDROID_SDK_ROOT:-$HOME/lince-android/sdk}"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)"
if [ -n "$APKSIGNER" ] && command -v java > /dev/null; then
    "$APKSIGNER" verify "$T/pong.apk" && ok "apksigner: la firma es válida" || mal "apksigner no la da por buena"
else
    echo "  ⏭  sin apksigner: no se comprueba la firma con él"
fi
[ "$fallos" -eq 0 ] && echo "✅ Android: todo correcto" || exit 1
