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
# Google Play de verdad (2.9): el icono del juego, el nombre de la versión y la ficha de la tienda
python3 -c "
from PIL import Image
i = Image.new('RGBA', (64, 64), (200, 40, 40, 255))
for x in range(16, 48):
    for y in range(16, 48): i.putpixel((x, y), (40, 200, 40, 255))
i.save('$T/icono.png')"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --aab --version 3 --version-nombre "1.2.3" \
    --icono "$T/icono.png" --guion "30:espacio" --capturas 60,200 -o "$T/ficha.aab" > "$T/salida6.txt" 2>&1
[ $? -eq 0 ] && ok "el .aab con icono, nombre de versión y ficha" || { mal "no se hizo el .aab con la ficha"; cat "$T/salida6.txt"; }
grep -qF "versión 3 (1.2.3)" "$T/salida6.txt" && ok "--version-nombre" || mal "--version-nombre"
grep -qF "mejor uno tuyo" "$T/salida6.txt" && ok "avisa del --id org.lince..." || mal "falta el aviso del --id"
unzip -p "$T/ficha.aab" base/manifest/AndroidManifest.xml | grep -qa "1.2.3" && ok "el manifiesto del .aab lleva 1.2.3" || mal "el manifiesto no lleva el nombre de versión"
unzip -p "$T/ficha.aab" base/res/drawable/icono.png > "$T/icono_aab.png" 2>/dev/null
F="$T/ficha-google-play"
python3 - "$T" <<'PY' && ok "la ficha: icono 512, gráfico 1024 × 500, capturas sin transparencia y el icono del juego dentro" || mal "la ficha"
import sys
from PIL import Image
t = sys.argv[1]
f = t + "/ficha-google-play/"
i = Image.open(t + "/icono_aab.png").convert("RGBA")
assert i.size == (192, 192) and i.getpixel((96, 96))[:3] == (40, 200, 40) and i.getpixel((5, 5))[:3] == (200, 40, 40)
assert Image.open(f + "icono-512.png").size == (512, 512)
g = Image.open(f + "grafico-destacado.png")
assert g.size == (1024, 500) and g.mode == "RGB"
for n in ("captura-1.png", "captura-2.png"):
    c = Image.open(f + n)
    assert c.mode == "RGB" and min(c.size) >= 1080 and max(c.size) <= 3840 and max(c.size) <= 2 * min(c.size)
assert open(f + "captura-1.png", "rb").read() != open(f + "captura-2.png", "rb").read()
PY
grep -q "BEGIN CERTIFICATE" "$F/certificado-de-subida.pem" 2>/dev/null && ok "el certificado de subida" || mal "falta el certificado de subida"
if command -v openssl > /dev/null; then
    openssl x509 -in "$F/certificado-de-subida.pem" -noout -enddate 2>/dev/null | grep -q "2049" && ok "openssl lee el certificado (vale hasta 2049)" || mal "openssl no lee el certificado"
fi
grep -qF "Seguridad de los datos" "$F/LEEME.txt" 2>/dev/null && ok "la guía de Play Console" || mal "falta el LEEME.txt de la ficha"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --android --icono "$T/icono.png" -o "$T/icono.apk" > /dev/null 2>&1
unzip -p "$T/icono.apk" res/drawable/icono.png > "$T/icono_apk.png" 2>/dev/null
python3 -c "
from PIL import Image
assert Image.open('$T/icono_apk.png').size == (192, 192)" 2>/dev/null && ok "el .apk, con el icono del juego" || mal "el .apk sin el icono del juego"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$RAIZ/examples/01_pong.lince" --aab --capturas "60,abc" > "$T/salida7.txt" 2>&1
grep -qF -- "--capturas son números de fotograma" "$T/salida7.txt" && ok "error de --capturas" || mal "falta el error de --capturas"

BUNDLETOOL="${BUNDLETOOL:-$HOME/lince-android/descargas/bundletool.jar}"
if [ -f "$BUNDLETOOL" ] && command -v java > /dev/null; then
    java -jar "$BUNDLETOOL" validate --bundle="$T/pong.aab" > /dev/null 2>&1 && ok "bundletool: el .aab es válido" || mal "bundletool no da por bueno el .aab"
    java -jar "$BUNDLETOOL" dump manifest --bundle="$T/pong.aab" 2>/dev/null | grep -qF 'android:versionCode="12"' \
        && ok "bundletool: versionCode 12" || mal "bundletool no ve el versionCode"
    java -jar "$BUNDLETOOL" validate --bundle="$T/ficha.aab" > /dev/null 2>&1 && ok "bundletool: el .aab con icono es válido" || mal "bundletool no da por bueno el .aab con icono"
    java -jar "$BUNDLETOOL" dump manifest --bundle="$T/ficha.aab" 2>/dev/null | grep -qF 'android:versionName="1.2.3"' \
        && ok "bundletool: versionName 1.2.3" || mal "bundletool no ve el versionName"
