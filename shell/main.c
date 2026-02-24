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
#include "history.h"
#include "ai.h"

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

    history_t hist;
    history_init(&hist);
    int last_exit_code = 0;

    char *line;
    while ((line = read_line()) != NULL) {
        if (line[0] == '\0') {
            continue;
        }

        /* AI: ? <question> */
        if (line[0] == '?' && (line[1] == ' ' || line[1] == '\t')) {
            char cwd[PROMPT_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL)
                strncpy(cwd, "???", sizeof(cwd));
            char *question = line + 2;
            while (*question == ' ' || *question == '\t')
                question++;
            ai_ask(question, &hist, cwd);
            continue;
        }

        /* AI: !! <intent> */
        if (line[0] == '!' && line[1] == '!' && (line[2] == ' ' || line[2] == '\t')) {
            char cwd[PROMPT_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL)
                strncpy(cwd, "???", sizeof(cwd));
            char *intent = line + 3;
            while (*intent == ' ' || *intent == '\t')
                intent++;
            ai_suggest(intent, &hist, cwd);
            continue;
        }

        /* Expand environment variables */
        char expanded[AXON_INPUT_MAX];
        if (expand_env(line, expanded, sizeof(expanded), last_exit_code) != 0) {
            continue;
        }

        if (builtin_execute(expanded)) {
            continue;
        }

        /* Save a copy of the expanded line before parsing mutates it */
        char line_copy[AXON_INPUT_MAX];
        strncpy(line_copy, expanded, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        pipeline_t pl;
        if (parse_pipeline(expanded, &pl) != 0) {
            continue;
        }

        if (pl.num_stages == 0) {
            continue;
        }

        cmd_result_t result;
        if (execute_pipeline(&pl, &result) != 0) {
            continue;
        }

        /* Record in history */
        char cwd[PROMPT_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            strncpy(cwd, "???", sizeof(cwd));
        history_add(&hist, line_copy, &result, cwd);
        last_exit_code = result.exit_code;

        if (result.exit_code != 0) {
            fprintf(stderr, "\033[1;31m[exit %d | %.1fms]\033[0m\n",
                    result.exit_code, result.elapsed_ms);
        }
    }

    fprintf(stderr, "\n");
    return 0;
}
