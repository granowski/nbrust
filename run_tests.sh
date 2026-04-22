#!/bin/sh

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Compiler binary
NBRUST="./nbrust"

# Ensure nbrust is built
if [ ! -f "$NBRUST" ]; then
    echo "nbrust not found, building..."
    ./build.sh || exit 1
fi

TEST_DIR="tests"
PASSED=0
FAILED=0
TOTAL=0

# Temporary file for binary
TMP_BIN="./tmp/test_bin"

echo "Running tests in $TEST_DIR..."
echo "---------------------------------------"

for test_file in "$TEST_DIR"/*.rs; do
    [ -e "$test_file" ] || continue
    test_name=$(basename "$test_file")
    
    # Skip ARM tests if not on appropriate hardware/OS
    # This is a simple heuristic
    if echo "$test_name" | grep -q "arm64" && [ "$(uname -m)" != "arm64" ]; then
        echo "Skipping $test_name (non-arm64 host)"
        continue
    fi
    if echo "$test_name" | grep -q "armv6" && [ "$(uname -m)" != "arm" ]; then
        echo "Skipping $test_name (non-arm host)"
        continue
    fi

    TOTAL=$((TOTAL + 1))
    test_name=$(basename "$test_file")
    
    # Run nbrust and compile
    # We use -o to let nbrust handle the C compilation via cc
    # We capture stderr to check for borrow checker or other errors
    $NBRUST "$test_file" -o "$TMP_BIN" > "./tmp/${test_file%%.*}-nbrust_test.log" 2>&1
    RET=$?
    
    if [ $RET -eq 0 ]; then
        # If compilation succeeded, try to run the binary
        # Some tests might be expected to fail at runtime or just compile
        # But for now, we assume if it compiles, we should try to run it.
        # However, some tests might not have a main function or might be library components.
        # We'll check if the binary was actually created.
        if [ -f "$TMP_BIN" ]; then
            "$TMP_BIN" > /dev/null 2>&1
            RUN_RET=$?
            # Some tests return non-zero values as success
            if [ $RUN_RET -eq 0 ] || \
               ([ "$test_name" = "structs.rs" ] && [ $RUN_RET -eq 30 ]) || \
               ([ "$test_name" = "impl.rs" ] && [ $RUN_RET -eq 200 ]) || \
               ([ "$test_name" = "arm64_basic.rs" ] && [ $RUN_RET -eq 30 ]) || \
               ([ "$test_name" = "arm64_complex.rs" ] && [ $RUN_RET -eq 120 ]) || \
               ([ "$test_name" = "functions.rs" ] && [ $RUN_RET -eq 3 ]); then
                echo "${GREEN}[PASS]${NC} $test_name"
                PASSED=$((PASSED + 1))
            else
                echo "${RED}[FAIL]${NC} $test_name (Execution failed with $RUN_RET)"
                FAILED=$((FAILED + 1))
            fi
            rm "$TMP_BIN"
        else
            # Compiled successfully but no binary (maybe -c was used or it's a lib)
            echo "${GREEN}[PASS]${NC} $test_name (Compiled only)"
            PASSED=$((PASSED + 1))
        fi
    else
        # Compilation failed. 
        # Check if it was a borrow checker error which might be expected for some tests
        # (e.g., test_analysis_borrow_fail.rs)
        if echo "$test_name" | grep -q "fail"; then
            echo "${GREEN}[PASS]${NC} $test_name (Expected failure)"
            PASSED=$((PASSED + 1))
        else
            echo "${RED}[FAIL]${NC} $test_name (Compilation failed)"
            FAILED=$((FAILED + 1))
        fi
    fi
done

echo "---------------------------------------"
echo "Tests complete: $TOTAL total, $PASSED passed, $FAILED failed."

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
