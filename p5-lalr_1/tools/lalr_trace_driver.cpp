#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using json = nlohmann::json;

// ==== Section 1: Domain Types =====
// ----- Parser action types ------

/**
 * @brief Represents the action type stored in the ACTION table.
 */
enum class ActionType
{
    Shift,
    Reduce,
    Accept,
    Error,
};

/**
 * @brief Single entry of the ACTION table.
 */
struct Action
{
    ActionType type = ActionType::Error;
    int value = -1;
};

/**
 * @brief Grammar production in the form A -> beta.
 */
struct Rule
{
    int lhs = -1;
    std::vector<int> rhs;
};

/**
 * @brief LALR(1) table loaded from parse_table.json.
 */
struct ParseTable
{
    int num_states = 0;
    int num_terminals_with_eof = 0;
    int num_non_terminals = 0;
    int eof_terminal = -1;
    std::vector<std::string> terminals;
    std::vector<std::string> non_terminals;
    std::vector<std::vector<Action>> action;
    std::vector<std::vector<int>> goto_table;
    std::unordered_map<std::string, int> terminal_to_id;
    std::unordered_map<std::string, int> non_terminal_to_id;
};

/**
 * @brief Text grammar already converted to ids compatible with the table.
 */
struct Grammar
{
    std::vector<Rule> productions;
};

/**
 * @brief Original input string together with its tokenized sequence.
 */
struct InputSequence
{
    std::string original_text;
    std::vector<int> tokens;
};

/**
 * @brief Single row of the execution trace.
 */
struct TraceRow
{
    std::string step;
    std::string stack;
    std::string token;
    std::string action;
    std::string rule;
};

/**
 * @brief Final result of parsing one input string.
 */
struct ParseResult
{
    bool accepted = false;
    std::string message;
    std::vector<TraceRow> rows;
};

/**
 * @brief Helper mapping used to recognize common lexemes during tokenization.
 */
struct TokenClassifier
{
    int identifier_terminal = -1;
    int integer_terminal = -1;
    int float_terminal = -1;
    int generic_number_terminal = -1;
    int string_terminal = -1;
    int char_terminal = -1;
    std::vector<int> literal_terminals;
};

// ==== Section 2: General Utilities =====
// ----- Subsection 2.1: Text reading and cleanup ------

/**
 * @brief Reads the full contents of a file into memory.
 * @param path Path of the file to load.
 * @return Full file contents.
 * @throws std::runtime_error If the file cannot be opened.
 */
std::string read_file(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Could not open file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

/**
 * @brief Opens the output file used to persist the generated trace.
 * @param path Destination path for the report.
 * @return Writable file stream.
 * @throws std::runtime_error If the file cannot be created or opened.
 */
std::ofstream open_output_file(const std::string &path)
{
    const std::filesystem::path output_path(path);
    if (output_path.has_parent_path())
    {
        std::filesystem::create_directories(output_path.parent_path());
    }

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("Could not open output file: " + path);
    }

    return output;
}

/**
 * @brief Removes leading and trailing whitespace from a string.
 * @param text Original string.
 * @return Trimmed string.
 */
std::string trim(const std::string &text)
{
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0)
    {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }

    return text.substr(start, end - start);
}

/**
 * @brief Splits a string using whitespace as separators.
 * @param text String to split.
 * @return List of whitespace-separated tokens.
 */
std::vector<std::string> split_whitespace(const std::string &text)
{
    std::vector<std::string> parts;
    std::istringstream input(text);
    std::string part;
    while (input >> part)
    {
        parts.push_back(part);
    }
    return parts;
}

/**
 * @brief Extracts the suffix of a line in the form "Prefix: content".
 * @param line Full line.
 * @param prefix Required prefix.
 * @return Content after the prefix.
 * @throws std::runtime_error If the line does not begin with the expected prefix.
 */
std::string suffix_after_colon(const std::string &line, const std::string &prefix)
{
    if (line.rfind(prefix, 0) != 0)
    {
        throw std::runtime_error("Expected line with prefix: " + prefix);
    }
    return trim(line.substr(prefix.size()));
}

// ----- Subsection 2.2: Type conversions ------

/**
 * @brief Converts the textual name of a JSON action into its enum value.
 * @param text Action name read from JSON.
 * @return Equivalent action type.
 * @throws std::runtime_error If the text does not correspond to a valid action.
 */
