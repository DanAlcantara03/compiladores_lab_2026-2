#include "regex.h"

#include <stddef.h>   // size_t
#include <stdio.h>    // FILE 
#include <stdbool.h> // bool
#include "data_structures.h" // Stack and Queue


/* --- Local Helpers to do the implementation easier --- */

/**
 * @brief Returns whether @p symbol is a literal (not one of ()|*).
 * @param symbol The character to classify.
 * @return true if @p symbol is a literal; false otherwise.
 */
static bool is_literal(char c){
    switch (c) {
    case REGEX_LPAREN:
    case REGEX_RPAREN:
    case REGEX_OP_STAR:
    case REGEX_OP_CONCAT:
    case REGEX_OP_OR:
        return false;
    default:
        return true;
    }
}

/**
 * @brief Returns whether @p symbol can start an atom.
 * @param symbol The character to classify.
 * @return true if @p symbol can start an atom; false otherwise.
 */
static bool starts_atom(char symbol){
    return is_literal(symbol) || symbol == REGEX_LPAREN;
}

/**
 * @brief Returns whether @p symbol can end an atom.
 * @param symbol The character to classify.
 * @return true if @p symbol can end an atom; false otherwise.
 */
static bool ends_atom(char symbol){
    return symbol == REGEX_RPAREN || symbol == REGEX_OP_STAR || is_literal(symbol);
}

/**
 * @brief Returns whether @p symbol is one of the supported regex operators.
 * @param symbol The character to classify.
 * @return true if @p symbol is `*`, `.`, or `|`; false otherwise.
 */
static bool is_operator(char symbol){
    return symbol == REGEX_OP_STAR || symbol == REGEX_OP_CONCAT || symbol == REGEX_OP_OR;
}

/**
 * @brief Returns the precedence level of a regex operator.
 *
 * Higher numbers mean higher precedence:
 * `*` (Kleene star) > `.` (concatenation) > `|` (alternation).
 *
 * @param op Operator symbol to evaluate.
 * @return Integer precedence value, or 0 if @p op is not an operator.
 */
static int precedence(regex_symbols op) {
    switch (op) {
        case REGEX_OP_STAR:   return 3;
        case REGEX_OP_CONCAT: return 2;
        case REGEX_OP_OR:     return 1;
        default:              return 0;
    }
}

/* --- Helpers in regex.h (useful for organization) --- */

/**
 * @brief Normalizes implicit concatenation using a single left-to-right pass.
 *
 * Implementation approach:
 * - Computes one worst-case allocation (`2*n` bytes including `'\0'` when `n>0`) so no reallocation is needed.
 * - Scans once by index and writes each current symbol.
 * - Inserts `'.'` between adjacent symbols when `ends_atom(current)` and `starts_atom(next)` are both true.
 *
 * Edge-case handling in this implementation:
 * - `NULL` input fails fast.
 * - Empty input returns an empty regex.
 */
regex from_implicit_to_explicit_concat(const regex *infix_implicit){
    if (!infix_implicit) return create_regex(NULL);
    if (!infix_implicit->items && infix_implicit->size > 0) return create_regex(NULL);

    size_t n = infix_implicit->size;
    if (n == 0) return create_regex("");

    // Worst case when every adjacent pair needs '.': 2*n bytes including '\0'.
    size_t out_len = 2 * n;

    char *out = malloc(out_len);
    if (!out) return create_regex(NULL);

    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char current = infix_implicit->items[i];
        out[k++] = current;

        if (i + 1 < n) {
            char next = infix_implicit->items[i + 1];
            if (ends_atom(current) && starts_atom(next)) {
                out[k++] = '.';
            }
        }
    }

    out[k] = '\0';
    return (regex){ .items = out, .size = k };
}

