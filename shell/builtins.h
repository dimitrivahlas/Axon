#ifndef AXON_BUILTINS_H
#define AXON_BUILTINS_H

/*
 * Try to execute a builtin command.
 * Returns -1 if not a builtin (caller should try external execution).
 * Returns 0 if builtin executed successfully.
 * Returns >0 (exit code) if builtin executed but failed.
 */
int builtin_execute(char *line);

#endif
