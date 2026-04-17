#include <algorithm>
#include <cctype>
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

} // namespace
