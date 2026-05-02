%code top {
    #ifndef YYLTYPE_IS_DECLARED
    typedef struct YYLTYPE {
        int first_line;
        int first_column;
        int last_line;
        int last_column;
    } YYLTYPE;
    #define YYLTYPE_IS_DECLARED 1
    #endif
}

%code requires {
    #ifndef YYLTYPE_IS_DECLARED
    typedef struct YYLTYPE {
        int first_line;
        int first_column;
        int last_line;
        int last_column;
    } YYLTYPE;
    #define YYLTYPE_IS_DECLARED 1
    #endif

    typedef struct ASTNode ASTNode;
    typedef struct ExprValue ExprValue;
    typedef struct ParamList ParamList;
    typedef struct ArgList ArgList;
}

%{
#include "ast.h"
#include "symtab.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ExprValue {
    ASTNode *node;
    char *type;
} ExprValue;

typedef struct Param {
    char *name;
    char *type;
    int line;
    int column;
    ASTNode *node;
} Param;

typedef struct ParamList {
    Param *items;
    size_t count;
    size_t capacity;
    ASTNode *node;
} ParamList;

typedef struct ArgList {
    char **types;
    size_t count;
    size_t capacity;
    ASTNode *node;
} ArgList;

int yylex(void);
void yyerror(const char *message);

extern int lexer_error_count;

static ASTNode *root = NULL;
static int semantic_error_count = 0;
static const char *current_return_type = NULL;
static const char *dot_output_path = NULL;

static char *copy_string(const char *text);
static void free_text(char *text);
static ASTNode *node_at(const char *kind, const char *value, YYLTYPE loc);
static ExprValue *expr_new(ASTNode *node, const char *type);
static void expr_release(ExprValue *expr);
static int is_error_type(const char *type);
static int is_numeric_type(const char *type);
static int is_assignable_type(const char *expected, const char *actual);
static char *infer_binary_type(const char *op, const char *left, const char *right, YYLTYPE loc);
static char *infer_unary_type(const char *op, const char *operand, YYLTYPE loc);
static void semantic_error_at(YYLTYPE loc, const char *format, ...);
static ASTNode *make_type_node(const char *type_name, YYLTYPE loc);
static ASTNode *make_decl_node(const char *name, const char *decl_type, ExprValue *expr, YYLTYPE loc);
static ASTNode *make_inferred_decl_node(const char *name, ExprValue *expr, YYLTYPE loc);
static ASTNode *make_assign_node(const char *name, ExprValue *expr, YYLTYPE loc);
static ASTNode *make_identifier_expr(const char *name, YYLTYPE loc, char **out_type);
static ParamList *params_new(void);
static void params_add(ParamList *params, const char *type, const char *name, YYLTYPE loc);
static void params_free(ParamList *params);
static ArgList *args_new(void);
static void args_add(ArgList *args, ExprValue *expr);
static void args_free(ArgList *args);
static void define_function_symbol(const char *name, ParamList *params, const char *return_type, YYLTYPE loc);
static void define_function_params(ParamList *params);
static ASTNode *make_function_node(const char *name, ParamList *params, const char *return_type,
                                   ASTNode *block, YYLTYPE loc);
static ExprValue *make_call_expr(const char *name, ArgList *args, YYLTYPE loc);
static ASTNode *make_return_node(ExprValue *expr, YYLTYPE loc);
static void syntax_error_expected_at(YYLTYPE loc, const char *expected, const char *got);
static const char *friendly_token_name(const char *token);
static void copy_message_token(const char *start, size_t length, char *target, size_t target_size);
static void print_expected_tokens(FILE *stream, const char *expected);
static void print_success_output(void);
static void cleanup_parser_state(void);
%}

%define parse.error detailed
%define parse.lac full
%locations

%union {
    char *text;
    ASTNode *node;
    ExprValue *expr;
    ParamList *params;
    ArgList *args;
}

%token IF ELIF ELSE WH FOR IN FN RET VAR
%token TYPE_INT TYPE_FLOAT TYPE_STR TYPE_BOOL
%token TRUE FALSE
%token INDENT DEDENT NEWLINE
%token PLUS MINUS STAR SLASH MOD
%token EQ NEQ LT LTE GT GTE
%token AND OR NOT
%token ASSIGN RANGE ARROW COLON COMMA LPAREN RPAREN
%token <text> ID INT_LIT FLOAT_LIT STRING_LIT

