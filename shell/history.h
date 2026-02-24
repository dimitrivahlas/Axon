#ifndef AXON_HISTORY_H
#define AXON_HISTORY_H

#include "executor.h"

#define AXON_HISTORY_MAX 10

typedef struct {
    char raw_line[4096];
    int exit_code;
    double elapsed_ms;
    char cwd[1024];
    char stderr_output[4096];
} history_entry_t;

typedef struct {
    history_entry_t entries[AXON_HISTORY_MAX];
    int count;
} history_t;

void history_init(history_t *hist);
void history_add(history_t *hist, const char *line, cmd_result_t *result, const char *cwd);

#endif
