# CLAUDE.md — Axon

## Project Vision

We are building a Linux development environment where AI lives at the system level, not the application layer. The goal is a shell that thinks — one that captures deep system context and feeds it to an AI that can understand, respond, and act on what is actually happening on the machine, not just what the user types.

This is not an IDE plugin. Not a chatbot. It is an environment where AI has the same awareness of the system that the developer does.

## Architecture Overview

The project is composed of three core components:

**1. Smart Shell (C)**
A custom shell written in C that captures rich execution context beyond just command text — exit codes, timing, resource usage, process tree, environment state. This context is serialized and made available to the AI layer.

**2. Sandbox Executor (C / Linux primitives — separate binary)**
A lightweight, secure execution environment using Linux namespaces (pid, net, mount, user) and seccomp filters. AI-generated code and commands run here. No Docker. No overhead. Just the kernel doing what it was built to do. Invoked as a separate process by the shell.

**3. Context Engine (C++)**
Persistent project-aware memory. Tracks files, recent changes, errors, command history, and patterns across sessions. Feeds structured context to the AI so it always knows where we are without re-explanation.

## Tech Stack

- **Shell and sandbox**: C (C11), Linux syscalls, libseccomp
- **Context engine**: C++ (C++17)
- **AI**: Anthropic Claude API (primary), Ollama for local fallback
- **Dev environment**: Docker on macOS (Linux container — Alpine 3.19)
- **Build system**: Makefile
- **Testing**: Unity (C unit tests)

## Developer Context

The developer has a systems programming background in C on Linux. Has built a shell, an interpreter, and worked with disassemblers. Comfortable with syscalls, process management, file descriptors, and memory. Currently developing on macOS using Docker for Linux environments.

## Git Workflow

- **Always create a feature branch** for every step/task before committing. Never commit directly to `main`.
- Branch naming: `step-<number>/<short-description>` (e.g., `step-18/build-ai-json`).
- Push the branch, then create a PR to merge into `main`.
- **Never attribute authorship to Claude/AI** in commit messages, PR descriptions, or code. Do not add `Co-Authored-By: Claude` trailers, "Generated with Claude" lines, or any similar attribution.

## Coding Principles

- Prefer simplicity over abstraction. Do not over-engineer early.
- C code must be C11 compliant, compile cleanly with `-Wall -Wextra -Werror`.
- C++ code must be C++17 compliant, compile cleanly with `-Wall -Wextra -Werror`.
- In C++ code, prefer `std::string` over raw `char*` internally. Use `strdup()` at the C boundary when returning `char*` to C callers. Minimize use of exceptions.
- Avoid external dependencies in the C layer unless absolutely necessary.
- The sandbox must fail closed — if something is uncertain, deny it.
- Context engine should be append-friendly and easy to inspect as plain text or JSON.
- Every component should be independently testable.
- Functions return 0 on success, -1 on failure. Pointer-returning functions return `NULL` on failure. Callers must always check return values — never silently swallow errors.

## Security Rules

These are non-negotiable. Every contributor (human or AI) must follow them.

- **No unsanitized input in shell commands.** Never pass user or AI-generated strings directly to `system()`, `popen()`, or `snprintf` format strings without escaping or validation.
- **Parameterized queries only.** All SQL must use `sqlite3_bind_*` — never string-concatenate values into SQL statements.
- **JSON-escape all dynamic strings.** Any string embedded in JSON output must go through a proper escape function (e.g., `json_escape_str`) to prevent injection.
- **API keys from environment variables only.** Keys are never hardcoded, logged, or written to files.
- **Bound all buffers.** No unbounded `malloc` from untrusted input. Stderr capture, command output, and file reads must have size limits.
- **Sandbox fails closed.** If a seccomp filter, namespace setup, or permission check is uncertain, deny the operation. Never default to permissive.

## Testing

Testing is mandatory, not optional. Code without tests does not ship.

- **Every new module must have a corresponding test file** (e.g., `context/storage.cpp` -> `tests/test_storage.cpp`).
- **Every new feature or function must include unit tests** that cover success cases, failure/error cases, and edge cases (NULL inputs, empty data, boundary values).
- **All tests must pass before merging.** Run `make clean && make build && make test` and verify 0 failures.
- **Tests must not depend on external state.** Tests that use the persistent database (`~/.axon/context.db`) must not assume an empty DB — previous test runs may have left data.
- **Test framework**: Unity for C tests. C++ tests use a lightweight custom runner (see `tests/test_storage.cpp` for the pattern).
- **CI runs all tests** on every push and PR via GitHub Actions.

