#!/bin/bash
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TEST_FILES="$ROOT_DIR/tests/test_files"
EXPECTED="$ROOT_DIR/tests/expected_results"
BCC="$ROOT_DIR/build/bcc"
FAILED_ASM="$ROOT_DIR/tests/failed_assemblies"
PASS=0
FAIL=0

mkdir -p "$FAILED_ASM"
rm -f "$FAILED_ASM"/*

shopt -s nullglob
bcfiles=("$TEST_FILES"/*.bc)
shopt -u nullglob

if [ ${#bcfiles[@]} -eq 0 ]; then
    echo "No test files found in $TEST_FILES."
    echo "=============================="
    echo "Total: 0, Passed: 0, Failed: 0"
    exit 0
fi

for bcfile in "${bcfiles[@]}"; do
    base=$(basename "$bcfile" .bc)
    expected_file="$EXPECTED/$base.expected"
    output_file="$ROOT_DIR/tests/$base.out"
    exec_file="$ROOT_DIR/$base"
    exec_file_elf="$ROOT_DIR/$base.elf"
    asm_file="$ROOT_DIR/$base.s"

    # Compile from project root, always with -s to keep .s file
    cd "$ROOT_DIR"
    "$BCC" -s "tests/test_files/$base.bc" > /dev/null 2>&1

    # Check for either executable
    if [ -x "$exec_file" ]; then
        exec_to_run="$exec_file"
    elif [ -x "$exec_file_elf" ]; then
        exec_to_run="$exec_file_elf"
    else
        echo "[FAIL] $base (compilation failed)"
        # Move .s file if it exists
        [ -f "$asm_file" ] && mv "$asm_file" "$FAILED_ASM/"
        FAIL=$((FAIL+1))
        continue
    fi

    # Run and capture output
    "$exec_to_run" > "$output_file" 2>&1

    if diff -q "$output_file" "$expected_file" > /dev/null; then
        echo "[PASS] $base"
        PASS=$((PASS+1))
        # Clean up .s file if test passed
        rm -f "$asm_file"
    else
        echo "[FAIL] $base"
        echo "Expected:"
        cat "$expected_file"
        echo "Actual:"
        cat "$output_file"
        # Move .s file if test failed
        [ -f "$asm_file" ] && mv "$asm_file" "$FAILED_ASM/"
        FAIL=$((FAIL+1))
    fi

    # Clean up output and executables
    rm -f "$output_file" "$exec_file" "$exec_file_elf"
done

echo "=============================="
echo "Total: $((PASS+FAIL)), Passed: $PASS, Failed: $FAIL"
exit $FAIL