ActionType parse_action_type(const std::string &text)
{
    if (text == "SHIFT")
    {
        return ActionType::Shift;
    }
    if (text == "REDUCE")
    {
        return ActionType::Reduce;
    }
    if (text == "ACCEPT")
    {
        return ActionType::Accept;
    }
    if (text == "ERROR")
    {
        return ActionType::Error;
    }
    throw std::runtime_error("Unknown action type: " + text);
}

/**
 * @brief Returns the highest reduction index used by the ACTION table.
 * @param table Parsing table.
 * @return Maximum reduction index or -1 if there are no reductions.
 */
int max_reduce_index(const ParseTable &table)
{
    int max_index = -1;
    for (const auto &row : table.action)
    {
        for (const Action &action : row)
        {
            if (action.type == ActionType::Reduce)
            {
                max_index = std::max(max_index, action.value);
            }
        }
    }
    return max_index;
}

// ==== Section 3: Loading parse_table.json =====
// ----- Reading the ACTION/GOTO table ------

/**
 * @brief Loads and validates the LALR(1) table from parse_table.json.
 * @param path Path to the JSON table file.
 * @return Fully initialized ParseTable structure.
 * @throws std::runtime_error If the file is inconsistent or invalid.
 */
ParseTable load_parse_table(const std::string &path)
{
    const json root = json::parse(read_file(path));

    ParseTable table;
    table.num_states = root.at("numStates").get<int>();
    table.num_terminals_with_eof = root.at("numTerminalsWithEof").get<int>();
    table.num_non_terminals = root.at("numNonTerminals").get<int>();
    table.terminals = root.at("terminals").get<std::vector<std::string>>();
    table.non_terminals = root.at("nonTerminals").get<std::vector<std::string>>();

    if (static_cast<int>(table.terminals.size()) != table.num_terminals_with_eof)
    {
        throw std::runtime_error("The number of terminals does not match numTerminalsWithEof.");
    }
    if (static_cast<int>(table.non_terminals.size()) != table.num_non_terminals)
    {
        throw std::runtime_error("The number of non-terminals does not match numNonTerminals.");
    }

    for (int i = 0; i < static_cast<int>(table.terminals.size()); ++i)
    {
        table.terminal_to_id.emplace(table.terminals[i], i);
        if (table.terminals[i] == "$")
        {
            table.eof_terminal = i;
        }
    }
    for (int i = 0; i < static_cast<int>(table.non_terminals.size()); ++i)
    {
        table.non_terminal_to_id.emplace(table.non_terminals[i], i);
    }

    if (table.eof_terminal < 0)
    {
        throw std::runtime_error("The JSON table does not contain the '$' terminal.");
    }

    const json &action_rows = root.at("action");
    const json &goto_rows = root.at("goto");
    if (static_cast<int>(action_rows.size()) != table.num_states)
    {
        throw std::runtime_error("The number of rows in action does not match numStates.");
    }
    if (static_cast<int>(goto_rows.size()) != table.num_states)
    {
        throw std::runtime_error("The number of rows in goto does not match numStates.");
    }

    table.action.resize(table.num_states);
    for (int state = 0; state < table.num_states; ++state)
    {
        const json &row = action_rows.at(state);
        if (static_cast<int>(row.size()) != table.num_terminals_with_eof)
        {
            throw std::runtime_error("An action row has an invalid size.");
        }

        table.action[state].resize(table.num_terminals_with_eof);
        for (int column = 0; column < table.num_terminals_with_eof; ++column)
        {
            const json &entry = row.at(column);
            table.action[state][column] = Action{
                parse_action_type(entry.at("type").get<std::string>()),
                entry.at("value").get<int>()};
        }
    }

    table.goto_table.resize(table.num_states);
    for (int state = 0; state < table.num_states; ++state)
    {
        const json &row = goto_rows.at(state);
        if (static_cast<int>(row.size()) != table.num_non_terminals)
        {
            throw std::runtime_error("A goto row has an invalid size.");
        }

        table.goto_table[state].resize(table.num_non_terminals);
        for (int column = 0; column < table.num_non_terminals; ++column)
        {
            table.goto_table[state][column] = row.at(column).get<int>();
        }
    }

    return table;
}

// ==== Section 4: Loading the grammar =====
// ----- Converting symbols and productions ------

