#ifndef AXON_PARSER_H
#define AXON_PARSER_H

#define AXON_MAX_ARGS 128

typedef struct {
    char *argv[AXON_MAX_ARGS + 1]; /* NULL-terminated argument list */
    int argc;
} command_t;

/*
 * Tokenize a command line into argv/argc.
 * Handles single quotes, double quotes, and whitespace separation.
 * Modifies the input string in place (inserts NULLs).
 * Returns 0 on success, -1 on error (e.g. unclosed quote).
 */
int parse_command(char *line, command_t *cmd);

#endif
