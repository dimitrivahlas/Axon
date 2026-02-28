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
#include "ai.h"
#include "../context/context.h"

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

    /* Initialize context engine (persistent history via SQLite) */
    context_init();

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
            char *ctx_json = context_build_ai_json(cwd, 0);
            ai_ask(question, ctx_json ? ctx_json : "{}", cwd);
            free(ctx_json);
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
            char *ctx_json = context_build_ai_json(cwd, 0);
            ai_suggest(intent, ctx_json ? ctx_json : "{}", cwd);
            free(ctx_json);
            continue;
        }

        /* Expand environment variables */
        char expanded[AXON_INPUT_MAX];
        if (expand_env(line, expanded, sizeof(expanded), last_exit_code) != 0) {
            continue;
        }

        /* Split on &&, ||, ; */
        chain_t chain;
        if (parse_chain(expanded, &chain) != 0) {
            continue;
        }

        for (int ci = 0; ci < chain.count; ci++) {
            char *seg = chain.entries[ci].segment;
            chain_op_t op = chain.entries[ci].op;

            /* Check chain operator conditions */
            if (op == CHAIN_AND && last_exit_code != 0)
                continue;
            if (op == CHAIN_OR && last_exit_code == 0)
                continue;

            /* Skip leading whitespace */
            while (*seg == ' ' || *seg == '\t')
                seg++;
            if (*seg == '\0')
                continue;

            int builtin_ret = builtin_execute(seg);
            if (builtin_ret >= 0) {
                last_exit_code = builtin_ret;
                continue;
            }

            /* Save a copy before parsing mutates it */
            char seg_copy[AXON_INPUT_MAX];
            strncpy(seg_copy, seg, sizeof(seg_copy) - 1);
            seg_copy[sizeof(seg_copy) - 1] = '\0';

            pipeline_t pl;
            if (parse_pipeline(seg, &pl) != 0) {
                last_exit_code = 1;
                continue;
            }

            if (pl.num_stages == 0)
                continue;

            cmd_result_t result;
            if (execute_pipeline(&pl, &result) != 0) {
                last_exit_code = 1;
                continue;
            }

            /* Record in history */
            char cwd[PROMPT_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL)
                strncpy(cwd, "???", sizeof(cwd));
            context_add(seg_copy, result.exit_code, result.elapsed_ms,
                        cwd, result.stderr_capture);
            last_exit_code = result.exit_code;

            if (result.exit_code != 0) {
                fprintf(stderr, "\033[1;31m[exit %d | %.1fms]\033[0m\n",
                        result.exit_code, result.elapsed_ms);
            }
        }
    }

    context_shutdown();
    fprintf(stderr, "\n");
    return 0;
}
