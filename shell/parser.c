#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

/* Find the next unquoted pipe character, skipping single/double quoted regions */
static char *find_unquoted_pipe(char *s)
{
    int in_single = 0;
    int in_double = 0;

    for (; *s != '\0'; s++) {
        if (*s == '\'' && !in_double)
            in_single = !in_single;
        else if (*s == '"' && !in_single)
            in_double = !in_double;
        else if (*s == '|' && !in_single && !in_double)
            return s;
    }
    return NULL;
}

int parse_pipeline(char *line, pipeline_t *pl)
{
    pl->num_stages = 0;

    char *segment = line;
    char *pipe_pos;

    while (segment != NULL) {
        pipe_pos = find_unquoted_pipe(segment);
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

int expand_env(const char *line, char *out, size_t out_max, int last_exit_code)
{
    size_t pos = 0;
    const char *p = line;
    int in_single_quote = 0;

    while (*p != '\0' && pos < out_max - 1) {
        if (*p == '\'' && !in_single_quote) {
            in_single_quote = 1;
            out[pos++] = *p++;
        } else if (*p == '\'' && in_single_quote) {
            in_single_quote = 0;
            out[pos++] = *p++;
        } else if (*p == '~' && !in_single_quote &&
                   (p == line || *(p - 1) == ' ' || *(p - 1) == '\t' ||
                    *(p - 1) == '=' || *(p - 1) == ':') &&
                   (*(p + 1) == '/' || *(p + 1) == '\0' || *(p + 1) == ' ' ||
                    *(p + 1) == '\t')) {
            const char *home = getenv("HOME");
            if (home != NULL) {
                size_t hlen = strlen(home);
                if (pos + hlen < out_max - 1) {
                    memcpy(out + pos, home, hlen);
                    pos += hlen;
                }
            }
            p++;
        } else if (*p == '$' && !in_single_quote) {
            p++;
            const char *val = NULL;
            char exit_buf[16];

            if (*p == '?') {
                snprintf(exit_buf, sizeof(exit_buf), "%d", last_exit_code);
                val = exit_buf;
                p++;
            } else if (*p == '{') {
                p++;
                const char *start = p;
                while (*p != '\0' && *p != '}')
                    p++;
                if (*p == '\0') {
                    fprintf(stderr, "axon: unclosed ${}\n");
                    return -1;
                }
                size_t name_len = (size_t)(p - start);
                char name[256];
                if (name_len >= sizeof(name))
                    name_len = sizeof(name) - 1;
                memcpy(name, start, name_len);
                name[name_len] = '\0';
                val = getenv(name);
                p++; /* skip } */
            } else if (isalpha((unsigned char)*p) || *p == '_') {
                const char *start = p;
                while (isalnum((unsigned char)*p) || *p == '_')
                    p++;
                size_t name_len = (size_t)(p - start);
                char name[256];
                if (name_len >= sizeof(name))
                    name_len = sizeof(name) - 1;
                memcpy(name, start, name_len);
                name[name_len] = '\0';
                val = getenv(name);
            } else {
                /* Lone $ — write it literally */
                out[pos++] = '$';
                continue;
            }

            if (val != NULL) {
                size_t vlen = strlen(val);
                if (pos + vlen >= out_max - 1)
                    vlen = out_max - 1 - pos;
                memcpy(out + pos, val, vlen);
                pos += vlen;
            }
        } else {
            out[pos++] = *p++;
        }
    }

    out[pos] = '\0';
    return 0;
}

int parse_chain(char *line, chain_t *chain)
{
    chain->count = 0;

    char *p = line;
    char *seg_start = line;
    int in_single = 0;
    int in_double = 0;

    while (*p != '\0') {
        if (*p == '\'' && !in_double) {
            in_single = !in_single;
            p++;
        } else if (*p == '"' && !in_single) {
            in_double = !in_double;
            p++;
        } else if (!in_single && !in_double) {
            chain_op_t op = CHAIN_NONE;

            if (*p == '&' && *(p + 1) == '&') {
                op = CHAIN_AND;
                *p = '\0';
                p += 2;
            } else if (*p == '|' && *(p + 1) == '|') {
                op = CHAIN_OR;
                *p = '\0';
                p += 2;
            } else if (*p == ';') {
                op = CHAIN_SEMI;
                *p = '\0';
                p++;
            } else {
                p++;
                continue;
            }

            if (chain->count >= AXON_MAX_CHAIN) {
                fprintf(stderr, "axon: too many chained commands\n");
                return -1;
            }

            chain->entries[chain->count].segment = seg_start;
            chain->entries[chain->count].op = (chain->count == 0) ? CHAIN_NONE : chain->entries[chain->count].op;
            chain->count++;

            /* Next entry's operator */
            if (chain->count < AXON_MAX_CHAIN) {
                chain->entries[chain->count].op = op;
            }

            seg_start = p;
        } else {
            p++;
        }
    }

    /* Last segment */
    if (chain->count < AXON_MAX_CHAIN) {
        chain->entries[chain->count].segment = seg_start;
        if (chain->count == 0)
            chain->entries[chain->count].op = CHAIN_NONE;
        chain->count++;
    }

    return 0;
}
