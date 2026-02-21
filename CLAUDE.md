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
- **Dev environment**: Docker on macOS (Linux container — Ubuntu 22.04 LTS)
- **Build system**: Makefile
- **Testing**: Unity (C unit tests)

## Developer Context

The developer has a systems programming background in C on Linux. Has built a shell, an interpreter, and worked with disassemblers. Comfortable with syscalls, process management, file descriptors, and memory. Currently developing on macOS using Docker for Linux environments.

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

## Repository Structure (Target)

```
/
├── shell/          # C source for the smart shell
├── sandbox/        # C source for the namespace/seccomp executor (separate binary)
├── context/        # C++ context engine
├── ai/             # AI integration layer (API calls, prompt templates)
├── tests/          # Unit and integration tests
├── docker/         # Dockerfile and container config
├── docs/           # Notes, design decisions, ADRs
└── Makefile
```

## AI Interaction Model

AI assistance is opt-in only:
- `? <question>` — ask Claude a question with full command context
- `!! <intent>` — Claude suggests a command based on intent
- No automatic AI triggers on errors

## Current Status

Project is in initial scaffolding phase. Building Milestone 1 — Smart Shell Core.

## First Milestone

A working shell in C that:
1. Accepts and executes commands (basic REPL)
2. Captures exit code, timing, and command text for every execution
3. Opt-in AI: user types `?` or `!!` to get Claude's help with context

This is the proof of concept. Everything else builds on top of it.
