#ifndef FINAL_CODEGEN_H
#define FINAL_CODEGEN_H

#include "ast.h"

#include <stdio.h>

typedef struct CodegenOptions {
    int optimize;
} CodegenOptions;

int codegen_emit_fis25(const ASTNode *root, FILE *out, CodegenOptions options);

#endif
