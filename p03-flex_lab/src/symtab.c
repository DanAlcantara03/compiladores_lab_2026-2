#include <stdlib.h>
#include <string.h>

#include "symtab.h"

#define SYMTAB_SIZE 1024

/* Linked-list node used for separate chaining inside each hash bucket. */
typedef struct SymNode{
    char *lexeme; 
    struct SymNode *next;
} SymNode; 

/* Hash table buckets. Each entry is the head of a chained list. */
static SymNode *table[SYMTAB_SIZE];

/* djb2 hash variant for string keys. */
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

    /* Reuse existing storage if the lexeme is already interned. */
    for (node = table[i]; node != NULL; node = node->next) {
        if (strcmp(node->lexeme, lexeme) == 0) {
            return node->lexeme;
        }
    }

    node = (SymNode *)malloc(sizeof(*node));
    if (!node) {
        return NULL;
    }

    {
        size_t len = strlen(lexeme) + 1;
        node->lexeme = (char *)malloc(len);
        if (node->lexeme) {
            memcpy(node->lexeme, lexeme, len);
        }
    }
    if (!node->lexeme) {
        free(node);
        return NULL;
    }

    /* Insert at bucket head (separate chaining collision strategy). */
    node->next = table[i];
    table[i] = node;
    return node->lexeme;
}

/* Free every bucket and every interned lexeme. */
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
