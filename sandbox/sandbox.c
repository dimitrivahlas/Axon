#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mount.h>
#include "sandbox.h"

#define SANDBOX_STACK_SIZE (1024 * 1024)  /* 1 MB stack for clone child */
#define SANDBOX_CAPTURE_MAX AXON_CAPTURE_MAX

/* Arguments passed to the cloned child process */
typedef struct {
    const char *command;
    int err_pipe_w;     /* write end of stderr capture pipe */
    int sync_pipe_r;    /* read end of sync pipe — child blocks until parent signals */
} sandbox_child_args_t;

/*
 * Write a string to a file. Used for UID/GID mapping in user namespaces.
 * Returns 0 on success, -1 on error.
 */
static int write_file(const char *path, const char *content)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0)
        return -1;

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);

    return (written == (ssize_t)len) ? 0 : -1;
}

/*
 * Write UID and GID mappings for the user namespace.
 * Maps the current user's UID/GID to root (0) inside the namespace.
 * This allows the child to function normally inside the sandbox.
 */
static int setup_uid_gid_map(pid_t child_pid)
{
    char path[256];
    char mapping[64];

    unsigned uid = (unsigned)getuid();
    unsigned gid = (unsigned)getgid();

    /* Write "deny" to setgroups before writing gid_map (required by kernel) */
    snprintf(path, sizeof(path), "/proc/%d/setgroups", child_pid);
    if (write_file(path, "deny") != 0) {
        fprintf(stderr, "axon: sandbox: failed to write setgroups\n");
        return -1;
    }

    /* Map container UID 0 → host UID */
    snprintf(path, sizeof(path), "/proc/%d/uid_map", child_pid);
    snprintf(mapping, sizeof(mapping), "0 %u 1\n", uid);
    if (write_file(path, mapping) != 0) {
        fprintf(stderr, "axon: sandbox: failed to write uid_map\n");
        return -1;
    }

    /* Map container GID 0 → host GID */
    snprintf(path, sizeof(path), "/proc/%d/gid_map", child_pid);
    snprintf(mapping, sizeof(mapping), "0 %u 1\n", gid);
    if (write_file(path, mapping) != 0) {
        fprintf(stderr, "axon: sandbox: failed to write gid_map\n");
        return -1;
    }

    return 0;
}

/*
 * Milliseconds remaining until deadline (may be <= 0 if already elapsed).
 */
static long ms_until(const struct timespec *deadline)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (deadline->tv_sec - now.tv_sec) * 1000L
         + (deadline->tv_nsec - now.tv_nsec) / 1000000L;
}

/*
 * Read stderr from the child and tee it to the real stderr + capture buffer.
 * Same pattern as executor.c read_and_tee_stderr(), extended with a deadline.
 *
 * When timeout_ms == 0 the read blocks until EOF (all writers closed) exactly
 * as before. When timeout_ms > 0 each read is gated by poll() against
 * *deadline; if the deadline passes before EOF the function stops early.
 *
 * Returns 1 if the deadline expired before the child's stderr closed,
 * 0 if it read all the way to EOF (or hit a fatal read/poll error).
 */
static int read_and_tee_stderr(int fd, char *capture, size_t cap_max,
                               long timeout_ms, const struct timespec *deadline)
{
    size_t pos = 0;
    char buf[1024];

    for (;;) {
        if (timeout_ms > 0) {
            long remaining = ms_until(deadline);
            if (remaining <= 0) {
                capture[pos] = '\0';
                return 1;   /* out of time */
            }

            struct pollfd pfd = { .fd = fd, .events = POLLIN };
            int pr = poll(&pfd, 1, (int)remaining);
            if (pr < 0) {
                if (errno == EINTR)
                    continue;
                break;      /* poll failed: stop capturing, reap as normal */
            }
            if (pr == 0) {
                capture[pos] = '\0';
                return 1;   /* deadline hit with no more data */
            }
            /* poll reports the fd readable (or hung up): read won't block */
        }

        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (n == 0)
            break;          /* EOF: every writer (child + descendants) closed */

        if (write(STDERR_FILENO, buf, (size_t)n) < 0) {
            /* Tee to the real stderr is best-effort; keep capturing */
        }

        size_t to_copy = (size_t)n;
        if (pos + to_copy >= cap_max - 1)
            to_copy = cap_max - 1 - pos;
        if (to_copy > 0) {
            memcpy(capture + pos, buf, to_copy);
            pos += to_copy;
        }
    }
    capture[pos] = '\0';
    return 0;
}

