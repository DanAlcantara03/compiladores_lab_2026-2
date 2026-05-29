/**
 * @file ast.c
 * @brief Implementacion de utilidades para nodos AST y exportacion Graphviz.
 */

#include "ast.h"

#include <stdlib.h>
#include <string.h>

/** @brief Duplica texto opcional para que el AST sea propietario de sus cadenas. */
static char *ast_copy_string(const char *text);
/** @brief Imprime dos espacios por nivel de indentacion. */
static void ast_print_indent(int indent);
/** @brief Escribe recursivamente un nodo y sus aristas en formato DOT. */
static void ast_write_dot_node(FILE *file, const ASTNode *node, int *next_id, int parent_id);
/** @brief Escapa caracteres especiales dentro de etiquetas DOT. */
static void ast_write_escaped(FILE *file, const char *text);

ASTNode *ast_new(const char *kind, const char *value, int line, int column) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "internal error: out of memory while creating AST node\n");
        exit(EXIT_FAILURE);
    }

    node->kind = ast_copy_string(kind);
    node->value = ast_copy_string(value);
    node->line = line;
    node->column = column;
    return node;
}

void ast_set_type(ASTNode *node, const char *semantic_type) {
    if (!node) {
        return;
    }
    free(node->semantic_type);
    node->semantic_type = ast_copy_string(semantic_type);
}

void ast_add_child(ASTNode *parent, ASTNode *child) {
    ASTNode **next_children;
    size_t next_capacity;

    if (!parent || !child) {
        return;
    }

    if (parent->child_count == parent->child_capacity) {
        next_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        next_children = (ASTNode **)realloc(parent->children, next_capacity * sizeof(ASTNode *));
        if (!next_children) {
            fprintf(stderr, "internal error: out of memory while growing AST children\n");
            exit(EXIT_FAILURE);
        }
        parent->children = next_children;
        parent->child_capacity = next_capacity;
    }

    parent->children[parent->child_count++] = child;
}

void ast_print(const ASTNode *node, int indent) {
    if (!node) {
        return;
    }

    ast_print_indent(indent);
    printf("%s", node->kind);
    if (node->value) {
        printf(": %s", node->value);
    }
    if (node->semantic_type) {
        printf(" <%s>", node->semantic_type);
    }
    if (node->line > 0) {
        printf(" [%d:%d]", node->line, node->column);
    }
    putchar('\n');

    for (size_t i = 0; i < node->child_count; i++) {
        ast_print(node->children[i], indent + 1);
    }
}

void ast_free(ASTNode *node) {
    if (!node) {
        return;
    }

    for (size_t i = 0; i < node->child_count; i++) {
        ast_free(node->children[i]);
    }
    free(node->children);
    free(node->kind);
    free(node->value);
    free(node->semantic_type);
    free(node);
}

int ast_write_dot(const ASTNode *node, const char *path) {
    FILE *file;
    int next_id = 0;

    if (!node || !path) {
        return -1;
    }

    file = fopen(path, "w");
    if (!file) {
        fprintf(stderr, "error: cannot write DOT file '%s'\n", path);
        return -1;
    }

    fprintf(file, "digraph AST {\n");
    fprintf(file, "  node [shape=box, style=rounded, fontname=\"Arial\"];\n");
    ast_write_dot_node(file, node, &next_id, -1);
    fprintf(file, "}\n");
    fclose(file);
    return 0;
}

static char *ast_copy_string(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        fprintf(stderr, "internal error: out of memory while copying string\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void ast_print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void ast_write_dot_node(FILE *file, const ASTNode *node, int *next_id, int parent_id) {
    int current_id = (*next_id)++;

    fprintf(file, "  node%d [label=\"", current_id);
    ast_write_escaped(file, node->kind);
    if (node->value) {
        fprintf(file, "\\n");
        ast_write_escaped(file, node->value);
    }
    if (node->semantic_type) {
        fprintf(file, "\\n<");
        ast_write_escaped(file, node->semantic_type);
        fprintf(file, ">");
    }
    fprintf(file, "\"];\n");

    if (parent_id >= 0) {
        fprintf(file, "  node%d -> node%d;\n", parent_id, current_id);
    }

    for (size_t i = 0; i < node->child_count; i++) {
        ast_write_dot_node(file, node->children[i], next_id, current_id);
    }
}

static void ast_write_escaped(FILE *file, const char *text) {
    if (!text) {
        return;
    }

    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '"' || text[i] == '\\') {
            fputc('\\', file);
        }
        fputc(text[i], file);
    }
}
