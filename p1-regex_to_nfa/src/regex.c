#include "regex.h"

#include <stddef.h>   // size_t
#include <stdio.h>    // FILE 
#include <stdbool.h> // bool


/* --- Local Helpers to do the implementation easier --- */



/* --- Helpers in regex.h (useful for organization) --- */


regex from_implicit_to_explicit_concat(const regex *infix_implicit){

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


regex parse_regex(const regex *infix_implicit){

}