/**
 * @brief Converts an explicit-infix regex into postfix form using Shunting-Yard.
 *
 * Implementation approach:
 * - Uses a stack (`operators`) to keep pending operators and parentheses.
 * - Uses a queue (`output`) to accumulate the postfix stream in emission order.
 * - Scans the input left-to-right once:
 *   - literals are emitted directly to `output`,
 *   - `(` is pushed to `operators`,
 *   - `)` drains operators until the matching `(`,
 *   - operators (`*`, `.`, `|`) pop higher/equal precedence operators from the stack before being pushed.
 * - After scanning, it drains remaining operators to `output`.
 *
 * Memory discipline in this implementation:
 * - Both temporary structures (`operators`, `output`) are always freed before return.
 * - The final textual postfix buffer is created via `ds_queue_to_string_newest_first`, converted to `regex` with `create_regex`, then immediately released.
 *
 * Validation and fail-fast behavior:
 * - Returns an empty regex on invalid input pointers, allocation/status failures, unsupported symbols, or mismatched parentheses.
 * - Empty input maps to empty output.
 */
regex regex_explicit_infix_to_postfix(const regex *infix_explicit){
    /* --- Part 1 - Initial validation and preparation --- */

    // 1.1 Validation via the input descriptor
    if (!infix_explicit) return create_regex(NULL);
    if (!infix_explicit->items && infix_explicit->size > 0) return create_regex(NULL);

    // 1.2 Void and auxiliar case
    size_t n = infix_explicit->size;
    if (n == 0) return create_regex("");

    // Every input token can be emitted at most once, plus '\0' in the string conversion.
    size_t out_len = n + 1;

    //1.3 Initial exit state (out) and temp state (*postfix_str)
    regex out = create_regex(NULL);
    char *postfix_str = NULL;

    // 1.4 Initialize and reserve Auxiliar structures.

    // Operator stack keeps pending operators and parentheses.
    ds_stack operators = {0};
    if (ds_stack_init(&operators, sizeof(char)) != DS_OK) goto cleanup;
    if (ds_stack_reserve(&operators, out_len) != DS_OK) goto cleanup;

    // Output queue stores postfix symbols in final emission order.
    ds_queue output = {0};
    if (ds_queue_init(&output, sizeof(char)) != DS_OK) goto cleanup;
    if (ds_queue_reserve(&output, out_len) != DS_OK) goto cleanup;

    /* --- Part 2 - Principal loop (token by token) --- */

    // 2.1 it goes from 0 to n-1
    for (size_t i = 0; i < n; i++) {
        char token = infix_explicit->items[i];

        //2.2 (Literals case) Literals go straight to output.
        if (is_literal(token)) {
            if (ds_queue_enqueue(&output, &token) != DS_OK) goto cleanup;
            continue;
        }

        //2.3 (Opens paren case) Opening parenthesis starts a nested scope.
        if (token == REGEX_LPAREN) {
            if (ds_stack_push(&operators, &token) != DS_OK) goto cleanup;
            continue;
        }

        //2.4 (Closes paren case)
        if (token == REGEX_RPAREN) {
            bool found_lparen = false;

            // Close scope: emit operators until matching '('.
            while (!ds_stack_empty(&operators)) {
                char top = '\0';
                if (ds_stack_pop(&operators, &top) != DS_OK) goto cleanup;

                if (top == REGEX_LPAREN) {
                    found_lparen = true;
                    break;
                }

                if (ds_queue_enqueue(&output, &top) != DS_OK) goto cleanup;
            }
            // If it finishes and doesn't find the operator
            if (!found_lparen) goto cleanup;
            continue;
        }

        //2.5 (Operators case)

        // Any remaining non-literal token must be a supported operator.
        if (!is_operator(token)) goto cleanup;

        // Pop higher/equal precedence operators (left-associative behavior).
        while (!ds_stack_empty(&operators)) {
            char top = '\0';
            if (ds_stack_peek(&operators, &top) != DS_OK) goto cleanup;

            if (top == REGEX_LPAREN) break;

            // Implements left associativity for operators with the same precedence, because when precedence(top) == precedence(token), it also pops from the stack.
            if (precedence((regex_symbols)top) < precedence((regex_symbols)token)) break;

            if (ds_stack_pop(&operators, &top) != DS_OK) goto cleanup;
            if (ds_queue_enqueue(&output, &top) != DS_OK) goto cleanup;
        }

        if (ds_stack_push(&operators, &token) != DS_OK) goto cleanup;
    }

    /* --- Part 3 - Pop everything off the stack --- */

    // Flush remaining operators; parentheses here mean invalid expression.
    while (!ds_stack_empty(&operators)) {
        char top = '\0';
        if (ds_stack_pop(&operators, &top) != DS_OK) goto cleanup;

        if (top == REGEX_LPAREN || top == REGEX_RPAREN) goto cleanup;

        if (ds_queue_enqueue(&output, &top) != DS_OK) goto cleanup;
    }

    /* --- Part 4 - Materializing the result --- */

    // 4.1 Serializes the queue to a string
    postfix_str = ds_queue_to_string_newest_first(&output);
    if (!postfix_str) goto cleanup;

    // 4.2 create a regex based on the queue string
    out = create_regex(postfix_str);

    /* --- Final part - Unified cleanup --- */

cleanup:
    free(postfix_str);
    ds_queue_free(&output);
    ds_stack_free(&operators);
    return out;
}