/**
 * @brief Converts a textual symbol into its internal id.
 * @param symbol Textual terminal or non-terminal symbol.
 * @param table Table containing symbol indices.
 * @return Internal symbol id.
 * @throws std::runtime_error If the symbol does not exist in the table.
 */
int symbol_id_from_text(const std::string &symbol, const ParseTable &table)
{
    auto terminal_it = table.terminal_to_id.find(symbol);
    if (terminal_it != table.terminal_to_id.end())
    {
        return terminal_it->second;
    }

    auto non_terminal_it = table.non_terminal_to_id.find(symbol);
    if (non_terminal_it != table.non_terminal_to_id.end())
    {
        return table.num_terminals_with_eof + non_terminal_it->second;
    }

    throw std::runtime_error("Unknown symbol in the grammar: " + symbol);
}

/**
 * @brief Converts a textual production "A -> beta" into the internal format.
 * @param line Textual production line.
 * @param table Parsing table used to resolve symbol ids.
 * @return Production converted to ids.
 * @throws std::runtime_error If the production is invalid or contains unknown symbols.
 */
Rule parse_rule_line(const std::string &line, const ParseTable &table)
{
    const std::size_t arrow = line.find("->");
    if (arrow == std::string::npos)
    {
        throw std::runtime_error("Invalid production: " + line);
    }

    const std::string lhs_text = trim(line.substr(0, arrow));
    const std::string rhs_text = trim(line.substr(arrow + 2));

    auto lhs_it = table.non_terminal_to_id.find(lhs_text);
    if (lhs_it == table.non_terminal_to_id.end())
    {
        throw std::runtime_error("Unknown non-terminal in the grammar: " + lhs_text);
    }

    Rule rule;
    rule.lhs = lhs_it->second;
    for (const std::string &symbol : split_whitespace(rhs_text))
    {
        rule.rhs.push_back(symbol_id_from_text(symbol, table));
    }
    return rule;
}

/**
 * @brief Verifies that two symbol lists match exactly.
 * @param expected Expected list.
 * @param actual Actual list.
 * @param label Label used in the error message.
 * @throws std::runtime_error If the lists differ.
 */
void validate_symbol_lists(const std::vector<std::string> &expected,
                           const std::vector<std::string> &actual,
                           const std::string &label)
{
    if (expected != actual)
    {
        throw std::runtime_error("The " + label + " list in the grammar does not match parse_table.json.");
    }
}

/**
 * @brief Loads the textual grammar and validates that it matches parse_table.json.
 * @param path Path to the grammar.txt file.
 * @param table Table already loaded from JSON.
 * @return Grammar with productions encoded as ids.
 * @throws std::runtime_error If the grammar is incompatible with the table.
 */
Grammar load_grammar(const std::string &path, const ParseTable &table)
{
    const std::string content = read_file(path);
    std::istringstream input(content);

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        const std::string stripped = trim(line);
        if (!stripped.empty())
        {
            lines.push_back(stripped);
        }
    }

    if (lines.size() < 3)
    {
        throw std::runtime_error("The grammar file must include headers and at least one production.");
    }

    const std::vector<std::string> grammar_non_terminals =
        split_whitespace(suffix_after_colon(lines[0], "Non-terminals:"));
    const std::vector<std::string> grammar_terminals =
        split_whitespace(suffix_after_colon(lines[1], "Terminals:"));

    validate_symbol_lists(table.non_terminals, grammar_non_terminals, "non-terminals");
    validate_symbol_lists(std::vector<std::string>(table.terminals.begin(), table.terminals.end() - 1),
                          grammar_terminals,
                          "terminals");

    Grammar grammar;
    for (std::size_t i = 2; i < lines.size(); ++i)
    {
        grammar.productions.push_back(parse_rule_line(lines[i], table));
    }

    const int needed_rules = max_reduce_index(table) + 1;
    if (needed_rules > static_cast<int>(grammar.productions.size()))
    {
        throw std::runtime_error("The grammar does not have enough productions for the table reductions.");
    }

    return grammar;
}

// ==== Section 5: Input Tokenization =====
// ----- Subsection 5.1: Lexeme classification ------

/**
 * @brief Determines whether a lexeme has identifier form.
 * @param token Lexeme to evaluate.
 * @return true if the lexeme looks like an identifier; false otherwise.
 */
