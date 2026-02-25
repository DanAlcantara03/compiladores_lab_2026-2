#include "nfa.h"

/**
 * @brief Struct to represent a transition in the NFA. It contains the source state, the symbol for the transition, and the destination state. This struct is used as an intermediate representation of transitions while building the NFA.
 */
typedef struct temp_transition {
    /* Represents a transition in the NFA */
    uint8_t from_state;
    /* The symbol for the transition */
    char symbol;
    /* The state to which the transition leads */
    uint8_t to_state;
} t_transition;

/**
 * @brief Struct to represent a temporary NFA during construction. It contains the start state and the end state. This struct is used as an intermediate representation while building the NFA from the regex.
 */
typedef struct temp_nfa {
    /* Start state of the temporary NFA */
    uint8_t start;
    /* End state of the temporary NFA */
    uint8_t end;
} t_nfa;

/**
 * @brief Struct to manage states and transitions during NFA construction. It keeps track of the next available state ID, the list of states, the list of transitions, and the alphabet used by the NFAs being constructed.
 */
typedef struct states_manager{
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
} states_manager;

// Function prototypes for internal helper functions

void epsilon_closure(nfa *automaton, uint8_t state);
void calculate_epsilon_closure(nfa *automaton);
nfa t_nfa_to_nfa(t_nfa temp_nfa, states_manager manager);

/**
 * @brief Validates manager pointer and internal counters.
 *
 * @param manager Pointer to validate.
 * @return true when the pointer is non-NULL and counters are in range.
 */
static bool manager_is_valid(const states_manager *manager) {
    if (manager == NULL) return false;
    if (manager->states_count > MAX_STATES) return false;
    if (manager->next_id > MAX_STATES) return false;
    return true;
}

/**
 * @brief Validate that a temporary fragment references existing manager states.
 *
 * @param manager States manager that owns the temporary fragment states.
 * @param fragment Temporary NFA fragment to validate.
 * @return true when manager metadata is valid and both fragment endpoints are in range.
 */
static bool manager_accepts_fragment(const states_manager *manager, t_nfa fragment) {
    if (!manager_is_valid(manager)) return false;
    if (manager->states_count == 0) return false;
    if (fragment.start >= manager->states_count) return false;
    if (fragment.end >= manager->states_count) return false;
    return true;
}

/**
 * @brief Build a canonical empty NFA value.
 *
 * The returned object owns no heap memory and can be safely passed to `nfa_free` multiple times.
 *
 * @return Empty initialized NFA.
 */
nfa nfa_init(void) {
    nfa automaton = {0};
    automaton.start_state = INVALID_NFA_STATE;
    automaton.nfa_alphabet = new_alphabet();
    return automaton;
}

/**
 * @brief Release all heap memory owned by an NFA and reset it.
 *
 * This function is idempotent: calling it multiple times on the same NFA is safe because it nulls pointers and restores canonical empty metadata.
 *
 * @param automaton Pointer to the NFA instance to release.
 */
void nfa_free(nfa *automaton) {
    if (automaton == NULL) return;

    if (automaton->transitions != NULL) {
        /* Free each state row when the state counter is within module bounds. */
        if (automaton->states <= MAX_STATES) {
            for (uint8_t s = 0; s < automaton->states; s++) {
                free(automaton->transitions[s]);
            }
        }
        free(automaton->transitions);
    }

    free(automaton->epsilon_closure_cache);
    *automaton = nfa_init();
}

/**
 * @brief Function to create a new alphabet. This function initializes an alphabet struct with default values, including setting the epsilon symbol and initializing the character-to-column mapping.
 */
alphabet new_alphabet() {
    alphabet a = {0};

    for (size_t i = 0; i < sizeof(a.char_to_col) / sizeof(a.char_to_col[0]); i++) {
        a.char_to_col[i] = -1;
    }

    a.symbols[0] = EPSILON_SYMBOL;
    a.char_to_col[(unsigned char) EPSILON_SYMBOL] = 0;
    a.symbol_count = 1;
    return a;
}

