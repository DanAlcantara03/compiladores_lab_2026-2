#include "regex.h"

#include <stddef.h>   // size_t
#include <stdio.h>    // FILE 
#include <stdbool.h> // bool


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


regex regex_explicit_infix_to_postfix(const regex *infix_explicit){

}


void regex_print(regex r, FILE *out){
    
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