#ifndef REGEX_H
#define REGEX_H

#include <stddef.h>   // size_t
#include <stdio.h>    // FILE 
#include <stdbool.h> // bool

/**
 * @brief Canonical symbols used by the regex parser.
 *
 * This enum centralizes the character constants for grouping and operators, so token classification and precedence logic can rely on named symbols instead of hard-coded literals.
 */
typedef enum regex_symbols{
    REGEX_OP_OR     = '|',
    REGEX_OP_CONCAT = '.',
    REGEX_OP_OPTIONAL = '?',
    REGEX_OP_POSITIVE_CLOSURE = '+',
    REGEX_OP_KLEENE_STAR   = '*',
    REGEX_LPAREN    = '(',
    REGEX_RPAREN    = ')'
} regex_symbols;

/**
 * @brief In-memory representation of a tokenized regex sequence.
 *
 * The parser stores regex symbols in a contiguous dynamic buffer (`items`), and `size` tracks how many symbols are currently valid.
 *
 * Implementation contract:
 * - `items` is heap-managed storage owned by this object.
 * - `size` is the logical length (not counting any terminator).
 * - The caller must release internal storage with `regex_free()`.
 */
typedef struct{
    /** Dynamic array of regex symbols. */
    char *items;
    /** Number of valid entries in `items`. */
    size_t size;
} regex;

/* --- Helpers (useful for organization) --- */

/**
 * @brief Converts implicit concatenation into explicit concatenation.
 * @param infix_implicit Input regex sequence with implicit concatenation.
 * @return Regex sequence with explicit concatenation.
 */
regex from_implicit_to_explicit_concat(const regex *infix_implicit);

/**
 * @brief Converts an explicit-concatenation infix regex to postfix.
 * @param infix_explicit Input regex sequence in explicit infix form.
 * @return Regex sequence in postfix form.
 */
regex regex_explicit_infix_to_postfix(const regex *infix_explicit);

/**
 * @brief Prints a regex sequence to a stream.
 * @param r Regex to print.
 * @param out Destination stream, or NULL to use stdout.
 * @return No return value.
 */
void regex_print(regex r, FILE *out);

/* --- Principal Functions --- */

/**
 * @brief Creates a regex object from a textual regular expression.
 * @param regex_expresion Input null-terminated regex string to parse.
 * @return Constructed regex object, or an empty regex on failure.
 */
regex create_regex(const char *regex_expression);

/**
 * @brief Releases memory owned by a regex object.
 * @param r Regex object to clear.
 * @return No return value.
 */
void regex_free(regex *r);

/**
 * @brief Parses an infix regex and returns its explicit postfix representation.
 * @param infix_implicit Input regex sequence in implicit infix form.
 * @return Parsed regex sequence in explicit postfix form.
 */
regex parse_regex(const regex *infix_implicit);



#endif