#include <stdlib.h>
#include <string.h>

#include "symtab.h"

#define SYMTAB_SIZE 1024

typedef struct SymNode{
    char *lexeme; 
    struct SymNode *next;
} SymNode; 

static SymNode *table[SYMTAB_SIZE];

static unsigned long hash_str(const char *s){
    unsigned long h = 5381;
    int c; 
    while ((c=(unsigned char)*s++)){
        h = ((h << 5) + h) + c;
    }
    return h % SYMTAB_SIZE;
}

const char *symtab_intern(const char *lexeme)
{
    unsigned long i = hash_str(lexeme);
    SymNode *node;

    for (node = table[i]; node != NULL; node = node->next) {
        if (strcmp(node->lexeme, lexeme) == 0) {
            return node->lexeme;
        }
    }

    node = (SymNode *)malloc(sizeof(*node));
    if (!node) {
        return NULL;
    }

    node->lexeme = strdup(lexeme);
    if (!node->lexeme) {
        free(node);
        return NULL;
    }

    node->next = table[i];
    table[i] = node;
    return node->lexeme;
}

void symtab_destroy(void)
{
    int i;
    for (i = 0; i < SYMTAB_SIZE; i++) {
        SymNode *node = table[i];
        while (node) {
            SymNode *next = node->next;
            free(node->lexeme);
            free(node);
            node = next;
        }
        table[i] = NULL;
    }
}