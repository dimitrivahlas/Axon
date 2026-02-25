#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
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

/* --- Env expansion tests --- */

void test_expand_env_var(void)
{
    setenv("AXON_TEST_VAR", "hello", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo $AXON_TEST_VAR", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("echo hello", out);
    unsetenv("AXON_TEST_VAR");
}

void test_expand_env_braces(void)
{
    setenv("AXON_TEST_VAR", "world", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo ${AXON_TEST_VAR}!", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("echo world!", out);
    unsetenv("AXON_TEST_VAR");
}

void test_expand_exit_code(void)
{
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo $?", out, sizeof(out), 42));
    TEST_ASSERT_EQUAL_STRING("echo 42", out);
}

void test_expand_single_quote_no_expand(void)
{
    setenv("AXON_TEST_VAR", "nope", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo '$AXON_TEST_VAR'", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("echo '$AXON_TEST_VAR'", out);
    unsetenv("AXON_TEST_VAR");
}

void test_expand_undefined_var(void)
{
    unsetenv("AXON_NONEXISTENT");
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo $AXON_NONEXISTENT end", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("echo  end", out);
}

void test_expand_no_vars(void)
{
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("ls -la /tmp", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("ls -la /tmp", out);
}

void test_expand_lone_dollar(void)
{
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo $ end", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("echo $ end", out);
}

/* --- Chain tests --- */

void test_chain_single_command(void)
{
    char line[] = "ls -la";
    chain_t chain;

    TEST_ASSERT_EQUAL_INT(0, parse_chain(line, &chain));
    TEST_ASSERT_EQUAL_INT(1, chain.count);
    TEST_ASSERT_EQUAL_INT(CHAIN_NONE, chain.entries[0].op);
}

void test_chain_and(void)
{
    char line[] = "echo hello && echo world";
    chain_t chain;

    TEST_ASSERT_EQUAL_INT(0, parse_chain(line, &chain));
    TEST_ASSERT_EQUAL_INT(2, chain.count);
    TEST_ASSERT_EQUAL_INT(CHAIN_NONE, chain.entries[0].op);
    TEST_ASSERT_EQUAL_INT(CHAIN_AND, chain.entries[1].op);
}

void test_chain_or(void)
{
    char line[] = "false || echo fallback";
    chain_t chain;

    TEST_ASSERT_EQUAL_INT(0, parse_chain(line, &chain));
    TEST_ASSERT_EQUAL_INT(2, chain.count);
    TEST_ASSERT_EQUAL_INT(CHAIN_NONE, chain.entries[0].op);
    TEST_ASSERT_EQUAL_INT(CHAIN_OR, chain.entries[1].op);
}

void test_chain_semicolon(void)
{
    char line[] = "echo a; echo b; echo c";
    chain_t chain;

    TEST_ASSERT_EQUAL_INT(0, parse_chain(line, &chain));
    TEST_ASSERT_EQUAL_INT(3, chain.count);
    TEST_ASSERT_EQUAL_INT(CHAIN_NONE, chain.entries[0].op);
    TEST_ASSERT_EQUAL_INT(CHAIN_SEMI, chain.entries[1].op);
    TEST_ASSERT_EQUAL_INT(CHAIN_SEMI, chain.entries[2].op);
}

void test_chain_mixed(void)
{
    char line[] = "echo a && echo b || echo c; echo d";
    chain_t chain;

    TEST_ASSERT_EQUAL_INT(0, parse_chain(line, &chain));
    TEST_ASSERT_EQUAL_INT(4, chain.count);
    TEST_ASSERT_EQUAL_INT(CHAIN_NONE, chain.entries[0].op);
    TEST_ASSERT_EQUAL_INT(CHAIN_AND, chain.entries[1].op);
    TEST_ASSERT_EQUAL_INT(CHAIN_OR, chain.entries[2].op);
    TEST_ASSERT_EQUAL_INT(CHAIN_SEMI, chain.entries[3].op);
}

void test_chain_respects_quotes(void)
{
    char line[] = "echo '&&' && echo done";
    chain_t chain;

    TEST_ASSERT_EQUAL_INT(0, parse_chain(line, &chain));
    TEST_ASSERT_EQUAL_INT(2, chain.count);
    /* First segment should contain the quoted && literally */
    TEST_ASSERT_NOT_NULL(strstr(chain.entries[0].segment, "'&&'"));
}

/* --- Pipe-in-quotes tests --- */

void test_pipeline_pipe_in_double_quotes(void)
{
    char line[] = "echo \"hello|world\"";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(1, pl.num_stages);
    TEST_ASSERT_EQUAL_INT(2, pl.stages[0].argc);
    TEST_ASSERT_EQUAL_STRING("echo", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello|world", pl.stages[0].argv[1]);
}

void test_pipeline_pipe_in_single_quotes(void)
{
    char line[] = "echo 'a|b' | grep a";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(2, pl.num_stages);
    TEST_ASSERT_EQUAL_STRING("echo", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("a|b", pl.stages[0].argv[1]);
    TEST_ASSERT_EQUAL_STRING("grep", pl.stages[1].argv[0]);
    TEST_ASSERT_EQUAL_STRING("a", pl.stages[1].argv[1]);
}

/* --- Tilde expansion tests --- */

void test_expand_tilde(void)
{
    setenv("HOME", "/root", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("cd ~", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("cd /root", out);
}

void test_expand_tilde_with_path(void)
{
    setenv("HOME", "/root", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("ls ~/projects", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("ls /root/projects", out);
}

void test_expand_tilde_in_single_quotes(void)
{
    setenv("HOME", "/root", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo '~'", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("echo '~'", out);
}

/* --- Edge case tests --- */

void test_max_args_boundary(void)
{
    /* Build a line with exactly AXON_MAX_ARGS arguments */
    char line[8192];
    int pos = 0;
    for (int i = 0; i < AXON_MAX_ARGS; i++) {
        if (i > 0) line[pos++] = ' ';
        line[pos++] = 'a' + (char)(i % 26);
    }
    line[pos] = '\0';

    command_t cmd;
    TEST_ASSERT_EQUAL_INT(0, parse_command(line, &cmd));
    TEST_ASSERT_EQUAL_INT(AXON_MAX_ARGS, cmd.argc);
    TEST_ASSERT_NULL(cmd.argv[AXON_MAX_ARGS]);
}

void test_redirect_in_pipeline_stage(void)
{
    /* Redirect output on last stage of a pipeline */
    char line[] = "cat file.txt | sort > sorted.txt";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(2, pl.num_stages);
    TEST_ASSERT_EQUAL_STRING("cat", pl.stages[0].argv[0]);
    TEST_ASSERT_EQUAL_STRING("file.txt", pl.stages[0].argv[1]);
    TEST_ASSERT_NULL(pl.stages[0].redir_out);
    TEST_ASSERT_EQUAL_STRING("sort", pl.stages[1].argv[0]);
    TEST_ASSERT_EQUAL_STRING("sorted.txt", pl.stages[1].redir_out);
}

void test_redirect_input_in_pipeline(void)
{
    /* Redirect input on first stage of a pipeline */
    char line[] = "sort < data.txt | uniq";
    pipeline_t pl;

    TEST_ASSERT_EQUAL_INT(0, parse_pipeline(line, &pl));
    TEST_ASSERT_EQUAL_INT(2, pl.num_stages);
    TEST_ASSERT_EQUAL_STRING("data.txt", pl.stages[0].redir_in);
    TEST_ASSERT_EQUAL_STRING("uniq", pl.stages[1].argv[0]);
}

void test_chain_empty_segment(void)
{
    /* Trailing semicolon — last segment is empty, should be harmless */
    char line[] = "echo hello;";
    chain_t chain;

    TEST_ASSERT_EQUAL_INT(0, parse_chain(line, &chain));
    TEST_ASSERT_EQUAL_INT(2, chain.count);
}

void test_expand_multiple_vars(void)
{
    setenv("AXON_A", "foo", 1);
    setenv("AXON_B", "bar", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("$AXON_A $AXON_B", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("foo bar", out);
    unsetenv("AXON_A");
    unsetenv("AXON_B");
}

void test_expand_var_in_double_quotes(void)
{
    setenv("AXON_TEST_VAR", "hello", 1);
    char out[4096];
    TEST_ASSERT_EQUAL_INT(0, expand_env("echo \"$AXON_TEST_VAR\"", out, sizeof(out), 0));
    TEST_ASSERT_EQUAL_STRING("echo \"hello\"", out);
    unsetenv("AXON_TEST_VAR");
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
    RUN_TEST(test_expand_env_var);
    RUN_TEST(test_expand_env_braces);
    RUN_TEST(test_expand_exit_code);
    RUN_TEST(test_expand_single_quote_no_expand);
    RUN_TEST(test_expand_undefined_var);
    RUN_TEST(test_expand_no_vars);
    RUN_TEST(test_expand_lone_dollar);
    RUN_TEST(test_chain_single_command);
    RUN_TEST(test_chain_and);
    RUN_TEST(test_chain_or);
    RUN_TEST(test_chain_semicolon);
    RUN_TEST(test_chain_mixed);
    RUN_TEST(test_chain_respects_quotes);
    RUN_TEST(test_pipeline_pipe_in_double_quotes);
    RUN_TEST(test_pipeline_pipe_in_single_quotes);
    RUN_TEST(test_expand_tilde);
    RUN_TEST(test_expand_tilde_with_path);
    RUN_TEST(test_expand_tilde_in_single_quotes);
    RUN_TEST(test_max_args_boundary);
    RUN_TEST(test_redirect_in_pipeline_stage);
    RUN_TEST(test_redirect_input_in_pipeline);
    RUN_TEST(test_chain_empty_segment);
    RUN_TEST(test_expand_multiple_vars);
    RUN_TEST(test_expand_var_in_double_quotes);
    return UNITY_END();
}
