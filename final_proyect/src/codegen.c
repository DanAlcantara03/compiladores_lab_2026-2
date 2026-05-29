/**
 * @file codegen.c
 * @brief Generador de codigo FIS-25 para nodos AST de Summarize.
 */

#include "codegen.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Vector dinamico propietario de cadenas.
 */
typedef struct StringVec {
    char **items;       /**< Cadenas almacenadas. */
    size_t count;       /**< Cantidad de elementos usados. */
    size_t capacity;    /**< Capacidad reservada. */
} StringVec;

/**
 * @brief Resultado temporal de emitir una expresion.
 */
typedef struct Value {
    char *place;        /**< Literal, nombre de variable o temporal donde queda el valor. */
    char *type;         /**< Tipo semantico usado por optimizaciones. */
    int is_literal;     /**< Indica si place puede evaluarse como constante. */
} Value;

/**
 * @brief Estado mutable de una corrida de generacion.
 */
typedef struct CodegenContext {
    StringVec vars;                 /**< Variables y temporales declarados en la salida. */
    StringVec lines;                /**< Instrucciones FIS-25 emitidas. */
    int temp_count;                 /**< Secuencia para temporales unicos. */
    int label_count;                /**< Secuencia para etiquetas unicas. */
    int errors;                     /**< Errores acumulados durante la emision. */
    int optimize;                   /**< Habilita plegado de constantes. */
    const char *current_function;   /**< Funcion en emision, o NULL fuera de una funcion. */
} CodegenContext;

/** @brief Inserta una cadena ya reservada y transfiere su propiedad al vector. */
static void vec_push_owned(StringVec *vec, char *text);
/** @brief Agrega una copia de text solo si no existe en el vector. */
static void vec_add_unique(StringVec *vec, const char *text);
/** @brief Indica si text ya esta almacenado en el vector. */
static int vec_contains(const StringVec *vec, const char *text);
/** @brief Libera todas las cadenas y el arreglo del vector. */
static void vec_free(StringVec *vec);
/** @brief Duplica una cadena opcional. */
static char *copy_string(const char *text);
/** @brief Construye una cadena dinamica con formato printf. */
static char *format_string(const char *format, ...);
/** @brief Compara de forma segura el kind de un nodo AST. */
static int same_kind(const ASTNode *node, const char *kind);
/** @brief Reporta un error de generacion asociado opcionalmente a un nodo. */
static void cg_error(CodegenContext *ctx, const ASTNode *node, const char *format, ...);
/** @brief Agrega una instruccion FIS-25 formateada al contexto. */
static void cg_emit(CodegenContext *ctx, const char *format, ...);
/** @brief Crea un temporal unico y lo registra como variable de salida. */
static char *cg_temp(CodegenContext *ctx);
/** @brief Crea una etiqueta unica usando un prefijo descriptivo. */
static char *cg_label(CodegenContext *ctx, const char *hint);
/** @brief Calcula la etiqueta de entrada para una funcion. */
static char *function_label(const char *name);
/** @brief Calcula la variable sintetica usada para el retorno de una funcion. */
static char *function_return_var(const char *name);
/** @brief Recorre el AST y recolecta variables necesarias antes de emitir codigo. */
static void collect_variables(CodegenContext *ctx, const ASTNode *node);
/** @brief Emite un programa completo, separando declaraciones de funcion del flujo principal. */
static void emit_program(CodegenContext *ctx, const ASTNode *root);
/** @brief Despacha la emision de una sentencia o bloque segun su kind. */
static void emit_node(CodegenContext *ctx, const ASTNode *node);
/** @brief Emite el prologo, parametros, cuerpo y retorno de una funcion. */
static void emit_function(CodegenContext *ctx, const ASTNode *node);
/** @brief Emite saltos y etiquetas para una sentencia if/elif/else. */
static void emit_if(CodegenContext *ctx, const ASTNode *node);
/** @brief Emite una estructura while con condicion al inicio. */
static void emit_while(CodegenContext *ctx, const ASTNode *node);
/** @brief Emite un ciclo for de rango semiabierto. */
static void emit_for(CodegenContext *ctx, const ASTNode *node);
/** @brief Emite una asignacion hacia un destino concreto. */
static void emit_assignment_to(CodegenContext *ctx, const char *dest, const ASTNode *expr);
/** @brief Emite una expresion y devuelve donde quedo su valor. */
static Value emit_expr(CodegenContext *ctx, const ASTNode *node);
/** @brief Emite llamadas nativas y llamadas a funciones de usuario. */
static Value emit_call(CodegenContext *ctx, const ASTNode *node);
/** @brief Construye un Value propietario de sus cadenas. */
static Value value_make(const char *place, const char *type, int is_literal);
/** @brief Libera las cadenas internas de un Value. */
static void value_free(Value value);
/** @brief Convierte literales booleanos del lenguaje a su forma FIS-25. */
static char *literal_text(const ASTNode *node);
/** @brief Indica si un tipo participa en operaciones numericas. */
static int is_numeric_type(const char *type);
/** @brief Indica si un tipo es int. */
static int is_int_type(const char *type);
/** @brief Indica si un tipo es bool. */
static int is_bool_type(const char *type);
/** @brief Intenta interpretar un Value literal como double. */
static int value_as_double(const Value *value, double *out);
/** @brief Intenta interpretar un Value literal como entero largo. */
static int value_as_long(const Value *value, long *out);
/** @brief Pliega una operacion unaria cuando su operando es constante. */
static char *fold_unary(const char *op, const Value *operand, const char *type);
/** @brief Pliega una operacion binaria cuando ambos operandos son constantes. */
static char *fold_binary(const char *op, const Value *left, const Value *right, const char *type);
/** @brief Formatea un double de forma compacta para la salida FIS-25. */
static char *format_double(double value);
/** @brief Traduce operadores binarios del lenguaje a instrucciones FIS-25. */
static const char *fis_binary_op(const char *op);

