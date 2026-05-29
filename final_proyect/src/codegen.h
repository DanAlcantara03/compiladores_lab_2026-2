/**
 * @file codegen.h
 * @brief API de generacion de codigo intermedio FIS-25 desde el AST.
 */

#ifndef FINAL_CODEGEN_H
#define FINAL_CODEGEN_H

#include "ast.h"

#include <stdio.h>

/**
 * @brief Opciones de generacion de codigo.
 */
typedef struct CodegenOptions {
    int optimize; /**< Activa plegado simple de constantes cuando es distinto de cero. */
} CodegenOptions;

/**
 * @brief Emite codigo FIS-25 para un arbol AST validado semanticamente.
 *
 * @param root Raiz del AST.
 * @param out Flujo de salida donde se escribira el codigo generado.
 * @param options Opciones de generacion.
 * @return 0 si la generacion tuvo exito, -1 si hubo errores de entrada o emision.
 */
int codegen_emit_fis25(const ASTNode *root, FILE *out, CodegenOptions options);

#endif
