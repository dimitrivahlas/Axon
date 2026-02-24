#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include "parser.h"

int parse_command(char *line, command_t *cmd)
{
    cmd->argc = 0;
    memset(cmd->argv, 0, sizeof(cmd->argv));

    char *p = line;

    while (*p != '\0') {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        if (cmd->argc >= AXON_MAX_ARGS) {
            fprintf(stderr, "axon: too many arguments\n");
            return -1;
        }

        char *start;

        if (*p == '\'') {
            /* Single-quoted: take everything until closing quote literally */
            p++;
            start = p;
            while (*p != '\0' && *p != '\'')
                p++;
            if (*p == '\0') {
                fprintf(stderr, "axon: unclosed single quote\n");
                return -1;
            }
            *p = '\0';
            p++;
        } else if (*p == '"') {
            /* Double-quoted: take everything until closing quote */
            p++;
            start = p;
            while (*p != '\0' && *p != '"')
                p++;
            if (*p == '\0') {
                fprintf(stderr, "axon: unclosed double quote\n");
                return -1;
            }
            *p = '\0';
            p++;
        } else {
            /* Unquoted: take until next whitespace */
            start = p;
            while (*p != '\0' && *p != ' ' && *p != '\t')
                p++;
            if (*p != '\0') {
                *p = '\0';
                p++;
            }
        }

        cmd->argv[cmd->argc] = start;
        cmd->argc++;
    }

    cmd->argv[cmd->argc] = NULL;
    return 0;
}

static int extract_redirects(command_t *cmd)
{
    cmd->redir_in = NULL;
    cmd->redir_out = NULL;
    cmd->append = 0;

    int dst = 0;
    for (int i = 0; i < cmd->argc; i++) {
        if (strcmp(cmd->argv[i], "<") == 0) {
            if (i + 1 >= cmd->argc) {
                fprintf(stderr, "axon: syntax error near <\n");
                return -1;
            }
            cmd->redir_in = cmd->argv[i + 1];
            i++;
        } else if (strcmp(cmd->argv[i], ">>") == 0) {
            if (i + 1 >= cmd->argc) {
                fprintf(stderr, "axon: syntax error near >>\n");
                return -1;
            }
            cmd->redir_out = cmd->argv[i + 1];
            cmd->append = 1;
            i++;
        } else if (strcmp(cmd->argv[i], ">") == 0) {
            if (i + 1 >= cmd->argc) {
                fprintf(stderr, "axon: syntax error near >\n");
                return -1;
            }
            cmd->redir_out = cmd->argv[i + 1];
            cmd->append = 0;
            i++;
        } else {
            cmd->argv[dst] = cmd->argv[i];
            dst++;
        }
    }
    cmd->argc = dst;
    cmd->argv[dst] = NULL;
    return 0;
}

int parse_pipeline(char *line, pipeline_t *pl)
{
    pl->num_stages = 0;

    char *segment = line;
    char *pipe_pos;

    while (segment != NULL) {
        pipe_pos = strchr(segment, '|');
        if (pipe_pos != NULL) {
            *pipe_pos = '\0';
        }
        if (pl->num_stages >= AXON_MAX_PIPE_STAGES) {
            fprintf(stderr, "axon: too many pipe stages\n");
            return -1;
        }

        command_t *cmd = &pl->stages[pl->num_stages];
        if (parse_command(segment, cmd) != 0) {
            return -1;
        }

        if (extract_redirects(cmd) != 0) {
            return -1;
        }

        if (cmd->argc == 0 && (pipe_pos != NULL || pl->num_stages > 0)) {
            fprintf(stderr, "axon: syntax error near |\n");
            return -1;
        }

        if (cmd->argc > 0) {
            pl->num_stages++;
        }

        segment = (pipe_pos != NULL) ? pipe_pos + 1 : NULL;
    }

    return 0;
}
