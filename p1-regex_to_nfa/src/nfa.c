#include "nfa.h"

/**
 * @brief Struct to represent a transition in the NFA. It contains the source state,
 * the symbol for the transition, and the destination state. This struct is used as
 * an intermediate representation of transitions while building the NFA.
 */
struct temp_transition
{
    /* Represents a transition in the NFA */
    uint8_t from_state;
    /* The symbol for the transition */
    char symbol;
    /* The state to which the transition leads */
    uint8_t to_state;
};
typedef struct temp_transition t_transition;

/**
 * @brief Struct to represent a temporary NFA during construction. It contains the start
 * state and the end state. This struct is used as an intermediate representation while
 * building the NFA from the regex.
 */
struct temp_nfa
{
    /* Start state of the temporary NFA */
    uint8_t start;
    /* End state of the temporary NFA */
    uint8_t end;
};
typedef struct temp_nfa t_nfa;

/**
 * @brief Struct to manage states and transitions during NFA construction. It keeps track
 * of the next available state ID, the list of states, the list of transitions, and the
 * alphabet used by the NFAs being constructed.
 */
struct states_manager
{
    /* Next available state ID */
    uint8_t next_id;

    /* List of states managed */
    uint8_t states[MAX_STATES];
    uint8_t states_count;

    /* List of transitions managed */
    t_transition transitions[MAX_STATES * MAX_STATES];
    uint8_t transitions_count;

    /* Alphabet used by the states manager */
    alphabet manager_alphabet;
};
typedef struct states_manager states_manager;

// Function prototypes for internal helper functions

void epsilon_closure(nfa *automaton, uint8_t state);
void calculate_epsilon_closure(nfa *automaton);
nfa t_nfa_to_nfa(t_nfa temp_nfa, states_manager manager);

/**
 * @brief Function to create a new alphabet. This function initializes an alphabet struct with
 * default values, including setting the epsilon symbol and initializing the character-to-column
 * mapping.
 */
alphabet new_alphabet()
{
    // TODO: Initialize alphabet with epsilon at column 0.
    // Suggested algorithm:
    // 1) Initialize char_to_col to -1 and symbols to 0.
    // 2) Place EPSILON_SYMBOL at symbols[0].
    // 3) Map EPSILON_SYMBOL -> 0 and set symbol_count = 1.
    alphabet a = {0};
    return a;
}

/**
 * @brief Function to add a symbol to the alphabet. This function checks if the symbol is already
 * in the alphabet, and if not, it adds the symbol to the symbols array and updates the
 * character-to-column mapping.
 * @param a Pointer to the alphabet struct to which the symbol should be added
 * @param symbol The symbol to add to the alphabet
 */
void add_symbol(alphabet *a, char symbol)
{
    (void)a;
    (void)symbol;
    // TODO: Insert non-epsilon symbol if absent.
    // Suggested algorithm:
    // 1) Reject EPSILON_SYMBOL and duplicates via char_to_col lookup.
    // 2) Append symbol into symbols[symbol_count].
    // 3) Update char_to_col and increment symbol_count.
}

/**
 * @brief Function to create a new states manager. This function initializes a states_manager struct
 * with default values, including setting the next available state ID to 0 and initializing the
 * alphabet.
 * @return A new states_manager struct initialized with default values
 */
states_manager new_states_manager()
{
    // TODO: Initialize state manager with empty states/transitions and fresh alphabet.
    states_manager manager = {0};
    return manager;
}

/**
 * @brief Function to create a new state in the states manager. This function assigns a new state ID,
 * adds it to the list of states, and returns the new state ID.
 * @param manager Pointer to the states_manager struct that manages the states
 * @return The ID of the newly created state
 */
uint8_t new_state(states_manager *manager)
{
    (void)manager;
    // TODO: Allocate a new state id, store in manager->states, and advance counters.
    return 0;
}