/**
 * @brief Function to add a symbol to the alphabet. This function checks if the symbol is already in teh alphabet, and if not, it adds the symbol to the symbols array and updates the character-to-column mapping
 *
 * The function is fail-safe and returns early when:
 * - the alphabet pointer is NULL,
 * - the input is '\0',
 * - the symbol already exists,
 * - the symbol is the epsilon marker,
 * - the alphabet is full (256 entries).
 *
 * @param a Pointer to the target alphabet.
 * @param symbol Symbol to insert.
 * @return true if the symbol was added, false otherwise.
 */
bool add_symbol(alphabet *a, char symbol) {
    if (a == NULL) return false;
    unsigned char u_symbol = (unsigned char) symbol;
    /* Ignore C-string terminator. */
    if (symbol == '\0') return false;
    /* Keep symbols unique. */
    if (a->char_to_col[u_symbol] != -1) return false;
    /* Epsilon stays reserved at column 0; do not insert it again. */
    if (u_symbol == (unsigned char)EPSILON_SYMBOL) return false;
    /* Defensive bound check for fixed-size storage. */
    if (a->symbol_count >= MAX_SYMBOLS) return false;

    /* Append at the next column and update reverse mapping. */
    a->symbols[a->symbol_count] = symbol;
    a->char_to_col[u_symbol] = a->symbol_count;
    a->symbol_count++;
    return true;
}

/**
 * @brief Function to create a new states manager. This function initializes a states_manager struct with default values, including setting the next available state ID to 0 and initializing the alphabet.
 * @return A new states_manager struct initialized with default values
 */
states_manager new_states_manager() {
    states_manager manager = {0};
    manager.manager_alphabet = new_alphabet();
    return manager;
}

/**
 * @brief Function to create a new state in the states manager. This function assigns a new state ID, adds it to the list of states, and returns the new state ID.
 * @param manager Pointer to the states_manager struct that manages the states
 * @return The ID of the newly created state
 */
uint8_t new_state(states_manager *manager) {
    if (!manager_is_valid(manager)) return INVALID_NFA_STATE;
    if (manager->states_count >= MAX_STATES) return INVALID_NFA_STATE;
    if (manager->next_id >= MAX_STATES) return INVALID_NFA_STATE;

    uint8_t id = manager->next_id;
    manager->states[manager->states_count] = id;
    manager->states_count ++;
    manager->next_id++;
    return id;
}

/**
 * @brief Function to add a transition to the states manager. This function creates a new transition struct and adds it to the list of transitions.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param from_state The state from which the transition originates
 * @param symbol The symbol on which the transition occurs
 * @param to_state The state to which the transition leads
 */
void add_transition(states_manager *manager, uint8_t from_state, char symbol, uint8_t to_state) {
    if (!manager_is_valid(manager)) return;
    if (manager->transitions_count == UINT8_MAX) return;
    if (from_state >= manager->next_id || to_state >= manager->next_id) return;

    manager->transitions[manager->transitions_count] = (t_transition){
        .from_state = from_state,
        .symbol = symbol,
        .to_state = to_state};
    manager->transitions_count ++;
}

/**
 * @brief Function to create a new NFA that represents the concatenation of two NFAs. This function takes two NFAs as input, creates a new NFA that has the start state of the first NFA and the end state of the second NFA, and adds an epsilon transition from the end state of the first NFA to the start state of the second NFA.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the first NFA
 * @param b Pointer to the second NFA
 * @return A new NFA struct representing the concatenation of the two input NFAs
 */
t_nfa concat_nfa(states_manager *manager, t_nfa *a, t_nfa *b) {
    t_nfa result = {0};
    if (!manager_is_valid(manager)) return result;

    result.start = a->start;
    result.end = b->end;
    add_transition(manager, a->end, EPSILON_SYMBOL, b->start);
    return result;
}

/**
 * @brief Function to create a new NFA that represents a single symbol. This function creates a new NFA with a start state and an end state, and adds a transition from the start state to the end state on the given symbol.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param symbol The symbol for which the NFA should be created
 * @return A new NFA struct representing the given symbol
 */
