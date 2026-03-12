#include <assert.h>
#include <string.h>

#include "scanner.h"

/** @brief Stub for scanner main dependency while testing token names. */
int yylex(void)
{
	return TOK_EOF;
}

/** @brief Stub lexeme pointer required by scanner.c linkage. */
char *yytext = "";

/**
 * @brief Asserts that a token maps to the expected printable name.
 */
static void expect_name(int token, const char *expected)
{
	assert(strcmp(scanner_token_name(token), expected) == 0);
}

/**
 * @brief Runs token-name mapping tests.
 * @return 0 on success.
 */
int main(void)
{
	expect_name(TOK_EOF, "EOF");
	expect_name(TOK_ERROR, "ERROR");
	expect_name(TOK_KW_INT, "KW_INT");
	expect_name(TOK_IDENTIFIER, "IDENTIFIER");
	expect_name(TOK_FLOAT_LITERAL, "FLOAT_LITERAL");
	expect_name(TOK_OCTAL_LITERAL, "OCTAL_LITERAL");
	expect_name(TOK_HEXADECIMAL_LITERAL, "HEXADECIMAL_LITERAL");
	expect_name(TOK_PP_INCLUDE, "PP_INCLUDE");
	expect_name(TOK_PP_DEFINE, "PP_DEFINE");
	expect_name(TOK_PLUS_ASSIGN, "PLUS_ASSIGN");
	expect_name(TOK_LBRACKET, "LBRACKET");
	expect_name(TOK_SEMICOLON, "SEMICOLON");
	expect_name(-1, "UNKNOWN");

	return 0;
}
