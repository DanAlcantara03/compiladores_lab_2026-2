#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "scanner.h"
#include "scanner_flex.h"
#include "symtab.h"

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

struct token_expectation {
	int token;
	const char *lexeme;
};

struct stderr_capture {
	FILE *file;
	int saved_fd;
};

static void reset_scanner_runtime(void)
{
	yylex_destroy();
	symtab_destroy();
	scanner_line = 1;
	scanner_col = 1;
	scanner_token_line = 1;
	scanner_token_col = 1;
}

static void run_subtest(const char *name, void (*fn)(void))
{
	reset_scanner_runtime();
	printf("Subtest %s: ejecutando...\n", name);
	fn();
	printf("Subtest %s: ya sucedio y paso correctamente.\n", name);
	reset_scanner_runtime();
}

static struct stderr_capture begin_stderr_capture(void)
{
	struct stderr_capture capture;

	capture.file = tmpfile();
	assert(capture.file != NULL);

	fflush(stderr);
	capture.saved_fd = dup(STDERR_FILENO);
	assert(capture.saved_fd >= 0);
	assert(dup2(fileno(capture.file), STDERR_FILENO) >= 0);
	clearerr(stderr);
	return capture;
}

static void end_stderr_capture(struct stderr_capture capture, char *buffer, size_t buffer_size)
{
	size_t read_bytes;

	assert(buffer_size > 0);
	fflush(stderr);
	assert(dup2(capture.saved_fd, STDERR_FILENO) >= 0);
	close(capture.saved_fd);
	clearerr(stderr);
	assert(fseek(capture.file, 0, SEEK_SET) == 0);
	read_bytes = fread(buffer, 1, buffer_size - 1, capture.file);
	buffer[read_bytes] = '\0';
	fclose(capture.file);
}

static void expect_token_sequence(const char *input, const struct token_expectation *expected, size_t expected_count)
{
	YY_BUFFER_STATE buffer = NULL;
	size_t i;
	int token;

	reset_scanner_runtime();
	buffer = yy_scan_string(input);
	assert(buffer != NULL);

	for (i = 0; i < expected_count; i++) {
		token = yylex();
		assert(token == expected[i].token);
		assert(strcmp(yytext, expected[i].lexeme) == 0);
	}

	token = yylex();
	assert(token == TOK_EOF);

	yy_delete_buffer(buffer);
	yylex_destroy();
	symtab_destroy();
}

static void test_printable_token_names(void)
{
	const struct {
		int token;
		const char *expected_name;
	} expectations[] = {
		{ TOK_EOF, "EOF" },
		{ TOK_ERROR, "ERROR" },
		{ TOK_KW_INT, "KW_INT" },
		{ TOK_IDENTIFIER, "IDENTIFIER" },
		{ TOK_FLOAT_LITERAL, "FLOAT_LITERAL" },
		{ TOK_OCTAL_LITERAL, "OCTAL_LITERAL" },
		{ TOK_HEXADECIMAL_LITERAL, "HEXADECIMAL_LITERAL" },
		{ TOK_PP_INCLUDE, "PP_INCLUDE" },
		{ TOK_PP_DEFINE, "PP_DEFINE" },
		{ TOK_PLUS_ASSIGN, "PLUS_ASSIGN" },
		{ TOK_LBRACKET, "LBRACKET" },
		{ TOK_SEMICOLON, "SEMICOLON" },
		{ -1, "UNKNOWN" },
	};
	size_t i;

	for (i = 0; i < ARRAY_LEN(expectations); i++) {
		const char *actual_name = scanner_token_name(expectations[i].token);
		assert(strcmp(actual_name, expectations[i].expected_name) == 0);
	}
}

static void test_scans_preprocessor_directives(void)
{
	const struct token_expectation expected[] = {
		{ TOK_PP_INCLUDE, "#include <stdio.h>" },
		{ TOK_PP_DEFINE, "#define MAX 10" },
	};

	expect_token_sequence("#include <stdio.h>\n#define MAX 10\n", expected, ARRAY_LEN(expected));
}

static void test_scans_octal_and_hexadecimal_literals(void)
{
	const struct token_expectation expected[] = {
		{ TOK_OCTAL_LITERAL, "0o77" },
		{ TOK_HEXADECIMAL_LITERAL, "0x1F" },
	};

	expect_token_sequence("0o77 0x1F\n", expected, ARRAY_LEN(expected));
}

static void test_reports_invalid_token_error(void)
{
	struct stderr_capture capture;
	YY_BUFFER_STATE buffer = NULL;
	char stderr_output[256];
	int token;

	reset_scanner_runtime();
	buffer = yy_scan_string("@\n");
	assert(buffer != NULL);

	capture = begin_stderr_capture();
	token = yylex();
	end_stderr_capture(capture, stderr_output, sizeof(stderr_output));

	assert(token == TOK_ERROR);
	assert(strcmp(yytext, "@") == 0);
	assert(strstr(stderr_output, "Lexical error at 1:1 near '@'") != NULL);
	token = yylex();
	assert(token == TOK_EOF);
	yy_delete_buffer(buffer);
	yylex_destroy();
	symtab_destroy();
}

static void test_reports_unterminated_comment_error(void)
{
	struct stderr_capture capture;
	YY_BUFFER_STATE buffer = NULL;
	char stderr_output[256];
	int token;

	reset_scanner_runtime();
	buffer = yy_scan_string("/* unterminated");
	assert(buffer != NULL);

	capture = begin_stderr_capture();
	token = yylex();
	end_stderr_capture(capture, stderr_output, sizeof(stderr_output));

	assert(token == TOK_ERROR);
	assert(strcmp(yytext, "") == 0);
	assert(strstr(stderr_output, "Lexical comment error at") != NULL);
	assert(strstr(stderr_output, "near ''") != NULL);
	token = yylex();
	assert(token == TOK_EOF);
	yy_delete_buffer(buffer);
	yylex_destroy();
	symtab_destroy();
}

/**
 * @brief Runs token-name mapping tests.
 * @return 0 on success.
 */
int main(void)
{
	run_subtest("printable_token_names", test_printable_token_names);
	run_subtest("scans_preprocessor_directives", test_scans_preprocessor_directives);
	run_subtest("scans_octal_and_hexadecimal_literals", test_scans_octal_and_hexadecimal_literals);
	run_subtest("reports_invalid_token_error", test_reports_invalid_token_error);
	run_subtest("reports_unterminated_comment_error", test_reports_unterminated_comment_error);
	puts("test_scanner_token_name: todos los subtests pasaron.");
	return 0;
}
