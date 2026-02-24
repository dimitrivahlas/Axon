#ifndef AXON_PARSER_H
#define AXON_PARSER_H

#define AXON_MAX_ARGS 128
#define AXON_MAX_PIPE_STAGES 16

typedef struct {
    char *argv[AXON_MAX_ARGS + 1]; /* NULL-terminated argument list */
    int argc;
    char *redir_in;    /* filename for < redirect, or NULL */
    char *redir_out;   /* filename for > or >> redirect, or NULL */
    int append;        /* 1 if >>, 0 if > */
} command_t;

typedef struct {
    command_t stages[AXON_MAX_PIPE_STAGES];
    int num_stages;
} pipeline_t;

/*
 * Tokenize a command line into argv/argc.
 * Handles single quotes, double quotes, and whitespace separation.
 * Modifies the input string in place (inserts NULLs).
 * Returns 0 on success, -1 on error (e.g. unclosed quote).
 */
int parse_command(char *line, command_t *cmd);

/*
 * Parse a full command line into a pipeline.
 * Splits on | into stages, then tokenizes each stage.
 * Detects <, >, >> redirections per stage.
 * Returns 0 on success, -1 on error.
 */
int parse_pipeline(char *line, pipeline_t *pl);

#endif
