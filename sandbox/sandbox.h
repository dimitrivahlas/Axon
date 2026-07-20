#ifndef AXON_SANDBOX_H
#define AXON_SANDBOX_H

#include "../shell/executor.h"

/*
 * Options for sandboxed execution.
 * All fields default to 0 (most restrictive).
 */
typedef struct {
    int allow_network;       /* 0 = isolated (default), 1 = allow */
    int read_only_cwd;       /* 0 = read-write (default), 1 = read-only */
    long timeout_ms;         /* Max execution time in ms (0 = no limit) */
} sandbox_opts_t;

/*
 * Execute a command string in a sandboxed environment.
 * Uses Linux namespaces (PID, user) to isolate the child process.
 * The command is run via /bin/sh -c "command".
 *
 * Returns 0 on success, -1 on error (namespace/fork failure).
 * Result (exit code, timing, stderr) written to *result.
 *
 * Fails closed: if sandbox setup fails, the command does NOT run.
 */
int sandbox_execute(const char *command, const sandbox_opts_t *opts,
                    cmd_result_t *result);

#endif
