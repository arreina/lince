#!/bin/bash
# 🐆 Con un emulador o un móvil conectado (adb): un juego que llama a
# juego.salir() al tocar la pantalla se cierra de verdad, 20 veces seguidas,
# tocando tras esperas distintas (en la 2.12.0 se quedaba congelado: fflush(NULL)
# esperaba al hilo que pasa lo escrito a logcat). También con un juego que
# importa "anuncios". Se salta si no hay nada conectado. Con varios aparatos,
# ANDROID_SERIAL dice cuál (adb lo usa solo).
#   bash tests/android/cerrar.sh ./lince [veces]
LINCE="$(cd "$(dirname "${1:-./lince}")" && pwd)/$(basename "${1:-./lince}")"
VECES="${2:-20}"
RAIZ="$(cd "$(dirname "$0")/../.." && pwd)"
ADB="$(command -v adb || echo "${ANDROID_SDK_ROOT:-$HOME/lince-android/sdk}/platform-tools/adb")"
echo "🐆 Android: cerrar el juego"
if [ ! -x "$ADB" ] || ! "$ADB" get-state > /dev/null 2>&1; then echo "  ⏭  no hay ningún móvil ni emulador conectado: se omite"; exit 0; fi
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
fallos=0
ok() { echo "  ✅ $1"; }
mal() { echo "  ❌ $1"; fallos=$((fallos+1)); }
cp "$RAIZ/tests/android/salir.lince" "$T/salir.lince"
sed 's/^importar "juego"$/importar "juego"\nimportar "anuncios"\nanuncios.configurar({"pruebas": verdadero})/' "$T/salir.lince" > "$T/salir_anuncios.lince"
read -r W H < <("$ADB" shell wm size | awk -F'[ x]' '/Physical/ {print $3, $4}')
for juego in salir salir_anuncios; do
    id="org.lince.prueba.$juego"
    "$LINCE" repartir "$T/$juego.lince" --android --id "$id" -o "$T/$juego.apk" > /dev/null 2>&1 || { mal "no se hizo $juego.apk"; continue; }
    "$ADB" install -r "$T/$juego.apk" > /dev/null 2>&1 || { mal "no se instaló $juego.apk"; continue; }
    cerradas=0
    for i in $(seq 1 "$VECES"); do
        "$ADB" shell am start -W -n "$id/org.lince.LinceActivity" > /dev/null 2>&1
        sleep $(( (i * 7) % 12 + 1 ))                # de 1 a 12 segundos antes de tocar
        "$ADB" shell input tap $((W / 2)) $((H / 2))
        for _ in 1 2 3 4 5 6; do
            sleep 0.5
            [ -z "$("$ADB" shell pidof "$id" | tr -d '\r')" ] && break
        done
        if [ -z "$("$ADB" shell pidof "$id" | tr -d '\r')" ]; then cerradas=$((cerradas+1)); else "$ADB" shell am force-stop "$id"; fi
    done
    [ "$cerradas" = "$VECES" ] && ok "$juego: se cierra $VECES de $VECES veces" || mal "$juego: se cierra $cerradas de $VECES veces"
    "$ADB" uninstall "$id" > /dev/null 2>&1
done
[ "$fallos" -eq 0 ] && echo "✅ Android (cerrar): todo correcto" || exit 1