t_nfa symbol_nfa(states_manager *manager, char symbol) {
    t_nfa result = {0};
    if (!manager_is_valid(manager)) return result;
    add_symbol(&manager->manager_alphabet, symbol);
    result.start = new_state(manager);
    result.end = new_state(manager);
    add_transition(manager, result.start, symbol, result.end);
    return result;
}

/**
 * @brief Function to create a new NFA that represents the union of two NFAs. This function takes two NFAs as input, creates a new NFA that has a new start state and a new end state, and adds epsilon transitions from the new start state to the start states of both input NFAs, and from the end states of both input NFAs to the new end state.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the first NFA
 * @param b Pointer to the second NFA
 * @return A new NFA struct representing the union of the two input NFAs
 */
t_nfa union_nfa(states_manager *manager, t_nfa *a, t_nfa *b) {
    t_nfa result = {0};
    if (!manager_is_valid(manager)) return result;
    result.start = new_state(manager);
    result.end = new_state(manager);
    add_transition(manager, result.start, EPSILON_SYMBOL, a->start);
    add_transition(manager, a->end, EPSILON_SYMBOL, result.end);
    add_transition(manager, result.start, EPSILON_SYMBOL, b->start);
    add_transition(manager, b->end, EPSILON_SYMBOL, result.end);
    return result;
}

/**
 * @brief Function to create a new NFA that represents the positive closure of an NFA.
 * This function takes an NFA as input, creates a new NFA that has a new start state and a new end state, and adds epsilon transitions from the new start state to the start state of the input NFA, from the end state of the input NFA to the start state of the input NFA, and from the end state of the input NFA to the new end state.
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the input NFA
 * @return A new NFA struct representing the positive closure of the input NFA
 */
t_nfa positive_closure_nfa(states_manager *manager, t_nfa *a) {
    t_nfa result = {0};
    if (!manager_is_valid(manager)) return result;
    result.start = new_state(manager);
    result.end = new_state(manager);
    add_transition(manager, result.start, EPSILON_SYMBOL, a->start);
    add_transition(manager, a->end, EPSILON_SYMBOL, a->start);
    add_transition(manager, a->end, EPSILON_SYMBOL, result.end);
    return result;
}


/**
 * @brief Function to create a new NFA that represents the Kleene closure of an NFA. 
 * This function takes an NFA as input, creates a new NFA that has a new start state and a new end state, and adds epsilon transitions from the new start state to the start state of the input NFA, from the end state of the input NFA to the start state of the input NFA, from the end state of the input NFA to the new end state, and from the new start state to the new end state (to allow for the empty string).
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the input NFA
 * @return A new NFA struct representing the Kleene closure of the input NFA
 */
t_nfa kleene_closure_nfa(states_manager *manager, t_nfa *a) {
    t_nfa result = {0};
    if (!manager_is_valid(manager)) return result; 
    result = positive_closure_nfa(manager, a);
    add_transition(manager, result.start, EPSILON_SYMBOL, result.end);
    return result;
}

/**
 * @brief Function to create a new NFA that represents the optionality of an NFA. This function takes an NFA as input, creates a new NFA that adds epsilon transitions from the start state of the input NFA to the end state (to allow for the empty string).
 * @param manager Pointer to the states_manager struct that manages the states and transitions
 * @param a Pointer to the input NFA
 * @return A new NFA struct representing the optionality of the input NFA
 */
t_nfa optional_nfa(states_manager *manager, t_nfa *a) {
    t_nfa result = *a;
    if (!manager_is_valid(manager)) return result;
    add_transition(manager, result.start, EPSILON_SYMBOL, result.end);
    return result;
}

/**
 * @brief Build an NFA from a regex encoded as postfix tokens.
 *
 * This implementation applies Thompson construction with a stack of temporary fragments (`t_nfa`, storing only start/end states). Literals push base fragments; operators pop one or two fragments and push the combined result.
 *
 * The function expects @p r to contain valid postfix tokens (for example, the output of `regex_parse`). If any stack operation fails, a helper builder returns invalid states, or the postfix expression is malformed, construction stops and returns `nfa_init()`.
 *
 * @param r Regular expression in postfix representation.
 * @return A constructed NFA on success, or `nfa_init()` on failure.
 */
