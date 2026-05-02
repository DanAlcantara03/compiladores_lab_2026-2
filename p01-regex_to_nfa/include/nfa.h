#ifndef NFA_H
#define NFA_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "regex.h"
#include "data_structures.h"

#define MAX_STATES 64
#define MAX_SYMBOLS 256

/** Epsilon symbol. Represents the empty string. Using 240 because it's not a common
 * character in regex and is outside the standard ASCII range */
#define EPSILON_SYMBOL 240
#define INVALID_NFA_STATE UINT8_MAX


/**
 * @brief Struct to represent an alphabet. It contains an array of symbols and a mapping from characters
 * to their corresponding column index in the symbols array. The symbol_count field keeps track of how many
 * unique symbols are in the alphabet.
 */
typedef struct alphabet
{
    /* Array of symbols in the alphabet. The index of each symbol corresponds to its column in the transition table. */
    char symbols[256];
    /* Mapping from character to column index in the symbols array */
    int char_to_col[256];
    /* Number of symbols in the alphabet */
    int symbol_count;
} alphabet;

/**
 * @brief Function to initialize an alphabet with default values.
 * This function prepares an empty alphabet by clearing symbol storage,
 * resetting the character-to-column map, and setting the symbol counter to zero.
 * @return An initialized alphabet struct ready to receive symbols.
 */
alphabet new_alphabet();

/**
 * @brief Function to initialize an alphabet from a null-terminated C string.
 * This function reads symbols from the input string and inserts unique values into the alphabet in left-to-right order.
 * @param symbols Null-terminated input string containing symbols to include.
 * @return An initialized alphabet struct populated from the given string.
 */
alphabet alphabet_init_from_cstr(const char *symbols);

/**
 * @brief Function to initialize an alphabet from a char array.
 * This function reads exactly `count` entries from the input array and inserts unique values into the alphabet in array order.
 * @param symbols Input array containing symbols to include.
 * @param count Number of entries to read from the input array.
 * @return An initialized alphabet struct populated from the given array.
 */
alphabet alphabet_init_from_array(const char *symbols, size_t count);

/**
 * @brief Struct to represent a non-deterministic finite automaton (NFA). It contains the start state,
 * a bitset representing the accept states, the total number of states, the alphabet used by the NFA,
 * a transition table, and a cache for epsilon closures. The transition table is a 2D array where each
 * entry is a bitset representing the set of states reachable from the current state on the given symbol.
 */
typedef struct nfa
{
    /* State id for the start state */
    uint8_t start_state;
    /* Bitset representing accept states */
    uint64_t accept_states;
    /* Number of states in the NFA */
    uint8_t states;
    /* Alphabet used by the NFA */
    alphabet nfa_alphabet;
    /** Transition table. Each entry is a bitset representing the set of states reachable from the current state on the given symbol. A MAX_STATES x nfa_alphabet.symbol_count matrix. */
    uint64_t** transitions;
    /* Cache for epsilon closures. Each entry is a bitset representing
    the epsilon closure of the corresponding state. */
    uint64_t* epsilon_closure_cache;
} nfa;

/**
 * @brief Function to initialize an empty NFA instance.
 * This function returns a safe default NFA value that can be used as the initial
 * state before building transitions and accept states.
 * @return An initialized NFA struct with default values.
 */
nfa nfa_init(void);

/**
 * @brief Function to release dynamic memory owned by an NFA instance.
 * This function must free transition-related buffers and reset the NFA to a safe
 * state to avoid dangling pointers and double-free issues.
 * @param automaton Pointer to the NFA instance to free.
 */
void nfa_free(nfa *automaton);

/** 
 * @brief Convert a regular expression represented as a regex struct into an NFA.
 * This function uses a stack-based approach to construct the NFA from the postfix
 * representation of the regex.
 * @param r The input regular expression as a regex struct
 * @return An NFA struct representing the non-deterministic finite automaton
 * for the given regex.
 */
nfa regex_to_nfa(const regex r);

/**
 * @brief Function to check if a given input string matches the language defined by the NFA.
 * This function simulates the NFA on the input string and returns true if the NFA accepts
 * the string, and false otherwise.
 * @param automaton The NFA to simulate
 * @param input The input string to check against the NFA
 * @param input_length The length of the input string
 * @return true if the NFA accepts the input string, false otherwise
 */
bool match_nfa(nfa automaton, const char *input, size_t input_length);

/**
 * @brief Function to print an NFA in a human-readable format.
 * This helper is useful for debugging transition tables, start state and accept states.
 * @param automaton The NFA to print.
 * @param out Output stream destination, or NULL to use stdout.
 */
void nfa_print(nfa automaton, FILE *out);

#endif // NFA_H
