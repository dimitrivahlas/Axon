#ifndef AXON_EXECUTOR_H
#define AXON_EXECUTOR_H

#include "parser.h"

#define AXON_CAPTURE_MAX 4096

typedef struct {
    int exit_code;
    double elapsed_ms;
    char stderr_capture[AXON_CAPTURE_MAX];
} cmd_result_t;

/*
 * Execute a parsed command via fork/exec.
 * Waits for the child to finish and fills result with
 * the exit code and wall clock time in milliseconds.
 * Returns 0 on success, -1 on fork/exec failure.
 */
int execute_command(command_t *cmd, cmd_result_t *result);

/*
 * Execute a full pipeline (one or more stages connected by pipes).
 * Handles I/O redirection (<, >, >>) per stage.
 * Result reflects the last stage's exit code and total wall time.
 * Returns 0 on success, -1 on error.
 */
int execute_pipeline(pipeline_t *pl, cmd_result_t *result);

#endif
