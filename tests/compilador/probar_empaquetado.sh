#!/bin/bash
# Comprueba lo que el runner de Lince no puede: argumentos, código de salida y
# que el ejecutable no dependa de los archivos originales ni del directorio.
set -e
LINCE="${LINCE_BIN:-./lince}"
LINCE="$(cd "$(dirname "$LINCE")" && pwd)/$(basename "$LINCE")"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/lib"
cat > "$T/lib/saludos.lince" <<'EOF'
funcion saludar(val texto n): texto { devolver "hola " + n }
EOF
cat > "$T/prog.lince" <<'EOF'
importar "./lib/saludos"
escribir(saludar("mundo"))
escribir(argumentos())
lanzar Error("fallo a propósito")
EOF
"$LINCE" compilar "$T/prog.lince" -o "$T/prog" > /dev/null
mv "$T/lib" "$T/lib_oculta"          # el ejecutable lleva sus archivos dentro
salida="$(cd / && "$T/prog" uno dos 2>&1 || true)"
codigo=0; (cd / && "$T/prog" > /dev/null 2>&1) || codigo=$?
echo "$salida" | grep -q "hola mundo" || { echo "❌ empaquetado: no usa el archivo importado"; exit 1; }
echo "$salida" | grep -q "uno" && echo "$salida" | grep -q "dos" || { echo "❌ empaquetado: no recibe los argumentos"; echo "$salida"; exit 1; }
[ "$codigo" -ne 0 ] || { echo "❌ empaquetado: un error no deja código de salida distinto de 0"; exit 1; }
echo "✅ empaquetado: archivos importados, argumentos y código de salida"