bool is_identifier_token(const std::string &token)
{
    if (token.empty())
    {
        return false;
    }
    if (std::isalpha(static_cast<unsigned char>(token.front())) == 0 && token.front() != '_')
    {
        return false;
    }
    return std::all_of(token.begin(),
                       token.end(),
                       [](char character)
                       {
                           return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
                       });
}

/**
 * @brief Determines whether a lexeme has integer form.
 * @param token Lexeme to evaluate.
 * @return true if the lexeme looks like an integer; false otherwise.
 */
bool is_integer_token(const std::string &token)
{
    if (token.empty())
    {
        return false;
    }

    std::size_t start = (token.front() == '+' || token.front() == '-') ? 1U : 0U;
    if (start >= token.size())
    {
        return false;
    }

    return std::all_of(token.begin() + static_cast<std::ptrdiff_t>(start),
                       token.end(),
                       [](char character)
                       {
                           return std::isdigit(static_cast<unsigned char>(character)) != 0;
                       });
}

/**
 * @brief Determines whether a lexeme has floating-point form.
 * @param token Lexeme to evaluate.
 * @return true if the lexeme looks like a float; false otherwise.
 */
bool is_float_token(const std::string &token)
{
    if (token.empty())
    {
        return false;
    }

    bool has_digit = false;
    bool has_dot = false;
    bool has_exponent = false;

    for (std::size_t i = 0; i < token.size(); ++i)
    {
        const char current = token[i];
        if (std::isdigit(static_cast<unsigned char>(current)) != 0)
        {
            has_digit = true;
            continue;
        }
        if (current == '.' && !has_dot && !has_exponent)
        {
            has_dot = true;
            continue;
        }
        if ((current == 'e' || current == 'E') && has_digit && !has_exponent)
        {
            has_exponent = true;
            if (i + 1 < token.size() && (token[i + 1] == '+' || token[i + 1] == '-'))
            {
                ++i;
            }
            continue;
        }
        return false;
    }

    return has_digit && (has_dot || has_exponent);
}

/**
 * @brief Determines whether a lexeme corresponds to a double-quoted string.
 * @param token Lexeme to evaluate.
 * @return true if it looks like a string literal; false otherwise.
 */
bool is_string_token(const std::string &token)
{
    return token.size() >= 2 && token.front() == '"' && token.back() == '"';
}

/**
 * @brief Determines whether a lexeme corresponds to a single-quoted character.
 * @param token Lexeme to evaluate.
 * @return true if it looks like a char literal; false otherwise.
 */
bool is_char_token(const std::string &token)
{
    return token.size() >= 3 && token.front() == '\'' && token.back() == '\'';
}

/**
 * @brief Finds the first existing terminal among several possible aliases.
 * @param table Table containing the terminal dictionary.
 * @param names Candidate aliases.
 * @return Id of the first terminal found, or -1 if none exists.
 */
int find_terminal(const ParseTable &table, std::initializer_list<const char *> names)
{
    for (const char *name : names)
    {
        auto it = table.terminal_to_id.find(name);
        if (it != table.terminal_to_id.end())
        {
            return it->second;
        }
    }
    return -1;
}

/**
 * @brief Builds heuristic rules to recognize identifiers, numbers, and literals.
 * @param table Parsing table.
 * @return Classifier ready to tokenize inputs.
 */
TokenClassifier build_classifier(const ParseTable &table)
{
    TokenClassifier classifier;
    classifier.identifier_terminal = find_terminal(table, {"IDENTIFIER", "ID", "id"});
    classifier.integer_terminal = find_terminal(table, {"INT_LITERAL", "INT"});
    classifier.float_terminal = find_terminal(table, {"FLOAT_LITERAL", "FLOAT"});
    classifier.generic_number_terminal = find_terminal(table, {"NUM", "num", "NUMBER", "number"});
    classifier.string_terminal = find_terminal(table, {"STRING_LITERAL", "STRING", "str"});
    classifier.char_terminal = find_terminal(table, {"CHAR_LITERAL", "CHAR", "char_lit"});

    for (int terminal = 0; terminal < table.num_terminals_with_eof; ++terminal)
    {
        if (table.terminals[terminal] == "$")
        {
            continue;
        }
        if (terminal == classifier.identifier_terminal ||
            terminal == classifier.integer_terminal ||
            terminal == classifier.float_terminal ||
            terminal == classifier.generic_number_terminal ||
            terminal == classifier.string_terminal ||
            terminal == classifier.char_terminal)
        {
            continue;
        }
        classifier.literal_terminals.push_back(terminal);
    }

    std::sort(classifier.literal_terminals.begin(),
              classifier.literal_terminals.end(),
              [&table](int left, int right)
              {
                  return table.terminals[left].size() > table.terminals[right].size();
              });

    return classifier;
}

