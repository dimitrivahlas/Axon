#define _POSIX_C_SOURCE 200809L
#include "unity.h"
#include "../shell/executor.h"

void setUp(void) {}
void tearDown(void) {}

void test_successful_command(void)
{
    command_t cmd = { .argv = {"true", NULL}, .argc = 1 };
    cmd_result_t result;

    TEST_ASSERT_EQUAL_INT(0, execute_command(&cmd, &result));
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
    TEST_ASSERT_TRUE(result.elapsed_ms >= 0.0);
}

void test_failing_command(void)
{
    command_t cmd = { .argv = {"false", NULL}, .argc = 1 };
    cmd_result_t result;

    TEST_ASSERT_EQUAL_INT(0, execute_command(&cmd, &result));
    TEST_ASSERT_EQUAL_INT(1, result.exit_code);
}

void test_command_not_found(void)
{
    command_t cmd = { .argv = {"__nonexistent_command__", NULL}, .argc = 1 };
    cmd_result_t result;

    TEST_ASSERT_EQUAL_INT(0, execute_command(&cmd, &result));
    TEST_ASSERT_EQUAL_INT(127, result.exit_code);
}

void test_command_with_args(void)
{
    command_t cmd = { .argv = {"echo", "hello", NULL}, .argc = 2 };
    cmd_result_t result;

    TEST_ASSERT_EQUAL_INT(0, execute_command(&cmd, &result));
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_timing_is_captured(void)
{
    command_t cmd = { .argv = {"true", NULL}, .argc = 1 };
    cmd_result_t result;

    execute_command(&cmd, &result);
    TEST_ASSERT_TRUE(result.elapsed_ms > 0.0);
    TEST_ASSERT_TRUE(result.elapsed_ms < 5000.0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_successful_command);
    RUN_TEST(test_failing_command);
    RUN_TEST(test_command_not_found);
    RUN_TEST(test_command_with_args);
    RUN_TEST(test_timing_is_captured);
    return UNITY_END();
}