%type <node> program top_items top_item statement statement_list simple_statement
%type <node> compound_statement block declaration assignment if_statement elif_list else_clause
%type <node> while_statement for_statement function_decl return_statement
%type <expr> expression
%type <text> type
%type <params> params param_list
%type <args> args arg_list

%left OR
%left AND
%right NOT
%nonassoc EQ NEQ LT LTE GT GTE
%left PLUS MINUS
%left STAR SLASH MOD
%right UMINUS

%%

program:
    top_items {
        root = $1;
        $$ = root;
    }
    ;

top_items:
    /* empty */ {
        $$ = ast_new("Program", NULL, @$.first_line, @$.first_column);
    }
    | top_items top_item {
        ast_add_child($1, $2);
        $$ = $1;
    }
    ;

top_item:
    function_decl { $$ = $1; }
    | statement { $$ = $1; }
    ;

function_decl:
    FN ID LPAREN params RPAREN ARROW type COLON {
        define_function_symbol($2, $4, $7, @2);
        symtab_enter_scope();
        define_function_params($4);
        current_return_type = $7;
    } block {
        $$ = make_function_node($2, $4, $7, $10, @1);
        current_return_type = NULL;
        symtab_leave_scope();
        params_free($4);
        free_text($2);
        free_text($7);
    }
    | FN ID LPAREN params RPAREN ARROW type NEWLINE {
        syntax_error_expected_at(@8, "':'", "'newline'");
        params_free($4);
        free_text($2);
        free_text($7);
        YYABORT;
    }
    ;

params:
    /* empty */ { $$ = params_new(); }
    | param_list { $$ = $1; }
    ;

param_list:
    type ID {
        $$ = params_new();
        params_add($$, $1, $2, @2);
        free_text($1);
        free_text($2);
    }
    | param_list COMMA type ID {
        params_add($1, $3, $4, @4);
        $$ = $1;
        free_text($3);
        free_text($4);
    }
    ;

type:
    TYPE_INT { $$ = copy_string("int"); }
    | TYPE_FLOAT { $$ = copy_string("float"); }
    | TYPE_STR { $$ = copy_string("str"); }
    | TYPE_BOOL { $$ = copy_string("bool"); }
    ;

statement_list:
    statement {
        $$ = ast_new("Statements", NULL, @1.first_line, @1.first_column);
        ast_add_child($$, $1);
    }
    | statement_list statement {
        ast_add_child($1, $2);
        $$ = $1;
    }
    ;

statement:
    simple_statement NEWLINE { $$ = $1; }
    | compound_statement { $$ = $1; }
    ;

simple_statement:
    declaration { $$ = $1; }
    | assignment { $$ = $1; }
    | return_statement { $$ = $1; }
    | expression {
        $$ = ast_new("ExpressionStatement", NULL, @1.first_line, @1.first_column);
        ast_add_child($$, $1->node);
        expr_release($1);
    }
    ;

compound_statement:
    if_statement { $$ = $1; }
    | while_statement { $$ = $1; }
    | for_statement { $$ = $1; }
    ;

block:
    NEWLINE INDENT {
        symtab_enter_scope();
    } statement_list DEDENT {
        $$ = ast_new("Block", NULL, @2.first_line, @2.first_column);
        ast_add_child($$, $4);
        symtab_leave_scope();
    }
    ;

declaration:
    type ID ASSIGN expression {
        $$ = make_decl_node($2, $1, $4, @2);
        free_text($1);
        free_text($2);
        expr_release($4);
    }
    | VAR ID ASSIGN expression {
        $$ = make_inferred_decl_node($2, $4, @2);
        free_text($2);
        expr_release($4);
    }
    ;

assignment:
    ID ASSIGN expression {
        $$ = make_assign_node($1, $3, @1);
        free_text($1);
        expr_release($3);
    }
    ;

return_statement:
    RET expression {
        $$ = make_return_node($2, @1);
        expr_release($2);
    }
    ;

if_statement:
    IF expression COLON block elif_list else_clause {
        if (!is_error_type($2->type) && strcmp($2->type, "bool") != 0) {
            semantic_error_at(@2, "if condition must be bool");
        }
        $$ = ast_new("If", NULL, @1.first_line, @1.first_column);
        ast_add_child($$, $2->node);
        ast_add_child($$, $4);
        ast_add_child($$, $5);
        ast_add_child($$, $6);
        expr_release($2);
    }
    | IF expression NEWLINE {
        syntax_error_expected_at(@3, "':'", "'newline'");
        expr_release($2);
        YYABORT;
    }
    ;

