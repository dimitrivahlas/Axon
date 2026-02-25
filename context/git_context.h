#ifndef AXON_GIT_CONTEXT_H
#define AXON_GIT_CONTEXT_H

#include <string>
#include <vector>

struct GitContext {
    std::string branch;
    std::vector<std::string> recent_commits;
    std::vector<std::string> modified_files;
    std::vector<std::string> staged_files;
    bool is_dirty;
    bool is_git_repo;
};

/* Gather full git context for the given directory.
 * Returns empty context if not in a git repo. */
GitContext git_gather_context(const std::string &cwd, int max_commits = 5);

#endif
