# 🐆 Lince — Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Wno-maybe-uninitialized -Wno-format-truncation -Wno-stringop-truncation
LDFLAGS = -lm
SRC     = src/main.c src/lexer.c src/parser.c src/interprete.c src/modulos.c src/repl.c src/paquetes.c src/lsp.c src/empaquetado.c src/depurador.c src/ogg.c src/recarga.c src/imagen.c src/repartir.c src/nuevo.c src/letra.c src/corrutina.c src/android.c src/firmar.c src/apk.c src/tiled.c

# ── Detección de plataforma ───────────────
UNAME := $(shell uname 2>/dev/null || echo Windows)

ifeq ($(OS),Windows_NT)
    BIN     = lince.exe
else ifeq ($(UNAME),Darwin)
    BIN     = lince
    LDFLAGS += -ldl -lpthread
else
    BIN     = lince
    LDFLAGS += -ldl -lpthread
endif
RM = rm -f

# ── Compilar ──────────────────────────────
all: $(BIN)

$(BIN): $(SRC) src/version.h src/corrutina.h src/juego.h src/ogg.h src/imagen.h src/letra.h src/biblioteca_juego.h
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)
	@echo "✅ Lince compilado — usa: ./$(BIN) archivo.lince"

# ── Windows desde Linux (compilación cruzada con MinGW) ──
# Necesita x86_64-w64-mingw32-gcc (paquete gcc-mingw-w64-x86-64). Se prueba con wine.
MINGW ?= x86_64-w64-mingw32-gcc
windows: $(SRC) src/juego.h
	$(MINGW) $(CFLAGS) -o lince.exe $(SRC) -lm
	@echo "✅ lince.exe compilado para Windows"

# ── Android: 'lince repartir --android' con la plantilla (se salta si no la hay) ──
test-android: $(BIN)
	@bash tests/android/repartir.sh ./$(BIN)

# ── Modo debug ────────────────────────────
debug: $(SRC)
	$(CC) -Wall -g -fsanitize=address -o lince_debug $(SRC) $(LDFLAGS)
	@echo "✅ Lince debug compilado"

# ── Fuzzing: los lectores con datos estropeados al azar (tests/fuzz/fuzz.c) ──
# FUZZ_N pruebas por lector (2000 por defecto); los fallos quedan en build/fuzz-fallos/
FUZZ_N ?= 2000
FUZZ_SRC = $(filter-out src/main.c,$(SRC))
fuzz: tests/fuzz/fuzz.c $(SRC)
	@mkdir -p build && rm -rf build/fuzz-fallos
	@$(CC) -g -O1 -w -fsanitize=address,undefined -fno-sanitize-recover=undefined -o build/fuzz-lince tests/fuzz/fuzz.c $(FUZZ_SRC) $(LDFLAGS)
	@bash -c 'export ASAN_OPTIONS=detect_leaks=0:exitcode=99 UBSAN_OPTIONS=exitcode=99; ulimit -s 65536; f=0; \
	  ./build/fuzz-lince json $(FUZZ_N) build/fuzz-fallos tests/fuzz/semillas/*.json tests/juego/tiled/nivel.tmj || f=1; \
	  ./build/fuzz-lince tiled $(FUZZ_N) build/fuzz-fallos tests/juego/tiled/nivel.tmx tests/juego/tiled/tiles.tsx tests/juego/tiled/nivel.tmj || f=1; \
	  ./build/fuzz-lince imagen $(FUZZ_N) build/fuzz-fallos tests/juego/tiled/tiles.png tests/fuzz/semillas/*.png tests/fuzz/semillas/*.bmp tests/fuzz/semillas/*.gif || f=1; \
	  ./build/fuzz-lince ogg $(FUZZ_N) build/fuzz-fallos tests/juego/tono.ogg || f=1; \
	  ./build/fuzz-lince lince $(FUZZ_N) build/fuzz-fallos examples/*.lince tests/lince/test_*.lince src/lince/juego.lince || f=1; exit $$f'

# ── Valgrind — detectar leaks de memoria ──
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all \
		./lince_debug examples/01_pong.lince 2>&1 | head -40

# ── Ejecutar un ejemplo ───────────────────
ejemplo: $(BIN)
	./$(BIN) examples/01_pong.lince

# ── Tests ─────────────────────────────────
test: $(BIN)
	@./$(BIN) tests/runner.lince

test-compilador: $(BIN)
	@./$(BIN) tests/runner_compilador.lince
	@bash tests/compilador/probar_empaquetado.sh

# Ejecuta cada minijuego de examples/ sin pantalla (con teclas y clics automáticos).
test-ejemplos: $(BIN)
	@bash tests/juego/ejemplos.sh ./$(BIN)

# ── Tests bajo AddressSanitizer ───────────
# Las mismas suites, pero ejecutando el binario instrumentado:
#   detect_leaks=0  el intérprete no libera el AST ni las tablas de
#                   módulos al salir, así que LeakSanitizer sólo daría
#                   ruido preexistente ajeno a lo que se prueba.
#   ulimit -s       ASan usa marcos de pila mucho mayores y la recursión
#                   profunda de tests/lince/test_recursion.lince no cabe
#                   en los 8 MB por defecto.
#   LINCE_BIN       los runners lanzan el intérprete como subproceso; sin
#                   esto probarían el binario normal, no el instrumentado.
ASAN_ENV = ASAN_OPTIONS=detect_leaks=0 LINCE_BIN=./lince_debug