### End-to-End Tests

- E2E tests live in `tests/e2e/` as shell scripts that pipe commands into
  `./axon` and assert on stdout, stderr, and exit codes.
- Every e2e test runs with an isolated HOME (`export HOME=$(mktemp -d)`)
  so the context DB starts fresh and tests never touch `~/.axon/`.
- AI commands (`?`, `!!`) are excluded from e2e — no API key in CI and
  responses are nondeterministic. Gate any AI e2e behind `AXON_E2E_AI=1`
  for local manual runs only.
- The shell must suppress the interactive prompt when stdin is not a TTY
  so e2e output is clean and assertable.
- Run via `make e2e`. CI runs `make test && make e2e` on every push/PR.
- New user-visible behavior (parsing, execution, persistence, sandbox)
  requires an e2e test alongside its unit tests. Convert items from
  MANUAL_TEST_PLAN.md into automated e2e where AI is not involved.

## What We Are NOT Building

- A full operating system or kernel
- A replacement for existing shells for general users
- A cloud product (yet)
- Anything that requires a GUI

## Repository Structure

```
axon/
├── shell/
│   ├── main.c            # Entry point, REPL loop, signal handling
│   ├── parser.c/.h       # Tokenization, quotes, pipes, redirects, env vars, chaining
│   ├── builtins.c/.h     # cd, exit, help
│   ├── executor.c/.h     # Fork/exec, pipelines, I/O redirection, stderr capture
│   └── ai.c/.h           # Claude API integration via libcurl (uses context engine)
├── context/
│   ├── context.cpp/.h    # Context engine public API (init, add, build JSON, shutdown)
│   ├── storage.cpp/.h    # SQLite-backed persistent storage for commands and sessions
│   └── git_context.cpp/.h # Git state gathering (branch, dirty, modified/staged files, commits)
├── sandbox/
│   └── sandbox.c/.h      # Namespace-isolated command execution (PID + user namespaces)
├── mcp/                  # (Milestone 4 — planned) Axon MCP server, separate binary
│   ├── json_value.cpp/.h # Minimal JSON parser (no external deps)
│   ├── rpc.cpp/.h        # JSON-RPC 2.0 dispatch (initialize, tools/list, tools/call)
│   └── main.cpp          # stdio loop for the axon-mcp binary
├── tests/
│   ├── test_parser.c     # Parser tests (45)
│   ├── test_executor.c   # Executor tests (5)
│   ├── test_builtins.c   # Builtin tests (6)
│   ├── test_storage.cpp  # Storage tests (9)
│   ├── test_context.c    # Context engine tests (11)
│   ├── test_git_context.cpp # Git context tests (4)
│   ├── test_sandbox.c    # Sandbox tests (10)
│   └── unity.*           # Unity test framework
├── docker/
│   └── Dockerfile        # Alpine Linux dev container
├── .github/
│   └── workflows/
│       └── ci.yml        # Build + test on every push/PR
├── Makefile
├── CLAUDE.md
└── README.md
```

## AI Interaction Model

AI assistance is opt-in only:
- `? <question>` — ask Claude a question with full command context
- `!! <intent>` — Claude suggests a command based on intent
- No automatic AI triggers on errors

## Current Status

Milestone 1 (Smart Shell Core) and Milestone 2 (Context Engine) are complete. Milestone 3 (Sandbox Executor) is in progress.

The shell is fully functional with:
- Interactive REPL with colored prompt and signal handling
- Command parsing with pipes, redirects, env vars, quotes, chaining
- Fork/exec execution with pipelines, I/O redirection, stderr capture
- AI integration via Claude API (`?` and `!!` commands) powered by context engine

The context engine is fully integrated with:
- SQLite-backed persistent storage for commands and sessions
- Session tracking with unique IDs across shell invocations
- Git state gathering (branch, dirty status, modified/staged files, recent commits)
- Full JSON context builder (`context_build_ai_json`) wiring storage + git into a structured AI payload
- AI commands receive rich structured context (history, errors, git state, session info)
- Old in-memory history ring buffer removed — all context flows through the context engine
- 90 unit tests across 7 test suites, CI via GitHub Actions

