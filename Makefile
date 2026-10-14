# 🐆 Lince — Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Wno-maybe-uninitialized -Wno-format-truncation -Wno-stringop-truncation
LDFLAGS = -lm -ldl
SRC     = src/main.c src/lexer.c src/parser.c src/interprete.c src/modulos.c src/repl.c src/paquetes.c src/compilador.c src/lsp.c

# ── Detección de plataforma ───────────────
UNAME := $(shell uname 2>/dev/null || echo Windows)

ifeq ($(OS),Windows_NT)
    BIN     = lince.exe
    LDFLAGS += -lws2_32
    RM      = del /Q
else ifeq ($(UNAME),Darwin)
    BIN     = lince
    RM      = rm -f
else
    BIN     = lince
    RM      = rm -f
endif

# ── Compilar ──────────────────────────────
all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)
	@echo "✅ Lince compilado — usa: ./$(BIN) archivo.lince"

# ── Modo debug ────────────────────────────
debug: $(SRC)
	$(CC) -Wall -g -fsanitize=address -o lince_debug $(SRC) $(LDFLAGS)
	@echo "✅ Lince debug compilado"

# ── Valgrind — detectar leaks de memoria ──
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all \
		./lince_debug ejemplos/01_hola_mundo.lince 2>&1 | head -40

# ── Ejecutar un ejemplo ───────────────────
ejemplo: $(BIN)
	./$(BIN) ejemplos/01_hola_mundo.lince

# ── Tests ─────────────────────────────────
test: $(BIN)
	@./$(BIN) tests/runner.lince

test-compilador: $(BIN)
	@./$(BIN) tests/runner_compilador.lince

# Test de humo del servidor: levanta un servidor real y le hace
# peticiones. Es la única cobertura del módulo 'servidor'.
test-servidor: $(BIN)
	@tests/servidor/smoke.sh ./$(BIN) 8123

# Comprueba que los ejercicios de examples/ siguen ejecutándose. No
# compara la salida: sólo evita que se queden obsoletos en silencio si
# cambia el lenguaje.
test-ejemplos: $(BIN)
	@for f in examples/*.lince; do \
		./$(BIN) "$$f" > /dev/null || { echo "❌ falla $$f"; exit 1; }; \
	done
	@echo "✅ Los ejercicios de examples/ se ejecutan sin errores"

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
	@bash -c 'ulimit -s 65536; $(ASAN_ENV) tests/servidor/smoke.sh ./lince_debug 8124'
	@bash -c '$(ASAN_ENV) python3 tests/lsp/smoke.py ./lince_debug'

# Servidor de lenguaje ('lince lsp'): habla el protocolo de verdad.
test-lsp: $(BIN)
	@python3 tests/lsp/smoke.py ./$(BIN)

# Soporte de editores: cada snippet es Lince válido, y la extensión de VS Code
# (con un 'vscode' de mentira) habla bien con el servidor.
test-editores: $(BIN)
	@python3 tests/editores/comprobar_snippets.py ./$(BIN)
	@node tests/editores/extension_mock.js ./$(BIN)

# Todo lo que corre la integración continua.
test-todo: test test-compilador test-servidor test-ejemplos test-lsp test-editores editores-check test-asan

# ── Manual ────────────────────────────────
# docs/manual.html se genera: los ejemplos son .lince de verdad y la salida
# que se publica es la que produce este intérprete al construir la página.
docs: $(BIN)
	@python3 docs/generar_manual.py ./$(BIN)

# Guardián: si alguien toca un ejemplo —o el lenguaje cambia de
# comportamiento— y no regenera, esto falla. Es lo que impide que el manual
# se quede mintiendo.
docs-check: $(BIN)
	@python3 docs/generar_manual.py ./$(BIN) --comprobar

# ── Soporte de editores ───────────────────
# Gramática, snippets y listas de palabras se generan del propio lenguaje
# (lexer, módulos, integradas), así que no se desfasan: 'editores-check' falla
# en la CI si alguien cambia el lenguaje sin regenerar.
editores:
	@python3 editores/generar.py

editores-check:
	@python3 editores/generar.py --comprobar

# Extensión de VS Code empaquetada (.vsix) en build/
vsix:
	@mkdir -p build
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

.PHONY: all debug ejemplo test test-compilador test-servidor test-ejemplos \
        test-asan test-lsp test-editores editores editores-check vsix \
        test-todo clean install uninstall docs docs-check

