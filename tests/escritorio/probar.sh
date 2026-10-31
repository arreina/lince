#!/bin/bash
# 🐆 Lince — pruebas de 'lince repartir' para el escritorio (2.7): el nombre,
# el icono, el .zip para itch.io, el archivo único de Linux (con SDL2 dentro y
# en el menú de aplicaciones) y el instalador de Windows (con Wine en Linux si
# hay un lince.exe de esta versión en LINCE_WINDOWS, y de verdad en Windows).
# Uso: bash tests/escritorio/probar.sh ./lince
LINCE="$(cd "$(dirname "${1:-./lince}")" && pwd)/$(basename "${1:-./lince}")"
DIR="$(cd "$(dirname "$0")" && pwd)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
cd "$T"
fallos=0
ok()  { echo "  ✅ $1"; }
mal() { echo "  ❌ $1"; fallos=$((fallos+1)); }
PY=python3; command -v python3 > /dev/null || PY=python
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) WIN=1 ;; *) WIN=0 ;; esac

cat > juego.lince <<'L'
importar "juego"
juego.abrir("Prueba", 64, 48)
mientras juego.abierto() {
    juego.limpiar([10, 20, 30])
    juego.presentar()
}
escribir("terminado")
L
$PY -c "
from PIL import Image
i = Image.new('RGBA', (300, 200), (200, 40, 40, 255))
for x in range(100, 200):
    for y in range(50, 150): i.putpixel((x, y), (40, 200, 40, 255))
i.save('icono.png')"

# ── errores ──
"$LINCE" repartir juego.lince --para mac > salida.txt 2>&1 && mal "--para mac debería fallar" || \
    { grep -qF -- "--para es windows o linux" salida.txt && ok "--para solo windows o linux" || mal "falta el error de --para"; }
"$LINCE" repartir juego.lince --icono no_existe.png > salida.txt 2>&1 && mal "un icono que no existe debería fallar" || \
    { grep -qF "No se puede leer el icono 'no_existe.png'" salida.txt && ok "el icono que no existe" || mal "falta el error del icono"; }

if [ "$WIN" = 0 ]; then
    # ── Linux: la carpeta, el .zip y el archivo único ──
    "$LINCE" repartir juego.lince --nombre "La Prueba Ñ" > salida.txt 2>&1 || { mal "repartir falló"; cat salida.txt; }
    [ -x "juego-linux/La Prueba Ñ" ] && ok "el ejecutable se llama como el juego" || mal "falta juego-linux/La Prueba Ñ"
    [ -x "La Prueba Ñ.x86_64" ] && ok "el archivo único" || mal "falta La Prueba Ñ.x86_64"
    grep -qF "Icono   : icono.png" salida.txt && ok "usa icono.png de junto al juego" || mal "no usa icono.png"
    $PY - <<'P' && ok "el .zip lleva .itch.toml en la raíz, que apunta al juego" || mal "el .itch.toml del .zip"
import zipfile, sys
z = zipfile.ZipFile("juego-linux.zip")
t = z.read(".itch.toml").decode()
sys.exit(0 if 'path = "juego-linux/La Prueba Ñ"' in t and 'name = "play"' in t else 1)
P
    if grep -qF "libSDL2-2.0.so.0" salida.txt; then
        mkdir casa
        HOME="$T/casa" XDG_CACHE_HOME= XDG_DATA_HOME= SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=3 \
            LINCE_PROBAR_SDL_DENTRO=1 "./La Prueba Ñ.x86_64" > salida2.txt 2>&1
        grep -qx "terminado" salida2.txt && ok "el archivo único juega con la SDL2 que lleva dentro" || { mal "el archivo único no juega"; cat salida2.txt; }
        ls casa/.cache/lince/sdl-*/libSDL2-2.0.so.0 > /dev/null 2>&1 && ok "saca SDL2 a la caché" || mal "no saca SDL2 a la caché"
        [ ! -e casa/.local/share/lince-juegos ] && ok "en las pruebas no pregunta lo del menú" || mal "ha tocado el menú sin preguntar"
        HOME="$T/casa" XDG_DATA_HOME= "./La Prueba Ñ.x86_64" --instalar-en-menu > salida3.txt 2>&1
        D=casa/.local/share
        [ -x "$D/lince-juegos/la-prueba-n/la-prueba-n" ] && ok "menú: la copia del juego" || mal "menú: falta la copia"
        grep -qxF "Name=La Prueba Ñ" "$D/applications/lince-la-prueba-n.desktop" 2>/dev/null && \
            grep -qxF "Icon=lince-la-prueba-n" "$D/applications/lince-la-prueba-n.desktop" && ok "menú: el .desktop" || mal "menú: el .desktop"
        $PY -c "