/*
 * Child function invoked by clone().
 * Runs inside PID + user namespaces.
 *
 * Waits on sync_pipe_r for the parent to finish writing UID/GID maps,
 * then redirects stderr to the capture pipe and execs the command.
 */
static int sandbox_child(void *arg)
{
    sandbox_child_args_t *args = (sandbox_child_args_t *)arg;

    /* Block until parent signals that UID/GID maps are ready */
    char sync_byte;
    if (read(args->sync_pipe_r, &sync_byte, 1) != 1 || sync_byte != 'G') {
        /* Parent signalled failure or pipe was closed */
        _exit(126);
    }
    close(args->sync_pipe_r);

    /* Redirect stderr to the capture pipe */
    dup2(args->err_pipe_w, STDERR_FILENO);
    close(args->err_pipe_w);

    /*
     * A fresh mount namespace (CLONE_NEWNS) still inherits its parent's
     * propagation type, which is "shared" by default on most distros. That
     * means mounts/unmounts performed in here would otherwise propagate
     * back into the host's mount table. Make the whole tree private and
     * recursive so nothing escapes the sandbox. Fail closed if this can't
     * be guaranteed.
     */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        fprintf(stderr, "axon: sandbox: failed to isolate mount namespace: %s\n",
                strerror(errno));
        _exit(126);
    }

    /* Execute the command via /bin/sh -c */
    execl("/bin/sh", "sh", "-c", args->command, (char *)NULL);

    /* If execl fails, report and exit */
    fprintf(stderr, "axon: sandbox: exec failed: %s\n", strerror(errno));
    _exit(127);
}

