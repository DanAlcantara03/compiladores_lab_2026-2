#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build"
EXECUTABLE = BUILD_DIR / ("p06_validator.exe" if sys.platform.startswith("win") else "p06_validator")

VALID_TESTS = [
    "valid_basic.lang",
    "valid_control.lang",
    "valid_functions.lang",
    "valid_for_range.lang",
    "valid_zero_literals.lang",
]

INVALID_TESTS = {
    "invalid_syntax_missing_colon.lang": "syntax error",
    "invalid_indent.lang": "lexical error",
    "invalid_undeclared.lang": "semantic error",
    "invalid_redeclare.lang": "semantic error",
    "invalid_type_assignment.lang": "semantic error",
    "invalid_condition_type.lang": "semantic error",
    "invalid_function_args.lang": "semantic error",
    "invalid_ret_type.lang": "semantic error",
    "invalid_leading_zero_int.lang": "lexical error",
    "invalid_leading_zero_float.lang": "lexical error",
}


def run_command(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, **kwargs)


def build():
    configure = run_command(["cmake", "-S", ".", "-B", str(BUILD_DIR)])
    if configure.returncode != 0:
        print(configure.stdout)
        print(configure.stderr, file=sys.stderr)
        return False

    compile_result = run_command(["cmake", "--build", str(BUILD_DIR)])
    if compile_result.returncode != 0:
        print(compile_result.stdout)
        print(compile_result.stderr, file=sys.stderr)
        return False

    return True


def run_case(path):
    source = path.read_text(encoding="utf-8")
    return run_command([str(EXECUTABLE)], input=source)


def main():
    if not build():
        return 1

    failures = 0

    for test_name in VALID_TESTS:
        result = run_case(ROOT / "tests" / test_name)
        if result.returncode == 0 and "Analysis success. AST:" in result.stdout:
            print(f"[PASS] {test_name}")
        else:
            failures += 1
            print(f"[FAIL] {test_name}")
            print(result.stdout)
            print(result.stderr, file=sys.stderr)

    for test_name, expected_error in INVALID_TESTS.items():
        result = run_case(ROOT / "tests" / test_name)
        if result.returncode != 0 and expected_error in result.stderr:
            print(f"[PASS] {test_name}")
        else:
            failures += 1
            print(f"[FAIL] {test_name}")
            print(result.stdout)
            print(result.stderr, file=sys.stderr)

    if failures:
        print(f"{failures} test(s) failed.")
        return 1

    print("All tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
