%{
#include "parser.tab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YY_DECL static int raw_yylex(void)

int lexer_error_count = 0;

static int current_line = 1;
static int current_column = 1;
static int pending_tokens[256];
static int pending_count = 0;
static int indent_stack[128] = {0};
static int indent_top = 0;
static int need_final_newline = 0;

static char *copy_text(const char *text);
static void update_location(void);
static int count_indent(const char *text);
static void enqueue_token(int token);
static int pop_pending_token(void);
static void handle_indent(int spaces);

#define YY_USER_INIT BEGIN(BOL);
#define YY_USER_ACTION update_location();
%}

%option noyywrap nounput noinput
%x BOL

DIGIT      [0-9]
ID_START   [A-Za-z_]
ID_CHAR    [A-Za-z0-9_]

%%

<BOL>[ \t]*#[^\n]*\n     { /* skip comment-only lines */ }
<BOL>[ \t]*\n            { /* skip blank lines */ }
<BOL>[ \t]+              {
                            int spaces = count_indent(yytext);
                            BEGIN(INITIAL);
                            handle_indent(spaces);
                            if (pending_count > 0) return pop_pending_token();
                         }
<BOL>[^ \t\n#]           {
                            yyless(0);
                            BEGIN(INITIAL);
                            handle_indent(0);
                            if (pending_count > 0) return pop_pending_token();
                         }

[ \t]+                   { /* skip intra-line whitespace */ }
#[^\n]*                  { /* skip trailing comments */ }
\n                       { BEGIN(BOL); return NEWLINE; }

"if"                     { return IF; }
"elif"                   { return ELIF; }
"else"                   { return ELSE; }
"wh"                     { return WH; }
"for"                    { return FOR; }
"in"                     { return IN; }
"fn"                     { return FN; }
"ret"                    { return RET; }

"int"                    { return TYPE_INT; }
"float"                  { return TYPE_FLOAT; }
"str"                    { return TYPE_STR; }
"bool"                   { return TYPE_BOOL; }

"T"                      { return TRUE; }
"F"                      { return FALSE; }

"=="                     { return EQ; }
"!="                     { return NEQ; }
"<="                     { return LTE; }
">="                     { return GTE; }
".."                     { return RANGE; }
"->"                     { return ARROW; }

"+"                      { return PLUS; }
"-"                      { return MINUS; }
"*"                      { return STAR; }
"/"                      { return SLASH; }
"%"                      { return MOD; }
"<"                      { return LT; }
">"                      { return GT; }
"&"                      { return AND; }
"|"                      { return OR; }
"!"                      { return NOT; }
"="                      { return ASSIGN; }
":"                      { return COLON; }
","                      { return COMMA; }
"("                      { return LPAREN; }
")"                      { return RPAREN; }

0{DIGIT}+("."{DIGIT}+)?  {
                            fprintf(stderr,
                                    "lexical error: numeric literal '%s' cannot have leading zeroes at line %d, column %d\n",
                                    yytext, yylloc.first_line, yylloc.first_column);
                            lexer_error_count++;
                         }
(0|[1-9]{DIGIT}*)"."{DIGIT}+ { yylval.text = copy_text(yytext); return FLOAT_LIT; }
(0|[1-9]{DIGIT}*)       { yylval.text = copy_text(yytext); return INT_LIT; }
\"([^\\\"]|\\.)*\"       { yylval.text = copy_text(yytext); return STRING_LIT; }
{ID_START}{ID_CHAR}*     { yylval.text = copy_text(yytext); return ID; }

.                        {
                            fprintf(stderr,
                                    "lexical error: unexpected character '%s' at line %d, column %d\n",
                                    yytext, yylloc.first_line, yylloc.first_column);
                            lexer_error_count++;
                         }

<<EOF>>                  {
                            if (need_final_newline) {
                                need_final_newline = 0;
                                BEGIN(BOL);
                                yylloc.first_line = current_line;
                                yylloc.first_column = current_column;
                                yylloc.last_line = current_line;
                                yylloc.last_column = current_column;
                                return NEWLINE;
                            }
                            if (indent_top > 0) {
                                indent_top--;
                                return DEDENT;
                            }
                            yyterminate();
                         }

%%

int yylex(void) {
    int token;

    if (pending_count > 0) {
        return pop_pending_token();
    }

    token = raw_yylex();
    if (token == NEWLINE) {
        need_final_newline = 0;
    } else if (token != 0 && token != INDENT && token != DEDENT) {
        need_final_newline = 1;
    }

    return token;
}

static char *copy_text(const char *text) {
    size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (!copy) {
        fprintf(stderr, "internal error: out of memory while copying token text\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void update_location(void) {
    int line = current_line;
    int column = current_column;

    yylloc.first_line = current_line;
    yylloc.first_column = current_column;

    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            line++;
            column = 1;
        } else if (yytext[i] == '\t') {
            column += 4;
        } else {
            column++;
        }
    }

    yylloc.last_line = line;
    yylloc.last_column = column > 1 ? column - 1 : 1;
    current_line = line;
    current_column = column;
}

static int count_indent(const char *text) {
    int spaces = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == ' ') {
            spaces++;
        } else if (text[i] == '\t') {
            spaces += 4;
        }
    }
    return spaces;
}

static void enqueue_token(int token) {
    if (pending_count >= (int)(sizeof(pending_tokens) / sizeof(pending_tokens[0]))) {
        fprintf(stderr, "internal error: too many pending indentation tokens\n");
        exit(EXIT_FAILURE);
    }
    pending_tokens[pending_count++] = token;
}

static int pop_pending_token(void) {
    int token = pending_tokens[0];
    for (int i = 1; i < pending_count; i++) {
        pending_tokens[i - 1] = pending_tokens[i];
    }
    pending_count--;
    return token;
}

static void handle_indent(int spaces) {
    int current_indent = indent_stack[indent_top];

    if (spaces > current_indent) {
        if (indent_top + 1 >= (int)(sizeof(indent_stack) / sizeof(indent_stack[0]))) {
            fprintf(stderr, "lexical error: indentation stack overflow at line %d\n", current_line);
            lexer_error_count++;
            return;
        }
        indent_stack[++indent_top] = spaces;
        enqueue_token(INDENT);
        return;
    }

    if (spaces < current_indent) {
        while (indent_top > 0 && spaces < indent_stack[indent_top]) {
            indent_top--;
            enqueue_token(DEDENT);
        }
        if (spaces != indent_stack[indent_top]) {
            fprintf(stderr,
                    "lexical error: inconsistent indentation at line %d, column %d\n",
                    current_line, current_column);
            lexer_error_count++;
        }
    }
}
