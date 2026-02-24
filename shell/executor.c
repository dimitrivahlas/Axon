#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include "executor.h"

static void setup_redirects(command_t *cmd)
{
    if (cmd->redir_in != NULL) {
        int fd = open(cmd->redir_in, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "axon: %s: %s\n", cmd->redir_in, strerror(errno));
            _exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->redir_out != NULL) {
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd->append ? O_APPEND : O_TRUNC;
        int fd = open(cmd->redir_out, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "axon: %s: %s\n", cmd->redir_out, strerror(errno));
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

static int wait_for_child(pid_t pid)
{
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "axon: waitpid: %s\n", strerror(errno));
            return -1;
        }
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return -1;
}

static void read_and_tee_stderr(int fd, char *capture, size_t cap_max)
{
    size_t pos = 0;
    char buf[1024];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        /* Write to real stderr so the user sees it */
        write(STDERR_FILENO, buf, (size_t)n);

        /* Capture the tail into the buffer */
        size_t to_copy = (size_t)n;
        if (pos + to_copy >= cap_max - 1)
            to_copy = cap_max - 1 - pos;
        if (to_copy > 0) {
            memcpy(capture + pos, buf, to_copy);
            pos += to_copy;
        }
    }
    capture[pos] = '\0';
}

int execute_command(command_t *cmd, cmd_result_t *result)
{
    result->exit_code = 0;
    result->elapsed_ms = 0.0;
    result->stderr_capture[0] = '\0';

    /* Create a pipe to capture child's stderr */
    int err_pipe[2];
    if (pipe(err_pipe) < 0) {
        fprintf(stderr, "axon: pipe: %s\n", strerror(errno));
        return -1;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "axon: fork: %s\n", strerror(errno));
        close(err_pipe[0]);
        close(err_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: redirect stderr to pipe write end */
        close(err_pipe[0]);
        dup2(err_pipe[1], STDERR_FILENO);
        close(err_pipe[1]);

        setup_redirects(cmd);
        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "axon: %s: %s\n", cmd->argv[0], strerror(errno));
        _exit(127);
    }

    /* Parent: read child's stderr from pipe, tee to terminal + capture */
    close(err_pipe[1]);
    read_and_tee_stderr(err_pipe[0], result->stderr_capture, AXON_CAPTURE_MAX);
    close(err_pipe[0]);

    int code = wait_for_child(pid);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (code < 0)
        return -1;

    result->exit_code = code;
    result->elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0
                       + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    return 0;
}

int execute_pipeline(pipeline_t *pl, cmd_result_t *result)
{
    result->exit_code = 0;
    result->elapsed_ms = 0.0;

    if (pl->num_stages == 0)
        return 0;

    if (pl->num_stages == 1)
        return execute_command(&pl->stages[0], result);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pids[AXON_MAX_PIPE_STAGES];
    int prev_fd = -1;

    for (int i = 0; i < pl->num_stages; i++) {
        int pipefd[2] = {-1, -1};

        if (i < pl->num_stages - 1) {
            if (pipe(pipefd) < 0) {
                fprintf(stderr, "axon: pipe: %s\n", strerror(errno));
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "axon: fork: %s\n", strerror(errno));
            return -1;
        }

        if (pid == 0) {
            /* Connect stdin to previous pipe's read end */
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            /* Connect stdout to current pipe's write end */
            if (pipefd[1] != -1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            /* Close read end — the next stage will use it */
            if (pipefd[0] != -1)
                close(pipefd[0]);

            setup_redirects(&pl->stages[i]);
            execvp(pl->stages[i].argv[0], pl->stages[i].argv);
            fprintf(stderr, "axon: %s: %s\n", pl->stages[i].argv[0], strerror(errno));
            _exit(127);
        }

        pids[i] = pid;

        /* Parent: close fds we no longer need */
        if (prev_fd != -1)
            close(prev_fd);
        if (pipefd[1] != -1)
            close(pipefd[1]);

        prev_fd = pipefd[0];
    }

    /* Wait for all children, keep last stage's exit code */
    int last_code = 0;
    for (int i = 0; i < pl->num_stages; i++) {
        int code = wait_for_child(pids[i]);
        if (i == pl->num_stages - 1)
            last_code = code;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    result->exit_code = last_code;
    result->elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0
                       + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    return 0;
}