int codegen_emit_fis25(const ASTNode *root, FILE *out, CodegenOptions options) {
    CodegenContext ctx = {0};

    if (!root || !out) {
        return -1;
    }

    ctx.optimize = options.optimize;
    collect_variables(&ctx, root);
    emit_program(&ctx, root);

    if (ctx.errors > 0) {
        vec_free(&ctx.vars);
        vec_free(&ctx.lines);
        return -1;
    }

    fprintf(out, "// FIS-25 generated by summc\n\n");
    for (size_t i = 0; i < ctx.vars.count; i++) {
        fprintf(out, "VAR %s\n", ctx.vars.items[i]);
    }
    if (ctx.vars.count > 0 && ctx.lines.count > 0) {
        fputc('\n', out);
    }
    for (size_t i = 0; i < ctx.lines.count; i++) {
        fprintf(out, "%s\n", ctx.lines.items[i]);
    }

    vec_free(&ctx.vars);
    vec_free(&ctx.lines);
    return 0;
}

static void vec_push_owned(StringVec *vec, char *text) {
    char **next_items;
    size_t next_capacity;

    if (vec->count == vec->capacity) {
        next_capacity = vec->capacity == 0 ? 16 : vec->capacity * 2;
        next_items = (char **)realloc(vec->items, next_capacity * sizeof(char *));
        if (!next_items) {
            fprintf(stderr, "internal error: out of memory while growing string vector\n");
            exit(EXIT_FAILURE);
        }
        vec->items = next_items;
        vec->capacity = next_capacity;
    }

    vec->items[vec->count++] = text;
}

static void vec_add_unique(StringVec *vec, const char *text) {
    if (!text || vec_contains(vec, text)) {
        return;
    }
    vec_push_owned(vec, copy_string(text));
}

static int vec_contains(const StringVec *vec, const char *text) {
    if (!vec || !text) {
        return 0;
    }

    for (size_t i = 0; i < vec->count; i++) {
        if (strcmp(vec->items[i], text) == 0) {
            return 1;
        }
    }
    return 0;
}

