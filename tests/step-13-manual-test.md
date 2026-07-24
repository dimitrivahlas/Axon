# Step 13 — Manual Test Plan: AI Suggestion → Sandbox Execution

The `!! <intent>` flow now offers to run Claude's suggested command in the
sandbox (`run in sandbox? [y/N]`, default No). Because the suggestion is
nondeterministic and requires a real API key, the full flow is verified
manually here. The decision logic (`is_affirmative`) is unit-tested, and the
prompt-appears path has an opt-in e2e (`AXON_E2E_AI=1 make e2e`).

## Prerequisites

```sh
cd ~/Desktop/projects/pm/axon
make clean && make build            # in the Linux container (see README)
export ANTHROPIC_API_KEY="your-key"
```

## Tests

| # | Test | Steps | Expected |
|---|------|-------|----------|
| 1 | Suggestion offers sandbox | Run `./axon`, type `!! print the word hello` | A command is printed, followed by `run in sandbox? [y/N]` on stderr |
| 2 | Declining does not execute | At the prompt, press Enter (or type `n`) | Nothing runs; no command output, no `[exit …]` line; shell returns to the prompt |
| 3 | Accepting runs sandboxed | Repeat step 1, then type `y` | The suggested command runs in the sandbox; its output appears; a nonzero result prints `[exit N | …ms]` |
| 4 | Result recorded in context | After a `y` run, `sqlite3 ~/.axon/context.db "SELECT command, exit_code FROM commands ORDER BY id DESC LIMIT 1;"` | The row shows the executed command tagged with a leading `& ` and its exit code |
| 5 | Never auto-executes | Type `!! delete everything` and press Enter at the prompt | The destructive suggestion is NOT run (default is No) |
| 6 | No API key | `unset ANTHROPIC_API_KEY`, then `!! list files` | Prints `axon: ANTHROPIC_API_KEY not set`; no prompt, no crash |

## Pass criteria

- The suggestion is never executed without an explicit `y`.
- Accepted commands run through the sandbox and are recorded (tagged `& …`).
- Declining leaves no trace beyond the recorded `!!` interaction.