elif_list:
    /* empty */ {
        $$ = ast_new("ElifList", NULL, @$.first_line, @$.first_column);
    }
    | elif_list ELIF expression COLON block {
        ASTNode *elif_node = ast_new("Elif", NULL, @2.first_line, @2.first_column);
        if (!is_error_type($3->type) && strcmp($3->type, "bool") != 0) {
            semantic_error_at(@3, "elif condition must be bool");
        }
        ast_add_child(elif_node, $3->node);
        ast_add_child(elif_node, $5);
        ast_add_child($1, elif_node);
        $$ = $1;
        expr_release($3);
    }
    | elif_list ELIF expression NEWLINE {
        syntax_error_expected_at(@4, "':'", "'newline'");
        ast_free($1);
        expr_release($3);
        YYABORT;
    }
    ;

else_clause:
    /* empty */ { $$ = NULL; }
    | ELSE COLON block {
        $$ = ast_new("Else", NULL, @1.first_line, @1.first_column);
        ast_add_child($$, $3);
    }
    | ELSE NEWLINE {
        syntax_error_expected_at(@2, "':'", "'newline'");
        YYABORT;
    }
    ;

while_statement:
    WH expression COLON block {
        if (!is_error_type($2->type) && strcmp($2->type, "bool") != 0) {
            semantic_error_at(@2, "wh condition must be bool");
        }
        $$ = ast_new("While", NULL, @1.first_line, @1.first_column);
        ast_add_child($$, $2->node);
        ast_add_child($$, $4);
        expr_release($2);
    }
    | WH expression NEWLINE {
        syntax_error_expected_at(@3, "':'", "'newline'");
        expr_release($2);
        YYABORT;
    }
    ;

for_statement:
    FOR ID IN expression RANGE expression COLON {
        Symbol *symbol = symtab_lookup($2);
        if (!is_error_type($4->type) && strcmp($4->type, "int") != 0) {
            semantic_error_at(@4, "for range start must be int");
        }
        if (!is_error_type($6->type) && strcmp($6->type, "int") != 0) {
            semantic_error_at(@6, "for range end must be int");
        }
        if (symbol && strcmp(symbol->type, "int") != 0) {
            semantic_error_at(@2, "for variable '%s' must be int", $2);
        } else if (!symbol) {
            Symbol loop_symbol = {0};
            loop_symbol.name = $2;
            loop_symbol.type = "int";
            loop_symbol.category = SYMBOL_VARIABLE;
            loop_symbol.decl_line = @2.first_line;
            loop_symbol.decl_column = @2.first_column;
            if (!symtab_define(loop_symbol, 0)) {
                semantic_error_at(@2, "variable '%s' is already declared in this scope", $2);
            }
        }
    } block {
        ASTNode *range_node = ast_new("Range", NULL, @5.first_line, @5.first_column);
        $$ = ast_new("ForRange", $2, @1.first_line, @1.first_column);
        ast_add_child(range_node, $4->node);
        ast_add_child(range_node, $6->node);
        ast_add_child($$, range_node);
        ast_add_child($$, $9);
        free_text($2);
        expr_release($4);
        expr_release($6);
    }
    | FOR ID IN expression RANGE expression NEWLINE {
        syntax_error_expected_at(@7, "':'", "'newline'");
        free_text($2);
        expr_release($4);
        expr_release($6);
        YYABORT;
    }
    ;

