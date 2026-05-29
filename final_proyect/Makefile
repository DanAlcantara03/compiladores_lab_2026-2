CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -Isrc -Ibuild
BIN := summc
BUILD := build

PARSER_C := $(BUILD)/parser.tab.c
PARSER_H := $(BUILD)/parser.tab.h
LEXER_C := $(BUILD)/lex.yy.c
SRC := $(PARSER_C) $(LEXER_C) src/ast.c src/symtab.c src/codegen.c

.PHONY: all run check clean

all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(PARSER_C) $(PARSER_H): src/parser.y | $(BUILD)
	bison -d -o $(PARSER_C) src/parser.y

$(LEXER_C): src/lexer.l $(PARSER_H) | $(BUILD)
	flex -o $(LEXER_C) src/lexer.l

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

run: all
	./$(BIN)

check: all
	python3 tests/run_tests.py

clean:
	rm -rf $(BUILD) $(BIN)
