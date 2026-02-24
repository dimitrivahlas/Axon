#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "unity.h"
#include "../shell/parser.h"

void setUp(void) {}
void tearDown(void) {}

void test_simple_command(void)
{
    char line[] = "ls -la /tmp";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(3, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("ls", cmd.argv[0]);
    TEST_ASSERT_EQUAL_STRING("-la", cmd.argv[1]);
    TEST_ASSERT_EQUAL_STRING("/tmp", cmd.argv[2]);
    TEST_ASSERT_NULL(cmd.argv[3]);
}

void test_single_arg(void)
{
    char line[] = "whoami";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(1, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("whoami", cmd.argv[0]);
    TEST_ASSERT_NULL(cmd.argv[1]);
}

void test_empty_input(void)
{
    char line[] = "";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(0, cmd.argc);
    TEST_ASSERT_NULL(cmd.argv[0]);
}

void test_whitespace_only(void)
{
    char line[] = "   \t  \t  ";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(0, cmd.argc);
    TEST_ASSERT_NULL(cmd.argv[0]);
}

void test_leading_trailing_whitespace(void)
{
    char line[] = "  echo hello  ";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(2, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("echo", cmd.argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello", cmd.argv[1]);
}

void test_single_quotes(void)
{
    char line[] = "echo 'hello world'";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(2, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("echo", cmd.argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello world", cmd.argv[1]);
}

void test_double_quotes(void)
{
    char line[] = "echo \"foo bar\" baz";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(3, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("echo", cmd.argv[0]);
    TEST_ASSERT_EQUAL_STRING("foo bar", cmd.argv[1]);
    TEST_ASSERT_EQUAL_STRING("baz", cmd.argv[2]);
}

void test_unclosed_single_quote(void)
{
    char line[] = "echo 'hello";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(-1, parse_command(line, &cmd));
}

void test_unclosed_double_quote(void)
{
    char line[] = "echo \"hello";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(-1, parse_command(line, &cmd));
}

void test_empty_quotes(void)
{
    char line[] = "echo '' \"\"";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(3, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("echo", cmd.argv[0]);
    TEST_ASSERT_EQUAL_STRING("", cmd.argv[1]);
    TEST_ASSERT_EQUAL_STRING("", cmd.argv[2]);
}

void test_multiple_spaces_between_args(void)
{
    char line[] = "ls    -l     /tmp";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(3, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("ls", cmd.argv[0]);
    TEST_ASSERT_EQUAL_STRING("-l", cmd.argv[1]);
    TEST_ASSERT_EQUAL_STRING("/tmp", cmd.argv[2]);
}

void test_tabs_as_separators(void)
{
    char line[] = "echo\thello\tworld";
    command_t cmd;

    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(3, cmd.argc);
    TEST_ASSERT_EQUAL_STRING("echo", cmd.argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello", cmd.argv[1]);
    TEST_ASSERT_EQUAL_STRING("world", cmd.argv[2]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_simple_command);
    RUN_TEST(test_single_arg);
    RUN_TEST(test_empty_input);
    RUN_TEST(test_whitespace_only);
    RUN_TEST(test_leading_trailing_whitespace);
    RUN_TEST(test_single_quotes);
    RUN_TEST(test_double_quotes);
    RUN_TEST(test_unclosed_single_quote);
    RUN_TEST(test_unclosed_double_quote);
    RUN_TEST(test_empty_quotes);
    RUN_TEST(test_multiple_spaces_between_args);
    RUN_TEST(test_tabs_as_separators);
    return UNITY_END();
}