The sandbox executor scaffold is in place:
- `sandbox_execute()` runs a command in new PID, user, mount, and network namespaces via `clone()`
- UID/GID mapped to root inside the namespace; sync-pipe handshake for map setup
- Mount namespace made recursively private (`MS_REC | MS_PRIVATE`) so nothing propagates to the host
- Network isolated by default; `sandbox_opts_t.allow_network` opts into the host netns
- Stderr capture, exit code, and timing reported via `cmd_result_t`
- Fails closed: if namespace setup is unavailable or fails, the command does not run
- Not yet wired into the shell; timeouts, read-only cwd, rlimits, and seccomp still to come

Next: Milestone 3 continued — see the step-by-step plan under Milestones below. Milestone 4 (Axon MCP Server, exposing the context engine to any MCP client) is planned and specified after it.

## Milestones

**Milestone 1 — Smart Shell Core (COMPLETE)**
A working shell in C that accepts and executes commands, captures execution metadata (exit code, timing, stderr), and provides opt-in AI assistance via Claude API.

**Milestone 2 — Context Engine (COMPLETE)**
Persistent project-aware memory in C++. Tracks files, errors, command history, and patterns across sessions. Storage, git context, session tracking, JSON builder, and AI command integration are all complete.

**Milestone 3 — Sandbox Executor (IN PROGRESS)**
Secure execution of AI-generated commands using Linux namespaces and seccomp filters.

Done so far (branch `step-7/sandbox-scaffold` and earlier): `sandbox_execute()` in `sandbox/sandbox.c` isolates commands in new PID, user, mount, and network namespaces, with UID/GID mapping, stderr capture, exit code, and timing via `cmd_result_t`.

Remaining work, one branch/PR per step. Each step must compile with `-Wall -Wextra -Werror`, add tests to `tests/test_sandbox.c` (success, failure, and edge cases), and keep the fail-closed rule: if setup fails, the command must not run.

*Step 8 — Timeout enforcement (`step-8/sandbox-timeout`)*
- Implement `sandbox_opts_t.timeout_ms` in `sandbox/sandbox.c` (currently documented as "not yet implemented" in `sandbox/sandbox.h`).
- In the parent, after `clone()`, wait with a deadline (e.g., `waitpid(WNOHANG)` polling loop or `sigtimedwait`). On expiry, `SIGKILL` the child, reap it, and report the timeout in `cmd_result_t` (nonzero exit code plus a stderr message like `axon: sandbox: timed out after <n> ms`).
- `timeout_ms == 0` means no limit (existing behavior must not change).
- Tests: command that finishes under the limit succeeds; `sleep`-style command over the limit is killed and reports a timeout; `timeout_ms = 0` runs unlimited.

*Step 9 — Read-only filesystem (`step-9/sandbox-readonly`)*
- Implement `sandbox_opts_t.read_only_cwd` in the child function of `sandbox/sandbox.c`, after the existing `MS_REC | MS_PRIVATE` remount.
- When set, bind-remount the working directory read-only: `mount(cwd, cwd, NULL, MS_BIND, NULL)` then remount with `MS_REMOUNT | MS_BIND | MS_RDONLY`. If either mount call fails, print an error to stderr and `_exit` without running the command (fail closed).
- Tests: with the flag set, a write to cwd (e.g., `touch f`) fails while a read succeeds; with the flag unset, writes still work; verify the host cwd is unaffected afterward.

*Step 10 — Resource limits (`step-10/sandbox-rlimits`)*
- In the child, before `execvp`, apply `setrlimit()` caps: `RLIMIT_NPROC` (e.g., 64), `RLIMIT_NOFILE` (e.g., 256), `RLIMIT_FSIZE` (e.g., 64 MB), `RLIMIT_AS` (e.g., 512 MB). Define the values as named constants at the top of `sandbox/sandbox.c`; no new options fields yet — hardcoded defaults are fine at this step.
- If any `setrlimit` call fails, fail closed (error to stderr, `_exit`, command does not run).
- Tests: a fork bomb-style loop hits the NPROC cap instead of running away; a command writing more than `RLIMIT_FSIZE` fails; a normal command still succeeds.

