# LALR(1) Parser + Flex Scanner

This project builds a LALR(1) parsing table from a grammar file and parses a token stream produced by Flex (`scanner.l`).

## Prerequisites

- CMake >= 3.16
- A C compiler (MSVC, MinGW, or Clang)
- Flex available in PATH (required by `find_package(FLEX REQUIRED)`)
- Python 3 (optional, only for `tools/lalr_trace_driver.py`)

## Build

From `lalr_1`:

### Windows

```powershell
cmake -S . -B build
cmake --build build
```
If your default generator does not work on Windows, specify one explicitly, for example:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

### Debian/Ubuntu

Install dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake flex bison
```

Build:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

### Rocky Linux

Install dependencies:

```bash
sudo dnf install -y gcc make cmake flex bison
```

Build:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Run

Program usage:

```text
first_and_follow <grammar_file> [source_file] [table_output.(csv|json)]
```

- `grammar_file`: grammar definition used to build automaton and table.
- `source_file`: optional input source scanned by Flex (`yyin`).
- `table_output.(csv|json)`: optional output file for the generated parse table.
	If omitted, the program writes `parse_table.csv` in the working directory.

### Example

```powershell
./build/first_and_follow ./examples/grammar_decl.txt ./examples/input_decl.c
```

Example writing JSON table:

```powershell
./build/first_and_follow ./examples/grammar_decl.txt ./examples/input_decl.c ./build/parse_table.json
```

Expected output should end with:

```text
Input accepted.
```

## Python Trace Driver

The repository also includes a trace driver implemented in Python:

```bash
python3 ./tools/lalr_trace_driver.py \
  ./examples/00_Example/grammar_decl.txt \
  ./examples/00_Example/parse_table_decl.json \
  ./examples/00_Example/input_decl.c
```

Optional output file:

```bash
python3 ./tools/lalr_trace_driver.py \
  ./examples/00_Example/grammar_decl.txt \
  ./examples/00_Example/parse_table_decl.json \
  ./examples/00_Example/input_decl.c \
  ./build/lalr_trace_report.txt
```

## Grammar File Format

The parser expects:

1. Line 1: non-terminals list with `Non-terminals:` prefix.
2. Line 2: terminals list with `Terminals:` prefix.
3. Following lines: productions in form `A -> alpha`.

Example:

```text
Non-terminals: S Decl Type
Terminals: int float ID ;
S -> Decl
Decl -> Type ID ;
Type -> int
Type -> float
```

## Notes on Token Mapping

`main.c` maps lexer tokens to grammar terminals by:

- Direct lexeme match (for symbols like `int`, `;`, `(`, `)` and so on).
- Aliases for common token classes (`IDENTIFIER`, `ID`, `id`, numeric/string literal aliases).
- EOF is mapped to parser end symbol `$` internally.