/**
 * @brief Emits the regex buffer as a single debug line.
 *
 * Implementation details:
 * - Resolves the destination stream with a simple fallback policy: use the caller stream when present, otherwise default to `stdout`.
 * - Traverses `r.items` linearly (`0 .. size-1`) and writes each stored symbol with `fputc`, avoiding temporary string assembly.
 * - Appends a final newline unconditionally so each call produces one complete line in logs/tests.
 */
void regex_print(regex r, FILE *out){
    FILE *dest = out ? out : stdout;
    for (size_t i = 0; i < r.size; i++) {
        fputc(r.items[i], dest);
    }
    fputc('\n', dest);
}

/* --- Principal Functions --- */

/**
 * @brief Creates an owning regex object from a C-string expression.
 *
 * Implementation details:
 * - Starts from a canonical empty descriptor (`items = NULL`, `size = 0`) so every early return remains valid.
 * - Treats `NULL` input as an empty regex and exits without allocating.
 * - Uses `strlen` to capture the logical length in `size` (excluding `'\0'`).
 * - Allocates `size + 1` bytes and copies `size + 1` bytes with `memcpy`, preserving the terminating null byte in the internal buffer.
 * - If allocation fails, normalizes the result back to empty (`size = 0`, `items = NULL`) and returns it.
 *
 * Ownership contract:
 * - On success, the returned object owns `items`; callers must release it with `regex_free`.
 */
regex create_regex(const char *regex_expression){
    regex r = (regex){ .items = NULL, .size = 0};
    if (!regex_expression) return r;

    r.size = strlen(regex_expression);

    // Save the expression as (array + '\0')
    r.items = (char*)malloc(r.size + 1);
    if(!r.items){
        r.size = 0;
        return r;
    }

    memcpy(r.items, regex_expression, r.size + 1); // the copy includes '\0'
    return r;
}

/**
 * @brief Applies the module teardown policy: free resources and normalize state.
 *
 * Implementation details:
 * - Uses a null-guard so callers can invoke cleanup unconditionally.
 * - Frees only the internal dynamic array (`items`); the struct itself remains owned by the caller.
 * - Rewrites the object to a canonical empty state right after free (`items = NULL`, `size = 0`) so future checks and repeated cleanup behave deterministically.
 */
void regex_free(regex *r){
    if (!r) return;
    free(r->items);
    r->items = NULL;
    r->size = 0;
}

/**
 * @brief Orchestrates the parsing pipeline with explicit ownership boundaries.
 *
 * Implementation strategy:
 * - First normalizes the user expression from implicit concatenation to an explicit infix form (`a(b|c)` -> `a.(b|c)`), using 'from_implicit_to_explicit_concat', because the next stage expects explicit operators.
 * - Then delegates conversion to postfix to `regex_explicit_infix_to_postfix`, keeping parsing responsibilities separated by stage.
 *
 * Memory discipline in this implementation:
 * - The normalized infix buffer is temporary and function-local.
 * - It is always released in this function after stage 2 runs.
 * - If normalization fails, the function returns immediately with a zero-initialized regex, avoiding partial states.
 */
regex parse_regex(const regex *infix_implicit){
    if (!infix_implicit) return create_regex(NULL);

    regex explicit_infix = from_implicit_to_explicit_concat(infix_implicit);
    regex out = regex_explicit_infix_to_postfix(&explicit_infix);
    regex_free(&explicit_infix);
    return out;
}