/**
 * @brief Function to add a transition to the states manager. This function creates a new transition
 * struct and adds it to the list of transitions.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param from_state The state from which the transition originates
 * @param symbol The symbol on which the transition occurs
 * @param to_state The state to which the transition leads
 */
void add_transition(states_manager *manager, uint8_t from_state, char symbol, uint8_t to_state)
{
    (void)manager;
    (void)from_state;
    (void)symbol;
    (void)to_state;
    // TODO: Append transition record to manager->transitions and increment count.
}

/**
 * @brief Function to create a new NFA that represents the concatenation of two NFAs. This function
 * takes two NFAs as input, creates a new NFA that has the start state of the first NFA and the end
 * state of the second NFA, and adds an epsilon transition from the end state of the first NFA to
 * the start state of the second NFA.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the first NFA
 * @param b Pointer to the second NFA
 * @return A new NFA struct representing the concatenation of the two input NFAs
 */
t_nfa concat_nfa(states_manager *manager, t_nfa *a, t_nfa *b)
{
    (void)manager;
    (void)a;
    (void)b;
    // TODO: Build concatenation fragment.
    // Suggested algorithm:
    // 1) start = a->start, end = b->end.
    // 2) Add epsilon transition a->end -> b->start.
    t_nfa result = {0};
    return result;
}

/**
 * @brief Function to create a new NFA that represents a single symbol. This function creates a new NFA
 * with a start state and an end state, and adds a transition from the start state to the end state on
 * the given symbol.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param symbol The symbol for which the NFA should be created
 * @return A new NFA struct representing the given symbol
 */
t_nfa symbol_nfa(states_manager *manager, char symbol)
{
    (void)manager;
    (void)symbol;
    // TODO: Build a 2-state fragment with one symbol transition start -> end.
    t_nfa result = {0};
    return result;
}

/**
 * @brief Function to create a new NFA that represents the union of two NFAs. This function takes two NFAs
 * as input, creates a new NFA that has a new start state and a new end state, and adds epsilon transitions
 * from the new start state to the start states of both input NFAs, and from the end states of both input
 * NFAs to the new end state.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the first NFA
 * @param b Pointer to the second NFA
 * @return A new NFA struct representing the union of the two input NFAs
 */
t_nfa union_nfa(states_manager *manager, t_nfa *a, t_nfa *b)
{
    (void)manager;
    (void)a;
    (void)b;
    // TODO: Build alternation fragment with new start/end and 4 epsilon transitions.
    t_nfa result = {0};
    return result;
}

/**
 * @brief Function to create a new NFA that represents the positive closure of an NFA.
 * This function takes an NFA as input, creates a new NFA that has a new start state and
 * a new end state, and adds epsilon transitions from the new start state to the start
 * state of the input NFA, from the end state of the input NFA to the start state of the
 * input NFA, and from the end state of the input NFA to the new end state.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the input NFA
 * @return A new NFA struct representing the positive closure of the input NFA
 */
t_nfa positive_closure_nfa(states_manager *manager, t_nfa *a)
{
    (void)manager;
    (void)a;
    // TODO: Build positive-closure fragment.
    // Suggested algorithm:
    // 1) Create new start/end.
    // 2) Epsilon start -> a.start.
    // 3) Epsilon a.end -> a.start (loop).
    // 4) Epsilon a.end -> end.
    t_nfa result = {0};
    return result;
}


/**
 * @brief Function to create a new NFA that represents the Kleene closure of an NFA. This function
 * takes an NFA as input, creates a new NFA that has a new start state and a new end state, and adds
 * epsilon transitions from the new start state to the start state of the input NFA, from
 * the end state of the input NFA to the start state of the input NFA, from the end state of the input
 * NFA to the new end state, and from the new start state to the new end state (to allow for the empty
 * string).
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the input NFA
 * @return A new NFA struct representing the Kleene closure of the input NFA
 */
