#!/bin/bash
# Prueba 'lince depurar --ide': cada mensaje es una línea "@lince {json}".
LINCE="$(realpath "${1:-./lince}")"
DIR="$(cd "$(dirname "$0")" && pwd)/ide"
fallos=0
# sesion "orden" "orden"...: cada una espera a que la anterior haya hecho efecto
sesion() { ( for o in "$@"; do echo "$o"; sleep 0.4; done ) | (cd "$DIR" && "$LINCE" depurar --ide prog.lince 2>&1); }
comprobar() {   # comprobar "descripción" "texto que debe aparecer" orden...
    d="$1"; esperado="$2"; shift 2
    salida="$(sesion "$@")"
    if echo "$salida" | grep -qF -- "$esperado"; then echo "  ✅ $d"; else echo "  ❌ $d (falta: $esperado)"; echo "$salida" | sed 's/^/      /'; fallos=$((fallos+1)); fi
}
echo "🐆 Depurador (--ide)"
comprobar "para al empezar" '@lince {"evento":"parado","motivo":"inicio","archivo":"./prog.lince","linea":1}' "q"
comprobar "parada en un fichero importado" '"motivo":"parada","archivo":"./libs/util.lince","linea":2}' "b ./libs/util.lince:2" "c" "q"
comprobar "responde a b con la ruta" '{"respuesta":"parada","archivo":"./libs/util.lince","linea":2}' "b libs/util.lince:2" "q"
comprobar "paso dentro de la función importada" '"motivo":"paso","archivo":"./libs/util.lince","linea":2}' "b prog.lince:6" "c" "s" "q"
comprobar "pila con los dos ficheros" '{"respuesta":"pila","marcos":[{"funcion":"doble","archivo":"./libs/util.lince","linea":2},{"funcion":null,"archivo":"./prog.lince","linea":6}]}' "b libs/util.lince:2" "c" "bt" "q"
comprobar "variables locales" '{"respuesta":"variables","variables":[{"nombre":"n","tipo":"numero","valor":"1"}]}' "b libs/util.lince:2" "c" "v" "q"
comprobar "variables globales" '{"nombre":"saludo","tipo":"texto","valor":"\"hola\\nadios\""}' "b libs/util.lince:2" "c" "g" "q"
comprobar "valor de una expresión" '{"respuesta":"valor","tipo":"lista","valor":"[1, \"a\", 2]"}' "p [1, \"a\"] + [2]"  "q"
comprobar "error de una expresión" '{"respuesta":"valor","error":"Error: La variable' "p nada" "q"
comprobar "quitar un punto de parada" '{"respuesta":"quitada","archivo":"./libs/util.lince","linea":2}' "b libs/util.lince:2" "d libs/util.lince:2" "q"
comprobar "comando desconocido" '{"respuesta":"error","mensaje":"Comando desconocido: zz"}' "zz" "q"
comprobar "para antes de un error sin capturar" '"motivo":"error","archivo":"./libs/util.lince","linea":7,"mensaje":"ErrorRango' "c" "q"
comprobar "al acabar dice el código" '{"evento":"terminado","codigo":1}' "c" "c"
# «pausa» llega mientras el programa corre
salida="$( ( echo c; sleep 0.5; echo pausa; sleep 0.4; echo p k; sleep 0.3; echo q ) | (cd "$DIR" && "$LINCE" depurar --ide larga.lince 2>&1) )"
if echo "$salida" | grep -qF '"motivo":"pausa","archivo":"./larga.lince","linea":3}' && echo "$salida" | grep -qF '{"respuesta":"valor","tipo":"numero"'; then
    echo "  ✅ pausa con el programa corriendo"
else echo "  ❌ pausa con el programa corriendo"; echo "$salida" | sed 's/^/      /'; fallos=$((fallos+1)); fi
# Al acabar el programa sale, aunque quien lo lanzó siga con la entrada abierta
if command -v mkfifo > /dev/null; then
    fifo="$(mktemp -u)"; mkfifo "$fifo"; exec 3<>"$fifo"
    (cd "$DIR" && "$LINCE" depurar --ide prog.lince < "$fifo" > /dev/null 2>&1) &
    pid=$!
    echo c >&3; sleep 0.4; echo c >&3; sleep 1
    if kill -0 "$pid" 2> /dev/null; then
        echo "  ❌ no sale al acabar con la entrada abierta"; kill "$pid" 2> /dev/null; fallos=$((fallos+1))
    else echo "  ✅ sale al acabar con la entrada abierta"; fi
    exec 3>&-; rm -f "$fifo"
fi
[ "$fallos" -eq 0 ] && echo "✅ Depurador (--ide): todo correcto" || { echo "❌ $fallos comprobaciones fallaron"; exit 1; }
