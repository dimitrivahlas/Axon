# Axon

A shell that thinks.

## The Problem

Every developer has the same workflow: run a command, hit an error, copy the output, paste it into a chatbot, wait for an answer, copy the suggestion back, paste it into the terminal. Repeat. The AI never sees what you see. It doesn't know your working directory, your project structure, what you ran before, or why something failed. You are the middleware between your terminal and your tools.

IDE plugins help, but they live in the wrong place. They see your editor, not your system. They can't watch a build fail, see the exit code, measure how long it took, or understand that you've been fighting the same linker error for twenty minutes. The terminal is where the real work happens, and right now, AI has no presence there.

## What Axon Is

Axon is a Linux shell written in C where AI lives at the system level. It captures deep execution context — exit codes, timing, command output, working directory, history — and makes all of it available to an AI that can actually understand what is happening on your machine.

This is not a chatbot wrapper. Not a CLI tool that calls an API. It is a shell. You use it the way you use bash or zsh, except the environment itself is aware.

AI assistance is opt-in. Axon doesn't interrupt you or second-guess your commands. When you want help, you ask for it:

- **`? <question>`** — Ask a question with full execution context. Axon sends your recent command history, exit codes, output, and working directory to Claude so it can give an answer grounded in what actually happened.
- **`!! <intent>`** — Describe what you want to do. Axon suggests a command based on your intent, your environment, and your history.

Everything else works like a normal shell. `ls`, `cd`, pipes, redirects — they all work. Axon just knows more about what happened.

## Architecture

Three components, each with a clear job:

| Component | Language | Role |
|---|---|---|
| **Smart Shell** | C | REPL, command parsing, execution, context capture |
| **Context Engine** | C++ | Persistent memory — tracks history, errors, patterns across sessions |
| **Sandbox Executor** | C | Secure execution of AI-generated commands using Linux namespaces and seccomp |

No frameworks. No runtimes. Just C, the kernel, and one API call when you ask for it.

## Current Status

Axon is in active development. Milestone 1 (Smart Shell Core) is complete:

- [x] Interactive REPL with colored prompt
- [x] Signal handling (Ctrl+C, Ctrl+D)
- [x] Builtins (`cd`, `exit`, `help`)
- [x] Command parser (tokenization, single/double quote handling)
- [x] Pipes (`cmd1 | cmd2 | cmd3`)
- [x] I/O redirection (`>`, `>>`, `<`)
- [x] Environment variable expansion (`$VAR`, `${VAR}`, `$?`, `~`)
- [x] Command chaining (`&&`, `||`, `;`)
- [x] Command execution (fork/exec, pipelines)
- [x] Output and metadata capture (exit code, timing, stderr)
- [x] AI integration (`?` and `!!` commands via Claude API)
- [x] Unit tests (42 tests, Unity framework)
- [x] CI pipeline (GitHub Actions)

## Quick Start

### Requirements

- Docker Desktop
- Git

### Build and Run

```sh
git clone https://github.com/dimitrivahlas/Axon.git
cd Axon
docker build -t axon -f docker/Dockerfile .
docker run -it axon
```

That's it. The Docker image compiles Axon from source inside an Alpine Linux container with all dependencies included.

### Build Locally (Linux)

```sh
# Install dependencies (Ubuntu/Debian)
sudo apt install gcc make libcurl4-openssl-dev

make build
./axon
```

### Build Locally (macOS)

```sh
make build
./axon
```

Note: Axon targets Linux. macOS builds work for development but the full feature set (namespaces, seccomp) requires Linux.

## Project Structure

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
│   ├── test_parser.c   # Parser unit tests
│   ├── test_executor.c # Executor unit tests
│   └── unity.*         # Unity test framework
├── docker/
│   └── Dockerfile      # Alpine Linux dev container
├── .github/
│   └── workflows/
│       └── ci.yml      # Build + test on every push/PR
├── Makefile
├── CLAUDE.md           # Project context for AI-assisted development
└── README.md
```

## License

This project is currently private and not licensed for external use.
