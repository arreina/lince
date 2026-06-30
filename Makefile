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

# ── Modo debug ────────────────────────────
debug: $(SRC)
	$(CC) -Wall -g -fsanitize=address -o lince_debug $(SRC) $(LDFLAGS)
	@echo "✅ Lince debug compilado"

# ── Valgrind — detectar leaks de memoria ──
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all \
		./lince_debug ejemplos/hola_mundo.lince 2>&1 | head -40

# ── Ejecutar un ejemplo ───────────────────
ejemplo: $(BIN)
	./$(BIN) ejemplos/hola_mundo.lince

# ── Tests ─────────────────────────────────
test: $(BIN)
	@./$(BIN) tests/runner.lince

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

.PHONY: all debug ejemplo test clean install uninstall

