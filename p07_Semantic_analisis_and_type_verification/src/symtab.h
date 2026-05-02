#ifndef P07_SYMTAB_H
#define P07_SYMTAB_H

#include <stddef.h>

typedef enum SymbolCategory {
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_FUNCTION
} SymbolCategory;

typedef struct Symbol {
    char *name;
    SymbolCategory category;
    char *type;
    int depth;
    char **param_types;
    size_t param_count;
    char *return_type;
} Symbol;

void symtab_init(void);
void symtab_enter_scope(void);
void symtab_leave_scope(void);
int symtab_define(Symbol symbol, int reject_visible);
Symbol *symtab_lookup_current(const char *name);
Symbol *symtab_lookup(const char *name);
int symtab_exists_current(const char *name);
int symtab_exists_visible(const char *name);
size_t symtab_current_depth(void);
void symtab_free(void);

#endif
