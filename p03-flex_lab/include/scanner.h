#ifndef SCANNER_H
#define SCANNER_H

/* Tokens produced by the Flex scanner. */
typedef enum ScannerToken {
	TOK_EOF = 0,
	TOK_ERROR = 256,

	/* Keywords */
	TOK_KW_INT,
	TOK_KW_FLOAT,
	TOK_KW_DOUBLE,
	TOK_KW_CHAR,
	TOK_KW_VOID,
	TOK_KW_IF,
	TOK_KW_ELSE,
	TOK_KW_WHILE,
	TOK_KW_FOR,
	TOK_KW_RETURN,
	TOK_KW_BREAK,
	TOK_KW_CONTINUE,

	/* Identifiers and literals */
	TOK_IDENTIFIER,
	TOK_INT_LITERAL,
	TOK_FLOAT_LITERAL,
	TOK_STRING_LITERAL,
	TOK_CHAR_LITERAL,
	TOK_OCTAL_LITERAL,
	TOK_HEXADECIMAL_LITERAL,

	/* Preprocessor directives */
	TOK_PP_INCLUDE,
	TOK_PP_DEFINE,

	/* Compound operators */
	TOK_INC,
	TOK_DEC,
	TOK_PLUS_ASSIGN,
	TOK_MINUS_ASSIGN,
	TOK_MUL_ASSIGN,
	TOK_DIV_ASSIGN,
	TOK_MOD_ASSIGN,
	TOK_ASSIGN,

	/* Comparison operators */
	TOK_EQ,
	TOK_NEQ,
	TOK_LT,
	TOK_LE,
	TOK_GT,
	TOK_GE,

	/* Logical operators */
	TOK_AND,
	TOK_OR,
	TOK_NOT,

	/* Arithmetic operators */
	TOK_PLUS,
	TOK_MINUS,
	TOK_MUL,
	TOK_DIV,
	TOK_MOD,

	/* Delimiters */
	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_LBRACKET,
	TOK_RBRACKET,
	TOK_COMMA,
	TOK_SEMICOLON
} ScannerToken;

/* Returns a printable name for a token id. */
const char *scanner_token_name(int token);

/* Current scanner position and token start position (line/column). */
extern int scanner_line;
extern int scanner_col;
extern int scanner_token_line;
extern int scanner_token_col;

#endif // SCANNER_H