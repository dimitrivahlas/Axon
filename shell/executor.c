#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include "executor.h"

int execute_command(command_t *cmd, cmd_result_t *result)
{
    result->exit_code = 0;
    result->elapsed_ms = 0.0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "axon: fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        /* Child: exec the command */
        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "axon: %s: %s\n", cmd->argv[0], strerror(errno));
        _exit(127);
    }

    /* Parent: wait for child */
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "axon: waitpid: %s\n", strerror(errno));
            return -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (WIFEXITED(status)) {
        result->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result->exit_code = 128 + WTERMSIG(status);
    }

    result->elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0
                       + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    return 0;
}
