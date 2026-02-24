#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "history.h"

void history_init(history_t *hist)
{
    hist->count = 0;
    memset(hist->entries, 0, sizeof(hist->entries));
}

void history_add(history_t *hist, const char *line, cmd_result_t *result, const char *cwd)
{
    /* Shift entries down if full */
    if (hist->count >= AXON_HISTORY_MAX) {
        memmove(&hist->entries[0], &hist->entries[1],
                sizeof(history_entry_t) * (AXON_HISTORY_MAX - 1));
        hist->count = AXON_HISTORY_MAX - 1;
    }

    history_entry_t *e = &hist->entries[hist->count];
    strncpy(e->raw_line, line, sizeof(e->raw_line) - 1);
    e->raw_line[sizeof(e->raw_line) - 1] = '\0';
    e->exit_code = result->exit_code;
    e->elapsed_ms = result->elapsed_ms;
    strncpy(e->cwd, cwd, sizeof(e->cwd) - 1);
    e->cwd[sizeof(e->cwd) - 1] = '\0';
    strncpy(e->stderr_output, result->stderr_capture, sizeof(e->stderr_output) - 1);
    e->stderr_output[sizeof(e->stderr_output) - 1] = '\0';

    hist->count++;
}