nfa regex_to_nfa(const regex r) {

    nfa result = nfa_init();

    /* Reject empty/null regex input early. */
    if (r.size == 0 || r.items == NULL) {
        return result;
    }

    states_manager manager = new_states_manager();

    /* Stack of partial Thompson fragments built from processed tokens. */
    ds_stack fragments = {0};
    if (ds_stack_init(&fragments, sizeof(t_nfa)) != DS_OK) {
        return result;
    }
    /* Worst case: every token pushes one fragment. */
    if (ds_stack_reserve(&fragments, r.size) != DS_OK) {
        ds_stack_free(&fragments);
        return result;
    }

    bool ok = true;

    /* Consume postfix tokens left-to-right and reduce to one fragment. */
    for (size_t i = 0; i < r.size && ok; i++) {
        char token = r.items[i];

        switch (token) {
            case REGEX_OP_CONCAT: {
                t_nfa right = {0};
                t_nfa left = {0};
                if (ds_stack_pop(&fragments, &right) != DS_OK) { ok = false; break; }
                if (ds_stack_pop(&fragments, &left) != DS_OK) { ok = false; break; }

                t_nfa combined = concat_nfa(&manager, &left, &right);
                if (ds_stack_push(&fragments, &combined) != DS_OK) ok = false;
                break;
            }

            case REGEX_OP_OR: {
                t_nfa right = {0};
                t_nfa left = {0};
                if (ds_stack_pop(&fragments, &right) != DS_OK) { ok = false; break; }
                if (ds_stack_pop(&fragments, &left) != DS_OK) { ok = false; break; }

                t_nfa combined = union_nfa(&manager, &left, &right);
                if (combined.start == INVALID_NFA_STATE || combined.end == INVALID_NFA_STATE) {
                    ok = false;
                    break;
                }
                if (ds_stack_push(&fragments, &combined) != DS_OK) ok = false;
                break;
            }

            case REGEX_OP_KLEENE_STAR: {
                t_nfa a = {0};
                if (ds_stack_pop(&fragments, &a) != DS_OK) { ok = false; break; }

                t_nfa closed = kleene_closure_nfa(&manager, &a);
                if (closed.start == INVALID_NFA_STATE || closed.end == INVALID_NFA_STATE) {
                    ok = false;
                    break;
                }
                if (ds_stack_push(&fragments, &closed) != DS_OK) ok = false;
                break;
            }

            case REGEX_OP_POSITIVE_CLOSURE: {
                t_nfa a = {0};
                if (ds_stack_pop(&fragments, &a) != DS_OK) { ok = false; break; }

                t_nfa closed = positive_closure_nfa(&manager, &a);
                if (closed.start == INVALID_NFA_STATE || closed.end == INVALID_NFA_STATE) {
                    ok = false;
                    break;
                }
                if (ds_stack_push(&fragments, &closed) != DS_OK) ok = false;
                break;
            }

            case REGEX_OP_OPTIONAL: {
                t_nfa a = {0};
                if (ds_stack_pop(&fragments, &a) != DS_OK) { ok = false; break; }

                t_nfa opt = optional_nfa(&manager, &a);
                if (ds_stack_push(&fragments, &opt) != DS_OK) ok = false;
                break;
            }

            default: {
                /* Operands become base fragments with one labeled transition. */
                t_nfa symbol = symbol_nfa(&manager, token);
                if (symbol.start == INVALID_NFA_STATE || symbol.end == INVALID_NFA_STATE) {
                    ok = false;
                    break;
                }
                if (ds_stack_push(&fragments, &symbol) != DS_OK) ok = false;
                break;
            }
        }
    }

    /* A valid postfix regex must collapse to exactly one root fragment. */
    if (!ok || ds_stack_size(&fragments) != 1) {
        ds_stack_free(&fragments);
        return nfa_init();
    }

    t_nfa root = {0};
    if (ds_stack_pop(&fragments, &root) != DS_OK) {
        ds_stack_free(&fragments);
        return nfa_init();
    }

    ds_stack_free(&fragments);
    /* Materialize final transition table and epsilon-closure cache. */
    result = t_nfa_to_nfa(root, manager);
    return result;
}