else
    echo "  ⏭  sin bundletool: no se comprueba el .aab con él"
fi

# 2.11: el permiso de la red, solo en el juego que llama a sistema.conexion()
printf 'importar "juego"\nimportar "sistema"\njuego.abrir("red", 100, 100)\nescribir(sistema.conexion())\n' > "$T/red.lince"
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$T/red.lince" --android -o "$T/red.apk" > "$T/salida8.txt" 2>&1
HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$T/red.lince" --aab --id org.ejemplo.red -o "$T/red.aab" > "$T/salida9.txt" 2>&1
tiene_permiso() {       # el manifiesto binario (textos en UTF-8 o UTF-16) o el del .aab (protobuf)
    python3 - "$1" "$2" <<'PY2'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); d = z.read(sys.argv[2])
p = "android.permission.ACCESS_NETWORK_STATE"
sys.exit(0 if p.encode() in d or p.encode("utf-16-le") in d else 1)
PY2
}
tiene_permiso "$T/red.apk" AndroidManifest.xml && ok "2.11: el .apk que usa sistema.conexion() lleva ACCESS_NETWORK_STATE" || mal "2.11: falta el permiso de la red"
tiene_permiso "$T/pong.apk" AndroidManifest.xml && mal "2.11: un juego sin sistema.conexion() lleva el permiso de la red" || ok "2.11: el que no la usa, sin permiso de la red"
tiene_permiso "$T/red.aab" base/manifest/AndroidManifest.xml && ok "2.11: también en el .aab" || mal "2.11: el .aab sin el permiso de la red"
tiene_permiso "$T/pong.aab" base/manifest/AndroidManifest.xml && mal "2.11: el .aab sin sistema.conexion() lleva el permiso" || ok "2.11: el .aab que no la usa, sin el permiso"
unzip -l "$T/red.apk" 2>/dev/null | grep -q " variantes/" && mal "2.11: el .apk lleva las variantes de la plantilla" || ok "2.11: sin las variantes de la plantilla"
if [ -n "$BUNDLETOOL" ] && [ -f "$BUNDLETOOL" ] && command -v java > /dev/null; then
    java -jar "$BUNDLETOOL" validate --bundle="$T/red.aab" > /dev/null 2>&1 && ok "2.11: bundletool da por bueno el .aab con el permiso" || mal "2.11: bundletool no da por bueno el .aab con el permiso"
fi

# 2.12: las bibliotecas de Google y sus permisos, solo en los juegos que importan anuncios o compras
if unzip -l "$PL" 2>/dev/null | grep -q " variantes/anuncios/"; then
    printf 'importar "anuncios"\nanuncios.configurar({"app": "ca-app-pub-1111111111111111~2222222222", "bloque": "ca-app-pub-1111111111111111/3333333333"})\n' > "$T/an.lince"
    printf 'importar "compras"\ncompras.restaurar()\n' > "$T/co.lince"
    printf 'importar "anuncios"\nimportar "compras"\nanuncios.configurar({})\n' > "$T/ambos.lince"
    for j in an co ambos; do
        HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$T/$j.lince" --android --id "org.ejemplo.$j" -o "$T/$j.apk" > "$T/salida_$j.txt" 2>&1 || { mal "2.12: no se hizo $j.apk"; cat "$T/salida_$j.txt"; }
        HOME="$T" LINCE_ANDROID_PLANTILLA="$PL" "$LINCE" repartir "$T/$j.lince" --aab --id "org.ejemplo.$j" -o "$T/$j.aab" > /dev/null 2>&1 || mal "2.12: no se hizo $j.aab"
    done
    lleva() {       # lleva archivo.apk|aab entrada texto
        python3 - "$@" <<'PY2'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); t = sys.argv[3]
if sys.argv[2] == "dex":
    d = b"".join(z.read(n) for n in z.namelist() if n.endswith(".dex"))
