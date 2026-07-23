#ifndef AXON_SANDBOX_H
#define AXON_SANDBOX_H

#include "../shell/executor.h"

/*
 * Options for sandboxed execution.
 * All fields default to 0 (most restrictive).
 */
typedef struct {
    int allow_network;       /* 0 = isolated (default), 1 = share host netns */
    int read_only_cwd;       /* 0 = read-write (default), 1 = read-only (not yet implemented) */
    long timeout_ms;         /* Max execution time in ms (0 = no limit; not yet implemented) */
} sandbox_opts_t;

/*
 * Execute a command string in a sandboxed environment.
 * Uses Linux namespaces (PID, user, mount, network) to isolate the child
 * process. The command is run via /bin/sh -c "command". Passing NULL for
 * opts is equivalent to the most restrictive settings (network isolated).
 *
 * Mount namespace: the child's mount table is private and recursive, so
 * mounts/unmounts inside the sandbox never propagate to or from the host.
 *
 * Network namespace: unless opts->allow_network is set, the child gets a
 * fresh network namespace with no route to the host's interfaces, routes,
 * or sockets. It has no real connectivity, though some kernels populate
 * every new netns with inert pseudo-devices (tunl0, gre0, sit0, ...) in
 * addition to the loopback.
 *
 * Returns 0 on success, -1 on error (namespace/fork failure).
 * Result (exit code, timing, stderr) written to *result.
 *
 * Fails closed: if sandbox setup fails, the command does NOT run.
 */
int sandbox_execute(const char *command, const sandbox_opts_t *opts,
                    cmd_result_t *result);

#endif