/**
 * @brief Function to convert a temporary NFA representation (t_nfa) into the final NFA struct. This function takes the start and end states from the temporary NFA, initializes the transition table based on the transitions stored in the states manager, and calculates the epsilon closures for all states.
 *
 * Implementation overview:
 * - Validates manager counters and temporary boundary states before allocating memory.
 * - Builds a dense transition matrix of shape `states x alphabet_size`, where each cell is a 64-bit bitset of destination states.
 * - Replays every temporary transition from `manager.transitions` into the matrix using `char_to_col` to map symbols to transition columns.
 * - Computes epsilon-closure cache once at the end so matching can reuse it.
 *
 * Memory behavior:
 * - Allocates the outer array of row pointers first.
 * - Allocates each row independently.
 * - On partial allocation failure, frees already allocated rows via `nfa_free` and returns `nfa_init()`.
 *
 * @param temp_nfa The temporary NFA representation containing the start and end states
 * @param manager The states_manager struct that contains the transitions and alphabet information
 * @return An NFA struct representing the final non-deterministic finite automaton
 */
nfa t_nfa_to_nfa(t_nfa temp_nfa, states_manager manager) {
    /* Default return value for any validation/allocation failure path. */
    nfa result = nfa_init();

    /* Validate manager metadata and start/end ids from temporary fragment. */
    if (!manager_accepts_fragment(&manager, temp_nfa)) return result;

    /* Transfer high-level automaton metadata. */
    result.start_state = temp_nfa.start;
    result.accept_states = (1ULL << temp_nfa.end);
    result.states = manager.states_count;
    result.nfa_alphabet = manager.manager_alphabet;

    /* Allocate rows: one pointer per state. */
    result.transitions = (uint64_t **)calloc(result.states, sizeof(uint64_t *));
    if (result.transitions == NULL) return nfa_init();

    /* Allocate columns per row: one bitset cell per alphabet symbol. */
    bool ok = true;
    for (uint8_t s = 0; s < result.states && ok; s++) {
        result.transitions[s] = (uint64_t *)calloc((size_t)result.nfa_alphabet.symbol_count, sizeof(uint64_t));
        if (result.transitions[s] == NULL) {
            ok = false;
            break;
        }
    }

    /* Roll back partially built matrix if any row allocation failed. */
    if (!ok) {
        nfa_free(&result);
        return result;
    }

    /* Replay temporary transitions into the final matrix representation. */
    for (uint8_t i = 0; i < manager.transitions_count; i++) {
        t_transition t = manager.transitions[i];
        /* Defensive bounds checks in case manager data contains stale edges. */
        if (t.from_state >= result.states || t.to_state >= result.states) continue;

        int col = result.nfa_alphabet.char_to_col[(unsigned char)t.symbol];
        /* Ignore symbols that are not mapped in the final alphabet. */
        if (col < 0 || col >= result.nfa_alphabet.symbol_count) continue;

        /* Set bit `to_state` in the destination set for (from_state, symbol). */
        result.transitions[t.from_state][col] |= (1ULL << t.to_state);
    }

    /* Precompute epsilon closures so match_nfa can use cached values directly. */
    calculate_epsilon_closure(&result);
    return result;
}

/**
 * @brief Function to calculate the epsilon closure for all states in the given NFA.
 * - This function initializes a cache to store the epsilon closures and computes the closure for each state using a depth-first search approach.
 * 
 * Implementation details:
 * - Reallocates the epsilon-closure cache every call to avoid stale values.
 * - Stores one 64-bit bitset per state in `epsilon_closure_cache`.
 * - Iterates all states and delegates each closure computation to
 *   `epsilon_closure`, which performs the graph traversal on epsilon edges.
 *
 * If allocation fails, the function returns early and leaves the cache as NULL.
 *
 * @param automaton Pointer to the NFA for which epsilon closures are to be calculated
 */
