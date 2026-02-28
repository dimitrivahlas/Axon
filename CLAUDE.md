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
├── tests/
│   ├── test_parser.c     # Parser tests (45)
│   ├── test_executor.c   # Executor tests (5)
│   ├── test_builtins.c   # Builtin tests (6)
│   ├── test_storage.cpp  # Storage tests (9)
│   ├── test_context.c    # Context engine tests (11)
│   ├── test_git_context.cpp # Git context tests (4)
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

Milestone 1 (Smart Shell Core) is complete. Milestone 2 (Context Engine) is nearly complete.

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
- 80 unit tests across 6 test suites, CI via GitHub Actions

Next: Milestone 3 (Sandbox Executor).

## Milestones

**Milestone 1 — Smart Shell Core (COMPLETE)**
A working shell in C that accepts and executes commands, captures execution metadata (exit code, timing, stderr), and provides opt-in AI assistance via Claude API.

**Milestone 2 — Context Engine (COMPLETE)**
Persistent project-aware memory in C++. Tracks files, errors, command history, and patterns across sessions. Storage, git context, session tracking, JSON builder, and AI command integration are all complete.

**Milestone 3 — Sandbox Executor (PLANNED)**
Secure execution of AI-generated commands using Linux namespaces and seccomp filters.
