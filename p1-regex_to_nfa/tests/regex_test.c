#include "regex.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef void (*test_fn) (void);
typedef regex (*regex_fn) (const regex*);

/*
 * Runs one test case and updates simple counters.
 *
 * How it works:
 * - Increments `total` before running the test.
 * - Calls the test function `fn()`.
 * - If the test did not abort (no assert failure), increments `passed`.
 * - Prints a standardized "[PASS]" line with the test name.
 *
 * Notes:
 * - This helper assumes tests fail-fast via `assert`.
 * - If a test fails, execution stops and this function does not reach `passed++`.
 */
static void run_test(test_fn fn, const char *name, int *passed, int *total) {
    (*total)++;
    fn();
    (*passed)++;
    printf("[PASS] %s\n", name);
}

/*
 * Validates basic structural invariants of a `regex` value.
 *
 * Checked invariants:
 * - If `size > 0`, then `items` must not be NULL.
 * - If `items` exists, byte `items[size]` must be '\0'
 *   (null-terminated internal buffer contract used by this project).
 *
 * Parameters:
 * - `r`: regex object to validate.
 * - `context`: label printed in failure messages to identify the failing test case.
 */
static void assert_regex_invariant(regex r, const char *context) {
    if (r.size > 0 && r.items == NULL) {
        fprintf(stderr, "FAIL: %s (size > 0 but items == NULL)\n", context);
        assert(r.items != NULL);
    }

    /* create_regex stores '\0' terminator when an internal buffer exists */
    if (r.items != NULL) {
        assert(r.items[r.size] == '\0');
    }
}

/*
 * Asserts that a `regex` exactly matches an expected C string.
 *
 * Verification steps:
 * 1) `expected` must not be NULL.
 * 2) `r` must satisfy internal invariants (`assert_regex_invariant`).
 * 3) `r.size` must equal `strlen(expected)`.
 * 4) If length > 0, content must match byte-by-byte via `memcmp`.
 *
 * On mismatch:
 * - Prints a descriptive "FAIL: <context> (...)" message to stderr.
 * - Triggers `assert(...)` to stop execution immediately.
 *
 * Parameters:
 * - `r`: actual regex result.
 * - `expected`: expected textual representation.
 * - `context`: label used in failure output for quick debugging.
 */
static void assert_regex_equals(regex r, const char *expected, const char *context) {
    assert(expected != NULL);
    assert_regex_invariant(r, context);

    size_t expected_len = strlen(expected);
    if (r.size != expected_len) {
        fprintf(stderr, "FAIL: %s (size=%zu expected=%zu)\n", context, r.size, expected_len);
        assert(r.size == expected_len);
    }

    int cmp = (expected_len > 0) ? memcmp(r.items, expected, expected_len) : 0;
    if (cmp != 0) {
        fprintf(stderr, "FAIL: %s (different content)\n", context);
        assert(cmp == 0);
    }
}

/*
 * Generic helper for transformation-based regex test cases.
 *
 * It runs one full case:
 * 1) Builds a `regex` from the source string (`src`).
 * 2) Applies the transformation function (`r_f`) to that input.
 * 3) Verifies the transformed output equals `expected`.
 * 4) Releases all temporary regex allocations.
 *
 * Parameters:
 * - `r_f`: transformation function under test.
 * - `src`: input regex string used to build the test input object.
 * - `expected`: expected textual representation after transformation.
 * - `ctx`: context label used in assertion failure messages.
 */
static void assert_regex_case(regex_fn r_f,const char *src, const char *expected, const char *ctx) {
    regex in = create_regex(src);
    regex out = r_f(&in);
    assert_regex_equals(out, expected, ctx);
    regex_free(&out);
    regex_free(&in);
}

/*
 * Verifies that a stream contains exactly the expected bytes and nothing else.
 */
static void assert_stream_equals(FILE *stream, const char *expected, const char *context) {
    assert(stream != NULL);
    assert(expected != NULL);

    rewind(stream);

    for (size_t i = 0; expected[i] != '\0'; i++) {
        int got = fgetc(stream);
        if (got == EOF || got != (unsigned char)expected[i]) {
            fprintf(stderr, "FAIL: %s (byte mismatch at index %zu)\n", context, i);
            assert(got != EOF && got == (unsigned char)expected[i]);
        }
    }

    if (fgetc(stream) != EOF) {
        fprintf(stderr, "FAIL: %s (unexpected extra output)\n", context);
        assert(0);
    }
}

/*
 * For each test I follow the test_<function>_<scenario> pattern.
 */

