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

/*
 * Expand environment variables in a command line.
 * Handles $VAR, ${VAR}, and $? (last exit code).
 * Single-quoted regions are not expanded.
 * Writes result into out (up to out_max bytes).
 * Returns 0 on success, -1 on error.
 */
int expand_env(const char *line, char *out, size_t out_max, int last_exit_code);

#define AXON_MAX_CHAIN 32

/* Operator between chained commands */
typedef enum {
    CHAIN_NONE,  /* first command, no preceding operator */
    CHAIN_SEMI,  /* ; — always run */
    CHAIN_AND,   /* && — run only if previous succeeded */
    CHAIN_OR     /* || — run only if previous failed */
} chain_op_t;

typedef struct {
    char *segment;   /* raw text for this pipeline segment */
    chain_op_t op;   /* operator before this segment */
} chain_entry_t;

typedef struct {
    chain_entry_t entries[AXON_MAX_CHAIN];
    int count;
} chain_t;

/*
 * Split a command line on &&, ||, and ; into chained segments.
 * Respects quoted strings. Modifies the input string in place.
 * Returns 0 on success, -1 on error.
 */
int parse_chain(char *line, chain_t *chain);

/*
 * Detect the sandbox prefix. If line begins with "& " (or "&\t"), return a
 * pointer into line at the command that follows (leading whitespace skipped).
 * Returns NULL if there is no sandbox prefix: a leading "&&" (chain operator),
 * "&" with no separator, or "& " with no command all return NULL.
 * Does not modify the input.
 */
const char *sandbox_prefix(const char *line);

/*
 * Interpret a yes/no answer for a prompt whose default is No.
 * Returns 1 only for an explicit yes (leading 'y' or 'Y', after optional
 * whitespace); everything else — empty, "n", or garbage — returns 0.
 */
int is_affirmative(const char *answer);

#endif
