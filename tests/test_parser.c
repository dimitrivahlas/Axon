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

/* --- Pipeline tests --- */

void test_pipeline_single_command(void)
{
    char line[] = "ls -la";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(1, pl.num_stages);
    TEST_ASSERT_EQUAL_INT(2, pl.stages[0].argc);
    TEST_ASSERT_EQUAL_STRING("ls", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("-la", pl.stages[0].argv[1]);
}

void test_pipeline_two_stages(void)
{
    char line[] = "ls | grep .c";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(2, pl.num_stages);
    TEST_ASSERT_EQUAL_STRING("ls", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("grep", pl.stages[1].argv[0]);
    TEST_ASSERT_EQUAL_STRING(".c", pl.stages[1].argv[1]);
}

void test_pipeline_three_stages(void)
{
    char line[] = "cat file | sort | uniq";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(3, pl.num_stages);
    TEST_ASSERT_EQUAL_STRING("cat", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("sort", pl.stages[1].argv[0]);
    TEST_ASSERT_EQUAL_STRING("uniq", pl.stages[2].argv[0]);
}

void test_redirect_output(void)
{
    char line[] = "echo hello > out.txt";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(1, pl.num_stages);
    TEST_ASSERT_EQUAL_INT(2, pl.stages[0].argc);
    TEST_ASSERT_EQUAL_STRING("echo", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello", pl.stages[0].argv[1]);
    TEST_ASSERT_EQUAL_STRING("out.txt", pl.stages[0].redir_out);
    TEST_ASSERT_EQUAL_INT(0, pl.stages[0].append);
}

void test_redirect_append(void)
{
    char line[] = "echo hello >> log.txt";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(1, pl.num_stages);
    TEST_ASSERT_EQUAL_STRING("log.txt", pl.stages[0].redir_out);
    TEST_ASSERT_EQUAL_INT(1, pl.stages[0].append);
}

void test_redirect_input(void)
{
    char line[] = "sort < data.txt";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(1, pl.num_stages);
    TEST_ASSERT_EQUAL_INT(1, pl.stages[0].argc);
    TEST_ASSERT_EQUAL_STRING("sort", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("data.txt", pl.stages[0].redir_in);
}

void test_redirect_both(void)
{
    char line[] = "sort < in.txt > out.txt";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(1, pl.num_stages);
    TEST_ASSERT_EQUAL_STRING("sort", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("in.txt", pl.stages[0].redir_in);
    TEST_ASSERT_EQUAL_STRING("out.txt", pl.stages[0].redir_out);
}

void test_empty_pipe_stage_error(void)
{
    char line[] = "ls | | grep foo";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(-1, parse_pipeline(line, &pl));
}

void test_redirect_missing_filename(void)
{
    char line[] = "echo hello >";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(-1, parse_pipeline(line, &pl));
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
    RUN_TEST(test_pipeline_single_command);
    RUN_TEST(test_pipeline_two_stages);
    RUN_TEST(test_pipeline_three_stages);
    RUN_TEST(test_redirect_output);
    RUN_TEST(test_redirect_append);
    RUN_TEST(test_redirect_input);
    RUN_TEST(test_redirect_both);
    RUN_TEST(test_empty_pipe_stage_error);
    RUN_TEST(test_redirect_missing_filename);
    return UNITY_END();
}