/* create_regex should return an empty descriptor for NULL input. */
static void test_create_regex_null_input(void) {
    regex out = create_regex(NULL);
    assert(out.size == 0);
    assert(out.items == NULL);
    regex_free(&out);
}

/* create_regex should produce a valid empty regex for "". */
static void test_create_regex_empty_string(void) {
    regex out = create_regex("");
    assert_regex_invariant(out,"create_regex(\"\")");
    assert(out.items[0] == '\0');
    regex_free(&out);
}

/* create_regex must deep-copy input data. */
static void test_create_regex_copies_input(void) {
    char input[] = "ab|c";
    regex out = create_regex(input);
    input[0] = 'z'; //mutate original buffer
    assert_regex_equals(out, "ab|c", "create_regex_copy_is_deep create_regex(\"ab|c\")");

    regex_free(&out);
}

/* regex_free should be idempotent and leave a reset descriptor. */
static void test_regex_free_resets_state(void) {
    regex r = create_regex("abc");
    regex_free(&r);
    regex_free(&r);
    assert(r.items == NULL);
    assert(r.size == 0);
}

/* from_implicit_to_explicit_concat should insert explicit '.' operators. */
static void test_from_implicit_to_explicit_concat (void){
    assert_regex_case(from_implicit_to_explicit_concat,"ab","a.b", "from_implicit_to_explicit_concat basic");
    assert_regex_case(from_implicit_to_explicit_concat,"a(b|c)*d","a.(b|c)*.d", "from_implicit_to_explicit_concat complex 1");
    assert_regex_case(from_implicit_to_explicit_concat,"(ab)c","(a.b).c", "from_implicit_to_explicit_concat complex 2");
    assert_regex_case(from_implicit_to_explicit_concat,"a*bc","a*.b.c", "from_implicit_to_explicit_concat complex 3");
    assert_regex_case(from_implicit_to_explicit_concat, "a(b)c","a.(b).c", "from_implicit_to_explicit_concat complex 4");
    assert_regex_case(from_implicit_to_explicit_concat, "a+b", "a+.b", "from_implicit_to_explicit_concat plus");
    assert_regex_case(from_implicit_to_explicit_concat, "a?b", "a?.b", "from_implicit_to_explicit_concat optional");
}


/* regex_explicit_infix_to_postfix should preserve operator precedence/associativity. */
static void test_regex_explicit_infix_to_postfix(void) {
    assert_regex_case(regex_explicit_infix_to_postfix, "a", "a", "regex_explicit_infix_to_postfix basic_1");
    assert_regex_case(regex_explicit_infix_to_postfix, "a*", "a*", "regex_explicit_infix_to_postfix basic_2");
    assert_regex_case(regex_explicit_infix_to_postfix, "a.b", "ab.", "regex_explicit_infix_to_postfix basic_3");
    assert_regex_case(regex_explicit_infix_to_postfix, "a|b", "ab|", "regex_explicit_infix_to_postfix basic_4");

    assert_regex_case(regex_explicit_infix_to_postfix, "a.(b|c)*.d", "abc|*.d.", "regex_explicit_infix_to_postfix_complex_1");
    assert_regex_case(regex_explicit_infix_to_postfix, "a.(b|c).d*.e", "abc|.d*.e.", "regex_explicit_infix_to_postfix complex_2");
    assert_regex_case(regex_explicit_infix_to_postfix, "(a|b).(c|d)*.e", "ab|cd|*.e.", "regex_explicit_infix_to_postfix complex_3");
    assert_regex_case(regex_explicit_infix_to_postfix, "a*.b.(c|d).e|f", "a*b.cd|.e.f|", "regex_explicit_infix_to_postfix complex_4");
    assert_regex_case(regex_explicit_infix_to_postfix, "a+.b", "a+b.", "regex_explicit_infix_to_postfix plus");
    assert_regex_case(regex_explicit_infix_to_postfix, "a?.b", "a?b.", "regex_explicit_infix_to_postfix optional");
}

/* parse_regex should cover implicit-concat + postfix conversion end-to-end. */
static void test_parse_regex_end_to_end(void) {
    assert_regex_case(parse_regex, "a", "a", "parse_regex end-to-end basic_1");
    assert_regex_case(parse_regex, "a*", "a*", "parse_regex end-to-end basic_2");
    assert_regex_case(parse_regex, "ab", "ab.", "parse_regex end-to-end basic_3");
    assert_regex_case(parse_regex, "a|b", "ab|", "parse_regex end-to-end basic_4");

    assert_regex_case(parse_regex, "a(b|c)*d", "abc|*.d.", "parse_regex end-to-end complex_1");
    assert_regex_case(parse_regex, "a(b|(ab(b|c)))*f", "abab.bc|.|*.f.", "parse_regex end-to-end complex_2)");
    assert_regex_case(parse_regex, "(a|b)(c|d)*e", "ab|cd|*.e.", "parse_regex end-to-end complex_1");
    assert_regex_case(parse_regex, "a*b(c|d)e|f", "a*b.cd|.e.f|", "parse_regex end-to-end complex_4");
    assert_regex_case(parse_regex, "a+b", "a+b.", "parse_regex end-to-end plus");
    assert_regex_case(parse_regex, "a?b", "a?b.", "parse_regex end-to-end optional");
}

