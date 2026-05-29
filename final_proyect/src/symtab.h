/**
 * @file symtab.h
 * @brief Tabla de simbolos con pila de ambitos para analisis semantico.
 */

#ifndef P07_SYMTAB_H
#define P07_SYMTAB_H

#include <stddef.h>

/**
 * @brief Categoria semantica de un simbolo declarado.
 */
typedef enum SymbolCategory {
    SYMBOL_VARIABLE, /**< Variable mutable. */
    SYMBOL_CONSTANT, /**< Constante declarada. */
    SYMBOL_FUNCTION  /**< Funcion definida por el usuario. */
} SymbolCategory;

/**
 * @brief Entrada almacenada en la tabla de simbolos.
 *
 * Guarda metadatos de declaracion, tipo, uso y firma de funciones. Las cadenas
 * y el arreglo de parametros son copiados por la tabla al definir el simbolo.
 */
typedef struct Symbol {
    char *name;                 /**< Identificador del simbolo. */
    SymbolCategory category;    /**< Rol del simbolo dentro del lenguaje. */
    char *type;                 /**< Tipo declarado o inferido para variables/constantes. */
    int depth;                  /**< Profundidad del ambito donde vive. */
    int decl_line;              /**< Linea de declaracion. */
    int decl_column;            /**< Columna de declaracion. */
    size_t read_count;          /**< Numero de lecturas registradas para advertencias. */
    char **param_types;         /**< Tipos de parametros si el simbolo es funcion. */
    size_t param_count;         /**< Cantidad de parametros. */
    char *return_type;          /**< Tipo de retorno si el simbolo es funcion. */
} Symbol;

/**
 * @brief Reinicia la tabla y crea el ambito global.
 */
void symtab_init(void);

/**
 * @brief Apila un nuevo ambito vacio.
 */
void symtab_enter_scope(void);

/**
 * @brief Desapila el ambito actual y libera sus simbolos.
 *
 * Emite advertencias por variables no usadas antes de liberar sus metadatos.
 */
void symtab_leave_scope(void);

/**
 * @brief Define un simbolo en el ambito actual.
 *
 * @param symbol Simbolo fuente; sus campos se copian internamente.
 * @param reject_visible Si es distinto de cero, rechaza tambien colisiones en ambitos visibles.
 * @return 1 si se inserto, 0 si el identificador ya existe segun la politica indicada.
 */
int symtab_define(Symbol symbol, int reject_visible);

/**
 * @brief Busca un simbolo solo en el ambito actual.
 *
 * @param name Identificador a buscar.
 * @return Simbolo encontrado o NULL.
 */
Symbol *symtab_lookup_current(const char *name);

/**
 * @brief Busca un simbolo desde el ambito actual hacia el global.
 *
 * @param name Identificador a buscar.
 * @return Simbolo visible mas cercano o NULL.
 */
Symbol *symtab_lookup(const char *name);

/**
 * @brief Registra una lectura de una variable.
 *
 * @param symbol Simbolo a marcar; se ignora si no es variable.
 */
void symtab_mark_read(Symbol *symbol);

/**
 * @brief Indica si un nombre existe en el ambito actual.
 *
 * @param name Identificador a consultar.
 * @return 1 si existe, 0 en caso contrario.
 */
int symtab_exists_current(const char *name);

/**
 * @brief Indica si un nombre existe en cualquier ambito visible.
 *
 * @param name Identificador a consultar.
 * @return 1 si existe, 0 en caso contrario.
 */
int symtab_exists_visible(const char *name);

/**
 * @brief Devuelve la profundidad del ambito actual.
 *
 * @return 0 para el ambito global o cuando la tabla esta vacia.
 */
size_t symtab_current_depth(void);

/**
 * @brief Libera todos los ambitos y deja la tabla sin inicializar.
 */
void symtab_free(void);

#endif
