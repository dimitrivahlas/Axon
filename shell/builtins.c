#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "builtins.h"

static int builtin_cd(char *arg)
{
    const char *target = arg;

    if (target == NULL || target[0] == '\0') {
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "axon: cd: HOME not set\n");
            return 1;
        }
    }

    if (chdir(target) != 0) {
        fprintf(stderr, "axon: cd: %s: %s\n", target, strerror(errno));
        return 1; /* failure */
    }

    return 0; /* success */
}

static int builtin_exit(void)
{
    fprintf(stderr, "exit\n");
    exit(0);
}

static int builtin_help(void)
{
    fprintf(stdout,
        "axon — AI-native shell\n"
        "\n"
        "Builtins:\n"
        "  cd [dir]         Change directory (default: $HOME)\n"
        "  exit             Exit the shell\n"
        "  help             Show this message\n"
        "\n"
        "AI commands:\n"
        "  ? <question>     Ask a question with command history context\n"
        "  !! <intent>      Describe what you want, get a suggested command\n"
        "\n"
        "Features:\n"
        "  Pipes:           cmd1 | cmd2 | cmd3\n"
        "  Redirects:       cmd > file, cmd >> file, cmd < file\n"
        "  Chaining:        cmd1 && cmd2, cmd1 || cmd2, cmd1; cmd2\n"
        "  Env vars:        $VAR, ${VAR}, $?, ~\n"
        "  Quotes:          'literal', \"grouped\"\n"
    );
    return 0;
}

int builtin_execute(char *line)
{
    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t')
        line++;

    if (strncmp(line, "cd", 2) == 0 &&
        (line[2] == ' ' || line[2] == '\t' || line[2] == '\0')) {
        char *arg = line + 2;
        while (*arg == ' ' || *arg == '\t')
            arg++;
        return builtin_cd(arg);
    }

    if (strncmp(line, "exit", 4) == 0 &&
        (line[4] == ' ' || line[4] == '\t' || line[4] == '\0')) {
        return builtin_exit();
    }

    if (strncmp(line, "help", 4) == 0 &&
        (line[4] == ' ' || line[4] == '\t' || line[4] == '\0')) {
        return builtin_help();
    }

    return -1; /* Not a builtin */
}
