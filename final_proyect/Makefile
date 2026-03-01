CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Wpedantic -Iinclude
BIN=app
SRC=src/main.c

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

run: all
	./$(BIN)

clean:
	rm -f $(BIN)