*Step 11 — Seccomp filter (`step-11/sandbox-seccomp`)*
- Add a seccomp allowlist using libseccomp in the child, applied last (after mounts and rlimits, immediately before `execvp`). Link with `-lseccomp` in the Makefile for the sandbox objects and test binary.
- Default action `SCMP_ACT_ERRNO(EPERM)` (not KILL, so violations are observable in stderr). Allowlist the syscalls needed by `/bin/sh` and typical child commands: read/write/open/close/stat family, mmap/brk, execve, fork/clone/wait, dup/pipe, fcntl, ioctl on ttys, exit/exit_group, rt_sigaction/rt_sigreturn, getpid/getuid family. Start from what BusyBox `sh` on Alpine actually needs (test in the Docker container) and expand only on observed failures.
- Explicitly deny (omit from allowlist): `mount`, `umount2`, `reboot`, `ptrace`, `kexec_load`, `init_module`, `setns`, socket syscalls when the network namespace is isolated.
- If `seccomp_init` or `seccomp_load` fails, fail closed.
- Tests: normal command (`echo`, `ls`) succeeds under the filter; a denied syscall (e.g., a command attempting `mount`) fails with EPERM; filter load failure path does not execute the command.

*Step 12 — Shell integration (`step-12/sandbox-shell-wire`)*
- Wire the sandbox into the shell as an opt-in prefix: `& <command>` (or similar single-token prefix — pick one not already used by the parser) runs the command through `sandbox_execute()` with default (most restrictive) options plus a default timeout (e.g., 30 s).
- Parse the prefix in `shell/main.c`/`shell/parser.c` alongside the existing `?` and `!!` handling; print the sandbox result (exit code, timing, captured stderr) the same way normal commands report.
- Record sandboxed commands in the context engine like any other command, tagged so history shows they ran sandboxed.
- Update `README.md` and the AI Interaction Model section of this file with the new prefix.
- Tests: parser recognizes the prefix; a sandboxed `echo` returns its output and exit code; sandbox setup failure surfaces an error without crashing the shell.

*Step 13 — AI command execution flow (`step-13/sandbox-ai-execute`)*
- After `!!` produces a suggested command, offer to run it in the sandbox: prompt `run in sandbox? [y/N]`, defaulting to No. Never auto-execute.
- On yes, run through the same path as step 12 and feed the result (exit code, stderr) back into the context engine.
- Tests: declining does not execute; accepting executes sandboxed; the result is recorded in context storage.

Milestone 3 is complete when all six steps are merged, `make clean && make build && make test` passes in the Docker container with 0 failures, and AI-suggested commands can only execute inside the sandbox.

**Milestone 4 — Axon MCP Server (PLANNED)**
Expose the context engine over the Model Context Protocol so any MCP client (Claude Code, Claude Desktop) can query the shell's memory — command history, error patterns, session info, full AI context. This is a from-scratch MCP server implementation: a separate `axon-mcp` binary speaking JSON-RPC 2.0 over stdio, written in C++17 against the existing SQLite store. No MCP SDK, no new runtime dependencies.

Design decisions (fixed — individual steps must not revisit them):
- **Language/binary**: C++17, compiled with the same `-Wall -Wextra -Werror` flags, as a separate `axon-mcp` binary with its own Makefile target, added to `make build`.
- **Transport**: MCP stdio — newline-delimited JSON-RPC 2.0 messages, one per line, on stdin/stdout. stdout carries protocol messages ONLY; all logging goes to stderr. Flush stdout after every message.
- **Protocol surface**: `initialize`, `notifications/initialized`, `ping`, `tools/list`, `tools/call`. Errors: malformed JSON → `-32700`, unknown method → `-32601`, invalid params → `-32602`. Protocol version: one constant (`"2025-06-18"`); on `initialize`, echo the client's requested version if it matches, otherwise reply with ours.
- **DB access is read-only**: open `~/.axon/context.db` with `sqlite3_open_v2(..., SQLITE_OPEN_READONLY, ...)`. The server never writes and never creates the DB — if it is missing, tool calls return an error (fail closed).
- **Security**: bound stdin line length (reject lines > 1 MB with `-32700`); every dynamic string in output goes through `json_escape_str`; SQL stays parameterized via the existing `Storage` layer; malformed input returns a JSON-RPC error, never crashes the process.

One branch/PR per step; every step adds tests covering success, failure, and edge cases, and `make clean && make build && make test` must pass before merging.

