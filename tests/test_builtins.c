#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unity.h"
#include "../shell/builtins.h"

void setUp(void) {}
void tearDown(void) {}

void test_cd_to_tmp(void)
{
    char line[] = "cd /tmp";
    int ret = builtin_execute(line);
    TEST_ASSERT_EQUAL_INT(0, ret);

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    /* macOS resolves /tmp to /private/tmp */
    TEST_ASSERT_TRUE(strcmp(cwd, "/tmp") == 0 || strcmp(cwd, "/private/tmp") == 0);
}

void test_cd_to_nonexistent(void)
{
    char line[] = "cd /nonexistent_axon_dir";
    int ret = builtin_execute(line);
    TEST_ASSERT_EQUAL_INT(1, ret);
}

void test_cd_no_arg_uses_home(void)
{
    setenv("HOME", "/tmp", 1);
    char line[] = "cd";
    int ret = builtin_execute(line);
    TEST_ASSERT_EQUAL_INT(0, ret);

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    /* macOS resolves /tmp to /private/tmp */
    TEST_ASSERT_TRUE(strcmp(cwd, "/tmp") == 0 || strcmp(cwd, "/private/tmp") == 0);
}

void test_help_returns_success(void)
{
    char line[] = "help";
    int ret = builtin_execute(line);
    TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_not_a_builtin(void)
{
    char line[] = "ls -la";
    int ret = builtin_execute(line);
    TEST_ASSERT_EQUAL_INT(-1, ret);
}

void test_not_a_builtin_similar_name(void)
{
    char line[] = "cdd /tmp";
    int ret = builtin_execute(line);
    TEST_ASSERT_EQUAL_INT(-1, ret);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cd_to_tmp);
    RUN_TEST(test_cd_to_nonexistent);
    RUN_TEST(test_cd_no_arg_uses_home);
    RUN_TEST(test_help_returns_success);
    RUN_TEST(test_not_a_builtin);
    RUN_TEST(test_not_a_builtin_similar_name);
    return UNITY_END();
}
