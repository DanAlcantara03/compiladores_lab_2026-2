#ifndef P07_AST_H
#define P07_AST_H

#include <stddef.h>
#include <stdio.h>

typedef struct ASTNode {
    char *kind;
    char *value;
    char *semantic_type;
    int line;
    int column;
    struct ASTNode **children;
    size_t child_count;
    size_t child_capacity;
} ASTNode;

ASTNode *ast_new(const char *kind, const char *value, int line, int column);
void ast_set_type(ASTNode *node, const char *semantic_type);
void ast_add_child(ASTNode *parent, ASTNode *child);
void ast_print(const ASTNode *node, int indent);
void ast_free(ASTNode *node);
int ast_write_dot(const ASTNode *node, const char *path);

#endif
