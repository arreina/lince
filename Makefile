# 🐆 Lince — Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Wno-maybe-uninitialized -Wno-format-truncation -Wno-stringop-truncation
LDFLAGS = -lm
SRC     = src/main.c src/lexer.c src/parser.c src/interprete.c src/modulos.c src/repl.c src/paquetes.c src/compilador.c

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

# ── Lince Motor (fase A3, opt-in) ─────────
#
# Build aparte: el `lince` normal (arriba) no depende de SDL2 ni sabe que
# el módulo `motor` existe. Este target enlaza libmotor.a de lince-motor
# (repo hermano, hay que compilarlo antes con `cmake --build build` ahí)
# más SDL2 y SDL2_image, y produce un binario distinto: lince-motor.
LINCE_MOTOR_DIR ?= ../lince-motor
MOTOR_BIN        = lince-motor
MOTOR_SRC        = $(SRC) src/modulo_motor.c
MOTOR_LIB        = $(LINCE_MOTOR_DIR)/build/libmotor.a
# Las flags propias del build del motor, en un solo sitio: las comparten
# 'motor' y 'motor-debug'. Duplicadas, añadir un -D o mover el include de
# lince-motor arreglaba uno y dejaba el otro fallando con un "no such file"
# que no parece tener nada que ver.
MOTOR_BASE_FLAGS = -DLINCE_MOTOR -I$(LINCE_MOTOR_DIR)/include \
                    $(shell pkg-config --cflags sdl2 SDL2_image)
MOTOR_CFLAGS     = $(CFLAGS) $(MOTOR_BASE_FLAGS)
MOTOR_LDFLAGS    = $(MOTOR_LIB) $(shell pkg-config --libs sdl2 SDL2_image) $(LDFLAGS)

motor: $(MOTOR_LIB)
	$(CC) $(MOTOR_CFLAGS) -o $(MOTOR_BIN) $(MOTOR_SRC) $(MOTOR_LDFLAGS)
	@echo "✅ Lince Motor compilado — usa: ./$(MOTOR_BIN) juego.lince"

$(MOTOR_LIB):
	@echo "❌ No encuentro $(MOTOR_LIB)"
	@echo "   Compila primero lince-motor: cd $(LINCE_MOTOR_DIR) && cmake -B build && cmake --build build -j"
	@exit 1

# El mismo binario con AddressSanitizer, que es el que corre el test de humo:
# los fallos del binding son de memoria y sin instrumentar no se ven.
MOTOR_BIN_DEBUG  = lince-motor-debug

# Mismos avisos que el build normal: si el binario instrumentado usara otras
# flags, la CI leería warnings que el release silencia a propósito (o al revés).
motor-debug: $(MOTOR_LIB)
	$(CC) $(CFLAGS) -O0 -g -fsanitize=address $(MOTOR_BASE_FLAGS) \
	    -o $(MOTOR_BIN_DEBUG) $(MOTOR_SRC) $(MOTOR_LDFLAGS)
	@echo "✅ Lince Motor debug compilado"

# Test de humo del motor: recorre el binding entero sin pantalla (SDL con el
# driver 'dummy'). Es opt-in como el propio target 'motor' — necesita SDL2 y
# lince-motor ya compilado — así que no entra en 'test-todo'.
test-motor: motor-debug
	@tests/motor/smoke.sh ./$(MOTOR_BIN_DEBUG) $(LINCE_MOTOR_DIR)

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

# Todo lo que corre la integración continua.
test-todo: test test-compilador test-servidor test-ejemplos test-asan

# ── Limpiar ───────────────────────────────
clean:
	$(RM) $(BIN) lince_debug lince.exe lince_debug.exe $(MOTOR_BIN) $(MOTOR_BIN_DEBUG)
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
        test-asan test-todo clean install uninstall motor motor-debug test-motor