expression:
    INT_LIT {
        $$ = expr_new(node_at("Literal", $1, @1), "int");
        free_text($1);
    }
    | FLOAT_LIT {
        $$ = expr_new(node_at("Literal", $1, @1), "float");
        free_text($1);
    }
    | STRING_LIT {
        $$ = expr_new(node_at("Literal", $1, @1), "str");
        free_text($1);
    }
    | TRUE {
        $$ = expr_new(node_at("Literal", "T", @1), "bool");
    }
    | FALSE {
        $$ = expr_new(node_at("Literal", "F", @1), "bool");
    }
    | ID {
        char *type = NULL;
        ASTNode *identifier = make_identifier_expr($1, @1, &type);
        $$ = expr_new(identifier, type);
        free_text(type);
        free_text($1);
    }
    | ID LPAREN args RPAREN {
        $$ = make_call_expr($1, $3, @1);
        free_text($1);
        args_free($3);
    }
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    | MINUS expression %prec UMINUS {
        char *type = infer_unary_type("-", $2->type, @1);
        $$ = expr_new(node_at("UnaryExpr", "-", @1), type);
        ast_add_child($$->node, $2->node);
        free_text(type);
        expr_release($2);
    }
    | NOT expression {
        char *type = infer_unary_type("!", $2->type, @1);
        $$ = expr_new(node_at("UnaryExpr", "!", @1), type);
        ast_add_child($$->node, $2->node);
        free_text(type);
        expr_release($2);
    }
    | expression PLUS expression {
        char *type = infer_binary_type("+", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "+", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression MINUS expression {
        char *type = infer_binary_type("-", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "-", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression STAR expression {
        char *type = infer_binary_type("*", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "*", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression SLASH expression {
        char *type = infer_binary_type("/", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "/", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression MOD expression {
        char *type = infer_binary_type("%", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "%", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression EQ expression {
        char *type = infer_binary_type("==", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "==", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression NEQ expression {
        char *type = infer_binary_type("!=", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "!=", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression LT expression {
        char *type = infer_binary_type("<", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "<", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression LTE expression {
        char *type = infer_binary_type("<=", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "<=", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression GT expression {
        char *type = infer_binary_type(">", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", ">", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression GTE expression {
        char *type = infer_binary_type(">=", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", ">=", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression AND expression {
        char *type = infer_binary_type("&", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "&", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    | expression OR expression {
        char *type = infer_binary_type("|", $1->type, $3->type, @2);
        $$ = expr_new(node_at("BinaryExpr", "|", @2), type);
        ast_add_child($$->node, $1->node);
        ast_add_child($$->node, $3->node);
        free_text(type);
        expr_release($1);
        expr_release($3);
    }
    ;

args:
    /* empty */ { $$ = args_new(); }
    | arg_list { $$ = $1; }
    ;

arg_list:
    expression {
        $$ = args_new();
        args_add($$, $1);
        expr_release($1);
    }
    | arg_list COMMA expression {
        args_add($1, $3);
        $$ = $1;
        expr_release($3);
    }
    ;

%%

void yyerror(const char *message) {
    const char *unexpected;
    const char *expecting;
    char got[64];

    if (lexer_error_count > 0) {
        return;
    }

    unexpected = strstr(message, "unexpected ");
    expecting = strstr(message, ", expecting ");
    if (unexpected) {
        const char *got_start = unexpected + strlen("unexpected ");
        const char *got_end = strchr(got_start, ',');
        if (!got_end) {
            got_end = got_start + strlen(got_start);
        }
        copy_message_token(got_start, (size_t)(got_end - got_start), got, sizeof(got));

        if (expecting) {
            fprintf(stderr, "syntax error: expected ");
            print_expected_tokens(stderr, expecting + strlen(", expecting "));
            fprintf(stderr, " but got %s at line %d, column %d\n",
                    friendly_token_name(got), yylloc.first_line, yylloc.first_column);
        } else {
            fprintf(stderr, "syntax error: unexpected %s at line %d, column %d\n",
                    friendly_token_name(got), yylloc.first_line, yylloc.first_column);
        }
        return;
    }

    if (strncmp(message, "syntax error, ", strlen("syntax error, ")) == 0) {
        message += strlen("syntax error, ");
    } else if (strcmp(message, "syntax error") == 0) {
        message = "invalid syntax";
    }

    fprintf(stderr, "syntax error: %s at line %d, column %d\n",
            message, yylloc.first_line, yylloc.first_column);
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
        fprintf(stderr, "internal error: out of memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void free_text(char *text) {
    free(text);
}

static ASTNode *node_at(const char *kind, const char *value, YYLTYPE loc) {
    return ast_new(kind, value, loc.first_line, loc.first_column);
}

static ExprValue *expr_new(ASTNode *node, const char *type) {
    ExprValue *expr = (ExprValue *)calloc(1, sizeof(ExprValue));
    if (!expr) {
        fprintf(stderr, "internal error: out of memory\n");
        exit(EXIT_FAILURE);
    }
    expr->node = node;
    expr->type = copy_string(type ? type : "error");
    ast_set_type(expr->node, expr->type);
    return expr;
}

static void expr_release(ExprValue *expr) {
    if (!expr) {
        return;
    }
    free(expr->type);
    free(expr);
}

static int is_error_type(const char *type) {
    return !type || strcmp(type, "error") == 0;
}

static int is_numeric_type(const char *type) {
    return type && (strcmp(type, "int") == 0 || strcmp(type, "float") == 0);
}

static int is_assignable_type(const char *expected, const char *actual) {
    if (is_error_type(expected) || is_error_type(actual)) {
        return 1;
    }
    if (strcmp(expected, actual) == 0) {
        return 1;
    }
    return strcmp(expected, "float") == 0 && strcmp(actual, "int") == 0;
}

static char *infer_binary_type(const char *op, const char *left, const char *right, YYLTYPE loc) {
    if (is_error_type(left) || is_error_type(right)) {
        return copy_string("error");
    }

    if (strcmp(op, "&") == 0 || strcmp(op, "|") == 0) {
        if (strcmp(left, "bool") != 0 || strcmp(right, "bool") != 0) {
            semantic_error_at(loc, "operator '%s' requires bool operands", op);
            return copy_string("error");
        }
        return copy_string("bool");
    }

    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        if (!is_assignable_type(left, right) && !is_assignable_type(right, left)) {
            semantic_error_at(loc, "operator '%s' requires compatible operands", op);
            return copy_string("error");
        }
        return copy_string("bool");
    }

    if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
        strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
        if (!is_numeric_type(left) || !is_numeric_type(right)) {
            semantic_error_at(loc, "operator '%s' requires numeric operands", op);
            return copy_string("error");
        }
        return copy_string("bool");
    }

    if (strcmp(op, "%") == 0) {
        if (strcmp(left, "int") != 0 || strcmp(right, "int") != 0) {
            semantic_error_at(loc, "operator '%%' requires int operands");
            return copy_string("error");
        }
        return copy_string("int");
    }

    if (!is_numeric_type(left) || !is_numeric_type(right)) {
        semantic_error_at(loc, "operator '%s' requires numeric operands", op);
        return copy_string("error");
    }

    if (strcmp(left, "float") == 0 || strcmp(right, "float") == 0) {
        return copy_string("float");
    }
    return copy_string("int");
}

static char *infer_unary_type(const char *op, const char *operand, YYLTYPE loc) {
    if (is_error_type(operand)) {
        return copy_string("error");
    }
    if (strcmp(op, "!") == 0) {
        if (strcmp(operand, "bool") != 0) {
            semantic_error_at(loc, "operator '!' requires bool operand");
            return copy_string("error");
        }
        return copy_string("bool");
    }
    if (strcmp(op, "-") == 0) {
        if (!is_numeric_type(operand)) {
            semantic_error_at(loc, "operator '-' requires numeric operand");
            return copy_string("error");
        }
        return copy_string(operand);
    }
    return copy_string("error");
}

static void semantic_error_at(YYLTYPE loc, const char *format, ...) {
    va_list args;

    fprintf(stderr, "semantic error: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, " at line %d, column %d\n", loc.first_line, loc.first_column);
    semantic_error_count++;
}

static ASTNode *make_type_node(const char *type_name, YYLTYPE loc) {
    return ast_new("Type", type_name, loc.first_line, loc.first_column);
}

static ASTNode *make_decl_node(const char *name, const char *decl_type, ExprValue *expr, YYLTYPE loc) {
    Symbol symbol = {0};
    ASTNode *decl = ast_new("VarDecl", name, loc.first_line, loc.first_column);

    symbol.name = (char *)name;
    symbol.type = (char *)decl_type;
    symbol.category = SYMBOL_VARIABLE;
    symbol.decl_line = loc.first_line;
    symbol.decl_column = loc.first_column;

    if (symtab_exists_current(name)) {
        semantic_error_at(loc, "variable '%s' is already declared in this scope", name);
    } else if (!symtab_define(symbol, 0)) {
        semantic_error_at(loc, "variable '%s' is already declared in this scope", name);
    }

    if (!is_assignable_type(decl_type, expr->type)) {
        semantic_error_at(loc, "cannot assign %s to %s", expr->type, decl_type);
    }

    ast_add_child(decl, make_type_node(decl_type, loc));
    ast_add_child(decl, expr->node);
    ast_set_type(decl, decl_type);
    return decl;
}

static ASTNode *make_inferred_decl_node(const char *name, ExprValue *expr, YYLTYPE loc) {
    Symbol symbol = {0};
    ASTNode *decl = ast_new("InferredVarDecl", name, loc.first_line, loc.first_column);

    symbol.name = (char *)name;
    symbol.type = expr->type;
    symbol.category = SYMBOL_VARIABLE;
    symbol.decl_line = loc.first_line;
    symbol.decl_column = loc.first_column;

    /* Invalid initializers keep the AST decorated but avoid defining a broken symbol. */
    if (symtab_exists_current(name)) {
        semantic_error_at(loc, "variable '%s' is already declared in this scope", name);
    } else if (!is_error_type(expr->type) && !symtab_define(symbol, 0)) {
        semantic_error_at(loc, "variable '%s' is already declared in this scope", name);
    }

    ast_add_child(decl, make_type_node(expr->type, loc));
    ast_add_child(decl, expr->node);
    ast_set_type(decl, expr->type);
    return decl;
}

static ASTNode *make_assign_node(const char *name, ExprValue *expr, YYLTYPE loc) {
    Symbol *symbol = symtab_lookup(name);
    ASTNode *assign = ast_new("Assign", name, loc.first_line, loc.first_column);

    if (!symbol) {
        Symbol inferred = {0};
        inferred.name = (char *)name;
        inferred.type = expr->type;
        inferred.category = SYMBOL_VARIABLE;
        inferred.decl_line = loc.first_line;
        inferred.decl_column = loc.first_column;
        if (!is_error_type(expr->type) && !symtab_define(inferred, 0)) {
            semantic_error_at(loc, "variable '%s' is already declared in this scope", name);
        }
        ast_set_type(assign, expr->type);
    } else if (symbol->category == SYMBOL_FUNCTION) {
        semantic_error_at(loc, "cannot assign to function '%s'", name);
        ast_set_type(assign, "error");
    } else {
        if (!is_assignable_type(symbol->type, expr->type)) {
            semantic_error_at(loc, "cannot assign %s to %s", expr->type, symbol->type);
        }
        ast_set_type(assign, symbol->type);
    }

    ast_add_child(assign, expr->node);
    return assign;
}

static ASTNode *make_identifier_expr(const char *name, YYLTYPE loc, char **out_type) {
    Symbol *symbol = symtab_lookup(name);
    ASTNode *identifier = ast_new("Identifier", name, loc.first_line, loc.first_column);

    if (!symbol) {
        semantic_error_at(loc, "variable '%s' is not declared", name);
        *out_type = copy_string("error");
    } else if (symbol->category == SYMBOL_FUNCTION) {
        semantic_error_at(loc, "function '%s' must be called with parentheses", name);
        *out_type = copy_string("error");
    } else {
        symtab_mark_read(symbol);
        *out_type = copy_string(symbol->type);
    }

    ast_set_type(identifier, *out_type);
    return identifier;
}

static ParamList *params_new(void) {
    ParamList *params = (ParamList *)calloc(1, sizeof(ParamList));
    if (!params) {
        fprintf(stderr, "internal error: out of memory\n");
        exit(EXIT_FAILURE);
    }
    params->node = ast_new("Params", NULL, 0, 0);
    return params;
}

static void params_add(ParamList *params, const char *type, const char *name, YYLTYPE loc) {
    Param *param;

    if (params->count == params->capacity) {
        size_t next_capacity = params->capacity == 0 ? 4 : params->capacity * 2;
        Param *next_items = (Param *)realloc(params->items, next_capacity * sizeof(Param));
        if (!next_items) {
            fprintf(stderr, "internal error: out of memory\n");
            exit(EXIT_FAILURE);
        }
        params->items = next_items;
        params->capacity = next_capacity;
    }

    param = &params->items[params->count++];
    param->name = copy_string(name);
    param->type = copy_string(type);
    param->line = loc.first_line;
    param->column = loc.first_column;
    param->node = ast_new("Param", name, loc.first_line, loc.first_column);
    ast_add_child(param->node, make_type_node(type, loc));
    ast_set_type(param->node, type);
    ast_add_child(params->node, param->node);
}

static void params_free(ParamList *params) {
    if (!params) {
        return;
    }
    for (size_t i = 0; i < params->count; i++) {
        free(params->items[i].name);
        free(params->items[i].type);
    }
    free(params->items);
    free(params);
}

static ArgList *args_new(void) {
    ArgList *args = (ArgList *)calloc(1, sizeof(ArgList));
    if (!args) {
        fprintf(stderr, "internal error: out of memory\n");
        exit(EXIT_FAILURE);
    }
    args->node = ast_new("Args", NULL, 0, 0);
    return args;
}

static void args_add(ArgList *args, ExprValue *expr) {
    if (args->count == args->capacity) {
        size_t next_capacity = args->capacity == 0 ? 4 : args->capacity * 2;
        char **next_types = (char **)realloc(args->types, next_capacity * sizeof(char *));
        if (!next_types) {
            fprintf(stderr, "internal error: out of memory\n");
            exit(EXIT_FAILURE);
        }
        args->types = next_types;
        args->capacity = next_capacity;
    }

    args->types[args->count++] = copy_string(expr->type);
    ast_add_child(args->node, expr->node);
}

static void args_free(ArgList *args) {
    if (!args) {
        return;
    }
    for (size_t i = 0; i < args->count; i++) {
        free(args->types[i]);
    }
    free(args->types);
    free(args);
}

static void define_function_symbol(const char *name, ParamList *params, const char *return_type, YYLTYPE loc) {
    Symbol symbol = {0};
    char **param_types = NULL;

    if (params->count > 0) {
        param_types = (char **)calloc(params->count, sizeof(char *));
        if (!param_types) {
            fprintf(stderr, "internal error: out of memory\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < params->count; i++) {
            param_types[i] = params->items[i].type;
        }
    }

    symbol.name = (char *)name;
    symbol.type = (char *)return_type;
    symbol.category = SYMBOL_FUNCTION;
    symbol.decl_line = loc.first_line;
    symbol.decl_column = loc.first_column;
    symbol.param_types = param_types;
    symbol.param_count = params->count;
    symbol.return_type = (char *)return_type;

    if (symtab_exists_current(name)) {
        semantic_error_at(loc, "function '%s' is already declared in this scope", name);
    } else if (!symtab_define(symbol, 0)) {
        semantic_error_at(loc, "function '%s' is already declared in this scope", name);
    }

    free(param_types);
}

static void define_function_params(ParamList *params) {
    for (size_t i = 0; i < params->count; i++) {
        Symbol symbol = {0};
        YYLTYPE loc = {params->items[i].line, params->items[i].column,
                       params->items[i].line, params->items[i].column};
        symbol.name = params->items[i].name;
        symbol.type = params->items[i].type;
        symbol.category = SYMBOL_VARIABLE;
        symbol.decl_line = params->items[i].line;
        symbol.decl_column = params->items[i].column;

        if (!symtab_define(symbol, 0)) {
            semantic_error_at(loc, "parameter '%s' is already declared in this scope", params->items[i].name);
        }
    }
}

static ASTNode *make_function_node(const char *name, ParamList *params, const char *return_type,
                                   ASTNode *block, YYLTYPE loc) {
    ASTNode *function = ast_new("FunctionDecl", name, loc.first_line, loc.first_column);
    ast_set_type(function, return_type);
    ast_add_child(function, make_type_node(return_type, loc));
    ast_add_child(function, params->node);
    ast_add_child(function, block);
    return function;
}

static ExprValue *make_call_expr(const char *name, ArgList *args, YYLTYPE loc) {
    Symbol *symbol = symtab_lookup(name);
    ASTNode *call = ast_new("Call", name, loc.first_line, loc.first_column);
    const char *return_type = "error";

    if (!symbol) {
        semantic_error_at(loc, "function '%s' is not declared", name);
    } else if (symbol->category != SYMBOL_FUNCTION) {
        semantic_error_at(loc, "'%s' is not a function", name);
    } else {
        return_type = symbol->return_type;
        if (symbol->param_count != args->count) {
            semantic_error_at(loc, "function '%s' expects %zu arguments but got %zu",
                              name, symbol->param_count, args->count);
        } else {
            for (size_t i = 0; i < args->count; i++) {
                if (!is_assignable_type(symbol->param_types[i], args->types[i])) {
                    semantic_error_at(loc, "argument %zu of function '%s' expects %s but got %s",
                                      i + 1, name, symbol->param_types[i], args->types[i]);
                }
            }
        }
    }

    ast_add_child(call, args->node);
    return expr_new(call, return_type);
}

static ASTNode *make_return_node(ExprValue *expr, YYLTYPE loc) {
    ASTNode *ret = ast_new("Return", "ret", loc.first_line, loc.first_column);

    if (!current_return_type) {
        semantic_error_at(loc, "ret can only be used inside a function");
    } else if (!is_assignable_type(current_return_type, expr->type)) {
        semantic_error_at(loc, "cannot return %s from function returning %s",
                          expr->type, current_return_type);
    }

    ast_add_child(ret, expr->node);
    ast_set_type(ret, current_return_type ? current_return_type : "error");
    return ret;
}

static void syntax_error_expected_at(YYLTYPE loc, const char *expected, const char *got) {
    fprintf(stderr, "syntax error: expected %s but got %s at line %d, column %d\n",
            expected, got, loc.first_line, loc.first_column);
}

static const char *friendly_token_name(const char *token) {
    static const struct {
        const char *raw;
        const char *friendly;
    } token_names[] = {
        {"ID", "'identifier'"},
        {"INT_LIT", "'number'"},
        {"FLOAT_LIT", "'number'"},
        {"STRING_LIT", "'string'"},
        {"TRUE", "'T'"},
        {"FALSE", "'F'"},
        {"IF", "'if'"},
        {"ELIF", "'elif'"},
        {"ELSE", "'else'"},
        {"WH", "'wh'"},
        {"FOR", "'for'"},
        {"IN", "'in'"},
        {"FN", "'fn'"},
        {"RET", "'ret'"},
        {"VAR", "'var'"},
        {"TYPE_INT", "'int'"},
        {"TYPE_FLOAT", "'float'"},
        {"TYPE_STR", "'str'"},
        {"TYPE_BOOL", "'bool'"},
        {"INDENT", "'indentation'"},
        {"DEDENT", "'dedentation'"},
        {"NEWLINE", "'newline'"},
        {"PLUS", "'+'"},
        {"MINUS", "'-'"},
        {"STAR", "'*'"},
        {"SLASH", "'/'"},
        {"MOD", "'%'"},
        {"EQ", "'=='"},
        {"NEQ", "'!='"},
        {"LT", "'<'"},
        {"LTE", "'<='"},
        {"GT", "'>'"},
        {"GTE", "'>='"},
        {"AND", "'&'"},
        {"OR", "'|'"},
        {"NOT", "'!'"},
        {"ASSIGN", "'='"},
        {"RANGE", "'..'"},
        {"ARROW", "'->'"},
        {"COLON", "':'"},
        {"COMMA", "','"},
        {"LPAREN", "'('"},
        {"RPAREN", "')'"},
        {"end of file", "end of file"},
    };

    if (!token) {
        return "unknown token";
    }

    for (size_t i = 0; i < sizeof(token_names) / sizeof(token_names[0]); i++) {
        if (strcmp(token, token_names[i].raw) == 0) {
            return token_names[i].friendly;
        }
    }
    return token;
}

static void copy_message_token(const char *start, size_t length, char *target, size_t target_size) {
    if (target_size == 0) {
        return;
    }
    if (length >= target_size) {
        length = target_size - 1;
    }
    memcpy(target, start, length);
    target[length] = '\0';
}

static void print_expected_tokens(FILE *stream, const char *expected) {
    const char *cursor = expected;
    int count = 0;

    while (cursor && *cursor != '\0') {
        const char *separator = strstr(cursor, " or ");
        const char *end = separator ? separator : cursor + strlen(cursor);
        char token[64];

        copy_message_token(cursor, (size_t)(end - cursor), token, sizeof(token));
        if (count > 0) {
            fprintf(stream, " or ");
        }
        fprintf(stream, "%s", friendly_token_name(token));
        count++;

        cursor = separator ? separator + strlen(" or ") : NULL;
    }
}

static void print_success_output(void) {
    if (!root) {
        return;
    }

    printf("Analysis success. AST:\n");
    ast_print(root, 0);
    if (dot_output_path) {
        if (ast_write_dot(root, dot_output_path) == 0) {
            printf("DOT written to %s\n", dot_output_path);
        }
    }
}

static void cleanup_parser_state(void) {
    ast_free(root);
    root = NULL;
    symtab_free();
}

int main(int argc, char **argv) {
    int parse_result;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dot") == 0 && i + 1 < argc) {
            dot_output_path = argv[++i];
        } else {
            fprintf(stderr, "usage: %s [--dot ast.dot] < source.summ\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    symtab_init();
    parse_result = yyparse();

    if (parse_result == 0 && lexer_error_count == 0 && semantic_error_count == 0) {
        print_success_output();
        cleanup_parser_state();
        return EXIT_SUCCESS;
    }

    cleanup_parser_state();
    return EXIT_FAILURE;
}