/**
 * @brief Attempts to map a lexeme to a terminal in the table.
 * @param token Lexeme to classify.
 * @param table Parsing table.
 * @param classifier Helper classifier for aliases and token classes.
 * @return Terminal id or -1 if there is no match.
 */
int classify_token_text(const std::string &token,
                        const ParseTable &table,
                        const TokenClassifier &classifier)
{
    auto direct = table.terminal_to_id.find(token);
    if (direct != table.terminal_to_id.end())
    {
        return direct->second;
    }
    if (is_string_token(token) && classifier.string_terminal >= 0)
    {
        return classifier.string_terminal;
    }
    if (is_char_token(token) && classifier.char_terminal >= 0)
    {
        return classifier.char_terminal;
    }
    if (is_float_token(token))
    {
        if (classifier.float_terminal >= 0)
        {
            return classifier.float_terminal;
        }
        if (classifier.generic_number_terminal >= 0)
        {
            return classifier.generic_number_terminal;
        }
    }
    if (is_integer_token(token))
    {
        if (classifier.integer_terminal >= 0)
        {
            return classifier.integer_terminal;
        }
        if (classifier.generic_number_terminal >= 0)
        {
            return classifier.generic_number_terminal;
        }
        if (classifier.float_terminal >= 0)
        {
            return classifier.float_terminal;
        }
    }
    if (is_identifier_token(token) && classifier.identifier_terminal >= 0)
    {
        return classifier.identifier_terminal;
    }
    return -1;
}

// ----- Subsection 5.2: Input string tokenization ------

/**
 * @brief Checks whether a literal matches at a position without breaking identifier boundaries.
 * @param text Full input text.
 * @param position Current scanning position.
 * @param literal Terminal literal to compare.
 * @return true if the literal can be consumed at that position; false otherwise.
 */
