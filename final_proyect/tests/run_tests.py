#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "summc"


CASES = [
    ("basic_io.summ", 0, ["INPUT", "PRINT n"]),
    ("control_codegen.summ", 0, ["IFFALSE", "LABEL __while", "PRINT y"]),
    ("functions_codegen.summ", 0, ["PARAM 2", "GOSUB __fn_sum", "PRINT result"]),
    ("native_graphics.summ", 0, ["PIXEL x x 1", "KEY 8", "PRINT \"space\""]),
    ("invalid_type.summ", 1, ["semantic error"]),
]


def run_case(name, expected_code, snippets):
    source = ROOT / "tests" / name
    result = subprocess.run(
        [str(BIN)],
        input=source.read_text(),
        text=True,
        capture_output=True,
        check=False,
    )
    combined = result.stdout + result.stderr

    if expected_code == 0 and result.returncode != 0:
        print(f"FAIL {name}: expected success, got {result.returncode}")
        print(combined)
        return False
    if expected_code != 0 and result.returncode == 0:
        print(f"FAIL {name}: expected failure")
        print(combined)
        return False

    for snippet in snippets:
        if snippet not in combined:
            print(f"FAIL {name}: missing snippet {snippet!r}")
            print(combined)
            return False

    print(f"PASS {name}")
    return True


def main():
    if not BIN.exists():
        print("summc binary not found. Run `make` first.", file=sys.stderr)
        return 1

    ok = True
    for name, expected_code, snippets in CASES:
        ok = run_case(name, expected_code, snippets) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
