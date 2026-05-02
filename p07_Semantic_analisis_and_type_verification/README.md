# P07 Semantic Analyzer and Type Validator

This practice implements the semantic-analysis phase requested in laboratory P07 for the compact indentation-based language from P06. It includes a Flex scanner, a Bison parser, a decorated AST, scoped symbol tables, type checking, and safe numeric promotion in C.

## Language Summary

The language is designed to keep common keywords short while preserving readable syntax:

```text
fn sum(int a, int b) -> int:
    ret a + b

int x = 3
y = 4
var inferred = x + 2.5

if x < y:
    y = y + 1
elif x == y:
    y = 0
else:
    y = y - 1

wh y > 0:
    y = y - 1

for i in 0..10:
    x = x + i
```

## Token Reference

| Category | Tokens |
| --- | --- |
| Control flow | `if`, `elif`, `else`, `wh`, `for`, `in` |
| Functions | `fn`, `ret`, `->` |
| Types | `int`, `float`, `str`, `bool`, `var` |
| Booleans | `T`, `F` |
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Logic | `&`, `\|`, `!` |
| Assignment and range | `=`, `..` |
| Structure | `:`, `,`, `(`, `)`, `NEWLINE`, `INDENT`, `DEDENT` |

Comments begin with `#`. Blocks are delimited by indentation, not braces.

Numeric literals do not allow leading zeroes. Valid examples include `0`, `7`, `120`, `0.5`, and `12.5`; invalid examples include `01`, `001`, `01.5`, and `001.5`.

## Semantics

- Variables can be explicitly typed: `int x = 3`.
- Variables can be explicitly inferred with `var x = expression`.
- Variables can be inferred on first assignment: `x = 3`.
- Reassignments must preserve type compatibility.
- `int` can be assigned to `float`, but incompatible assignments fail.
- Variables must be declared or inferred before use.
- Each symbol stores its name, category, data type, scope depth, and function signature when applicable.
- Redeclaration in the same scope is rejected.
- Shadowing is allowed in nested scopes, so an inner declaration can hide an outer one without deleting it.
- `if`, `elif`, and `wh` conditions must be `bool`.
- `for i in a..b` requires integer range bounds.
- Functions must be declared before use.
- Function arguments are checked by count and type.
- `ret` must match the declared function return type.
- Variables and parameters that are declared but never read emit warnings without changing the success exit code.

## Build

Requirements:

- CMake >= 3.16
- C compiler
- Flex
- Bison
- Optional: Graphviz for DOT rendering

Build:

```bash
cmake -S . -B build
cmake --build build
```

## Run

The validator reads source code from `stdin`.

```bash
./build/p07_validator < tests/valid_functions.summ
```

Successful programs print a readable AST to `stdout`. Lexical, syntax, and semantic errors are printed to `stderr`.

Optional DOT export:

```bash
./build/p07_validator --dot ast.dot < tests/valid_functions.summ
dot -Tpng ast.dot -o ast.png
```

Optional Docker run:

```bash
docker build -t p07-validator .
docker run --rm -i p07-validator < tests/valid_basic.summ
```

## Tests

Run the complete suite:

```bash
python3 tests/run_tests.py
```

Included valid cases:

- `tests/valid_basic.summ`
- `tests/valid_control.summ`
- `tests/valid_functions.summ`
- `tests/valid_for_range.summ`
- `tests/valid_shadowing.summ`
- `tests/valid_var_inference.summ`
- `tests/valid_warnings_clean.summ`
- `tests/valid_zero_literals.summ`

Included invalid cases:

- `tests/invalid_syntax_missing_colon.summ`
- `tests/invalid_indent.summ`
- `tests/invalid_undeclared.summ`
- `tests/invalid_redeclare.summ`
- `tests/invalid_local_redeclare.summ`
- `tests/invalid_type_assignment.summ`
- `tests/invalid_condition_type.summ`
- `tests/invalid_function_args.summ`
- `tests/invalid_var_inference_error.summ`
- `tests/invalid_ret_type.summ`
- `tests/invalid_leading_zero_int.summ`
- `tests/invalid_leading_zero_float.summ`

Included warning cases:

- `tests/warning_unused_local.summ`
- `tests/warning_unused_parameter.summ`

## Optional Extra Work

Implemented optional extra-point items are documented in [`EXTRA_IMPLEMENTATION_TODO.md`](EXTRA_IMPLEMENTATION_TODO.md).

## Implementation Notes

- `src/lexer.l` recognizes compact tokens and emits `NEWLINE`, `INDENT`, and `DEDENT` for Python-like blocks.
- `src/parser.y` defines the grammar, creates the AST, and performs semantic checks during reductions while accumulating errors.
- `src/ast.c` and `src/ast.h` implement AST nodes, console printing, memory cleanup, and DOT export.
- `src/symtab.c` and `src/symtab.h` implement scoped symbol tables for variables, constants, and functions, including current-scope lookup and visible-scope lookup.

## Documentation Used

- Flex manual: https://westes.github.io/flex/manual/
- Bison manual: https://www.gnu.org/software/bison/manual/bison.html
- CMake tutorial: https://cmake.org/cmake/help/latest/guide/tutorial/index.html
- Graphviz DOT language: https://graphviz.org/doc/info/lang.html
