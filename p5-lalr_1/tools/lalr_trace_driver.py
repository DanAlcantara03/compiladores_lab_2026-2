#!/usr/bin/env python3

"""Python LALR(1) trace driver.

This module validates a parse table stored in JSON, loads a grammar
file, tokenizes a list of input strings, simulates the shift-reduce
process, and emits a readable report both to stdout and, optionally, to
a destination file.

File organization:
1. Domain models
2. Basic file and text utilities
3. Parse table loading
4. Grammar loading
5. Input tokenization helpers
6. Parsing engine
7. Report rendering
8. Command-line entry point
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Iterable


# ==== Section 1: Domain Models ============================================

class ActionType(Enum):
    """Kinds of actions stored in the ACTION table."""

    SHIFT = "SHIFT"
    REDUCE = "REDUCE"
    ACCEPT = "ACCEPT"
    ERROR = "ERROR"


@dataclass(frozen=True)
class Action:
    """Single ACTION-table entry.

    Attributes:
        type: Action category such as SHIFT, REDUCE, ACCEPT, or ERROR.
        value: Numeric payload associated with the action.
    """

    type: ActionType
    value: int


@dataclass(frozen=True)
class Rule:
    """Grammar production encoded with symbol identifiers."""

    lhs: int
    rhs: list[int]


@dataclass
class ParseTable:
    """Fully loaded LALR(1) parse table and helper dictionaries."""

    num_states: int
    num_terminals_with_eof: int
    num_non_terminals: int
    terminals: list[str]
    non_terminals: list[str]
    action: list[list[Action]]
    goto_table: list[list[int]]
    eof_terminal: int
    epsilon_terminal: int
    terminal_to_id: dict[str, int] = field(default_factory=dict)
    non_terminal_to_id: dict[str, int] = field(default_factory=dict)


@dataclass
class Grammar:
    """Grammar definition stored as an ordered list of productions."""

    productions: list[Rule]


@dataclass
class InputSequence:
    """Original input string together with its tokenized form."""

    original_text: str
    tokens: list[int]


@dataclass
class TraceRow:
    """Single row of the parsing trace shown in the final report."""

    step: str
    stack: str
    lookahead: str
    decision: str
    detail: str


@dataclass
class ParseResult:
    """Final status and trace produced for one input string."""

    accepted: bool
    summary: str
    rows: list[TraceRow]


@dataclass
class TokenClassifier:
    """Heuristic terminal aliases used while tokenizing raw input text."""

    identifier_terminal: int = -1
    integer_terminal: int = -1
    float_terminal: int = -1
    generic_number_terminal: int = -1
    string_terminal: int = -1
    char_terminal: int = -1
    literal_terminals: list[int] = field(default_factory=list)


# ==== Section 2: Basic File And Text Utilities ============================

def read_text(path: str | Path) -> str:
    """Read an entire text file using UTF-8 encoding."""

    return Path(path).read_text(encoding="utf-8")


def write_text(path: str | Path, content: str) -> None:
    """Write text to a file, creating parent directories when needed."""

    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(content, encoding="utf-8")


def trim(text: str) -> str:
    """Return a string without leading or trailing whitespace."""

    return text.strip()


def split_whitespace(text: str) -> list[str]:
    """Split text using any whitespace as a separator."""

    return text.split()


def suffix_after_colon(line: str, prefix: str) -> str:
    """Extract the portion of a header line that comes after a prefix."""

    if not line.startswith(prefix):
        raise ValueError(f"Expected line with prefix: {prefix}")
    return trim(line[len(prefix):])


def parse_action_type(raw: str) -> ActionType:
    """Convert an action name read from JSON into its enum value."""

    try:
        return ActionType(raw)
    except ValueError as exc:
        raise ValueError(f"Unknown action type: {raw}") from exc


def max_reduce_index(table: ParseTable) -> int:
    """Return the largest reduction index referenced by the ACTION table."""

    max_index = -1
    for row in table.action:
        for action in row:
            if action.type is ActionType.REDUCE and action.value > max_index:
                max_index = action.value
    return max_index


# ==== Section 3: Parse Table Loading =====================================

def load_parse_table(path: str | Path) -> ParseTable:
    """Load and validate the parse table stored in a JSON file.

    Args:
        path: Location of the JSON file that contains the serialized
            ACTION and GOTO tables.

    Returns:
        A fully initialized ``ParseTable`` instance with symbol lists,
        transition matrices, and helper lookup dictionaries.

    Raises:
        ValueError: If the JSON structure is inconsistent, missing
            required fields, or does not contain a valid ``$`` terminal.
        OSError: If the file cannot be read from disk.
        json.JSONDecodeError: If the file content is not valid JSON.
    """

    root = json.loads(read_text(path))

    terminals = root["terminals"]
    non_terminals = root["nonTerminals"]
    action_rows = root["action"]
    goto_rows = root["goto"]
    terminal_to_id = {symbol: index for index, symbol in enumerate(terminals)}
    non_terminal_to_id = {symbol: index for index, symbol in enumerate(non_terminals)}

    if len(terminals) != root["numTerminalsWithEof"]:
        raise ValueError("The number of terminals does not match numTerminalsWithEof.")
    if len(non_terminals) != root["numNonTerminals"]:
        raise ValueError("The number of non-terminals does not match numNonTerminals.")
    if len(action_rows) != root["numStates"]:
        raise ValueError("The number of rows in action does not match numStates.")
    if len(goto_rows) != root["numStates"]:
        raise ValueError("The number of rows in goto does not match numStates.")
    if "$" not in terminal_to_id:
        raise ValueError("The JSON table does not contain the '$' terminal.")

    action: list[list[Action]] = []
    for row in action_rows:
        if len(row) != root["numTerminalsWithEof"]:
            raise ValueError("An action row has an invalid size.")
        action.append(
            [Action(parse_action_type(entry["type"]), int(entry["value"])) for entry in row]
        )

    goto_table: list[list[int]] = []
    for row in goto_rows:
        if len(row) != root["numNonTerminals"]:
            raise ValueError("A goto row has an invalid size.")
        goto_table.append([int(value) for value in row])

    return ParseTable(
        num_states=int(root["numStates"]),
        num_terminals_with_eof=int(root["numTerminalsWithEof"]),
        num_non_terminals=int(root["numNonTerminals"]),
        terminals=terminals,
        non_terminals=non_terminals,
        action=action,
        goto_table=goto_table,
        eof_terminal=terminal_to_id["$"],
        epsilon_terminal=terminal_to_id.get("epsilon", -1),
        terminal_to_id=terminal_to_id,
        non_terminal_to_id=non_terminal_to_id,
    )


# ==== Section 4: Grammar Loading =========================================

def symbol_id_from_text(symbol: str, table: ParseTable) -> int:
    """Resolve a terminal or non-terminal name into the internal symbol id."""

    if symbol in table.terminal_to_id:
        return table.terminal_to_id[symbol]
    if symbol in table.non_terminal_to_id:
        return table.num_terminals_with_eof + table.non_terminal_to_id[symbol]
    raise ValueError(f"Unknown symbol in the grammar: {symbol}")


def parse_rule_line(line: str, table: ParseTable) -> Rule:
    """Parse one production line of the form `A -> alpha beta ...`."""

    if "->" not in line:
        raise ValueError(f"Invalid production: {line}")

    lhs_text, rhs_text = line.split("->", 1)
    lhs_text = trim(lhs_text)
    rhs_text = trim(rhs_text)

    if lhs_text not in table.non_terminal_to_id:
        raise ValueError(f"Unknown non-terminal in the grammar: {lhs_text}")

    rhs = [symbol_id_from_text(symbol, table) for symbol in split_whitespace(rhs_text)]
    return Rule(lhs=table.non_terminal_to_id[lhs_text], rhs=rhs)


def validate_symbol_lists(expected: list[str], actual: list[str], label: str) -> None:
    """Verify that a grammar header matches the symbols in the parse table."""

    if expected != actual:
        raise ValueError(f"The {label} list in the grammar does not match parse_table.json.")


def load_grammar(path: str | Path, table: ParseTable) -> Grammar:
    """Load the grammar file and confirm it is compatible with the JSON table.

    Args:
        path: Path to the grammar file in the expected textual format.
        table: Previously loaded parse table used to validate terminals,
            non-terminals, and production symbol identifiers.

    Returns:
        A ``Grammar`` object containing the production list in the same
        reduction order expected by the parse table.

    Raises:
        ValueError: If the grammar file is malformed, its headers do not
            match the parse table, or it does not define enough rules for
            the reductions referenced by the ACTION table.
        OSError: If the grammar file cannot be read.
    """

    lines = [trim(line) for line in read_text(path).splitlines() if trim(line)]
    if len(lines) < 3:
        raise ValueError("The grammar file must include headers and at least one production.")

    grammar_non_terminals = split_whitespace(suffix_after_colon(lines[0], "Non-terminals:"))
    grammar_terminals = split_whitespace(suffix_after_colon(lines[1], "Terminals:"))

    validate_symbol_lists(table.non_terminals, grammar_non_terminals, "non-terminals")
    validate_symbol_lists(table.terminals[:-1], grammar_terminals, "terminals")

    productions = [parse_rule_line(line, table) for line in lines[2:]]
    needed_rules = max_reduce_index(table) + 1
    if needed_rules > len(productions):
        raise ValueError("The grammar does not have enough productions for the table reductions.")

    return Grammar(productions=productions)


# ==== Section 5: Input Tokenization Helpers ==============================

def is_identifier_token(token: str) -> bool:
    """Check whether a token has identifier form."""

    return bool(token) and (token[0].isalpha() or token[0] == "_") and all(
        character.isalnum() or character == "_" for character in token
    )


def is_integer_token(token: str) -> bool:
    """Check whether a token looks like an integer literal."""

    if not token:
        return False
    start = 1 if token[0] in "+-" else 0
    return start < len(token) and all(character.isdigit() for character in token[start:])


def is_float_token(token: str) -> bool:
    """Check whether a token looks like a floating-point literal."""

    if not token:
        return False

    has_digit = False
    has_dot = False
    has_exponent = False
    index = 0

    while index < len(token):
        current = token[index]
        if current.isdigit():
            has_digit = True
            index += 1
            continue
        if current == "." and not has_dot and not has_exponent:
            has_dot = True
            index += 1
            continue
        if current in "eE" and has_digit and not has_exponent:
            has_exponent = True
            index += 1
            if index < len(token) and token[index] in "+-":
                index += 1
            continue
        return False

    return has_digit and (has_dot or has_exponent)


def is_string_token(token: str) -> bool:
    """Check whether a token is a double-quoted string literal."""

    return len(token) >= 2 and token[0] == '"' and token[-1] == '"'


def is_char_token(token: str) -> bool:
    """Check whether a token is a single-quoted character literal."""

    return len(token) >= 3 and token[0] == "'" and token[-1] == "'"


def find_terminal(table: ParseTable, names: Iterable[str]) -> int:
    """Return the first terminal id found among several candidate names."""

    for name in names:
        if name in table.terminal_to_id:
            return table.terminal_to_id[name]
    return -1


def build_classifier(table: ParseTable) -> TokenClassifier:
    """Build the heuristic classifier used to recognize token categories."""

    classifier = TokenClassifier(
        identifier_terminal=find_terminal(table, ("IDENTIFIER", "ID", "id")),
        integer_terminal=find_terminal(table, ("INT_LITERAL", "INT")),
        float_terminal=find_terminal(table, ("FLOAT_LITERAL", "FLOAT")),
        generic_number_terminal=find_terminal(table, ("NUM", "num", "NUMBER", "number")),
        string_terminal=find_terminal(table, ("STRING_LITERAL", "STRING", "str")),
        char_terminal=find_terminal(table, ("CHAR_LITERAL", "CHAR", "char_lit")),
    )

    for terminal_id, terminal_text in enumerate(table.terminals):
        if terminal_text == "$":
            continue
        if terminal_id in {
            classifier.identifier_terminal,
            classifier.integer_terminal,
            classifier.float_terminal,
            classifier.generic_number_terminal,
            classifier.string_terminal,
            classifier.char_terminal,
        }:
            continue
        classifier.literal_terminals.append(terminal_id)

    classifier.literal_terminals.sort(key=lambda terminal_id: len(table.terminals[terminal_id]), reverse=True)
    return classifier


def classify_token_text(token: str, table: ParseTable, classifier: TokenClassifier) -> int:
    """Map one raw token string to a terminal id whenever possible."""

    if token in table.terminal_to_id:
        return table.terminal_to_id[token]
    if is_string_token(token) and classifier.string_terminal >= 0:
        return classifier.string_terminal
    if is_char_token(token) and classifier.char_terminal >= 0:
        return classifier.char_terminal
    if is_float_token(token):
        if classifier.float_terminal >= 0:
            return classifier.float_terminal
        if classifier.generic_number_terminal >= 0:
            return classifier.generic_number_terminal
    if is_integer_token(token):
        if classifier.integer_terminal >= 0:
            return classifier.integer_terminal
        if classifier.generic_number_terminal >= 0:
            return classifier.generic_number_terminal
        if classifier.float_terminal >= 0:
            return classifier.float_terminal
    if is_identifier_token(token) and classifier.identifier_terminal >= 0:
        return classifier.identifier_terminal
    return -1


def matches_literal_boundary(text: str, position: int, literal: str) -> bool:
    """Check whether a literal matches at a position without breaking identifiers."""

    if position + len(literal) > len(text):
        return False
    if text[position : position + len(literal)] != literal:
        return False

    identifier_like = bool(literal) and (literal[0].isalpha() or literal[0] == "_")
    if not identifier_like:
        return True

    end = position + len(literal)
    if end < len(text):
        next_char = text[end]
        if next_char.isalnum() or next_char == "_":
            return False
    return True


def tokenize_line(line: str, table: ParseTable, classifier: TokenClassifier) -> InputSequence:
    """Tokenize one input line into table-compatible terminal ids.

    Args:
        line: Raw line taken from the input file.
        table: Parse table that defines the valid terminal symbols.
        classifier: Heuristic terminal matcher used for identifiers,
            numeric literals, strings, chars, and literal tokens.

    Returns:
        An ``InputSequence`` containing the original line and the token
        ids recognized for that line. If the line is blank after
        trimming, the returned sequence contains no tokens.

    Raises:
        ValueError: If the line cannot be fully tokenized using the
            grammar terminals and the available classifier heuristics.
    """

    stripped = trim(line)
    sequence = InputSequence(original_text=line, tokens=[])
    if not stripped:
        return sequence

    whitespace_ok = True
    for token in split_whitespace(stripped):
        terminal_id = classify_token_text(token, table, classifier)
        if terminal_id < 0:
            whitespace_ok = False
            break
        sequence.tokens.append(terminal_id)

    if not whitespace_ok or not sequence.tokens:
        sequence.tokens = []
        position = 0

        while position < len(stripped):
            if stripped[position].isspace():
                position += 1
                continue

            matched_literal = False
            for terminal_id in classifier.literal_terminals:
                literal = table.terminals[terminal_id]
                if matches_literal_boundary(stripped, position, literal):
                    sequence.tokens.append(terminal_id)
                    position += len(literal)
                    matched_literal = True
                    break
            if matched_literal:
                continue

            if stripped[position] in {"'", '"'}:
                quote = stripped[position]
                terminal_id = (
                    classifier.string_terminal if quote == '"' else classifier.char_terminal
                )
                if terminal_id < 0:
                    raise ValueError(f"Could not tokenize input string: {stripped}")

                end = position + 1
                escaped = False
                while end < len(stripped):
                    current = stripped[end]
                    if not escaped and current == quote:
                        break
                    escaped = (not escaped) and current == "\\"
                    if escaped and current != "\\":
                        escaped = False
                    end += 1

                if end >= len(stripped):
                    raise ValueError(f"Could not tokenize input string: {stripped}")

                sequence.tokens.append(terminal_id)
                position = end + 1
                continue

            end = position
            while end < len(stripped) and not stripped[end].isspace():
                end += 1

            matched_dynamic = False
            while end > position:
                candidate = stripped[position:end]
                terminal_id = classify_token_text(candidate, table, classifier)
                if terminal_id >= 0:
                    sequence.tokens.append(terminal_id)
                    position = end
                    matched_dynamic = True
                    break
                end -= 1

            if not matched_dynamic:
                raise ValueError(f"Could not tokenize input string: {stripped}")

    if sequence.tokens and sequence.tokens[-1] != table.eof_terminal:
        sequence.tokens.append(table.eof_terminal)

    return sequence


def load_inputs(path: str | Path, table: ParseTable) -> list[InputSequence]:
    """Load all non-empty, non-comment input lines from a file.

    Args:
        path: File containing one candidate input string per line.
        table: Parse table used to tokenize each line consistently.

    Returns:
        A list of tokenized input sequences. Blank lines and lines that
        start with ``#`` are ignored.

    Raises:
        ValueError: If any non-comment line cannot be tokenized.
        OSError: If the input file cannot be read.
    """

    classifier = build_classifier(table)
    inputs: list[InputSequence] = []

    for raw_line in read_text(path).splitlines():
        stripped = trim(raw_line)
        if not stripped or stripped.startswith("#"):
            continue

        sequence = tokenize_line(raw_line, table, classifier)
        if sequence.tokens:
            inputs.append(sequence)

    return inputs


# ==== Section 6: Parsing Engine ==========================================

def symbol_name(table: ParseTable, symbol_id: int) -> str:
    """Convert an internal symbol id back into its printable name."""

    if 0 <= symbol_id < table.num_terminals_with_eof:
        return table.terminals[symbol_id]
    non_terminal_id = symbol_id - table.num_terminals_with_eof
    if 0 <= non_terminal_id < table.num_non_terminals:
        return table.non_terminals[non_terminal_id]
    return "?"


def rule_to_string(table: ParseTable, rule_index: int, rule: Rule) -> str:
    """Format one production as `pN: A -> ...` for the report."""

    if not rule.rhs:
        rhs_text = " ε"
    else:
        rhs_text = "".join(f" {symbol_name(table, symbol_id)}" for symbol_id in rule.rhs)
    return f"p{rule_index}: {table.non_terminals[rule.lhs]} ->{rhs_text}"


def stack_to_string(table: ParseTable, states: list[int], symbols: list[int]) -> str:
    """Render the parser stack in an interleaved `state symbol state` form."""

    pieces: list[str] = []
    for index, state in enumerate(states):
        if index > 0:
            pieces.append(symbol_name(table, symbols[index - 1]))
        pieces.append(str(state))
    return " ".join(pieces)


def parse_sequence(table: ParseTable, grammar: Grammar, sequence: InputSequence) -> ParseResult:
    """Simulate the shift-reduce process for one tokenized input sequence.

    Args:
        table: Parse table that provides ACTION and GOTO decisions.
        grammar: Grammar productions referenced by reduce actions.
        sequence: Tokenized input sequence to parse.

    Returns:
        A ``ParseResult`` containing the acceptance status, a summary
        message, and the detailed trace rows generated during parsing.
    """

    states = [0]
    symbols: list[int] = []
    rows: list[TraceRow] = []
    input_position = 0
    step_limit = 1 + len(sequence.tokens) * 8 + len(grammar.productions) * 4

    for step in range(1, step_limit + 1):
        if input_position >= len(sequence.tokens):
            return ParseResult(False, "The input finished before reaching ACCEPT.", rows)

        state = states[-1]
        lookahead = sequence.tokens[input_position]
        action = table.action[state][lookahead]

        row = TraceRow(
            step=str(step),
            stack=stack_to_string(table, states, symbols),
            lookahead=table.terminals[lookahead],
            decision="",
            detail="",
        )

        if action.type is ActionType.SHIFT:
            row.decision = f"shift->{action.value}"
            row.detail = "consume lookahead and push next state"
            rows.append(row)
            symbols.append(lookahead)
            states.append(action.value)
            input_position += 1
            continue

        if action.type is ActionType.REDUCE:
            row.decision = f"reduce p{action.value}"
            if action.value < 0 or action.value >= len(grammar.productions):
                row.detail = "reduction index not present in grammar"
                rows.append(row)
                return ParseResult(False, f"Reduction p{action.value} does not exist in the grammar.", rows)

            rule = grammar.productions[action.value]
            row.detail = rule_to_string(table, action.value, rule)
            rows.append(row)

            rhs_to_pop = [
                symbol_id
                for symbol_id in rule.rhs
                if symbol_id != table.epsilon_terminal
            ]

            if len(rhs_to_pop) > len(symbols):
                return ParseResult(False, "The stack does not contain enough symbols for the reduction.", rows)

            for expected in reversed(rhs_to_pop):
                found = symbols[-1]
                if expected != found:
                    return ParseResult(
                        False,
                        (
                            f"Inconsistent reduction in p{action.value}: expected "
                            f"'{symbol_name(table, expected)}' but found '{symbol_name(table, found)}'."
                        ),
                        rows,
                    )
                symbols.pop()
                states.pop()

            goto_state = table.goto_table[states[-1]][rule.lhs]
            if goto_state < 0:
                return ParseResult(
                    False,
                    f"GOTO[{states[-1]}, {table.non_terminals[rule.lhs]}] does not exist.",
                    rows,
                )

            symbols.append(table.num_terminals_with_eof + rule.lhs)
            states.append(goto_state)
            continue

        if action.type is ActionType.ACCEPT:
            row.decision = "accept"
            row.detail = "successful parse"
            rows.append(row)
            return ParseResult(True, "Accepted.", rows)

        row.decision = "error"
        row.detail = "no valid action for this state/lookahead"
        rows.append(row)
        return ParseResult(False, f"Syntax error at state {state} with lookahead '{table.terminals[lookahead]}'.", rows)

    return ParseResult(False, "The trace stopped because the step limit was reached.", rows)


# ==== Section 7: Report Rendering ========================================

def action_cell(action: Action) -> str:
    """Render an ACTION entry using compact parsing-table notation."""

    if action.type is ActionType.SHIFT:
        return f"s{action.value}"
    if action.type is ActionType.REDUCE:
        return f"r{action.value}"
    if action.type is ActionType.ACCEPT:
        return "acc"
    return "-"


def render_table(headers: list[str], rows: list[list[str]]) -> str:
    """Render a generic ASCII table from headers and row data."""

    widths = [len(header) for header in headers]
    for row in rows:
        for index, value in enumerate(row):
            widths[index] = max(widths[index], len(value))

    border = "+" + "+".join("-" * (width + 2) for width in widths) + "+"
    output = [border]
    output.append(
        "| "
        + " | ".join(header.ljust(widths[index]) for index, header in enumerate(headers))
        + " |"
    )
    output.append(border)

    for row in rows:
        output.append(
            "| "
            + " | ".join(value.ljust(widths[index]) for index, value in enumerate(row))
            + " |"
        )
    output.append(border)
    return "\n".join(output)


def render_rules(table: ParseTable, grammar: Grammar) -> str:
    """Render the grammar productions section of the report."""

    lines = ["Loaded rules:"]
    for rule_index, rule in enumerate(grammar.productions):
        lines.append(f"  {rule_to_string(table, rule_index, rule)}")
    return "\n".join(lines)


def render_parse_table(table: ParseTable) -> str:
    """Render the complete ACTION/GOTO parse table as an ASCII table.

    Args:
        table: Parse table to format for the report.

    Returns:
        A multiline string containing the parsing table in ASCII form.
    """

    headers = ["State"] + [f"A:{terminal}" for terminal in table.terminals] + [
        f"G:{non_terminal}" for non_terminal in table.non_terminals
    ]
    rows: list[list[str]] = []

    for state in range(table.num_states):
        row = [str(state)]
        row.extend(action_cell(action) for action in table.action[state])
        row.extend(str(value) if value >= 0 else "-" for value in table.goto_table[state])
        rows.append(row)

    return "Parsing table:\n" + render_table(headers, rows)


def render_trace(sequence: InputSequence, index: int, result: ParseResult) -> str:
    """Render the trace section for one parsed input case.

    Args:
        sequence: Original input line and token sequence used for parsing.
        index: One-based case number used in the report.
        result: Trace and status produced by the parser simulation.

    Returns:
        A multiline string with the case header, trace table, and final
        outcome message.
    """

    headers = ["Step", "Stack", "Lookahead", "Decision", "Detail"]
    rows = [
        [row.step, row.stack, row.lookahead, row.decision, row.detail]
        for row in result.rows
    ]

    parts = [
        f"Case {index}: {sequence.original_text}",
        render_table(headers, rows),
        f"Outcome: {result.summary}",
    ]
    return "\n".join(parts)


def build_report(table: ParseTable, grammar: Grammar, inputs: list[InputSequence]) -> str:
    """Assemble the full multi-section report shown in terminal and file output.

    Args:
        table: Parse table to display and use during parsing.
        grammar: Grammar production list shown in the report header.
        inputs: Tokenized input cases to parse and render.

    Returns:
        The complete textual report, ready to be printed or written to a
        file.
    """

    sections = [render_rules(table, grammar), render_parse_table(table)]
    for index, sequence in enumerate(inputs, start=1):
        result = parse_sequence(table, grammar, sequence)
        sections.append(render_trace(sequence, index, result))
    return "\n\n".join(sections) + "\n"


# ==== Section 8: Command-Line Entry Point ================================

def main(argv: list[str]) -> int:
    """Parse CLI arguments, generate the report, and emit the final output.

    Args:
        argv: Command-line argument vector. The expected order is
            ``<grammar.txt> <parse_table.json> <inputs.txt> [results.txt]``.

    Returns:
        ``0`` when the report is generated successfully, or ``1`` when
        an error is detected.
    """

    if len(argv) not in {4, 5}:
        print(
            f"Usage: {argv[0]} <grammar.txt> <parse_table.json> <inputs.txt> [results.txt]",
            file=sys.stderr,
        )
        return 1

    try:
        grammar_path = argv[1]
        table_path = argv[2]
        inputs_path = argv[3]
        output_path = argv[4] if len(argv) == 5 else None

        table = load_parse_table(table_path)
        grammar = load_grammar(grammar_path, table)
        inputs = load_inputs(inputs_path, table)

        if not inputs:
            raise ValueError("The input file does not contain valid strings.")

        report = build_report(table, grammar, inputs)

        if output_path is not None:
            write_text(output_path, report)

        try:
            print(report, end="")
        except BrokenPipeError:
            return 0

        return 0
    except Exception as error:  # noqa: BLE001
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))