%{
#include "ast.h"
#include "parser.tab.h"
#include <string.h>

extern int yylineno;
void yyerror(const char *s);

// Identificadores estándar
%}

%option noyywrap
%option yylineno

%%

"quackint"          { return QUACKINT; }
"quackle"           { return QUACKLE; }
"quacks"            { return QUACKS; }
"pond"              { return POND; }
"quack_return"      { return QUACK_RETURN; }

"quack?"            { return IF; }
"quack??"           { return ELSIF; }
"quack???"          { return ELSE; }
"quack!"            { return WHILE; }
"qu@ck"             { return FOR; }

"(^)>"              { return PATO_START; }
"<(^)"              { return PATO_END; }

"(+)>"              { return PLUS; }
"(-)>"              { return MINUS; }
"(*)>"              { return MULT; }
"(/)>"              { return DIV; }
"(%)>"              { return MOD; }
"(==)>"             { return EQ; }
"(!=)>"             { return NEQ; }
"(>)>"              { return GT; }
"(<)>"              { return LT; }

"="                 { return ASSIGN; }
";"                 { return SEMI; }
","                 { return COMMA; }
"("                 { return LPAREN; }
")"                 { return RPAREN; }

[0-9]+\.[0-9]+      { yylval.sval = strdup(yytext); return FLOAT_LIT; }
[0-9]+              { yylval.sval = strdup(yytext); return INT_LIT; }
\"[^\"]*\"          { yylval.sval = strdup(yytext); return STRING_LIT; }

0x[0-9A-Fa-f]+         { yylval.sval = strdup(yytext); return INT_LIT; }
[a-zA-Z_][a-zA-Z0-9_]*  { yylval.sval = strdup(yytext); return ID; }

"//".*              { /* skip comments */ }
[ \t\n\r]           { /* ignorar espacios */ }
.                   { printf("Error léxico: caracter plumífero desconocido '%s' en línea %d\n", yytext, yylineno); }

%%