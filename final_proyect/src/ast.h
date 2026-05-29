/**
 * @file ast.h
 * @brief API para construir, recorrer y exportar el arbol de sintaxis abstracta.
 */

#ifndef P07_AST_H
#define P07_AST_H

#include <stddef.h>
#include <stdio.h>

/**
 * @brief Nodo generico del arbol de sintaxis abstracta.
 *
 * Cada nodo guarda su clase sintactica, un valor opcional, el tipo semantico
 * inferido o validado, la posicion fuente y una lista dinamica de hijos.
 */
typedef struct ASTNode {
    char *kind;                  /**< Nombre de la construccion sintactica. */
    char *value;                 /**< Lexema, identificador u operador asociado; puede ser NULL. */
    char *semantic_type;         /**< Tipo semantico calculado; puede ser NULL. */
    int line;                    /**< Linea de origen, o 0 cuando no aplica. */
    int column;                  /**< Columna de origen, o 0 cuando no aplica. */
    struct ASTNode **children;   /**< Arreglo dinamico de hijos. */
    size_t child_count;          /**< Cantidad de hijos usados. */
    size_t child_capacity;       /**< Capacidad reservada para hijos. */
} ASTNode;

/**
 * @brief Crea un nodo AST.
 *
 * @param kind Clase sintactica del nodo.
 * @param value Valor opcional asociado al nodo.
 * @param line Linea fuente del nodo.
 * @param column Columna fuente del nodo.
 * @return Puntero al nodo nuevo. Termina el proceso si no hay memoria.
 */
ASTNode *ast_new(const char *kind, const char *value, int line, int column);

/**
 * @brief Reemplaza el tipo semantico de un nodo.
 *
 * @param node Nodo a actualizar; se ignora si es NULL.
 * @param semantic_type Nuevo tipo semantico; puede ser NULL.
 */
void ast_set_type(ASTNode *node, const char *semantic_type);

/**
 * @brief Agrega un hijo al final de la lista de hijos del padre.
 *
 * @param parent Nodo padre.
 * @param child Nodo hijo que pasa a ser propiedad del padre.
 */
void ast_add_child(ASTNode *parent, ASTNode *child);

/**
 * @brief Imprime el arbol en formato textual indentado.
 *
 * @param node Raiz del subarbol a imprimir.
 * @param indent Nivel inicial de indentacion.
 */
void ast_print(const ASTNode *node, int indent);

/**
 * @brief Libera recursivamente un arbol AST.
 *
 * @param node Raiz del subarbol a liberar; acepta NULL.
 */
void ast_free(ASTNode *node);

/**
 * @brief Escribe una representacion Graphviz DOT del AST.
 *
 * @param node Raiz del arbol a exportar.
 * @param path Ruta del archivo DOT de salida.
 * @return 0 si se escribio correctamente, -1 si faltan argumentos o falla la apertura.
 */
int ast_write_dot(const ASTNode *node, const char *path);

#endif
