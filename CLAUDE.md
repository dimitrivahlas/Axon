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
- **Context engine**: C++
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
- C code should be C11 compliant, compile cleanly with `-Wall -Wextra -Werror`.
- Avoid external dependencies in the C layer unless absolutely necessary.
- The sandbox must fail closed — if something is uncertain, deny it.
- Context engine should be append-friendly and easy to inspect as plain text or JSON.
- Every component should be independently testable.

## What We Are NOT Building

- A full operating system or kernel
- A replacement for existing shells for general users
- A cloud product (yet)
- Anything that requires a GUI

## Repository Structure

```
axon/
├── shell/
│   ├── main.c          # Entry point, REPL loop, signal handling
│   ├── parser.c/.h     # Tokenization, quotes, pipes, redirects, env vars, chaining
│   ├── builtins.c/.h   # cd, exit, help
│   ├── executor.c/.h   # Fork/exec, pipelines, I/O redirection, stderr capture
│   ├── history.c/.h    # Command history ring buffer for AI context
│   └── ai.c/.h         # Claude API integration via libcurl
├── tests/
│   ├── test_parser.c   # 37 parser tests
│   ├── test_executor.c # 5 executor tests
│   └── unity.*         # Unity test framework
├── docker/
│   └── Dockerfile      # Alpine Linux dev container
├── .github/
│   └── workflows/
│       └── ci.yml      # Build + test on every push/PR
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

Milestone 1 (Smart Shell Core) is complete. The shell is fully functional with:
- Interactive REPL with colored prompt and signal handling
- Command parsing with pipes, redirects, env vars, quotes, chaining
- Fork/exec execution with pipelines, I/O redirection, stderr capture
- AI integration via Claude API (`?` and `!!` commands)
- Command history ring buffer feeding context to AI
- 42 unit tests (Unity framework) and GitHub Actions CI

Next: Milestone 2 — Context Engine (C++, persistent memory across sessions).

## Milestones

**Milestone 1 — Smart Shell Core (COMPLETE)**
A working shell in C that accepts and executes commands, captures execution metadata (exit code, timing, stderr), and provides opt-in AI assistance via Claude API.

**Milestone 2 — Context Engine (PLANNED)**
Persistent project-aware memory in C++. Tracks files, errors, command history, and patterns across sessions.

**Milestone 3 — Sandbox Executor (PLANNED)**
Secure execution of AI-generated commands using Linux namespaces and seccomp filters.
