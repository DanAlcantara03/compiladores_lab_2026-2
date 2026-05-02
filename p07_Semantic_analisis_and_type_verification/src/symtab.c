#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Scope {
    Symbol *symbols;
    size_t count;
    size_t capacity;
} Scope;

static Scope *scopes = NULL;
static size_t scope_count = 0;
static size_t scope_capacity = 0;

static char *copy_string(const char *text);
static void symbol_copy(Symbol *destination, const Symbol *source);
static void symbol_free(Symbol *symbol);
static Symbol *scope_lookup(Scope *scope, const char *name);

void symtab_init(void) {
    symtab_free();
    symtab_enter_scope();
}

void symtab_enter_scope(void) {
    Scope *next_scopes;
    size_t next_capacity;

    if (scope_count == scope_capacity) {
        next_capacity = scope_capacity == 0 ? 4 : scope_capacity * 2;
        next_scopes = (Scope *)realloc(scopes, next_capacity * sizeof(Scope));
        if (!next_scopes) {
            fprintf(stderr, "internal error: out of memory while growing scope stack\n");
            exit(EXIT_FAILURE);
        }
        scopes = next_scopes;
        scope_capacity = next_capacity;
    }

    scopes[scope_count].symbols = NULL;
    scopes[scope_count].count = 0;
    scopes[scope_count].capacity = 0;
    scope_count++;
}

void symtab_leave_scope(void) {
    Scope *scope;

    if (scope_count == 0) {
        return;
    }

    scope = &scopes[scope_count - 1];
    for (size_t i = 0; i < scope->count; i++) {
        symbol_free(&scope->symbols[i]);
    }
    free(scope->symbols);
    scope_count--;
}

int symtab_define(Symbol symbol, int reject_visible) {
    Scope *scope;
    Symbol *next_symbols;
    size_t next_capacity;

    if (scope_count == 0) {
        symtab_enter_scope();
    }

    if (reject_visible && symtab_exists_visible(symbol.name)) {
        return 0;
    }

    scope = &scopes[scope_count - 1];
    if (!reject_visible && scope_lookup(scope, symbol.name)) {
        return 0;
    }

    if (scope->count == scope->capacity) {
        next_capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        next_symbols = (Symbol *)realloc(scope->symbols, next_capacity * sizeof(Symbol));
        if (!next_symbols) {
            fprintf(stderr, "internal error: out of memory while growing symbol table\n");
            exit(EXIT_FAILURE);
        }
        scope->symbols = next_symbols;
        scope->capacity = next_capacity;
    }

    symbol_copy(&scope->symbols[scope->count++], &symbol);
    return 1;
}

Symbol *symtab_lookup(const char *name) {
    if (!name) {
        return NULL;
    }

    for (size_t i = scope_count; i > 0; i--) {
        Symbol *symbol = scope_lookup(&scopes[i - 1], name);
        if (symbol) {
            return symbol;
        }
    }

    return NULL;
}

int symtab_exists_visible(const char *name) {
    return symtab_lookup(name) != NULL;
}

void symtab_free(void) {
    while (scope_count > 0) {
        symtab_leave_scope();
    }
    free(scopes);
    scopes = NULL;
    scope_capacity = 0;
}

static char *copy_string(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        fprintf(stderr, "internal error: out of memory while copying symbol text\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void symbol_copy(Symbol *destination, const Symbol *source) {
    destination->name = copy_string(source->name);
    destination->type = copy_string(source->type);
    destination->is_function = source->is_function;
    destination->param_count = source->param_count;
    destination->return_type = copy_string(source->return_type);

    if (source->param_count > 0) {
        destination->param_types = (char **)calloc(source->param_count, sizeof(char *));
        if (!destination->param_types) {
            fprintf(stderr, "internal error: out of memory while copying parameter types\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < source->param_count; i++) {
            destination->param_types[i] = copy_string(source->param_types[i]);
        }
    } else {
        destination->param_types = NULL;
    }
}

static void symbol_free(Symbol *symbol) {
    if (!symbol) {
        return;
    }

    free(symbol->name);
    free(symbol->type);
    free(symbol->return_type);
    for (size_t i = 0; i < symbol->param_count; i++) {
        free(symbol->param_types[i]);
    }
    free(symbol->param_types);
}

static Symbol *scope_lookup(Scope *scope, const char *name) {
    if (!scope || !name) {
        return NULL;
    }

    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->symbols[i].name, name) == 0) {
            return &scope->symbols[i];
        }
    }

    return NULL;
}