t_nfa kleene_closure_nfa(states_manager *manager, t_nfa *a)
{
    (void)manager;
    (void)a;
    // TODO: Build Kleene-star fragment (positive closure + epsilon start->end).
    t_nfa result = {0};
    return result;
}

/**
 * @brief Function to create a new NFA that represents the optionality of an NFA. This function takes
 * an NFA as input, creates a new NFA that adds epsilon transitions from the start state of the input
 * NFA to the end state (to allow for the empty string).
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the input NFA
 * @return A new NFA struct representing the optionality of the input NFA
 */
t_nfa optional_nfa(states_manager *manager, t_nfa *a)
{
    (void)manager;
    (void)a;
    // TODO: Build optional fragment by adding epsilon path that skips sub-NFA.
    t_nfa result = {0};
    return result;
}


nfa regex_to_nfa(const regex r)
{
    (void)r;
    // TODO: Convert postfix regex into NFA via Thompson construction.
    // Suggested algorithm:
    // 1) Iterate tokens using a stack of temporary fragments.
    // 2) Push symbol fragment for operands.
    // 3) Pop/apply operators (concat, alternation, unary closures).
    // 4) Validate stack ends with exactly one fragment.
    // 5) Materialize transition table and epsilon-closure cache.
    nfa result = {0};
    return result;
}

/**
 * @brief Function to convert a temporary NFA representation (t_nfa) into the final NFA struct. This function
 * takes the start and end states from the temporary NFA, initializes the transition table based on the
 * transitions stored in the states manager, and calculates the epsilon closures for all states.
 * @param temp_nfa The temporary NFA representation containing the start and end states
 * @param manager The states_manager struct that contains the transitions and alphabet information
 * @return An NFA struct representing the final non-deterministic finite automaton
 */
nfa t_nfa_to_nfa(t_nfa temp_nfa, states_manager manager)
{
    (void)temp_nfa;
    (void)manager;
    // TODO: Convert temporary builder representation into final NFA struct.
    // Suggested algorithm:
    // 1) Initialize metadata (start, accept mask, state count, alphabet).
    // 2) Allocate transition matrix and zero-initialize.
    // 3) Populate transitions from manager records.
    // 4) Precompute epsilon closures.
    nfa result = {0};
    return result;
}

/**
 * @brief Function to calculate the epsilon closure for all states in the given NFA.
 * This function initializes a cache to store the epsilon closures and computes the closure
 * for each state using a depth-first search approach.
 * @param automaton Pointer to the NFA for which epsilon closures are to be calculated
 */
void calculate_epsilon_closure(nfa *automaton)
{
    (void)automaton;
    // TODO: Allocate and fill epsilon-closure cache for every state.
    // Suggested algorithm:
    // 1) Allocate cache array size automaton->states.
    // 2) Mark entries as "not computed".
    // 3) Invoke epsilon_closure() for each state.
}

/**
 * @brief Function to compute the epsilon closure for a given state in the NFA.
 * This function uses a depth-first search approach to find all states reachable
 * from the given state via epsilon transitions.
 * @param automaton Pointer to the NFA
 * @param state The state for which the epsilon closure is to be computed
 */
void epsilon_closure(nfa *automaton, uint8_t state)
{
    (void)automaton;
    (void)state;
    // TODO: Compute epsilon closure for one state using DFS/BFS.
    // Suggested algorithm:
    // 1) Seed closure with the input state.
    // 2) Traverse epsilon edges using stack/queue.
    // 3) Add each discovered state exactly once.
    // 4) Store resulting bitset into cache[state].
}

bool match_nfa(nfa automaton, const char *input, size_t input_length)
{
    (void)automaton;
    (void)input;
    (void)input_length;
    // TODO: Simulate NFA with epsilon closures.
    // Suggested algorithm:
    // 1) current = epsilon_closure(start_state).
    // 2) For each symbol, compute move(current, symbol).
    // 3) Expand via epsilon closures of reached states.
    // 4) Early reject if current set becomes empty.
    // 5) Accept if current intersects accept_states.
    return false;
}