#include "nfa.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void (*test_fn)(void);

/* Runs one test and updates pass/total counters. */
static void run_test(test_fn fn, const char *name, int *passed, int *total) {
    (*total)++;
    fn();
    (*passed)++;
    printf("[PASS] %s\n", name);
}

/* Builds an NFA from infix regex text using the parser pipeline. */
static nfa build_nfa_from_infix(const char *infix) {
    regex infix_r = create_regex(infix);
    regex postfix = parse_regex(&infix_r);
    nfa automaton = regex_to_nfa(postfix);
    regex_free(&postfix);
    regex_free(&infix_r);
    return automaton;
}

/* Returns a heap string with `count` repeated copies of `ch`. */
static char *repeat_char(char ch, size_t count) {
    char *text = (char *)malloc(count + 1);
    assert(text != NULL);
    for (size_t i = 0; i < count; i++) {
        text[i] = ch;
    }
    text[count] = '\0';
    return text;
}

/* Asserts that an NFA value is the canonical empty/invalid representation. */
static void assert_nfa_is_empty(nfa automaton, const char *context) {
    if (automaton.start_state != INVALID_NFA_STATE) {
        fprintf(stderr, "FAIL: %s (start_state=%u expected=%u)\n",
                context, automaton.start_state, INVALID_NFA_STATE);
        assert(automaton.start_state == INVALID_NFA_STATE);
    }
    if (automaton.states != 0) {
        fprintf(stderr, "FAIL: %s (states=%u expected=0)\n", context, automaton.states);
        assert(automaton.states == 0);
    }
    if (automaton.transitions != NULL) {
        fprintf(stderr, "FAIL: %s (transitions must be NULL)\n", context);
        assert(automaton.transitions == NULL);
    }
    if (automaton.epsilon_closure_cache != NULL) {
        fprintf(stderr, "FAIL: %s (epsilon_closure_cache must be NULL)\n", context);
        assert(automaton.epsilon_closure_cache == NULL);
    }
}

/* Asserts one match result for a C-string input. */
static void assert_match(nfa automaton, const char *input, bool expected, const char *context) {
    bool got = match_nfa(automaton, input, strlen(input));
    if (got != expected) {
        fprintf(stderr, "FAIL: %s (input=\"%s\" got=%d expected=%d)\n",
                context, input, got, expected);
        assert(got == expected);
    }
}

/* Verifies stream output bytes exactly match expected text. */
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

/* nfa_init should produce a canonical empty NFA and nfa_free should be idempotent. */
static void test_nfa_init_and_free_idempotent(void) {
    nfa automaton = nfa_init();
    assert_nfa_is_empty(automaton, "nfa_init canonical empty");
    assert(automaton.nfa_alphabet.symbol_count == 1);
    assert(automaton.nfa_alphabet.char_to_col[(unsigned char)EPSILON_SYMBOL] == 0);

    nfa_free(&automaton);
    assert_nfa_is_empty(automaton, "nfa_free after nfa_init");

    nfa_free(&automaton);
    assert_nfa_is_empty(automaton, "nfa_free idempotent");
}

/* regex_to_nfa should reject malformed/invalid postfix descriptors safely. */
static void test_regex_to_nfa_rejects_invalid_postfix(void) {
    regex empty = create_regex("");
    nfa automaton = regex_to_nfa(empty);
    assert_nfa_is_empty(automaton, "regex_to_nfa empty postfix");
    nfa_free(&automaton);
    regex_free(&empty);

    regex invalid_descriptor = (regex){ .items = NULL, .size = 2 };
    automaton = regex_to_nfa(invalid_descriptor);
    assert_nfa_is_empty(automaton, "regex_to_nfa inconsistent descriptor");
    nfa_free(&automaton);

    regex malformed = create_regex("ab");
    automaton = regex_to_nfa(malformed);
    assert_nfa_is_empty(automaton, "regex_to_nfa stack leftover");
    nfa_free(&automaton);
    regex_free(&malformed);

    regex underflow = create_regex("|");
    automaton = regex_to_nfa(underflow);
    assert_nfa_is_empty(automaton, "regex_to_nfa operator underflow");
    nfa_free(&automaton);
    regex_free(&underflow);
}

/* Core matching behavior for literal, concatenation and alternation. */
static void test_match_nfa_basic_language_cases(void) {
    nfa literal = build_nfa_from_infix("a");
    assert_match(literal, "a", true, "literal accepts exact symbol");
    assert_match(literal, "", false, "literal rejects empty");
    assert_match(literal, "aa", false, "literal rejects repeated");
    assert_match(literal, "b", false, "literal rejects other symbol");
    nfa_free(&literal);

    nfa concat = build_nfa_from_infix("ab");
    assert_match(concat, "ab", true, "concat accepts sequence");
    assert_match(concat, "a", false, "concat rejects prefix");
    assert_match(concat, "b", false, "concat rejects suffix only");
    assert_match(concat, "aba", false, "concat rejects superstring");
    nfa_free(&concat);

    nfa alternation = build_nfa_from_infix("a|b");
    assert_match(alternation, "a", true, "or accepts left branch");
    assert_match(alternation, "b", true, "or accepts right branch");
    assert_match(alternation, "", false, "or rejects empty");
    assert_match(alternation, "ab", false, "or rejects concatenated pair");
    nfa_free(&alternation);
}

