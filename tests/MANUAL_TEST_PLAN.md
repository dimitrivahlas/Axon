# Step 6 — Manual Test Plan: Context Engine Wired into AI Commands

## Prerequisites

```sh
cd ~/Desktop/projects/pm/axon
make clean && make build && make test   # All 80 tests must pass
export ANTHROPIC_API_KEY="your-key"     # Required for AI call tests
```

---

## 1. Build & Unit Tests (Sanity Check)

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 1.1 | Clean build | `make clean && make build` | Zero warnings, zero errors |
| 1.2 | All tests pass | `make test` | 80 tests, 0 failures |
| 1.3 | history.h fully removed | `grep -r 'history\.h\|history_t\|history_init\|history_add' shell/ context/ tests/` | No matches |

---

## 2. Context Persistence Across Sessions

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 2.1 | DB file created | `rm -f ~/.axon/context.db && ./axon`, type `echo hello`, then `exit` | `~/.axon/context.db` exists |
| 2.2 | Commands persist | Open `sqlite3 ~/.axon/context.db "SELECT command, exit_code FROM commands ORDER BY id DESC LIMIT 5;"` | Shows `echo hello` with exit_code 0 |
| 2.3 | Data survives restart | Run `./axon`, type `ls`, `exit`. Run `./axon` again, type `? what was my last command` | AI response references `ls` (from previous session) |
| 2.4 | Session tracking | `sqlite3 ~/.axon/context.db "SELECT session_id, started_at, ended_at FROM sessions ORDER BY started_at DESC LIMIT 3;"` | Each shell invocation has a distinct session_id, started_at is set, ended_at is set after exit |

---

## 3. AI Ask (`? <question>`) with Context

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 3.1 | Basic question | Run `./axon`, type `echo hello`, then `? what did I just run` | AI response references `echo hello` |
| 3.2 | Error context | Run `cat /nonexistent/file`, then `? why did that fail` | AI explains file not found, references the failed command and non-zero exit code |
| 3.3 | Multi-command context | Run `ls`, `pwd`, `echo test`, then `? summarize my recent commands` | AI lists the recent commands with context |
| 3.4 | No API key | `unset ANTHROPIC_API_KEY`, then `? hello` | Prints `axon: ANTHROPIC_API_KEY not set` to stderr, no crash |
| 3.5 | Empty question | `?` (no text after) | Should not trigger AI mode (treated as unknown command) |
| 3.6 | Git context included | Run inside the axon repo, `? what branch am I on` | AI correctly identifies the current git branch |

---

## 4. AI Suggest (`!! <intent>`) with Context

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 4.1 | Basic suggestion | `!! list all files sorted by size` | Prints a single command like `ls -lS` or `du -sh * \| sort -h` |
| 4.2 | Context-aware suggestion | Run `cd /tmp`, then `!! delete all .log files here` | Suggested command uses current directory context (e.g., `rm *.log` or `find . -name '*.log' -delete`) |
| 4.3 | After error | Run `gcc nonexistent.c`, then `!! fix the last command` | Suggestion is context-aware of the failed gcc command |
| 4.4 | No API key | `unset ANTHROPIC_API_KEY`, then `!! list files` | Prints `axon: ANTHROPIC_API_KEY not set`, no crash |

---

## 5. JSON Payload Validation

Verify the JSON the AI actually receives is well-formed and complete.

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 5.1 | Inspect JSON structure | Add a temporary `fprintf(stderr, "%s\n", ctx_json);` in `main.c` before the `ai_ask` call, rebuild, run a few commands, then `? test` | JSON printed to stderr is valid (pipe through `jq .`), contains: `session_id`, `recent_commands[]`, `error_summary`, `cwd` |
| 5.2 | Git section present | Run inside a git repo, inspect JSON | `git` object present with `branch`, `is_dirty`, `modified_files`, `staged_files`, `recent_commits` |
| 5.3 | Git section absent | Run from `/tmp` (not a git repo), inspect JSON | No `git` key in JSON |
| 5.4 | Error summary accurate | Run 3 commands (2 fail, 1 succeeds), inspect JSON | `error_summary.recent_failures` reflects actual failure count |
| 5.5 | Special chars escaped | Run `echo "hello \"world\""`, inspect JSON | Command string properly escaped in JSON (no broken syntax) |

> **Cleanup**: Remove the temporary `fprintf` after testing.

---

## 6. Edge Cases & Robustness

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 6.1 | Fresh DB (no history) | `rm -f ~/.axon/context.db`, run `./axon`, immediately type `? help me` | AI responds (with empty context), no crash. `recent_commands` is `[]` |
| 6.2 | Long session | Run 30+ commands in a single session, then `? what have I been doing` | AI receives up to 20 recent commands (default limit), response is coherent |
| 6.3 | Rapid commands | Run `echo 1 && echo 2 && echo 3 && echo 4 && echo 5`, then `? what happened` | All chained commands recorded, AI references them |
| 6.4 | Stderr capture | Run `ls /nonexistent 2>&1`, then `? what error did I get` | AI references the stderr output from the failed command |
| 6.5 | Signal handling | Start `./axon`, press Ctrl+C mid-prompt, then type `? am I still here` | Shell recovers, AI call works normally |
| 6.6 | Non-git directory | `cd /tmp && /path/to/axon`, type `? where am I` | Works without git section in context, AI correctly says `/tmp` |

---

## 7. Regression Checks

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 7.1 | Pipes still work | `echo hello \| tr a-z A-Z` | Prints `HELLO` |
| 7.2 | Redirects still work | `echo test > /tmp/axon_test.txt && cat /tmp/axon_test.txt` | Prints `test` |
| 7.3 | Builtins still work | `cd /tmp`, `help`, `cd -` | All work as before |
| 7.4 | Env vars still work | `echo $HOME`, `echo $?` | Correct expansion |
| 7.5 | Chaining still works | `true && echo yes`, `false \|\| echo fallback`, `echo a; echo b` | Correct chaining behavior |
| 7.6 | Exit works | `exit` | Shell exits cleanly, session end time recorded in DB |

---

## 8. Cleanup

```sh
# Remove any temp debug prints added during testing
# Verify final clean build
make clean && make build && make test
```

---

## Pass Criteria

- All 80 unit tests pass
- All manual tests above pass (AI tests require valid API key)
- No crashes, no memory errors, no broken JSON
- Zero references to `history.h` / `history_t` remain in codebase
- Context DB (`~/.axon/context.db`) persists data correctly across sessions