bool matches_literal_boundary(const std::string &text, std::size_t position, const std::string &literal)
{
    if (position + literal.size() > text.size())
    {
        return false;
    }
    if (text.compare(position, literal.size(), literal) != 0)
    {
        return false;
    }

    const bool identifier_like =
        !literal.empty() &&
        (std::isalpha(static_cast<unsigned char>(literal.front())) != 0 || literal.front() == '_');
    if (!identifier_like)
    {
        return true;
    }

    const std::size_t end = position + literal.size();
    if (end < text.size())
    {
        const char next = text[end];
        if (std::isalnum(static_cast<unsigned char>(next)) != 0 || next == '_')
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Tokenizes one single line from the input file.
 * @param line Original string.
 * @param table Parsing table.
 * @param classifier Helper classifier.
 * @return Tokenized input sequence terminated with '$'.
 * @throws std::runtime_error If the full line cannot be tokenized.
 */
InputSequence tokenize_line(const std::string &line,
                            const ParseTable &table,
                            const TokenClassifier &classifier)
{
    InputSequence sequence;
    sequence.original_text = line;

    const std::string stripped = trim(line);
    if (stripped.empty())
    {
        return sequence;
    }

    bool whitespace_ok = true;
    for (const std::string &token : split_whitespace(stripped))
    {
        const int terminal = classify_token_text(token, table, classifier);
        if (terminal < 0)
        {
            whitespace_ok = false;
            break;
        }
        sequence.tokens.push_back(terminal);
    }

    if (!whitespace_ok || sequence.tokens.empty())
    {
        sequence.tokens.clear();

        std::size_t position = 0;
        while (position < stripped.size())
        {
            if (std::isspace(static_cast<unsigned char>(stripped[position])) != 0)
            {
                ++position;
                continue;
            }

            bool matched_literal = false;
            for (int terminal : classifier.literal_terminals)
            {
                const std::string &literal = table.terminals[terminal];
                if (!matches_literal_boundary(stripped, position, literal))
                {
                    continue;
                }
                sequence.tokens.push_back(terminal);
                position += literal.size();
                matched_literal = true;
                break;
            }
            if (matched_literal)
            {
                continue;
            }

            const auto scan_quoted = [&](char quote, int terminal_id) -> bool
            {
                if (stripped[position] != quote)
                {
                    return false;
                }

                std::size_t end = position + 1;
                bool escaped = false;
                while (end < stripped.size())
                {
                    const char current = stripped[end];
                    if (!escaped && current == quote)
                    {
                        break;
                    }
                    escaped = (!escaped && current == '\\');
                    if (escaped && current != '\\')
                    {
                        escaped = false;
                    }
                    ++end;
                }

                if (end >= stripped.size() || terminal_id < 0)
                {
                    throw std::runtime_error("Could not tokenize input string: " + stripped);
                }

                sequence.tokens.push_back(terminal_id);
                position = end + 1;
                return true;
            };

            if (scan_quoted('"', classifier.string_terminal) || scan_quoted('\'', classifier.char_terminal))
            {
                continue;
            }

            std::size_t end = position;
            while (end < stripped.size() && std::isspace(static_cast<unsigned char>(stripped[end])) == 0)
            {
                ++end;
            }

            bool matched_dynamic = false;
            while (end > position)
            {
                const std::string candidate = stripped.substr(position, end - position);
                const int terminal = classify_token_text(candidate, table, classifier);
                if (terminal >= 0)
                {
                    sequence.tokens.push_back(terminal);
                    position = end;
                    matched_dynamic = true;
                    break;
                }
                --end;
            }

            if (!matched_dynamic)
            {
                throw std::runtime_error("Could not tokenize input string: " + stripped);
            }
        }
    }

    if (!sequence.tokens.empty() && sequence.tokens.back() != table.eof_terminal)
    {
        sequence.tokens.push_back(table.eof_terminal);
    }

    return sequence;
}

/**
 * @brief Loads and tokenizes all strings from the input file.
 * @param path Path to the input file.
 * @param table Parsing table.
 * @return List of tokenized input strings.
 */
std::vector<InputSequence> load_inputs(const std::string &path, const ParseTable &table)
{
    const std::string content = read_file(path);
    const TokenClassifier classifier = build_classifier(table);

    std::vector<InputSequence> inputs;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line))
    {
        const std::string stripped = trim(line);
        if (stripped.empty() || stripped.rfind('#', 0) == 0)
        {
            continue;
        }

        InputSequence sequence = tokenize_line(line, table, classifier);
        if (!sequence.tokens.empty())
        {
            inputs.push_back(std::move(sequence));
        }
    }

    return inputs;
}

// ==== Section 6: Formatting Symbols and Rules =====
// ----- Text conversion for the trace ------

/**
 * @brief Converts an internal symbol id to its textual representation.
 * @param table Table containing symbol names.
 * @param symbol_id Terminal or non-terminal id.
 * @return Textual symbol name.
 */
std::string symbol_name(const ParseTable &table, int symbol_id)
{
    if (symbol_id >= 0 && symbol_id < table.num_terminals_with_eof)
    {
        return table.terminals[symbol_id];
    }

    const int non_terminal = symbol_id - table.num_terminals_with_eof;
    if (non_terminal >= 0 && non_terminal < table.num_non_terminals)
    {
        return table.non_terminals[non_terminal];
    }
    return "?";
}

/**
 * @brief Converts a production to the format "pN: A -> beta".
 * @param table Table containing symbol names.
 * @param rule_index Production index.
 * @param rule Production to print.
 * @return Rule in a human-readable trace format.
 */
std::string rule_to_string(const ParseTable &table, int rule_index, const Rule &rule)
{
    std::ostringstream output;
    output << "p" << rule_index << ": " << table.non_terminals[rule.lhs] << " ->";
    if (rule.rhs.empty())
    {
        output << " ε";
    }
    else
    {
        for (int symbol : rule.rhs)
        {
            output << ' ' << symbol_name(table, symbol);
        }
    }
    return output.str();
}

/**
 * @brief Converts the parser stack into a readable string.
 * @param table Table containing symbol names.
 * @param states State stack.
 * @param symbols Symbol stack interleaved between states.
 * @return Textual representation of the stack.
 */