test-asan: debug
	@bash -c 'ulimit -s 65536; $(ASAN_ENV) ./lince_debug tests/runner.lince'
	@bash -c 'ulimit -s 65536; $(ASAN_ENV) ./lince_debug tests/runner_compilador.lince'
	@bash -c '$(ASAN_ENV) python3 tests/lsp/smoke.py ./lince_debug'
	@bash -c '$(ASAN_ENV) bash tests/depurador/smoke.sh ./lince_debug'
	@bash -c '$(ASAN_ENV) bash tests/depurador/ide.sh ./lince_debug'
	@bash -c '$(ASAN_ENV) bash tests/juego/smoke.sh ./lince_debug'
	@bash -c '$(ASAN_ENV) bash tests/juego/ejemplos.sh ./lince_debug 400'

# Servidor de lenguaje ('lince lsp'): habla el protocolo de verdad.
test-lsp: $(BIN)
	@python3 tests/lsp/smoke.py ./$(BIN)

# Soporte de editores: cada snippet es Lince válido, y la extensión de VS Code
# (con un 'vscode' de mentira) habla bien con el servidor de lenguaje.
test-editores: $(BIN)
	@python3 tests/editores/comprobar_snippets.py ./$(BIN)
	@node tests/editores/extension_mock.js ./$(BIN)

# Todo lo que corre la integración continua.
test-todo: test test-compilador test-ejemplos test-lsp test-editores editores-check bibliotecas-check test-depurador test-juego test-android test-asan

test-juego: $(BIN)
	@bash tests/juego/smoke.sh ./$(BIN)

# ── Manual ────────────────────────────────
# docs/manual.html se genera: los ejemplos son .lince de verdad y la salida
# que se publica es la que produce este intérprete al construir la página.
docs: $(BIN)
	@python3 docs/generar_manual.py ./$(BIN)
	@python3 docs/generar_tutorial.py ./$(BIN)
	@python3 docs/generar_juego.py ./$(BIN)

# Guardián: si alguien toca un ejemplo —o el lenguaje cambia de
# comportamiento— y no regenera, esto falla. Es lo que impide que el manual
# se quede mintiendo.
docs-check: $(BIN)
	@python3 docs/generar_manual.py ./$(BIN) --comprobar
	@python3 docs/generar_tutorial.py ./$(BIN) --comprobar
	@python3 docs/generar_juego.py ./$(BIN) --comprobar

# ── Soporte de editores ───────────────────
# Gramática, snippets y listas de palabras se generan del propio lenguaje
# (lexer, módulos, integradas), así que no se desfasan: 'editores-check' falla
# en la CI si alguien cambia el lenguaje sin regenerar.
editores:
	@python3 editores/generar.py

editores-check:
	@python3 editores/generar.py --comprobar

# ── Bibliotecas escritas en Lince ─────────
# src/lince/*.lince van dentro del ejecutable (src/biblioteca_*.h, generados
# y en el repositorio para compilar sin Python).
bibliotecas:
	@python3 src/lince/incrustar.py

bibliotecas-check:
	@python3 src/lince/incrustar.py --comprobar

test-depurador: $(BIN)
	@bash tests/depurador/smoke.sh ./$(BIN)
	@bash tests/depurador/ide.sh ./$(BIN)

# Medidas de rendimiento del intérprete (bench/*.lince): tiempo y memoria de cada una
bench: all
	@for f in bench/*.lince; do /usr/bin/time -f "$$(basename $$f) %es %MKB" ./lince $$f >/dev/null; done

# Extensión de VS Code empaquetada (.vsix) en build/
vsix:
	@mkdir -p build && rm -rf build/fuzz-fallos
	@python3 editores/vscode/empaquetar.py build

# ── Limpiar ───────────────────────────────
clean:
	$(RM) $(BIN) lince_debug lince.exe lince_debug.exe
	@echo "🧹 Limpiado"

# ── Instalar en el sistema ────────────────
install: $(BIN)
ifeq ($(OS),Windows_NT)
	copy $(BIN) C:\Windows\System32\lince.exe
	@echo "✅ Lince instalado en C:\Windows\System32\"
else
	cp $(BIN) /usr/local/bin/lince
	@echo "✅ Lince instalado en /usr/local/bin/lince"
	@echo "   Ya puedes usar: lince archivo.lince"
endif

uninstall:
ifeq ($(OS),Windows_NT)
	del C:\Windows\System32\lince.exe
else
	rm -f /usr/local/bin/lince
	@echo "🗑️  Lince desinstalado"
endif

.PHONY: all windows debug ejemplo test test-compilador test-ejemplos \
        test-asan test-depurador test-lsp test-editores editores editores-check bench vsix \
        test-juego test-android test-todo clean install uninstall docs docs-check bibliotecas bibliotecas-check