from PIL import Image
i = Image.open('$D/icons/hicolor/256x256/apps/lince-la-prueba-n.png').convert('RGBA')
assert i.size == (256, 256) and i.getpixel((128, 128))[:3] == (40, 200, 40) and i.getpixel((128, 10))[3] == 0" 2>/dev/null \
            && ok "menú: el icono, de 256 y centrado" || mal "menú: el icono"
        HOME="$T/casa" XDG_DATA_HOME= "./La Prueba Ñ.x86_64" --desinstalar > salida4.txt 2>&1
        [ ! -e "$D/applications/lince-la-prueba-n.desktop" ] && [ ! -e "$D/lince-juegos/la-prueba-n/la-prueba-n" ] && \
            grep -qx "no" "$D/lince-juegos/la-prueba-n.menu" && ok "menú: --desinstalar lo quita y no vuelve a preguntar" || mal "menú: --desinstalar"
    else
        echo "  (sin SDL2 en este equipo: el archivo único no se prueba)"
    fi
    # ── El de Windows, desde aquí (si hay un lince.exe de esta versión) ──
    if [ -n "$LINCE_WINDOWS" ]; then
        LINCE_WINDOWS="$LINCE_WINDOWS" "$LINCE" repartir juego.lince --nombre "La Prueba Ñ" --para windows > salida5.txt 2>&1 || { mal "repartir --para windows falló"; cat salida5.txt; }
        [ -f "juego-windows/La Prueba Ñ.exe" ] && [ -f "La Prueba Ñ-instalar.exe" ] && ok "--para windows: la carpeta y el instalador" || mal "--para windows: faltan archivos"
        $PY "$DIR/pe_iconos.py" "juego-windows/La Prueba Ñ.exe" ico > salida6.txt 2>&1
        grep -qx "subsistema 2" salida6.txt && ok "--para windows: sin consola" || mal "--para windows: con consola"
        grep -qx "icono 256" salida6.txt && $PY -c "
from PIL import Image
assert Image.open('ico_256.png').getpixel((128, 128))[:3] == (40, 200, 40)" && ok "--para windows: con su icono" || mal "--para windows: el icono"
    fi
    "$LINCE" repartir juego.lince --para windows > salida7.txt 2>&1 < /dev/null
    if [ -z "$LINCE_WINDOWS" ] && [ ! -d "$HOME/.lince/windows" ]; then
        grep -qF "hace falta el Lince de Windows de esta versión" salida7.txt && ok "--para windows sin el Lince de Windows: dice cómo" || mal "falta cómo conseguir el Lince de Windows"
    fi
else
    # ── Windows: el instalador de verdad ──
    "$LINCE" repartir juego.lince --nombre "La Prueba Ñ" > salida.txt 2>&1 || { mal "repartir falló"; cat salida.txt; }
    [ -f "juego-windows/La Prueba Ñ.exe" ] && [ -f "La Prueba Ñ-instalar.exe" ] && ok "la carpeta y el instalador" || mal "faltan archivos"
    $PY "$DIR/pe_iconos.py" "La Prueba Ñ-instalar.exe" ico > salida6.txt 2>&1
    grep -qx "subsistema 2" salida6.txt && grep -qx "icono 256" salida6.txt && ok "el instalador, con su icono y sin consola" || { mal "el icono o la consola del instalador"; cat salida6.txt; }
    EN="$(cygpath -w "$T")\\instalado"
    LINCE_INSTALAR_EN="$EN" "./La Prueba Ñ-instalar.exe" --sin-preguntar > salida2.txt 2>&1
    grep -qF "instalado en" salida2.txt && [ -f "instalado/La Prueba Ñ.exe" ] && [ -f instalado/SDL2.dll ] && ok "instala el juego y sus DLL" || { mal "no instala"; cat salida2.txt; ls instalado; }
    reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lince-la-prueba-n' > salida3.txt 2>&1
    grep -q "UninstallString" salida3.txt && ok "se puede desinstalar desde Configuración" || { mal "falta la entrada del registro"; cat salida3.txt; }
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LINCE_JUEGO_FOTOGRAMAS=3 "instalado/La Prueba Ñ.exe" > salida4.txt 2>&1
    grep -q "terminado" salida4.txt && ok "lo instalado juega" || { mal "lo instalado no juega"; cat salida4.txt; }
    "instalado/La Prueba Ñ.exe" --desinstalar --sin-preguntar > salida5.txt 2>&1
    sleep 6
    [ ! -e instalado ] && ok "desinstala (la carpeta)" || mal "la carpeta sigue ahí"
    reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lince-la-prueba-n' > /dev/null 2>&1 && mal "la entrada del registro sigue ahí" || ok "desinstala (el registro)"
fi
[ "$fallos" -eq 0 ] && echo "✅ Escritorio: todo correcto" || { echo "❌ Escritorio: $fallos fallos"; exit 1; }
