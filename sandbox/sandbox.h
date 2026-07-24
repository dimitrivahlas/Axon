#ifndef AXON_SANDBOX_H
#define AXON_SANDBOX_H

#include "../shell/executor.h"

/*
 * Exit code reported in cmd_result_t when a command is killed for exceeding
 * its timeout. Matches the convention used by GNU coreutils `timeout`, so a
 * timeout is distinguishable from a command that happened to die by SIGKILL
 * on its own (which reports 128 + 9 = 137).
 */
#define SANDBOX_TIMEOUT_EXIT 124

/*
 * Options for sandboxed execution.
 * All fields default to 0 (most restrictive).
 */
typedef struct {
    int allow_network;       /* 0 = isolated (default), 1 = share host netns */
    int read_only_cwd;       /* 0 = read-write (default), 1 = working dir mounted read-only */
    long timeout_ms;         /* Max wall-clock execution time in ms (0 = no limit) */
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
 * Resource limits & privileges: every sandboxed command runs under fixed
 * setrlimit() caps (open files, file size, and address space are enforced;
 * process count is best-effort inside the user namespace) and with all
 * capabilities dropped plus NO_NEW_PRIVS, so it executes unprivileged and
 * cannot exhaust host resources. If any limit or the capability drop cannot
 * be applied, the command does not run (fail closed).
 *
 * Seccomp: a syscall allowlist is loaded just before exec. Syscalls outside
 * the list fail with EPERM; dangerous ones (mount, ptrace, reboot, kernel
 * module and namespace calls, and the socket family) are denied. If the
 * filter cannot be installed, the command does not run (fail closed).
 *
 * Read-only cwd: if opts->read_only_cwd is set, the working directory is
 * bind-remounted read-only inside the sandbox, so the command can read but
 * not create, modify, or delete files under it. Only the cwd subtree is
 * affected (the rest of the filesystem keeps its normal permissions), and
 * because the remount lives in the child's private mount namespace the host
 * cwd is never changed. If the remount cannot be set up the command does not
 * run (fail closed).
 *
 * Timeout: if opts->timeout_ms > 0, the command is given at most that many
 * milliseconds of wall-clock time. On expiry the child (PID 1 in its PID
 * namespace, so killing it tears down every descendant) is SIGKILLed, a
 * "timed out" notice is written to stderr and the capture buffer, and
 * result->exit_code is set to SANDBOX_TIMEOUT_EXIT (124). timeout_ms == 0
 * means no limit. A timeout is still a successful invocation (returns 0):
 * inspect result->exit_code to detect it.
 *
 * Returns 0 on success, -1 on error (namespace/fork failure).
 * Result (exit code, timing, stderr) written to *result.
 *
 * Fails closed: if sandbox setup fails, the command does NOT run.
 */
int sandbox_execute(const char *command, const sandbox_opts_t *opts,
                    cmd_result_t *result);

#endif