std::string stack_to_string(const ParseTable &table,
                            const std::vector<int> &states,
                            const std::vector<int> &symbols)
{
    std::ostringstream output;
    for (std::size_t i = 0; i < states.size(); ++i)
    {
        if (i > 0)
        {
            output << ' ' << symbol_name(table, symbols[i - 1]) << ' ';
        }
        output << states[i];
    }
    return output.str();
}

// ==== Section 7: Parsing Engine =====
// ----- Shift-reduce simulation ------

/**
 * @brief Runs the LALR(1) parser on an already tokenized input.
 * @param table ACTION/GOTO table.
 * @param grammar Grammar with productions in order.
 * @param sequence Tokenized input string.
 * @return Parsing result together with its tabular trace.
 */
ParseResult parse_sequence(const ParseTable &table,
                           const Grammar &grammar,
                           const InputSequence &sequence)
{
    ParseResult result;
    std::vector<int> states = {0};
    std::vector<int> symbols;
    std::size_t input_position = 0;
    const int step_limit = 1 + static_cast<int>(sequence.tokens.size()) * 8 + static_cast<int>(grammar.productions.size()) * 4;

    for (int step = 1; step <= step_limit; ++step)
    {
        if (input_position >= sequence.tokens.size())
        {
            result.message = "Input ended without reaching ACCEPT.";
            return result;
        }

        const int state = states.back();
        const int lookahead = sequence.tokens[input_position];
        const Action &action = table.action[state][lookahead];

        TraceRow row;
        row.step = std::to_string(step);
        row.stack = stack_to_string(table, states, symbols);
        row.token = table.terminals[lookahead];

        if (action.type == ActionType::Shift)
        {
            row.action = "SHIFT " + std::to_string(action.value);
            row.rule = "-";
            result.rows.push_back(row);

            symbols.push_back(lookahead);
            states.push_back(action.value);
            ++input_position;
            continue;
        }

        if (action.type == ActionType::Reduce)
        {
            row.action = "REDUCE " + std::to_string(action.value);

            if (action.value < 0 || action.value >= static_cast<int>(grammar.productions.size()))
            {
                row.rule = "-";
                result.rows.push_back(row);
                result.message = "Reduction p" + std::to_string(action.value) + " does not exist in the grammar.";
                return result;
            }

            const Rule &rule = grammar.productions[action.value];
            row.rule = rule_to_string(table, action.value, rule);
            result.rows.push_back(row);

            if (rule.rhs.size() > symbols.size())
            {
                result.message = "The stack does not contain enough symbols for the reduction.";
                return result;
            }

            for (std::size_t i = 0; i < rule.rhs.size(); ++i)
            {
                const int expected = rule.rhs[rule.rhs.size() - 1 - i];
                const int found = symbols.back();
                if (expected != found)
                {
                    std::ostringstream error;
                    error << "Inconsistent reduction in p" << action.value
                          << ": expected '" << symbol_name(table, expected)
                          << "' but found '" << symbol_name(table, found) << "'.";
                    result.message = error.str();
                    return result;
                }
                symbols.pop_back();
                states.pop_back();
            }

            const int goto_state = table.goto_table[states.back()][rule.lhs];
            if (goto_state < 0)
            {
                std::ostringstream error;
                error << "GOTO[" << states.back() << ", " << table.non_terminals[rule.lhs] << "] does not exist.";
                result.message = error.str();
                return result;
            }

            symbols.push_back(table.num_terminals_with_eof + rule.lhs);
            states.push_back(goto_state);
            continue;
        }

        if (action.type == ActionType::Accept)
        {
            row.action = "ACCEPT";
            row.rule = "-";
            result.rows.push_back(row);
            result.accepted = true;
            result.message = "Input accepted.";
            return result;
        }

        row.action = "ERROR";
        row.rule = "-";
        result.rows.push_back(row);

        std::ostringstream error;
        error << "Syntax error in state " << state
              << " with lookahead '" << table.terminals[lookahead] << "'.";
        result.message = error.str();
        return result;
    }

    result.message = "The trace step limit was reached.";
    return result;
}

// ==== Section 8: Result Presentation =====
// ----- Printing tables and rules ------

/**
 * @brief Prints the horizontal separator line of an ASCII table.
 * @param out Output stream.
 * @param widths Width of each column.
 */
