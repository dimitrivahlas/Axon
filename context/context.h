#ifndef AXON_CONTEXT_H
#define AXON_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the context engine. Creates ~/.axon/ and context.db if needed.
 * Returns 0 on success, -1 on error. */
int context_init(void);

/* Record a command execution with full metadata.
 * Called from main.c after every command runs. */
int context_add(const char *command, int exit_code, double elapsed_ms,
                const char *cwd, const char *stderr_output);

/* Build rich JSON context for the AI. Caller must free() the result.
 * Includes: recent history, error patterns, git state, session info.
 * max_entries: how many history entries to include (0 = default 20). */
char *context_build_ai_json(const char *cwd, int max_entries);

/* Shut down the context engine. Closes DB connection. */
void context_shutdown(void);

/* Get the current session ID. Returns NULL if not initialized. */
const char *context_session_id(void);

#ifdef __cplusplus
}
#endif

#endif
