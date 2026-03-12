#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "symtab.h"

/**
 * @brief Verifies interning returns the same pointer for the same lexeme.
 */
static void test_reuses_existing_lexeme(void)
{
	const char *a1 = symtab_intern("alpha");
	const char *a2 = symtab_intern("alpha");

	assert(a1 != NULL);
	assert(a2 != NULL);
	assert(a1 == a2);
}

/**
 * @brief Verifies distinct lexemes are stored as distinct entries.
 */
static void test_distinct_lexemes_are_distinct_entries(void)
{
	const char *a = symtab_intern("alpha");
	const char *b = symtab_intern("beta");

	assert(a != NULL);
	assert(b != NULL);
	assert(a != b);
	assert(strcmp(a, "alpha") == 0);
	assert(strcmp(b, "beta") == 0);
}

/**
 * @brief Stress test with many identifiers and reintern checks.
 */
static void test_many_identifiers(void)
{
	enum { N = 2048 };
	const char *stored[N];
	char name[32];
	int i;

	for (i = 0; i < N; i++) {
		snprintf(name, sizeof(name), "id_%d", i);
		stored[i] = symtab_intern(name);
		assert(stored[i] != NULL);
		assert(strcmp(stored[i], name) == 0);
	}

	for (i = 0; i < N; i++) {
		const char *again;
		snprintf(name, sizeof(name), "id_%d", i);
		again = symtab_intern(name);
		assert(again == stored[i]);
	}
}

/**
 * @brief Verifies table cleanup and reuse after destroy.
 */
static void test_destroy_and_reuse(void)
{
	const char *before = symtab_intern("persist");
	const char *after;

	assert(before != NULL);
	symtab_destroy();

	after = symtab_intern("persist");
	assert(after != NULL);
	assert(strcmp(after, "persist") == 0);
}

/**
 * @brief Runs all symtab unit tests.
 * @return 0 on success.
 */
int main(void)
{
	test_reuses_existing_lexeme();
	test_distinct_lexemes_are_distinct_entries();
	test_many_identifiers();
	test_destroy_and_reuse();

	symtab_destroy();
	puts("test_symtab: OK");
	return 0;
}