void print_separator(std::ostream &out, const std::vector<std::size_t> &widths)
{
    out << '+';
    for (std::size_t width : widths)
    {
        out << std::string(width + 2, '-') << '+';
    }
    out << '\n';
}

/**
 * @brief Prints the full trace of an input string in tabular format.
 * @param out Output stream.
 * @param rows Trace rows.
 */
void print_trace_table(std::ostream &out, const std::vector<TraceRow> &rows)
{
    const std::vector<std::string> headers = {"Step", "Stack", "Token", "Action", "Rule"};
    std::vector<std::size_t> widths(headers.size());

    for (std::size_t i = 0; i < headers.size(); ++i)
    {
        widths[i] = headers[i].size();
    }
    for (const TraceRow &row : rows)
    {
        widths[0] = std::max(widths[0], row.step.size());
        widths[1] = std::max(widths[1], row.stack.size());
        widths[2] = std::max(widths[2], row.token.size());
        widths[3] = std::max(widths[3], row.action.size());
        widths[4] = std::max(widths[4], row.rule.size());
    }

    print_separator(out, widths);
    out << "| " << std::left << std::setw(static_cast<int>(widths[0])) << headers[0]
        << " | " << std::setw(static_cast<int>(widths[1])) << headers[1]
        << " | " << std::setw(static_cast<int>(widths[2])) << headers[2]
        << " | " << std::setw(static_cast<int>(widths[3])) << headers[3]
        << " | " << std::setw(static_cast<int>(widths[4])) << headers[4] << " |\n";
    print_separator(out, widths);

    for (const TraceRow &row : rows)
    {
        out << "| " << std::left << std::setw(static_cast<int>(widths[0])) << row.step
            << " | " << std::setw(static_cast<int>(widths[1])) << row.stack
            << " | " << std::setw(static_cast<int>(widths[2])) << row.token
            << " | " << std::setw(static_cast<int>(widths[3])) << row.action
            << " | " << std::setw(static_cast<int>(widths[4])) << row.rule << " |\n";
    }
    print_separator(out, widths);
}

/**
 * @brief Prints all rules loaded from the grammar.
 * @param out Output stream.
 * @param table Table with symbol names.
 * @param grammar Already loaded grammar.
 */
void print_rules(std::ostream &out, const ParseTable &table, const Grammar &grammar)
{
    out << "Loaded rules:\n";
    for (int i = 0; i < static_cast<int>(grammar.productions.size()); ++i)
    {
        out << "  " << rule_to_string(table, i, grammar.productions[i]) << '\n';
    }
    out << '\n';
}

/**
 * @brief Writes the complete parsing report for all input strings.
 * @param out Output stream.
 * @param table Table with symbol names.
 * @param grammar Already loaded grammar.
 * @param inputs Tokenized input strings.
 */
void write_report(std::ostream &out,
                  const ParseTable &table,
                  const Grammar &grammar,
                  const std::vector<InputSequence> &inputs)
{
    print_rules(out, table, grammar);
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        out << "Input " << (i + 1) << ": " << inputs[i].original_text << '\n';
        const ParseResult result = parse_sequence(table, grammar, inputs[i]);
        print_trace_table(out, result.rows);
        out << result.message << "\n\n";
    }
}

} // namespace


// ==== Section 9: Entry Point =====
// ----- Main program flow ------

/**
 * @brief Entry point of the LALR(1) trace driver.
 * @param argc Number of arguments.
 * @param argv Command-line arguments.
 * @return 0 on successful execution; 1 on error.
 */
int main(int argc, char **argv)
{
    try
    {
        if (argc != 4 && argc != 5)
        {
            std::cerr << "Usage: " << argv[0]
                      << " <parse_table.json> <grammar.txt> <inputs.txt> [results.txt]\n";
            return 1;
        }

        const ParseTable table = load_parse_table(argv[1]);
        const Grammar grammar = load_grammar(argv[2], table);
        const std::vector<InputSequence> inputs = load_inputs(argv[3], table);

        if (inputs.empty())
        {
            throw std::runtime_error("The input file does not contain valid strings.");
        }

        std::ostringstream report;
        write_report(report, table, grammar, inputs);

        const std::string report_text = report.str();
        std::cout << report_text;

        if (argc == 5)
        {
            std::ofstream output = open_output_file(argv[4]);
            output << report_text;
        }

        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