static void vec_free(StringVec *vec) {
    if (!vec) {
        return;
    }
    for (size_t i = 0; i < vec->count; i++) {
        free(vec->items[i]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

static char *copy_string(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        fprintf(stderr, "internal error: out of memory while copying text\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static char *format_string(const char *format, ...) {
    va_list args;
    va_list copy;
    int needed;
    char *text;

    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        fprintf(stderr, "internal error: cannot format string\n");
        exit(EXIT_FAILURE);
    }

    text = (char *)malloc((size_t)needed + 1);
    if (!text) {
        va_end(args);
        fprintf(stderr, "internal error: out of memory while formatting string\n");
        exit(EXIT_FAILURE);
    }
    vsnprintf(text, (size_t)needed + 1, format, args);
    va_end(args);
    return text;
}

static int same_kind(const ASTNode *node, const char *kind) {
    return node && node->kind && strcmp(node->kind, kind) == 0;
}

static void cg_error(CodegenContext *ctx, const ASTNode *node, const char *format, ...) {
    va_list args;

    fprintf(stderr, "codegen error: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    if (node && node->line > 0) {
        fprintf(stderr, " at line %d, column %d", node->line, node->column);
    }
    fputc('\n', stderr);
    ctx->errors++;
}

static void cg_emit(CodegenContext *ctx, const char *format, ...) {
    va_list args;
    va_list copy;
    int needed;
    char *line;

    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        fprintf(stderr, "internal error: cannot format code line\n");
        exit(EXIT_FAILURE);
    }

    line = (char *)malloc((size_t)needed + 1);
    if (!line) {
        va_end(args);
        fprintf(stderr, "internal error: out of memory while emitting code\n");
        exit(EXIT_FAILURE);
    }
    vsnprintf(line, (size_t)needed + 1, format, args);
    va_end(args);
    vec_push_owned(&ctx->lines, line);
}

static char *cg_temp(CodegenContext *ctx) {
    char *name = format_string("__tmp_%d", ctx->temp_count++);
    vec_add_unique(&ctx->vars, name);
    return name;
}

static char *cg_label(CodegenContext *ctx, const char *hint) {
    return format_string("__%s_%d", hint ? hint : "L", ctx->label_count++);
}

static char *function_label(const char *name) {
    return format_string("__fn_%s", name);
}

static char *function_return_var(const char *name) {
    return format_string("__ret_%s", name);
}

static void collect_variables(CodegenContext *ctx, const ASTNode *node) {
    if (!node) {
        return;
    }

    if ((same_kind(node, "VarDecl") || same_kind(node, "InferredVarDecl") ||
         same_kind(node, "Assign") || same_kind(node, "ForRange") ||
         same_kind(node, "Param")) && node->value) {
        vec_add_unique(&ctx->vars, node->value);
    } else if (same_kind(node, "FunctionDecl") && node->value) {
        char *ret = function_return_var(node->value);
        vec_add_unique(&ctx->vars, ret);
        free(ret);
    }

    for (size_t i = 0; i < node->child_count; i++) {
        collect_variables(ctx, node->children[i]);
    }
}

static void emit_program(CodegenContext *ctx, const ASTNode *root) {
    int has_functions = 0;
    char *start_label = NULL;

    if (!same_kind(root, "Program")) {
        emit_node(ctx, root);
        return;
    }

    for (size_t i = 0; i < root->child_count; i++) {
        if (same_kind(root->children[i], "FunctionDecl")) {
            has_functions = 1;
            break;
        }
    }

    if (has_functions) {
        start_label = cg_label(ctx, "program_start");
        cg_emit(ctx, "GOTO %s", start_label);
        for (size_t i = 0; i < root->child_count; i++) {
            if (same_kind(root->children[i], "FunctionDecl")) {
                emit_function(ctx, root->children[i]);
            }
        }
        cg_emit(ctx, "LABEL %s", start_label);
    }

    for (size_t i = 0; i < root->child_count; i++) {
        if (!same_kind(root->children[i], "FunctionDecl")) {
            emit_node(ctx, root->children[i]);
        }
    }

    free(start_label);
}

static void emit_node(CodegenContext *ctx, const ASTNode *node) {
    if (!node) {
        return;
    }

    if (same_kind(node, "Program") || same_kind(node, "Statements") || same_kind(node, "Block")) {
        for (size_t i = 0; i < node->child_count; i++) {
            emit_node(ctx, node->children[i]);
        }
        return;
    }

    if (same_kind(node, "VarDecl") || same_kind(node, "InferredVarDecl")) {
        if (node->child_count >= 2) {
            emit_assignment_to(ctx, node->value, node->children[1]);
        }
        return;
    }

    if (same_kind(node, "Assign")) {
        if (node->child_count >= 1) {
            emit_assignment_to(ctx, node->value, node->children[0]);
        }
        return;
    }

    if (same_kind(node, "ExpressionStatement")) {
        if (node->child_count >= 1) {
            Value value = emit_expr(ctx, node->children[0]);
            value_free(value);
        }
        return;
    }

    if (same_kind(node, "If")) {
        emit_if(ctx, node);
        return;
    }

    if (same_kind(node, "While")) {
        emit_while(ctx, node);
        return;
    }

    if (same_kind(node, "ForRange")) {
        emit_for(ctx, node);
        return;
    }

    if (same_kind(node, "FunctionDecl")) {
        emit_function(ctx, node);
        return;
    }

    if (same_kind(node, "Return")) {
        if (!ctx->current_function) {
            cg_error(ctx, node, "return outside function");
            return;
        }
        if (node->child_count >= 1) {
            char *ret = function_return_var(ctx->current_function);
            emit_assignment_to(ctx, ret, node->children[0]);
            free(ret);
        }
        cg_emit(ctx, "RETURN");
        return;
    }

    cg_error(ctx, node, "unsupported statement kind '%s'", node->kind ? node->kind : "(null)");
}

static void emit_function(CodegenContext *ctx, const ASTNode *node) {
    char *label;
    const char *previous_function;
    const ASTNode *params = NULL;
    const ASTNode *block = NULL;

    if (!node || !node->value) {
        return;
    }

    label = function_label(node->value);
    previous_function = ctx->current_function;
    ctx->current_function = node->value;
    cg_emit(ctx, "LABEL %s", label);

    if (node->child_count >= 2) {
        params = node->children[1];
    }
    if (node->child_count >= 3) {
        block = node->children[2];
    }

    if (params) {
        for (size_t i = 0; i < params->child_count; i++) {
            const ASTNode *param = params->children[i];
            if (same_kind(param, "Param") && param->value) {
                cg_emit(ctx, "PARAM_GET %s", param->value);
            }
        }
    }

    emit_node(ctx, block);
    cg_emit(ctx, "RETURN");
    ctx->current_function = previous_function;
    free(label);
}

static void emit_if(CodegenContext *ctx, const ASTNode *node) {
    char *end_label = cg_label(ctx, "endif");
    char *next_label = cg_label(ctx, "else");
    const ASTNode *elif_list = node->child_count >= 3 ? node->children[2] : NULL;
    const ASTNode *else_clause = node->child_count >= 4 ? node->children[3] : NULL;
    Value cond;

    cond = emit_expr(ctx, node->children[0]);
    cg_emit(ctx, "IFFALSE %s GOTO %s", cond.place, next_label);
    value_free(cond);
    emit_node(ctx, node->children[1]);
    cg_emit(ctx, "GOTO %s", end_label);
    cg_emit(ctx, "LABEL %s", next_label);
    free(next_label);

    if (same_kind(elif_list, "ElifList")) {
        for (size_t i = 0; i < elif_list->child_count; i++) {
            const ASTNode *elif_node = elif_list->children[i];
            char *after_elif = cg_label(ctx, "elif_next");
            if (elif_node->child_count >= 2) {
                cond = emit_expr(ctx, elif_node->children[0]);
                cg_emit(ctx, "IFFALSE %s GOTO %s", cond.place, after_elif);
                value_free(cond);
                emit_node(ctx, elif_node->children[1]);
                cg_emit(ctx, "GOTO %s", end_label);
                cg_emit(ctx, "LABEL %s", after_elif);
            }
            free(after_elif);
        }
    }

    if (same_kind(else_clause, "Else") && else_clause->child_count >= 1) {
        emit_node(ctx, else_clause->children[0]);
    }

    cg_emit(ctx, "LABEL %s", end_label);
    free(end_label);
}

static void emit_while(CodegenContext *ctx, const ASTNode *node) {
    char *start_label = cg_label(ctx, "while");
    char *end_label = cg_label(ctx, "endwhile");
    Value cond;

    cg_emit(ctx, "LABEL %s", start_label);
    cond = emit_expr(ctx, node->children[0]);
    cg_emit(ctx, "IFFALSE %s GOTO %s", cond.place, end_label);
    value_free(cond);
    emit_node(ctx, node->children[1]);
    cg_emit(ctx, "GOTO %s", start_label);
    cg_emit(ctx, "LABEL %s", end_label);

    free(start_label);
    free(end_label);
}

static void emit_for(CodegenContext *ctx, const ASTNode *node) {
    const ASTNode *range = node->child_count >= 1 ? node->children[0] : NULL;
    const ASTNode *block = node->child_count >= 2 ? node->children[1] : NULL;
    char *start_label = cg_label(ctx, "for");
    char *end_label = cg_label(ctx, "endfor");
    char *cond_name = cg_temp(ctx);
    Value start;
    Value end;

    if (!node->value || !range || range->child_count < 2) {
        cg_error(ctx, node, "malformed for range");
        free(start_label);
        free(end_label);
        free(cond_name);
        return;
    }

    start = emit_expr(ctx, range->children[0]);
    end = emit_expr(ctx, range->children[1]);
    cg_emit(ctx, "ASSIGN %s %s", start.place, node->value);
    cg_emit(ctx, "LABEL %s", start_label);
    cg_emit(ctx, "LT %s %s %s", node->value, end.place, cond_name);
    cg_emit(ctx, "IFFALSE %s GOTO %s", cond_name, end_label);
    emit_node(ctx, block);
    cg_emit(ctx, "ADD %s 1 %s", node->value, node->value);
    cg_emit(ctx, "GOTO %s", start_label);
    cg_emit(ctx, "LABEL %s", end_label);

    value_free(start);
    value_free(end);
    free(start_label);
    free(end_label);
    free(cond_name);
}

static void emit_assignment_to(CodegenContext *ctx, const char *dest, const ASTNode *expr) {
    Value value;

    if (!dest) {
        cg_error(ctx, expr, "missing assignment destination");
        return;
    }

    value = emit_expr(ctx, expr);
    cg_emit(ctx, "ASSIGN %s %s", value.place, dest);
    value_free(value);
}

static Value emit_expr(CodegenContext *ctx, const ASTNode *node) {
    if (!node) {
        cg_error(ctx, NULL, "missing expression");
        return value_make("0", "int", 1);
    }

    if (same_kind(node, "Literal")) {
        char *text = literal_text(node);
        Value value = value_make(text, node->semantic_type, 1);
        free(text);
        return value;
    }

    if (same_kind(node, "Identifier")) {
        return value_make(node->value, node->semantic_type, 0);
    }

    if (same_kind(node, "Call")) {
        return emit_call(ctx, node);
    }

    if (same_kind(node, "UnaryExpr")) {
        Value operand = emit_expr(ctx, node->children[0]);
        char *folded = ctx->optimize ? fold_unary(node->value, &operand, node->semantic_type) : NULL;
        char *dest;
        Value result;

        if (folded) {
            result = value_make(folded, node->semantic_type, 1);
            free(folded);
            value_free(operand);
            return result;
        }

        dest = cg_temp(ctx);
        if (strcmp(node->value, "-") == 0) {
            cg_emit(ctx, "SUB 0 %s %s", operand.place, dest);
        } else if (strcmp(node->value, "!") == 0) {
            cg_emit(ctx, "EQ %s 0 %s", operand.place, dest);
        } else {
            cg_error(ctx, node, "unsupported unary operator '%s'", node->value);
        }
        result = value_make(dest, node->semantic_type, 0);
        free(dest);
        value_free(operand);
        return result;
    }

    if (same_kind(node, "BinaryExpr")) {
        Value left = emit_expr(ctx, node->children[0]);
        Value right = emit_expr(ctx, node->children[1]);
        char *folded = ctx->optimize ? fold_binary(node->value, &left, &right, node->semantic_type) : NULL;
        char *dest;
        Value result;
        const char *instruction;

        if (folded) {
            result = value_make(folded, node->semantic_type, 1);
            free(folded);
            value_free(left);
            value_free(right);
            return result;
        }

        dest = cg_temp(ctx);
        instruction = fis_binary_op(node->value);
        if (instruction) {
            cg_emit(ctx, "%s %s %s %s", instruction, left.place, right.place, dest);
        } else if (strcmp(node->value, "&") == 0) {
            cg_emit(ctx, "MUL %s %s %s", left.place, right.place, dest);
        } else if (strcmp(node->value, "|") == 0) {
            char *sum = cg_temp(ctx);
            cg_emit(ctx, "ADD %s %s %s", left.place, right.place, sum);
            cg_emit(ctx, "GT %s 0 %s", sum, dest);
            free(sum);
        } else {
            cg_error(ctx, node, "unsupported binary operator '%s'", node->value);
        }

        result = value_make(dest, node->semantic_type, 0);
        free(dest);
        value_free(left);
        value_free(right);
        return result;
    }

    cg_error(ctx, node, "unsupported expression kind '%s'", node->kind ? node->kind : "(null)");
    return value_make("0", "int", 1);
}

static Value emit_call(CodegenContext *ctx, const ASTNode *node) {
    const ASTNode *args = node->child_count >= 1 ? node->children[0] : NULL;
    const char *name = node->value ? node->value : "";
    Value result;

    if (strcmp(name, "print") == 0) {
        if (!args || args->child_count != 1) {
            cg_error(ctx, node, "print expects exactly one argument");
            return value_make("0", "int", 1);
        }
        result = emit_expr(ctx, args->children[0]);
        cg_emit(ctx, "PRINT %s", result.place);
        value_free(result);
        return value_make("0", "int", 1);
    }

    if (strcmp(name, "input") == 0) {
        char *dest;
        if (args && args->child_count != 0) {
            cg_error(ctx, node, "input expects no arguments");
        }
        dest = cg_temp(ctx);
        cg_emit(ctx, "INPUT %s", dest);
        result = value_make(dest, node->semantic_type ? node->semantic_type : "int", 0);
        free(dest);
        return result;
    }

    if (strcmp(name, "pixel") == 0) {
        Value x;
        Value y;
        Value color;
        if (!args || args->child_count != 3) {
            cg_error(ctx, node, "pixel expects x, y and color");
            return value_make("0", "int", 1);
        }
        x = emit_expr(ctx, args->children[0]);
        y = emit_expr(ctx, args->children[1]);
        color = emit_expr(ctx, args->children[2]);
        cg_emit(ctx, "PIXEL %s %s %s", x.place, y.place, color.place);
        value_free(x);
        value_free(y);
        value_free(color);
        return value_make("0", "int", 1);
    }

    if (strcmp(name, "key") == 0) {
        Value key;
        char *dest;
        if (!args || args->child_count != 1) {
            cg_error(ctx, node, "key expects exactly one argument");
            return value_make("0", "int", 1);
        }
        key = emit_expr(ctx, args->children[0]);
        dest = cg_temp(ctx);
        cg_emit(ctx, "KEY %s %s", key.place, dest);
        value_free(key);
        result = value_make(dest, node->semantic_type ? node->semantic_type : "bool", 0);
        free(dest);
        return result;
    }

    if (args) {
        for (size_t i = 0; i < args->child_count; i++) {
            Value arg = emit_expr(ctx, args->children[i]);
            cg_emit(ctx, "PARAM %s", arg.place);
            value_free(arg);
        }
    }

    {
        char *label = function_label(name);
        char *ret = function_return_var(name);
        char *dest = cg_temp(ctx);
        cg_emit(ctx, "GOSUB %s", label);
        cg_emit(ctx, "ASSIGN %s %s", ret, dest);
        result = value_make(dest, node->semantic_type ? node->semantic_type : "int", 0);
        free(label);
        free(ret);
        free(dest);
        return result;
    }
}

static Value value_make(const char *place, const char *type, int is_literal) {
    Value value;
    value.place = copy_string(place ? place : "0");
    value.type = copy_string(type ? type : "int");
    value.is_literal = is_literal;
    return value;
}

static void value_free(Value value) {
    free(value.place);
    free(value.type);
}

static char *literal_text(const ASTNode *node) {
    if (node && node->semantic_type && strcmp(node->semantic_type, "bool") == 0) {
        if (node->value && strcmp(node->value, "T") == 0) {
            return copy_string("1");
        }
        if (node->value && strcmp(node->value, "F") == 0) {
            return copy_string("0");
        }
    }
    return copy_string(node && node->value ? node->value : "0");
}

static int is_numeric_type(const char *type) {
    return type && (strcmp(type, "int") == 0 || strcmp(type, "float") == 0);
}

static int is_int_type(const char *type) {
    return type && strcmp(type, "int") == 0;
}

static int is_bool_type(const char *type) {
    return type && strcmp(type, "bool") == 0;
}

static int value_as_double(const Value *value, double *out) {
    char *end = NULL;
    double parsed;

    if (!value || !value->is_literal || !is_numeric_type(value->type)) {
        return 0;
    }

    parsed = strtod(value->place, &end);
    if (!end || *end != '\0') {
        return 0;
    }
    *out = parsed;
    return 1;
}

static int value_as_long(const Value *value, long *out) {
    char *end = NULL;
    long parsed;

    if (!value || !value->is_literal || (!is_int_type(value->type) && !is_bool_type(value->type))) {
        return 0;
    }

    parsed = strtol(value->place, &end, 10);
    if (!end || *end != '\0') {
        return 0;
    }
    *out = parsed;
    return 1;
}

static char *fold_unary(const char *op, const Value *operand, const char *type) {
    long int_value;
    double double_value;

    if (!op || !operand || !operand->is_literal) {
        return NULL;
    }

    if (strcmp(op, "!") == 0 && value_as_long(operand, &int_value)) {
        return copy_string(int_value == 0 ? "1" : "0");
    }

    if (strcmp(op, "-") == 0) {
        if (is_int_type(type) && value_as_long(operand, &int_value)) {
            return format_string("%ld", -int_value);
        }
        if (is_numeric_type(type) && value_as_double(operand, &double_value)) {
            return format_double(-double_value);
        }
    }

    return NULL;
}

static char *fold_binary(const char *op, const Value *left, const Value *right, const char *type) {
    long left_int;
    long right_int;
    double left_double;
    double right_double;

    if (!op || !left || !right || !left->is_literal || !right->is_literal) {
        return NULL;
    }

    if ((strcmp(op, "&") == 0 || strcmp(op, "|") == 0) &&
        value_as_long(left, &left_int) && value_as_long(right, &right_int)) {
        if (strcmp(op, "&") == 0) {
            return copy_string((left_int != 0 && right_int != 0) ? "1" : "0");
        }
        return copy_string((left_int != 0 || right_int != 0) ? "1" : "0");
    }

    if (is_bool_type(type) && value_as_double(left, &left_double) && value_as_double(right, &right_double)) {
        if (strcmp(op, "==") == 0) {
            return copy_string(left_double == right_double ? "1" : "0");
        }
        if (strcmp(op, "!=") == 0) {
            return copy_string(left_double != right_double ? "1" : "0");
        }
        if (strcmp(op, "<") == 0) {
            return copy_string(left_double < right_double ? "1" : "0");
        }
        if (strcmp(op, "<=") == 0) {
            return copy_string(left_double <= right_double ? "1" : "0");
        }
        if (strcmp(op, ">") == 0) {
            return copy_string(left_double > right_double ? "1" : "0");
        }
        if (strcmp(op, ">=") == 0) {
            return copy_string(left_double >= right_double ? "1" : "0");
        }
    }

    if (strcmp(op, "%") == 0 && value_as_long(left, &left_int) && value_as_long(right, &right_int)) {
        if (right_int == 0) {
            return NULL;
        }
        return format_string("%ld", left_int % right_int);
    }

    if (is_int_type(type) && value_as_long(left, &left_int) && value_as_long(right, &right_int)) {
        if (strcmp(op, "+") == 0) {
            return format_string("%ld", left_int + right_int);
        }
        if (strcmp(op, "-") == 0) {
            return format_string("%ld", left_int - right_int);
        }
        if (strcmp(op, "*") == 0) {
            return format_string("%ld", left_int * right_int);
        }
        if (strcmp(op, "/") == 0 && right_int != 0) {
            return format_string("%ld", left_int / right_int);
        }
    }

    if (is_numeric_type(type) && value_as_double(left, &left_double) && value_as_double(right, &right_double)) {
        if (strcmp(op, "+") == 0) {
            return format_double(left_double + right_double);
        }
        if (strcmp(op, "-") == 0) {
            return format_double(left_double - right_double);
        }
        if (strcmp(op, "*") == 0) {
            return format_double(left_double * right_double);
        }
        if (strcmp(op, "/") == 0 && right_double != 0.0) {
            return format_double(left_double / right_double);
        }
    }

    return NULL;
}

static char *format_double(double value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.12g", value);
    return copy_string(buffer);
}

static const char *fis_binary_op(const char *op) {
    static const struct {
        const char *source;
        const char *target;
    } ops[] = {
        {"+", "ADD"},
        {"-", "SUB"},
        {"*", "MUL"},
        {"/", "DIV"},
        {"%", "MOD"},
        {"==", "EQ"},
        {"!=", "NEQ"},
        {">", "GT"},
        {">=", "GTE"},
        {"<", "LT"},
        {"<=", "LTE"},
    };

    if (!op) {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        if (strcmp(op, ops[i].source) == 0) {
            return ops[i].target;
        }
    }
    return NULL;
}