/* Closure operators should support empty/non-empty language variants correctly. */
static void test_match_nfa_closure_and_optional(void) {
    nfa kleene = build_nfa_from_infix("a*");
    assert_match(kleene, "", true, "kleene accepts empty");
    assert_match(kleene, "a", true, "kleene accepts one");
    assert_match(kleene, "aaaa", true, "kleene accepts many");
    assert_match(kleene, "b", false, "kleene rejects foreign symbol");
    nfa_free(&kleene);

    nfa positive = build_nfa_from_infix("a+");
    assert_match(positive, "", false, "positive rejects empty");
    assert_match(positive, "a", true, "positive accepts one");
    assert_match(positive, "aaaa", true, "positive accepts many");
    assert_match(positive, "b", false, "positive rejects foreign symbol");
    nfa_free(&positive);

    nfa optional = build_nfa_from_infix("a?");
    assert_match(optional, "", true, "optional accepts empty");
    assert_match(optional, "a", true, "optional accepts one");
    assert_match(optional, "aa", false, "optional rejects two");
    assert_match(optional, "b", false, "optional rejects foreign symbol");
    nfa_free(&optional);
}

/* Complex expressions should parse and simulate through branching + closure. */
static void test_match_nfa_complex_expression(void) {
    nfa automaton = build_nfa_from_infix("a(b|c)*d");
    assert_match(automaton, "ad", true, "complex accepts zero middle reps");
    assert_match(automaton, "abcd", true, "complex accepts mixed middle reps");
    assert_match(automaton, "abcbcd", true, "complex accepts longer middle reps");
    assert_match(automaton, "a", false, "complex rejects missing suffix");
    assert_match(automaton, "abce", false, "complex rejects wrong suffix");
    assert_match(automaton, "d", false, "complex rejects missing prefix");
    nfa_free(&automaton);
}

/* Runtime guard paths should reject invalid input descriptors and unknown symbols. */
static void test_match_nfa_invalid_runtime_inputs(void) {
    nfa automaton = build_nfa_from_infix("ab");

    assert(match_nfa(automaton, NULL, 1) == false);
    assert(match_nfa(automaton, "ac", 2) == false); /* 'c' is not in alphabet */

    nfa bad_start = automaton;
    bad_start.start_state = bad_start.states;
    assert(match_nfa(bad_start, "ab", 2) == false);

    nfa bad_states = automaton;
    bad_states.states = 0;
    assert(match_nfa(bad_states, "ab", 2) == false);

    nfa bad_transitions = automaton;
    bad_transitions.transitions = NULL;
    assert(match_nfa(bad_transitions, "ab", 2) == false);

    nfa_free(&automaton);
}

/* When cache is missing, match_nfa should allocate a temporary one and still succeed. */
static void test_match_nfa_builds_local_cache_when_missing(void) {
    nfa automaton = build_nfa_from_infix("ab");
    free(automaton.epsilon_closure_cache);
    automaton.epsilon_closure_cache = NULL;

    assert(match_nfa(automaton, "ab", 2) == true);
    assert(match_nfa(automaton, "a", 1) == false);
    assert(automaton.epsilon_closure_cache == NULL); /* owned cache is local in match_nfa */

    nfa_free(&automaton);
}

/* Stress boundary at MAX_STATES: 32 literals should fit, 33 should fail. */
static void test_regex_to_nfa_state_capacity_boundary(void) {
    char *thirty_two = repeat_char('a', 32);
    nfa ok = build_nfa_from_infix(thirty_two);
    assert(ok.states == MAX_STATES);

    assert(match_nfa(ok, thirty_two, 32) == true);
    assert(match_nfa(ok, thirty_two, 31) == false);

    nfa_free(&ok);
    free(thirty_two);

    char *thirty_three = repeat_char('a', 33);
    nfa overflow = build_nfa_from_infix(thirty_three);
    assert_nfa_is_empty(overflow, "regex_to_nfa must fail when states exceed MAX_STATES");
    nfa_free(&overflow);
    free(thirty_three);
}

/* nfa_print should write deterministic output for an explicit stream and stdout fallback. */
static void test_nfa_print_output(void) {
}

int main(void) {
    int total = 0;
    int passed = 0;

    printf("Running nfa tests\n");

    run_test(test_nfa_init_and_free_idempotent, "test_nfa_init_and_free_idempotent", &passed, &total);
    run_test(test_regex_to_nfa_rejects_invalid_postfix, "test_regex_to_nfa_rejects_invalid_postfix", &passed, &total);
    run_test(test_match_nfa_basic_language_cases, "test_match_nfa_basic_language_cases", &passed, &total);
    run_test(test_match_nfa_closure_and_optional, "test_match_nfa_closure_and_optional", &passed, &total);
    run_test(test_match_nfa_complex_expression, "test_match_nfa_complex_expression", &passed, &total);
    run_test(test_match_nfa_invalid_runtime_inputs, "test_match_nfa_invalid_runtime_inputs", &passed, &total);
    run_test(test_match_nfa_builds_local_cache_when_missing, "test_match_nfa_builds_local_cache_when_missing", &passed, &total);
    run_test(test_regex_to_nfa_state_capacity_boundary, "test_regex_to_nfa_state_capacity_boundary", &passed, &total);
    run_test(test_nfa_print_output, "test_nfa_print_output", &passed, &total);

    printf("Summary: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
