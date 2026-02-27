#include "git_context.h"
#include <cstdio>
#include <cstring>

static std::vector<std::string> git_lines(const std::string &cwd, const char *args)
{
    std::vector<std::string> lines;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "git -C '%s' %s 2>/dev/null", cwd.c_str(), args);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return lines;

    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
            buf[len - 1] = '\0';
        lines.push_back(buf);
    }

    pclose(fp);
    return lines;
}

GitContext git_gather_context(const std::string &cwd, int max_commits)
{
    GitContext ctx;
    ctx.is_dirty    = false;
    ctx.is_git_repo = false;

    auto branch_lines = git_lines(cwd, "rev-parse --abbrev-ref HEAD");
    if (branch_lines.empty())
        return ctx;

    ctx.is_git_repo = true;
    ctx.branch      = branch_lines[0];

    char log_args[64];
    snprintf(log_args, sizeof(log_args), "log --oneline -%d", max_commits);
    ctx.recent_commits = git_lines(cwd, log_args);

    ctx.modified_files = git_lines(cwd, "diff --name-only");
    ctx.staged_files   = git_lines(cwd, "diff --cached --name-only");

    ctx.is_dirty = !ctx.modified_files.empty() || !ctx.staged_files.empty();

    return ctx;
}
