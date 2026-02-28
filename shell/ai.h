#ifndef AXON_AI_H
#define AXON_AI_H

/*
 * ? mode: send a question with structured context to Claude.
 * context_json is the JSON payload from context_build_ai_json().
 * Prints the response to stdout.
 * Returns 0 on success, -1 on error.
 */
int ai_ask(const char *question, const char *context_json, const char *cwd);

/*
 * !! mode: send an intent with structured context, Claude suggests a command.
 * context_json is the JSON payload from context_build_ai_json().
 * Prints the suggested command to stdout.
 * Returns 0 on success, -1 on error.
 */
int ai_suggest(const char *intent, const char *context_json, const char *cwd);

#endif