*Step 14 — Minimal JSON parser (`step-14/mcp-json-parser`)*
- New module `mcp/json_value.cpp/.h`: parse a UTF-8 JSON document into a value tree (object → map, array → vector, string, number, bool, null). API shape: a static `parse(const std::string &input, std::string &err)` returning an empty/invalid value on failure, plus typed accessors (`get_string`, `get_int`, `get_object_member`, `get_array`) that fail cleanly on type mismatch.
- Must handle string escapes (`\" \\ \/ \b \f \n \r \t \uXXXX` including surrogate pairs → UTF-8) and enforce a nesting-depth limit (64; exceeding it is a parse error — fail closed). No external libraries.
- Tests in `tests/test_json_parse.cpp` (follow the `tests/test_storage.cpp` runner pattern): valid objects/arrays/nesting, all escape forms, numbers; malformed inputs (truncated, trailing garbage, bad escapes, over-depth, empty string) each return an error without crashing.

*Step 15 — JSON-RPC stdio scaffold (`step-15/mcp-rpc-scaffold`)*
- `mcp/rpc.cpp/.h`: `std::string rpc_handle_message(const std::string &line)` — returns the serialized response, or an empty string for notifications. Implement `initialize` (result: `protocolVersion`, `capabilities: {"tools": {}}`, `serverInfo: {"name": "axon-mcp", "version": ...}`), `notifications/initialized` (no reply), `ping` (empty object result), `tools/list` (empty `tools` array for now), and the three error codes above.
- `mcp/main.cpp`: read stdin line by line with the 1 MB bound, pass each line to `rpc_handle_message`, write non-empty responses followed by `\n`, flush, exit 0 on EOF.
- Makefile: `axon-mcp` target; wire into `make build`.
- Tests in `tests/test_mcp_rpc.cpp` (unit-test `rpc_handle_message` directly, no subprocess needed): initialize handshake returns the right shape, ping works, unknown method → `-32601`, malformed JSON → `-32700`, oversized input rejected, notification produces no output.

*Step 16 — Context tools (`step-16/mcp-context-tools`)*
- Add a read-only open path to `context/storage.cpp` (e.g. `Storage::open_readonly(path)` using `SQLITE_OPEN_READONLY`); it must fail (not create the file) when the DB is absent.
- Implement `tools/list` with real definitions (name, description, `inputSchema` with typed properties) and `tools/call` dispatch for three tools:
  - `axon_recent_commands` — args: `limit` (int, default 20, clamp to 1–200), `cwd` (string, optional). Backed by `query_recent` / `query_recent_by_cwd`. Returns a text content block listing timestamp, cwd, command, exit code, elapsed ms per row.
  - `axon_recent_errors` — args: `limit` (default 10, clamp to 1–100). Failed commands (`exit_code != 0`) with their captured stderr. Add a parameterized `Storage::query_recent_failures(int)` if the existing queries don't cover it.
  - `axon_session_info` — most recent sessions: session id, start time, initial cwd.
- MCP semantics: an unknown tool name or a tool that fails at runtime returns a **result** with `isError: true` and a text explanation — JSON-RPC errors are reserved for protocol problems.
- Tests: seed a temp SQLite DB through `Storage` (same isolation approach as `tests/test_storage.cpp` — never assume an empty `~/.axon/context.db`), then assert each tool returns the seeded rows, limits clamp, unknown tool → `isError`, and a missing DB path fails closed.

*Step 17 — Full context tool, registration, docs (`step-17/mcp-register`)*
- Add an `axon_context` tool that returns the same structured payload the AI commands use, via `context_build_ai_json(cwd, max_entries)` — link `axon-mcp` against the `context/` objects and `free()` the returned buffer.
- Registration: the server must run where the DB lives — inside the Docker container. Add `.mcp.json` at the repo root with a `docker run --rm -i` command that mounts the checkout and the same `~/.axon` volume the shell container uses (both must see one DB; without a shared volume the DB is ephemeral). Document the equivalent Claude Desktop config snippet in the README.
- README section: what the server exposes, the four tools, and how to register it (`claude mcp add axon -- docker run --rm -i ...`).
- Add `docs/step-17-test-plan.md`: a piped end-to-end sequence (`initialize` → `notifications/initialized` → `tools/list` → `tools/call`) against a seeded DB with expected outputs, plus a manual verification from a real Claude client.
- Tests: `axon_context` returns parseable JSON containing the session id; tool works when the DB has data from prior suites (no empty-DB assumption).

Milestone 4 is complete when all four steps are merged, `make clean && make build && make test` passes with the new suites at 0 failures, and a real MCP client can list and call the Axon tools against a live `context.db`.
