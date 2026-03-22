#include "analyzer.h"

/**
 * @brief Finds a terminal identifier by terminal name.
 * @param g Parsed grammar.
 * @param name Terminal name to search.
 * @return Terminal id if found, otherwise -1.
 */
static int find_terminal_id(const grammar *g, const char *name)
{
	if (g == NULL || name == NULL || g->terminals == NULL || g->num_terminals <= 0) { 
		return -1; 
	}
	size_t name_length = strlen(name);
	char first_char = name[0];
	for (int i = 0; i < g->num_terminals; i++) {
		if (g->terminals[i].symbol == NULL) { 
			continue; 
		}
		if ((size_t)g->terminals[i].symbol_length != name_length) {
			continue; 
		}
		if (g->terminals[i].symbol[0] != first_char){ 
			continue; 
		}
		if (strcmp(g->terminals[i].symbol, name) == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * @brief Appends one symbol to a dynamically sized symbol array.
 * @param arr Target array pointer.
 * @param count Current element count; incremented on success.
 * @param text Symbol text to copy.
 * @param is_terminal Indicates terminal/non-terminal role.
 * @return true on success, false on allocation failure.
 */
static bool add_symbol_to_array(symbol **arr, int *count, const char *text, bool is_terminal)
{

	if (arr == NULL || count == NULL || text == NULL || *count < 0) {
		return false;
	}
	symbol *resized = (symbol *)realloc(*arr, (size_t)(*count + 1) *sizeof(symbol));
	if (resized == NULL) { 
		return false; 
	}
	*arr = resized;
	char *symbol_copy = strdup(text);
	if (symbol_copy == NULL) { 
		return false; 
	}
	(*arr)[*count].symbol = symbol_copy;
	(*arr)[*count].symbol_length = (int)strlen(text);
	(*arr)[*count].is_terminal = is_terminal;
	(*count)++;
	return true;

}


/**
 * @brief Builds FIRST and nullable tables for all non-terminals.
 * @param g Parsed grammar.
 * @param first_table Output flattened table: non-terminal x terminal.
 * @param nullable Output nullable flags per non-terminal.
 * @param epsilon_id Output id of terminal "epsilon", or -1 if absent.
 * @return true when tables were built, false on invalid input or allocation error.
 */
static bool compute_first_tables(const grammar *g, bool **first_table, bool **nullable, int *epsilon_id)
{
	// Reject invalid inputs early because all outputs are heap-allocated.
	if (g == NULL || first_table == NULL || nullable == NULL || epsilon_id == NULL || g->num_non_terminals <= 0) {
		return false;
	}

	*first_table = NULL;
	*nullable = NULL;
	// "epsilon" is handled separately through nullable flags.
	*epsilon_id = find_terminal_id(g, "epsilon");

	size_t first_size = (size_t)g->num_non_terminals * (size_t)g->num_terminals;
	bool *first = NULL;
	if (first_size > 0) {
		// FIRST[A, t] is true when terminal t belongs to FIRST(A).
		first = (bool *)calloc(first_size, sizeof(bool));
		if (first == NULL) {
			return false;
		}
	}

	// nullable[A] is true when non-terminal A can derive epsilon.
	bool *nullable_flags = (bool *)calloc((size_t)g->num_non_terminals, sizeof(bool));
	if (nullable_flags == NULL) {
		free(first);
		return false;
	}

	bool changed;
	do {
		// Repeat until a full pass adds no new FIRST or nullable information.
		changed = false;

		for (int i = 0; i < g->num_productions; i++) {
			const production *prod = &g->productions[i];
			int lhs = prod->non_terminal_id;

			if (lhs < 0 || lhs >= g->num_non_terminals) {
				continue;
			}

			bool production_nullable = true;

			for (int j = 0; j < prod->production_length; j++) {
				int symbol_id = prod->production_symbol_ids[j];

				if (symbol_id >= 0 && symbol_id < g->num_terminals) {
					// A concrete terminal contributes directly to FIRST(lhs) and stops the scan.
					if (symbol_id == *epsilon_id) {
						continue;
					}

					if (first != NULL && !first[(lhs * g->num_terminals) + symbol_id]) {
						first[(lhs * g->num_terminals) + symbol_id] = true;
						changed = true;
					}
					production_nullable = false;
					break;
				}

				int rhs_non_terminal_id = symbol_id - g->num_terminals;
				if (rhs_non_terminal_id < 0 || rhs_non_terminal_id >= g->num_non_terminals) {
					production_nullable = false;
					break;
				}

				// Merge FIRST(rhs symbol) into FIRST(lhs), excluding epsilon.
				for (int terminal_id = 0; terminal_id < g->num_terminals; terminal_id++) {
					if (terminal_id == *epsilon_id) {
						continue;
					}
					if (first == NULL || !first[(rhs_non_terminal_id * g->num_terminals) + terminal_id]) {
						continue;
					}
					if (first[(lhs * g->num_terminals) + terminal_id]) {
						continue;
					}

					first[(lhs * g->num_terminals) + terminal_id] = true;
					changed = true;
				}

				// If this symbol is not nullable, later symbols cannot affect this production prefix.
				if (!nullable_flags[rhs_non_terminal_id]) {
					production_nullable = false;
					break;
				}
			}

			// The whole production is nullable only if every symbol was nullable or epsilon.
			if (production_nullable && !nullable_flags[lhs]) {
				nullable_flags[lhs] = true;
				changed = true;
			}
		}
	} while (changed);

	*first_table = first;
	*nullable = nullable_flags;

	return true;
}

/**
 * @brief Builds FOLLOW table for all non-terminals.
 * @param g Parsed grammar.
 * @param first_table FIRST table from compute_first_tables.
 * @param nullable Nullable flags from compute_first_tables.
 * @param epsilon_id Terminal id for "epsilon", or -1.
 * @param out_follow Output flattened table: non-terminal x (terminals + '$').
 * @param out_follow_cols Output number of columns for out_follow.
 * @return true on success, false on allocation error or invalid input.
 */
static bool compute_follow_table(
	const grammar *g,
	const bool *first_table,
	const bool *nullable,
	int epsilon_id,
	bool **out_follow,
	int *out_follow_cols)
{
	// FOLLOW needs valid grammar metadata plus the nullable information.
	if (g == NULL || nullable == NULL || out_follow == NULL || out_follow_cols == NULL || g->num_non_terminals <= 0) {
		return false;
	}
	if (g->num_terminals > 0 && first_table == NULL) {
		return false;
	}

	*out_follow = NULL;
	*out_follow_cols = 0;

	// Extra column stores the end marker '$'.
	const int follow_cols = g->num_terminals + 1;
	const size_t follow_row_size = (size_t)follow_cols * sizeof(bool);
	const size_t follow_size = (size_t)g->num_non_terminals * (size_t)follow_cols;

	bool *follow = (bool *)calloc(follow_size, sizeof(bool));
	if (follow == NULL) {
		return false;
	}

	bool *trailer = (bool *)malloc(follow_row_size);
	if (trailer == NULL) {
		free(follow);
		return false;
	}

	// '$' is stored in the extra column and belongs to FOLLOW(start).
	follow[g->num_terminals] = true;

	bool changed;
	do {
		// Repeat until no FOLLOW set grows during a full pass.
		changed = false;

		for (int i = 0; i < g->num_productions; i++) {
			const production *prod = &g->productions[i];
			const int lhs = prod->non_terminal_id;

			if (lhs < 0 || lhs >= g->num_non_terminals) {
				continue;
			}

			// Start from FOLLOW(lhs) and move right-to-left through the production.
			memcpy(trailer, &follow[lhs * follow_cols], follow_row_size);

			for (int j = prod->production_length - 1; j >= 0; j--) {
				const int symbol_id = prod->production_symbol_ids[j];

				if (symbol_id >= 0 && symbol_id < g->num_terminals) {
					// A terminal becomes the new suffix seen by symbols to its left.
					if (symbol_id == epsilon_id) {
						continue;
					}

					memset(trailer, 0, follow_row_size);
					trailer[symbol_id] = true;
					continue;
				}

				const int rhs_non_terminal_id = symbol_id - g->num_terminals;
				if (rhs_non_terminal_id < 0 || rhs_non_terminal_id >= g->num_non_terminals) {
					memset(trailer, 0, follow_row_size);
					continue;
				}

				// Every symbol currently in trailer belongs to FOLLOW(this non-terminal).
				bool *rhs_follow = &follow[rhs_non_terminal_id * follow_cols];
				for (int col = 0; col < follow_cols; col++) {
					if (!trailer[col] || rhs_follow[col]) {
						continue;
					}
					rhs_follow[col] = true;
					changed = true;
				}

				// If the current symbol is not nullable, the old trailer cannot pass further left.
				if (!nullable[rhs_non_terminal_id]) {
					memset(trailer, 0, follow_row_size);
				}

				if (first_table != NULL) {
					// FIRST(current symbol) also belongs to the suffix seen by symbols on the left.
					const bool *rhs_first = &first_table[rhs_non_terminal_id * g->num_terminals];
					for (int terminal_id = 0; terminal_id < g->num_terminals; terminal_id++) {
						if (terminal_id == epsilon_id || !rhs_first[terminal_id]) {
							continue;
						}
						trailer[terminal_id] = true;
					}
				}

				// '$' keeps moving left only through nullable symbols.
				if (!nullable[rhs_non_terminal_id]) {
					trailer[g->num_terminals] = false;
				}
			}
		}
	} while (changed);

	free(trailer);
	*out_follow = follow;
	*out_follow_cols = follow_cols;
	return true;
}

/**
 * @brief Collects FIRST symbols for one non-terminal from the computed table.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index.
 * @param first_table FIRST table.
 * @param nullable Nullable flags.
 * @param epsilon_id Terminal id for "epsilon", or -1.
 * @param out_first Output array with FIRST symbols.
 * @return Number of collected symbols, or 0 on error.
 */
static int collect_first_for_non_terminal(
	const grammar *g,
	int non_terminal_id,
	const bool *first_table,
	const bool *nullable,
	int epsilon_id,
	symbol **out_first)
{
	if (g == NULL || nullable == NULL || out_first == NULL || non_terminal_id < 0 || non_terminal_id >= g->num_non_terminals) {
		return 0;
	}
	if (g->num_terminals > 0 && first_table == NULL) { 
		return 0; 
	}
	*out_first = NULL;
	int count = 0;
	const bool *first_row = (g->num_terminals > 0) ? &first_table[non_terminal_id * g->num_terminals] : NULL;
	for (int terminal_id = 0; terminal_id < g->num_terminals; terminal_id++) {
		bool should_add = false;
		if (terminal_id == epsilon_id) {
			should_add = nullable[non_terminal_id];
		} else if (first_row != NULL) {
			should_add = first_row[terminal_id];
		}
		if (!should_add) { 
			continue; 
		}
		if (g->terminals[terminal_id].symbol == NULL) { 
			continue; 
		}
		if (!add_symbol_to_array(out_first, &count, g->terminals[terminal_id].symbol, true)) {
			free_symbol_array(*out_first, count);
			*out_first = NULL;
			return 0;
		}
	}
	return count;
}

/**
 * @brief Collects FOLLOW symbols for one non-terminal from the computed table.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index.
 * @param follow_table FOLLOW table.
 * @param follow_cols Number of columns in follow_table.
 * @param out_follow Output array with FOLLOW symbols.
 * @return Number of collected symbols, or 0 on error.
 */
static int collect_follow_for_non_terminal(
	const grammar *g,
	int non_terminal_id,
	const bool *follow_table,
	int follow_cols,
	symbol **out_follow)
{
	// Validate the requested row and output pointer before reading the table.
	if (g == NULL || follow_table == NULL || out_follow == NULL || non_terminal_id < 0 || non_terminal_id >= g->num_non_terminals) {
		return 0;
	}
	// FOLLOW has one extra column reserved for '$'.
	if (follow_cols != g->num_terminals + 1) {
		return 0;
	}

	*out_follow = NULL;
	int count = 0;
	const int epsilon_id = find_terminal_id(g, "epsilon");
	// Read only the FOLLOW row that belongs to the target non-terminal.
	const bool *follow_row = &follow_table[non_terminal_id * follow_cols];

	for (int terminal_id = 0; terminal_id < g->num_terminals; terminal_id++) {
		// Skip terminals that are not present in FOLLOW and never expose epsilon here.
		if (terminal_id == epsilon_id || !follow_row[terminal_id]) {
			continue;
		}
		if (g->terminals[terminal_id].symbol == NULL) {
			continue;
		}
		// Copy each terminal symbol into the output array.
		if (!add_symbol_to_array(out_follow, &count, g->terminals[terminal_id].symbol, true)) {
			free_symbol_array(*out_follow, count);
			*out_follow = NULL;
			return 0;
		}
	}

	// The extra column indicates whether '$' belongs to this FOLLOW set.
	if (follow_row[g->num_terminals]) {
		if (!add_symbol_to_array(out_follow, &count, "$", true)) {
			free_symbol_array(*out_follow, count);
			*out_follow = NULL;
			return 0;
		}
	}

	return count;
}

/**
 * @brief Computes FIRST set for one non-terminal by index.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index in g->non_terminals.
 * @param out_first Output array with FIRST symbols.
 * @return Number of symbols in out_first, or 0 on error.
 */
int compute_first_for_non_terminal(const grammar *g, int non_terminal_id, symbol **out_first)
{
	// Reject invalid requests before allocating shared tables.
	if (g == NULL || out_first == NULL || non_terminal_id < 0 || non_terminal_id >= g->num_non_terminals) {
		return 0;
	}
	*out_first = NULL;

	// Build the common FIRST/nullable tables used by all non-terminals.
	bool *first_table = NULL;
	bool *nullable = NULL;
	int epsilon_id = -1;
	if (!compute_first_tables(g, &first_table, &nullable, &epsilon_id)) {
		return 0; 
	}

	// Extract only the FIRST set of the requested non-terminal.
	int count = collect_first_for_non_terminal(g, non_terminal_id,
	first_table, nullable, epsilon_id, out_first);free(nullable);
	free(first_table);
	return count;
}

/**
 * @brief Computes FOLLOW set for one non-terminal by index.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index in g->non_terminals.
 * @param out_follow Output array with FOLLOW symbols.
 * @return Number of symbols in out_follow, or 0 on error.
 */
int compute_follow_for_non_terminal(const grammar *g, int non_terminal_id, symbol **out_follow)
{
	// Reject invalid requests before allocating shared tables.
	if (g == NULL || out_follow == NULL || non_terminal_id < 0 || non_terminal_id >= g->num_non_terminals) {
		return 0;
	}
	*out_follow = NULL;

	// Build the shared FIRST/nullable data needed to derive FOLLOW.
	bool *first_table = NULL;
	bool *nullable = NULL;
	int epsilon_id = -1;
	if (!compute_first_tables(g, &first_table, &nullable, &epsilon_id)) {
		return 0;
	}

	// Build the global FOLLOW table and then extract the requested row.
	bool *follow_table = NULL;
	int follow_cols = 0;
	if (!compute_follow_table(g, first_table, nullable, epsilon_id, &follow_table, &follow_cols)) {
		free(nullable);
		free(first_table);
		return 0;
	}

	int count = collect_follow_for_non_terminal(g, non_terminal_id, follow_table, follow_cols, out_follow);
	free(follow_table);
	free(nullable);
	free(first_table);
	return count;
}

/**
 * @brief Computes FIRST set for the start symbol.
 * @param g Parsed grammar.
 * @param out_first Output array with FIRST(start) terminals.
 * @return Number of symbols in out_first, or 0 on error.
 */
int compute_first_for_start_symbol(const grammar *g, symbol **out_first)
{
	if (g == NULL || g->num_non_terminals <= 0) { 
		return 0; 
	}
	return compute_first_for_non_terminal(g, 0, out_first);
}

/**
 * @brief Computes FOLLOW set for the start symbol.
 * @param g Parsed grammar.
 * @param out_follow Output array with FOLLOW(start) terminals.
 * @return Number of symbols in out_follow, or 0 on error.
 */
int compute_follow_for_start_symbol(const grammar *g, symbol **out_follow)
{
	if (g == NULL || g->num_non_terminals <= 0) { 
		return 0; 
	}
	return compute_follow_for_non_terminal(g, 0, out_follow);
}

/**
 * @brief Frees a symbol array and each duplicated symbol string.
 * @param symbols Symbol array to release.
 * @param count Number of initialized entries.
 * @return This function does not return a value.
 */
void free_symbol_array(symbol *symbols, int count)
{
	if (symbols == NULL) { 
		return; 
	}
	for (int i = 0; i < count; i++) {
		free(symbols[i].symbol);
	}
	free(symbols);
}