/* parse_regex should safely reject malformed input cases. */
static void test_parse_regex_invalid_input(void) {
    /* NULL pointer input */
    regex out = parse_regex(NULL);
    assert_regex_equals(out, "", "parse_regex invalid: NULL input pointer");
    regex_free(&out);

    /* Inconsistent descriptor: size > 0 with NULL items */
    regex invalid_descriptor = (regex){ .items = NULL, .size = 4 };
    out = parse_regex(&invalid_descriptor);
    assert_regex_equals(out, "", "parse_regex invalid: inconsistent descriptor");
    regex_free(&out);

    assert_regex_case(parse_regex,"a(b|c", "", "parse_regex invalid: missing ')'");
    assert_regex_case(parse_regex,"a)b|c", "", "parse_regex invalid: unexpected '('");
    assert_regex_case(parse_regex,"((a|b)", "", "parse_regex invalid: nested unclosed paren");
}

/* regex_print should write correct output to explicit stream and stdout fallback. */
static void test_regex_print(void) {
    /* Case 1: explicit output stream with regular content */
    regex r = create_regex("ab|c");
    FILE *tmp = tmpfile();
    assert(tmp != NULL);

    regex_print(r, tmp);
    fflush(tmp);
    assert_stream_equals(tmp, "ab|c\n", "regex_print explicit stream regular");

    fclose(tmp);
    regex_free(&r);

    /* Case 2: explicit output stream with empty regex */
    r = create_regex("");
    tmp = tmpfile();
    assert(tmp != NULL);

    regex_print(r, tmp);
    fflush(tmp);
    assert_stream_equals(tmp, "\n", "regex_print explicit stream empty regex");

    fclose(tmp);
    regex_free(&r);

    /* Case 3: NULL output stream should fallback to stdout */
    r = create_regex("x.y");

    int pipefd[2] = { -1, -1 };
    assert(pipe(pipefd) == 0);

    fflush(stdout);
    int saved_stdout_fd = dup(STDOUT_FILENO);
    assert(saved_stdout_fd >= 0);
    assert(dup2(pipefd[1], STDOUT_FILENO) >= 0);
    close(pipefd[1]);

    regex_print(r, NULL);
    fflush(stdout);

    assert(dup2(saved_stdout_fd, STDOUT_FILENO) >= 0);
    close(saved_stdout_fd);

    char captured[64] = {0};
    size_t used = 0;
    size_t nread = 0;

    while ((nread = read(pipefd[0], captured + used, sizeof(captured) - 1 - used)) > 0) {
        used += (size_t)nread;
        if (used == sizeof(captured) - 1) {
            fprintf(stderr, "FAIL: regex_print NULL stream fallback to stdout (capture buffer too small)\n");
            assert(0);
        }
    }
    assert(nread >= 0);
    close(pipefd[0]);

    captured[used] = '\0';
    assert(strcmp(captured, "x.y\n") == 0);

    regex_free(&r);
}

/* Executes the regex test suite and prints a summary line. */
int main(void) {
    int total = 0;
    int passed = 0;

    printf("Running regex tests\n");

    run_test(test_create_regex_null_input, "test_create_regex_null_input", &passed, &total);
    run_test(test_create_regex_empty_string, "test_create_regex_empty_string", &passed, &total);
    run_test(test_create_regex_copies_input, "test_create_regex_copies_input", &passed, &total);
    run_test(test_regex_free_resets_state, "test_regex_free_resets_state", &passed, &total);
    run_test(test_from_implicit_to_explicit_concat, "test_from_implicit_to_explicit_concat", &passed, &total);
    run_test(test_regex_explicit_infix_to_postfix, "test_regex_explicit_infix_to_postfix", &passed, &total);
    run_test(test_parse_regex_end_to_end, "test_parse_regex_end_to_end", &passed, &total);
    run_test(test_parse_regex_invalid_input, "test_parse_regex_invalid_input", &passed, &total);
    run_test(test_regex_print, "test_regex_print__todo", &passed, &total);

    printf("Summary: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
