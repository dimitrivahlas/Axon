#ifndef AXON_AI_H
#define AXON_AI_H

#include "history.h"

/*
 * ? mode: send a question with command history context to Claude.
 * Prints the response to stdout.
 * Returns 0 on success, -1 on error.
 */
int ai_ask(const char *question, history_t *hist, const char *cwd);

/*
 * !! mode: send an intent, Claude suggests a command.
 * Prints the suggested command to stdout.
 * Returns 0 on success, -1 on error.
 */
int ai_suggest(const char *intent, history_t *hist, const char *cwd);

#endif
