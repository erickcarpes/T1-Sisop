CC = cc
CFLAGS = -std=c89 -Wall -Wextra -pedantic
PTHREAD = -pthread
BIN = bin

.PHONY: all clean test bench

all: $(BIN)/sequencial $(BIN)/paralelo $(BIN)/gerar

$(BIN):
	mkdir -p $(BIN)

$(BIN)/sequencial: src/sequencial.c src/comum.c src/comum.h | $(BIN)
	$(CC) $(CFLAGS) src/sequencial.c src/comum.c -o $@

$(BIN)/paralelo: src/paralelo.c src/comum.c src/comum.h | $(BIN)
	$(CC) $(CFLAGS) $(PTHREAD) src/paralelo.c src/comum.c -o $@

$(BIN)/gerar: src/gerar.c | $(BIN)
	$(CC) $(CFLAGS) src/gerar.c -o $@

test: all
	bash scripts/testes.sh

bench: all
	bash scripts/bench.sh

clean:
	rm -rf $(BIN)