void calculate_epsilon_closure(nfa *automaton) {
    /* Validate required structure before touching cache or transitions. */
    if (automaton == NULL) return;
    if (automaton->states == 0) return;
    if (automaton->transitions == NULL) return;

    /* Rebuild cache from scratch to keep it consistent with current transitions. */
    free(automaton->epsilon_closure_cache);
    /* One 64-bit bitset per state: cache[state] = epsilon-closure(state). */
    automaton->epsilon_closure_cache = (uint64_t *)calloc(automaton->states, sizeof(uint64_t));
    if (automaton->epsilon_closure_cache == NULL) return;

    /* Fill cache entry-by-entry by delegating each computation to epsilon_closure. */
    for (uint8_t state = 0; state < automaton->states; state++) {
        epsilon_closure(automaton, state);
    }
}

/**
 * @brief Compute and cache the epsilon-closure of one state.
 *
 * Implementation details:
 * - The closure is represented as a 64-bit bitset (`closure`), where bit `i` means state `i` is epsilon-reachable.
 * - Traversal uses a fixed-size FIFO queue (`queue`, `head`, `tail`), so this is a breadth-first exploration over epsilon edges.
 * - `visited[]` prevents enqueuing the same state multiple times and avoids infinite loops on epsilon cycles.
 * - Epsilon moves are read from one transition-table column identified by `EPSILON_SYMBOL`; each row entry is itself a bitset of destination states.
 *
 * If the epsilon symbol is not present in the alphabet mapping, the closure is just the state itself. The result is written to `automaton->epsilon_closure_cache[state]`.
 *
 * @param automaton Pointer to the NFA.
 * @param state State for which the closure is computed.
 */
void epsilon_closure(nfa *automaton, uint8_t state) {
    /* Validate prerequisites for reading transitions and writing cache. */
    if (automaton == NULL) return;
    if (automaton->transitions == NULL) return;
    if (automaton->epsilon_closure_cache == NULL) return;
    if (state >= automaton->states) return;

    int epsilon_col = automaton->nfa_alphabet.char_to_col[(unsigned char)EPSILON_SYMBOL];
    /* If epsilon is absent from alphabet, closure contains only the seed state. */
    if (epsilon_col < 0 || epsilon_col >= automaton->nfa_alphabet.symbol_count) {
        automaton->epsilon_closure_cache[state] = (1ULL << state);
        return;
    }

    /* BFS workspace over finite state ids [0, automaton->states). */
    bool visited[MAX_STATES] = { false };
    uint8_t queue[MAX_STATES] = {0};
    uint8_t head = 0;
    uint8_t tail = 0;
    uint64_t closure = 0;

    /* Seed with the queried state (reflexive closure). */
    visited[state] = true;
    queue[tail++] = state;
    /* We start with the state */
    closure |= (1ULL << state);

    /* Expand all states reachable through repeated epsilon transitions. */
    while (head < tail) {
        uint8_t current = queue[head++];
        uint64_t epsilon_moves = automaton->transitions[current][epsilon_col];

        for (uint8_t next = 0; next < automaton->states; next++) {
            if ((epsilon_moves & (1ULL << next)) == 0) continue;
            if (visited[next]) continue;

            visited[next] = true;
            closure |= (1ULL << next);
            queue[tail++] = next;
        }
    }

    /* Persist closure for later reuse by matcher/simulator code. */
    automaton->epsilon_closure_cache[state] = closure;
}

/**
 * @brief Simulate an NFA over an input string using bitset state sets.
 *
 * Implementation details:
 * - `current` stores the active state set as a 64-bit bitmask.
 * - Before consuming input, `current` is initialized with the epsilon-closure of the start state.
 * - For each input symbol, the function performs two phases:
 *   1) move: union transitions on the symbol from all active states
 *   2) expand: union epsilon-closures of all moved-to states
 *
 * The function can work with an NFA that already has an epsilon-closure cache, or lazily compute a temporary one when missing. On any validation failure, unknown symbol, or dead configuration, it rejects early.
 *
 * @param automaton NFA to simulate.
 * @param input Input buffer (may be NULL only when @p input_length is zero).
 * @param input_length Number of symbols to consume.
 * @return true when at least one active state is accepting after full
 *         consumption; false otherwise.
 */
