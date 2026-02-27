#include <cstdio>
#include <cassert>
#include "../context/git_context.h"

static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  test_%s: ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

void test_non_git_dir()
{
    tests_run++;
    GitContext ctx = git_gather_context("/tmp");
    assert(ctx.is_git_repo == false);
    assert(ctx.branch.empty());
    assert(ctx.recent_commits.empty());
    assert(ctx.modified_files.empty());
    assert(ctx.staged_files.empty());
    assert(ctx.is_dirty == false);
}

void test_git_repo_detected()
{
    tests_run++;
    GitContext ctx = git_gather_context(".");
    assert(ctx.is_git_repo == true);
    assert(!ctx.branch.empty());
}

void test_commit_limit()
{
    tests_run++;
    GitContext ctx = git_gather_context(".", 3);
    assert(ctx.is_git_repo == true);
    assert(ctx.recent_commits.size() <= 3);
}

void test_default_commits()
{
    tests_run++;
    GitContext ctx = git_gather_context(".", 5);
    assert(ctx.is_git_repo == true);
    assert(ctx.recent_commits.size() <= 5);
}

int main()
{
    printf("=== Git Context Tests ===\n");

    TEST(non_git_dir);
    TEST(git_repo_detected);
    TEST(commit_limit);
    TEST(default_commits);

    printf("\n-----------------------\n");
    printf("%d Tests %d Failures 0 Ignored\n", tests_passed, tests_run - tests_passed);
    printf("OK\n");

    return 0;
}
