#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "builtins.h"
#include "parser.h"
#include "executor.h"

#define AXON_INPUT_MAX 4096
#define PROMPT_MAX 1024

static volatile sig_atomic_t interrupted = 0;

static void sigint_handler(int sig)
{
    (void)sig;
    interrupted = 1;
}

static void print_prompt(void)
{
    char cwd[PROMPT_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strncpy(cwd, "???", sizeof(cwd));
    }
    fprintf(stderr, "\033[1;36maxon\033[0m:\033[1;34m%s\033[0m$ ", cwd);
}

static char *read_line(void)
{
    static char buf[AXON_INPUT_MAX];

    print_prompt();

    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        if (interrupted) {
            interrupted = 0;
            buf[0] = '\0';
            fprintf(stderr, "\n");
            return buf;
        }
        return NULL; /* EOF (Ctrl+D) */
    }

    if (interrupted) {
        interrupted = 0;
        buf[0] = '\0';
        return buf;
    }

    /* Strip trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    return buf;
}

int main(void)
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    char *line;
    while ((line = read_line()) != NULL) {
        if (line[0] == '\0') {
            continue;
        }

        if (builtin_execute(line)) {
            continue;
        }

        pipeline_t pl;
        if (parse_pipeline(line, &pl) != 0) {
            continue;
        }

        if (pl.num_stages == 0) {
            continue;
        }

        cmd_result_t result;
        if (execute_pipeline(&pl, &result) != 0) {
            continue;
        }

        if (result.exit_code != 0) {
            fprintf(stderr, "\033[1;31m[exit %d | %.1fms]\033[0m\n",
                    result.exit_code, result.elapsed_ms);
        }
    }

    fprintf(stderr, "\n");
    return 0;
}
