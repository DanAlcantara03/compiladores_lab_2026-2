#ifndef P06_SYMTAB_H
#define P06_SYMTAB_H

#include <stddef.h>

typedef struct Symbol {
    char *name;
    char *type;
    int is_function;
    char **param_types;
    size_t param_count;
    char *return_type;
} Symbol;

void symtab_init(void);
void symtab_enter_scope(void);
void symtab_leave_scope(void);
int symtab_define(Symbol symbol, int reject_visible);
Symbol *symtab_lookup(const char *name);
int symtab_exists_visible(const char *name);
void symtab_free(void);

#endif
