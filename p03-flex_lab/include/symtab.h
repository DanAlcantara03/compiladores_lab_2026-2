#ifndef SYMTAB_H
#define SYMTAB_H

/* Returns a stable pointer for a lexeme, inserting it if not present. */
const char *symtab_intern(const char *lexeme);

/* Releases all memory owned by the symbol table. */
void symtab_destroy(void);

#endif // SYMTAB_H