int sandbox_execute(const char *command, const sandbox_opts_t *opts,
                    cmd_result_t *result)
{
    /* opts == NULL means most restrictive: network isolated, no timeout */
    int allow_network = (opts != NULL) ? opts->allow_network : 0;

    if (command == NULL || result == NULL) {
        fprintf(stderr, "axon: sandbox: NULL argument\n");
        return -1;
    }

    result->exit_code = 0;
    result->elapsed_ms = 0.0;
    result->stderr_capture[0] = '\0';

    /*
     * Both pipes are O_CLOEXEC so no sandbox-internal fds leak into the
     * exec'd command. The child's stderr survives because dup2() clears
     * the close-on-exec flag on the duplicated fd.
     */
    int err_pipe[2];
    if (pipe2(err_pipe, O_CLOEXEC) < 0) {
        fprintf(stderr, "axon: sandbox: pipe: %s\n", strerror(errno));
        return -1;
    }

    /* Create sync pipe: parent writes 'G' (go) after UID/GID maps are set */
    int sync_pipe[2];
    if (pipe2(sync_pipe, O_CLOEXEC) < 0) {
        fprintf(stderr, "axon: sandbox: pipe: %s\n", strerror(errno));
        close(err_pipe[0]);
        close(err_pipe[1]);
        return -1;
    }

    /* Allocate stack for clone (grows downward) */
    char *stack = malloc(SANDBOX_STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "axon: sandbox: malloc: %s\n", strerror(errno));
        close(err_pipe[0]);
        close(err_pipe[1]);
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return -1;
    }
    char *stack_top = stack + SANDBOX_STACK_SIZE;

    sandbox_child_args_t args = {
        .command = command,
        .err_pipe_w = err_pipe[1],
        .sync_pipe_r = sync_pipe[0],
    };

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /*
     * Create child in new PID, user, mount, and (unless explicitly
     * allowed) network namespaces. A fresh network namespace starts with
     * only a down loopback interface and no routes, so it is isolated
     * from the host by construction.
     */
    int clone_flags = CLONE_NEWPID | CLONE_NEWUSER | CLONE_NEWNS | SIGCHLD;
    if (!allow_network)
        clone_flags |= CLONE_NEWNET;

    pid_t child = clone(sandbox_child, stack_top, clone_flags, &args);

    if (child < 0) {
        fprintf(stderr, "axon: sandbox: clone: %s\n", strerror(errno));
        free(stack);
        close(err_pipe[0]);
        close(err_pipe[1]);
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return -1;
    }

    /* Parent: close ends we don't use */
    close(err_pipe[1]);
    close(sync_pipe[0]);

    /* Set up UID/GID mappings, then signal child to proceed */
    if (setup_uid_gid_map(child) != 0) {
        fprintf(stderr, "axon: sandbox: failed to set up user namespace\n");
        close(sync_pipe[1]);  /* closing without writing signals failure to child */
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        free(stack);
        close(err_pipe[0]);
        return -1;
    }

    /* Signal child: UID/GID maps are ready, proceed to exec */
    char go = 'G';
    if (write(sync_pipe[1], &go, 1) != 1) {
        /* Child will read EOF and refuse to exec (fail closed) */
        fprintf(stderr, "axon: sandbox: failed to signal child: %s\n",
                strerror(errno));
        close(sync_pipe[1]);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        free(stack);
        close(err_pipe[0]);
        return -1;
    }
    close(sync_pipe[1]);

    /*
     * Compute the wall-clock deadline. timeout_ms == 0 means no limit, in
     * which case read_and_tee_stderr blocks until EOF and `deadline` is
     * ignored. The deadline is measured from `start` (captured just before
     * clone), so it bounds total sandbox time, matching elapsed_ms.
     */
    long timeout_ms = (opts != NULL) ? opts->timeout_ms : 0;
    struct timespec deadline = start;
    if (timeout_ms > 0) {
        deadline.tv_sec  += timeout_ms / 1000;
        deadline.tv_nsec += (timeout_ms % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    /* Read and capture stderr from the child, bounded by the deadline */
    int timed_out = read_and_tee_stderr(err_pipe[0], result->stderr_capture,
                                        SANDBOX_CAPTURE_MAX, timeout_ms, &deadline);

    if (timed_out) {
        /*
         * The child is PID 1 in its PID namespace; SIGKILLing it makes the
         * kernel reap every other process in that namespace too, so no
         * runaway descendant is left behind.
         */
        kill(child, SIGKILL);

        char msg[80];
        int len = snprintf(msg, sizeof(msg),
                           "axon: sandbox: timed out after %ld ms\n", timeout_ms);
        if (len > 0) {
            if (fwrite(msg, 1, (size_t)len, stderr) < (size_t)len) {
                /* Notice on real stderr is best-effort */
            }
            size_t used = strlen(result->stderr_capture);
            if (used < SANDBOX_CAPTURE_MAX - 1)
                strncat(result->stderr_capture, msg,
                        SANDBOX_CAPTURE_MAX - 1 - used);
        }
    }
    close(err_pipe[0]);

    /* Wait for child to finish (it is dead already if we timed out) */
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "axon: sandbox: waitpid: %s\n", strerror(errno));
            free(stack);
            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (timed_out)
        result->exit_code = SANDBOX_TIMEOUT_EXIT;
    else if (WIFEXITED(status))
        result->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        result->exit_code = 128 + WTERMSIG(status);
    else
        result->exit_code = -1;

    result->elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0
                       + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    free(stack);
    return 0;
}
