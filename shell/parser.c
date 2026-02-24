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
