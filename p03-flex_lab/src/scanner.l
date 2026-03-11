%option noyywrap yylineno

%{
#include "scanner.h"
%}

%x COMMENT

ESC            \\.
EXP ([eE][+-]?[0-9]+)

ID [a-zA-Z_][a-zA-Z0-9_]*
INT (0|[1-9][0-9]*)
FLOAT (({INT}\.[0-9]*({EXP})?)|({INT}{EXP}))
CHAR   \'([^\\\n]|{ESC})\'
STRING \"([^\\\n]|{ESC})*\"
OCTAL        0[oO][0-7]+
HEXADECIMAL  0[xX][0-9a-fA-F]+


WS [ \t\r\n]+

%%

"/*"               { BEGIN(COMMENT); }
<COMMENT>"*/"      { BEGIN(INITIAL); }
<COMMENT>\n        { /* ignore, and yylineno gets updated */ }
<COMMENT>.         { /* ignore any other char */ }
<COMMENT><<EOF>>   { BEGIN(INITIAL); return TOK_ERROR; }
"//"[^\n]*         { /* ignore line comments */ }

{WS}               { /* ignore whitespaces */ }

"int"              { return TOK_KW_INT; }
"float"            { return TOK_KW_FLOAT; }
"double"           { return TOK_KW_DOUBLE; }
"char"             { return TOK_KW_CHAR; }
"void"             { return TOK_KW_VOID; }

"if"               { return TOK_KW_IF; }
"else"             { return TOK_KW_ELSE; }
"while"            { return TOK_KW_WHILE; }
"for"              { return TOK_KW_FOR; }
"return"           { return TOK_KW_RETURN; }
"break"            { return TOK_KW_BREAK; }
"continue"         { return TOK_KW_CONTINUE; }

"++"               { return TOK_INC; }
"--"               { return TOK_DEC; }
"+="               { return TOK_PLUS_ASSIGN; }
"-="               { return TOK_MINUS_ASSIGN; }
"*="               { return TOK_MUL_ASSIGN; }
"/="               { return TOK_DIV_ASSIGN; }
"%="               { return TOK_MOD_ASSIGN; }

"=="               { return TOK_EQ; }
"!="               { return TOK_NEQ; }
"<="               { return TOK_LE; }
">="               { return TOK_GE; }

"&&"               { return TOK_AND; }
"||"               { return TOK_OR; }

"="                { return TOK_ASSIGN; }

"<"                { return TOK_LT; }
">"                { return TOK_GT; }

"!"                { return TOK_NOT; }

"+"                { return TOK_PLUS; }
"-"                { return TOK_MINUS; }
"*"                { return TOK_MUL; }
"/"                { return TOK_DIV; }
"%"                { return TOK_MOD; }

"("                { return TOK_LPAREN; }
")"                { return TOK_RPAREN; }
"{"                { return TOK_LBRACE; }
"}"                { return TOK_RBRACE; }
"["                { return TOK_LBRACKET; }
"]"                { return TOK_RBRACKET; }
","                { return TOK_COMMA; }
";"                { return TOK_SEMICOLON; }

{FLOAT}            { return TOK_FLOAT_LITERAL; }
{INT}              { return TOK_INT_LITERAL; }
{CHAR}             { return TOK_CHAR_LITERAL; }
{STRING}           { return TOK_STRING_LITERAL; }
{OCTAL}            { return TOK_OCTAL_LITERAL; }
{HEXADECIMAL}      { return TOK_HEXADECIMAL_LITERAL; }
{ID}               { return TOK_IDENTIFIER; }

<<EOF>>            { return TOK_EOF; }
.                  { return TOK_ERROR; }
%%