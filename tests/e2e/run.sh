#!/bin/sh
#
# End-to-end tests for the axon shell.
#
# Each case pipes a script into the built ./axon binary and asserts on what it
# prints. Every case runs with an isolated HOME ($(mktemp -d)) so the context
# database starts fresh and the tests never touch the real ~/.axon/. AI
# commands (?, !!) are intentionally not covered here: they need an API key and
# their responses are nondeterministic.
#
# Exits 0 if every case passes, 1 otherwise.

AXON="./axon"
if [ ! -x "$AXON" ]; then
    echo "e2e: $AXON not found or not executable (run 'make build' first)" >&2
    exit 1
fi

pass=0
fail=0

# out=$(run_out "<input>")  — feed input to axon, capture stdout only.
run_out() {
    h=$(mktemp -d)
    printf '%s\n' "$1" | HOME="$h" "$AXON" 2>/dev/null
    rm -rf "$h"
}

# err=$(run_err "<input>") — feed input to axon, capture stderr only.
run_err() {
    h=$(mktemp -d)
    printf '%s\n' "$1" | HOME="$h" "$AXON" 2>&1 1>/dev/null
    rm -rf "$h"
}

# check "<description>" "<expected>" "<actual>"
check() {
    if [ "$2" = "$3" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf 'FAIL: %s\n  expected: [%s]\n  actual:   [%s]\n' "$1" "$2" "$3"
    fi
}

# check_contains "<description>" "<needle>" "<haystack>"
check_contains() {
    case "$3" in
        *"$2"*) pass=$((pass + 1)) ;;
        *)
            fail=$((fail + 1))
            printf 'FAIL: %s\n  expected to contain: [%s]\n  actual:              [%s]\n' \
                "$1" "$2" "$3"
            ;;
    esac
}

# --- Core shell behavior ---

check "simple command" "hello" "$(run_out 'echo hello')"

check "pipeline" "HELLO" "$(run_out 'echo hello | tr a-z A-Z')"

check "redirect write then read" "persisted" \
    "$(run_out 'echo persisted > e2e_out.txt
cat e2e_out.txt')"

check "chain && runs on success" "second" \
    "$(run_out 'true && echo second')"

check "chain || runs on failure" "recovered" \
    "$(run_out 'false || echo recovered')"

check "exit-code expansion (\$?)" "code 0" \
    "$(run_out 'true
echo code $?')"

# axon expands $VAR / ${VAR} from its environment; export one so it inherits it.
export AXON_WORD=is
check "env var expansion" "home is set" \
    "$(run_out 'echo home ${AXON_WORD} set')"

check "nonzero exit reported on stderr" "" "$(run_out 'false')"
check_contains "exit code shown for failing command" "exit 1" "$(run_err 'false')"

check "no prompt when piped" "" "$(run_err 'true')"

# --- Sandbox prefix (Step 12) ---

check "sandbox runs command" "sandboxed" "$(run_out '& echo sandboxed')"

check "sandbox pipeline works" "OUT" "$(run_out '& echo out | tr a-z A-Z')"

check_contains "sandbox propagates nonzero exit" "exit 3" \
    "$(run_err '& sh -c "exit 3"')"

check "&& is still the chain operator, not sandbox" "chained" \
    "$(run_out 'true && echo chained')"

# --- AI-driven flow (Step 13; opt-in, needs a real API key, excluded from CI) ---
#
# The suggested command is nondeterministic, so we only assert the flow that is
# stable regardless of what Claude returns: after `!!`, the shell offers to run
# the suggestion in the sandbox, and answering "n" declines without running it.
if [ "${AXON_E2E_AI:-0}" = "1" ] && [ -n "${ANTHROPIC_API_KEY:-}" ]; then
    check_contains "!! offers to run suggestion in sandbox" "run in sandbox?" \
        "$(run_err '!! print the word hello
n')"
fi

# --- Summary ---

total=$((pass + fail))
echo "e2e: $pass/$total passed"
[ "$fail" -eq 0 ]