else:
    d = z.read(sys.argv[2])
sys.exit(0 if t.encode() in d or t.encode("utf-16-le") in d else 1)
PY2
    }
    lleva "$T/an.apk" AndroidManifest.xml android.permission.INTERNET && ok "2.12: anuncios: INTERNET" || mal "2.12: anuncios sin INTERNET"
    lleva "$T/an.apk" dex com/google/android/gms/ads/rewarded/RewardedAd && ok "2.12: anuncios: lleva la biblioteca de anuncios" || mal "2.12: anuncios sin la biblioteca"
    lleva "$T/an.apk" AndroidManifest.xml ca-app-pub-1111111111111111~2222222222 && ok "2.12: anuncios: la app de AdMob del código en el manifiesto" || mal "2.12: el manifiesto sin la app de AdMob del juego"
    lleva "$T/an.apk" AndroidManifest.xml org.ejemplo.an.mobileadsinitprovider && ok "2.12: anuncios: los proveedores con el paquete del juego" || mal "2.12: los proveedores siguen con el de la plantilla"
    lleva "$T/an.apk" AndroidManifest.xml org.lince.plantilla && mal "2.12: queda org.lince.plantilla en el manifiesto" || ok "2.12: nada de org.lince.plantilla en el manifiesto"
    lleva "$T/an.apk" AndroidManifest.xml com.android.vending.BILLING && mal "2.12: anuncios lleva BILLING" || ok "2.12: anuncios sin BILLING"
    lleva "$T/co.apk" AndroidManifest.xml com.android.vending.BILLING && ok "2.12: compras: BILLING" || mal "2.12: compras sin BILLING"
    lleva "$T/co.apk" dex com/android/billingclient/api/BillingClient && ok "2.12: compras: lleva Play Billing" || mal "2.12: compras sin Play Billing"
    lleva "$T/co.apk" dex com/google/android/gms/ads/rewarded && mal "2.12: compras lleva lo de anuncios" || ok "2.12: compras sin lo de anuncios"
    lleva "$T/co.apk" AndroidManifest.xml com.google.android.gms.permission.AD_ID && mal "2.12: compras pide AD_ID" || ok "2.12: compras sin AD_ID"
    lleva "$T/ambos.apk" AndroidManifest.xml com.android.vending.BILLING && lleva "$T/ambos.apk" dex com/google/android/gms/ads/rewarded/RewardedAd \
        && ok "2.12: los dos módulos: las dos bibliotecas" || mal "2.12: con los dos módulos falta alguna"
    for j in pong red; do
        lleva "$T/$j.apk" AndroidManifest.xml android.permission.INTERNET && mal "2.12: $j lleva INTERNET" || ok "2.12: $j, sin INTERNET"
        lleva "$T/$j.apk" dex com/google/android/gms && mal "2.12: $j lleva bibliotecas de Google" || ok "2.12: $j, sin bibliotecas de Google"
    done
    lleva "$T/an.aab" base/manifest/AndroidManifest.xml ca-app-pub-1111111111111111~2222222222 && ok "2.12: el .aab con la app de AdMob" || mal "2.12: el .aab sin la app de AdMob"
    lleva "$T/co.aab" base/manifest/AndroidManifest.xml com.android.vending.BILLING && ok "2.12: el .aab de compras con BILLING" || mal "2.12: el .aab de compras sin BILLING"
    if [ -n "$BUNDLETOOL" ] && [ -f "$BUNDLETOOL" ] && command -v java > /dev/null; then
        for j in an co ambos; do
            java -jar "$BUNDLETOOL" validate --bundle="$T/$j.aab" > /dev/null 2>&1 && ok "2.12: bundletool da por bueno $j.aab" || mal "2.12: bundletool no da por bueno $j.aab"
        done
    fi
else
    echo "  ⏭  la plantilla no trae las variantes de anuncios y compras (2.12): se omite"
fi

APKSIGNER="$(ls -d "${ANDROID_SDK_ROOT:-$HOME/lince-android/sdk}"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)"
if [ -n "$APKSIGNER" ] && command -v java > /dev/null; then
    "$APKSIGNER" verify "$T/pong.apk" && ok "apksigner: la firma es válida" || mal "apksigner no la da por buena"
else
    echo "  ⏭  sin apksigner: no se comprueba la firma con él"
fi
[ "$fallos" -eq 0 ] && echo "✅ Android: todo correcto" || exit 1
