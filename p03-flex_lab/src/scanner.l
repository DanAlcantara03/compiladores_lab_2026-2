/*
Plantilla guiada para el scanner de Flex.

Objetivo para el alumnado:
1) Completar cada bloque en este mismo orden.
2) Mantener los nombres de macros y tokens tal cual se indican.
3) Al finalizar, el archivo debe comportarse como el scanner final esperado.
*/

/*
SECCIÓN 1: Opciones globales de Flex
- Agrega la opción para NO requerir yywrap.
- Agrega la opción para llevar conteo automático de líneas.

Pista: la línea esperada usa dos opciones en una sola directiva %option.
*/

%option noyywrap yylineno

/*
SECCIÓN 2: Código C embebido
- Abre el bloque %{ ... %}.
- Incluye el header del scanner con #include "scanner.h".
- Cierra el bloque.
*/

%{
#include "scanner.h"
%}

/*
SECCIÓN 3: Estados léxicos
- Declara un estado exclusivo llamado COMMENT para comentarios de bloque.
*/

%x COMMENT

/*
SECCIÓN 4: Macros (definiciones de regex reutilizables)
- ID: identificadores tipo C (empieza con letra o _, luego letras/dígitos/_).
- INT: uno o más dígitos.
- FLOAT: número con punto opcionalmente con exponente, o entero con exponente.
- WS: espacios en blanco (espacio, tab, retorno de carro y salto de línea).
*/

ID [a-zA-Z_][a-zA-Z0-9_]*
INT (0|[1-9][0-9]*)
FLOAT (0|[1-9][0-9]*)\.[0-9]*
WS [ \t\r\n]+


/*
SECCIÓN 5: Inicio de reglas
- Coloca el separador %% para comenzar reglas.
*/
%%

/*
SECCIÓN 6: Comentarios
- Regla para iniciar comentario de bloque al leer "/*" (cambiar a estado COMMENT).
- En COMMENT, regla para "*/" (volver a INITIAL).
- En COMMENT, consumir \n sin acción (el conteo de línea se actualiza solo).
- En COMMENT, consumir cualquier otro carácter y descartarlo.
- En COMMENT, si llega EOF sin cerrar, devolver TOK_ERROR.
- Regla para comentario de línea: "//" hasta antes de \n, sin devolver token.
*/

"/*"               { BEGIN(COMMENT); }
<COMMENT>"*/"      { BEGIN(INITIAL); }
<COMMENT>\n        { /* ignore, and yylineno gets updated */ }
<COMMENT>.         { /* ignore any other char */ }
<COMMENT><<EOF>>   { return TOK_ERROR; }
"//"[^\n]*         { /* ignore line comments */ }

/*
SECCIÓN 7: Espacios en blanco
- Regla usando {WS} para ignorar whitespace.
*/
{WS}               { /* ignore whitespaces */ }" " 

/*
SECCIÓN 8: Palabras reservadas
- Agrega reglas exactas para:
	int, float, double, char, void,
	if, else, while, for, return, break, continue
- Cada una debe retornar su token TOK_KW_* correspondiente.
*/

/*
SECCIÓN 9: Operadores compuestos (siempre antes de los simples)
- ++, --, +=, -=, *=, /=, %=, ==, !=, <=, >=, &&, ||
- Devuelven los TOK_* correspondientes.
*/

/*
SECCIÓN 10: Operadores simples
- =, <, >, !, +, -, *, /, %
- Devuelven los TOK_* correspondientes.
*/

/*
SECCIÓN 11: Delimitadores
- (, ), {, }, [, ], ,, ;
- Devuelven los TOK_* correspondientes.
*/

/*
SECCIÓN 12: Literales e identificadores (orden importante)
- Primero FLOAT -> TOK_FLOAT_LITERAL.
- Luego INT -> TOK_INT_LITERAL.
- Luego literal de char (admitiendo escapes) -> TOK_CHAR_LITERAL.
- Luego literal de string (admitiendo escapes) -> TOK_STRING_LITERAL.
- Finalmente ID -> TOK_IDENTIFIER.

Nota: si pones ID antes de keywords, las keywords se reconocerán como identificadores.
*/

/*
SECCIÓN 13: Fin de archivo y error genérico
- Regla <<EOF>> que retorna TOK_EOF.
- Regla final con . que retorna TOK_ERROR para cualquier carácter no reconocido.
*/

/*
SECCIÓN 14: Cierre de reglas
- Coloca el separador %% final.
*/
%%