bool match_nfa(nfa automaton, const char *input, size_t input_length) {
    /* Basic structural validation before simulation. */
    if (automaton.states == 0 || automaton.transitions == NULL) return false;
    if (automaton.start_state >= automaton.states) return false;
    if (input == NULL && input_length > 0) return false;

    /* Build a cache on demand; free it only if this function allocated it. */
    bool owns_local_cache = false;
    if (automaton.epsilon_closure_cache == NULL) {
        calculate_epsilon_closure(&automaton);
        if (automaton.epsilon_closure_cache == NULL) return false;
        owns_local_cache = true;
    }

    /* Start from epsilon-closure(start). */
    uint64_t current = automaton.epsilon_closure_cache[automaton.start_state];
    bool accepted = false;

    for (size_t i = 0; i < input_length; i++) {
        unsigned char symbol = (unsigned char)input[i];
        int col = automaton.nfa_alphabet.char_to_col[symbol];
        /* Reject symbols not present in the automaton alphabet. */
        if (col < 0 || col >= automaton.nfa_alphabet.symbol_count) goto clear_automata;

        /* Move on the current symbol from all active states. */
        uint64_t moved = 0;
        for (uint8_t s = 0; s < automaton.states; s++) {
            if ((current & (1ULL << s)) == 0) continue;
            moved |= automaton.transitions[s][col];
        }

        /* No reachable state after consuming this symbol -> reject. */
        if (moved == 0) goto clear_automata;

        /* Expand with epsilon-closures of the moved-to states. */
        uint64_t expanded = 0;
        for (uint8_t s = 0; s < automaton.states; s++) {
            if ((moved & (1ULL << s)) == 0) continue;
            expanded |= automaton.epsilon_closure_cache[s];
        }

        current = expanded;
        /* Defensive rejection for an empty active set. */
        if (current == 0) goto clear_automata;
    }

    /* Accept if any active state is in the accept set. */
    accepted = (current & automaton.accept_states) != 0;
clear_automata:
    if (owns_local_cache) free(automaton.epsilon_closure_cache);
    return accepted;
}

/**
 * @brief Print the NFA in a compact, debug-friendly format.
 *
 * @param automaton NFA instance to print.
 * @param out Destination stream, or NULL to use stdout.
 */
void nfa_print(nfa automaton, FILE *out) {
    FILE *dest = out ? out : stdout;
    fprintf(dest, "NFA{states=%u,start=%u,accept=0x%016llx}\n",
            automaton.states,
            automaton.start_state,
            (unsigned long long)automaton.accept_states);

    fprintf(dest, "alphabet:");
    for (int col = 0; col < automaton.nfa_alphabet.symbol_count; col++) {
        unsigned char sym = (unsigned char)automaton.nfa_alphabet.symbols[col];
        if (sym == (unsigned char)EPSILON_SYMBOL) {
            fprintf(dest, " [eps]");
        } else if (sym >= 32 && sym <= 126) {
            fprintf(dest, " [%c]", (char)sym);
        } else {
            fprintf(dest, " [0x%02x]", sym);
        }
    }
    fputc('\n', dest);

    if (automaton.transitions == NULL || automaton.states == 0 || automaton.nfa_alphabet.symbol_count <= 0) {
        return;
    }

    for (uint8_t from = 0; from < automaton.states; from++) {
        for (int col = 0; col < automaton.nfa_alphabet.symbol_count; col++) {
            uint64_t to_set = automaton.transitions[from][col];
            if (to_set == 0) continue;

            unsigned char sym = (unsigned char)automaton.nfa_alphabet.symbols[col];
            if (sym == (unsigned char)EPSILON_SYMBOL) {
                fprintf(dest, "q%u -eps-> ", from);
            } else if (sym >= 32 && sym <= 126) {
                fprintf(dest, "q%u -%c-> ", from, (char)sym);
            } else {
                fprintf(dest, "q%u -0x%02x-> ", from, sym);
            }

            fputc('{', dest);
            bool first = true;
            for (uint8_t to = 0; to < automaton.states; to++) {
                if ((to_set & (1ULL << to)) == 0) continue;
                if (!first) fputc(',', dest);
                fprintf(dest, "q%u", to);
                first = false;
            }
            fprintf(dest, "}\n");
        }
    }
}
