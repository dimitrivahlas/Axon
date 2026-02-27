#define _POSIX_C_SOURCE 200809L
#include "unity.h"
#include "../context/context.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_init_and_shutdown(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());
    TEST_ASSERT_NOT_NULL(context_session_id());
    /* Session ID should be a non-empty string */
    TEST_ASSERT_TRUE(strlen(context_session_id()) > 0);
    context_shutdown();
    TEST_ASSERT_NULL(context_session_id());
}

void test_double_init_is_safe(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());
    const char *id1 = context_session_id();
    TEST_ASSERT_NOT_NULL(id1);

    /* Second init should return success and keep same session */
    TEST_ASSERT_EQUAL_INT(0, context_init());
    const char *id2 = context_session_id();
    TEST_ASSERT_EQUAL_STRING(id1, id2);

    context_shutdown();
}

void test_add_before_init(void)
{
    /* Should fail gracefully, not crash */
    TEST_ASSERT_EQUAL_INT(-1, context_add("ls", 0, 1.0, "/tmp", ""));
}

void test_add_command(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());
    TEST_ASSERT_EQUAL_INT(0, context_add("ls -la", 0, 1.5, "/tmp", ""));
    TEST_ASSERT_EQUAL_INT(0, context_add("gcc main.c", 1, 200.0, "/tmp", "error: ..."));
    TEST_ASSERT_EQUAL_INT(0, context_add("echo hello", 0, 0.5, "/home", ""));
    context_shutdown();
}

void test_add_with_null_fields(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());
    /* NULL stderr should not crash */
    TEST_ASSERT_EQUAL_INT(0, context_add("test", 0, 1.0, "/tmp", NULL));
    /* NULL command should not crash */
    TEST_ASSERT_EQUAL_INT(0, context_add(NULL, 0, 1.0, "/tmp", ""));
    context_shutdown();
}

void test_session_id_unique(void)
{
    /* Two separate init/shutdown cycles should produce different session IDs */
    TEST_ASSERT_EQUAL_INT(0, context_init());
    const char *raw = context_session_id();
    char id1[64];
    strncpy(id1, raw, sizeof(id1) - 1);
    id1[sizeof(id1) - 1] = '\0';
    context_shutdown();

    TEST_ASSERT_EQUAL_INT(0, context_init());
    const char *id2 = context_session_id();
    TEST_ASSERT_NOT_NULL(id2);
    /* Very unlikely to collide */
    TEST_ASSERT_TRUE(strcmp(id1, id2) != 0);
    context_shutdown();
}

void test_build_ai_json_not_initialized(void)
{
    /* Should return NULL when context not initialized */
    char *json = context_build_ai_json("/tmp", 0);
    TEST_ASSERT_NULL(json);
}

void test_build_ai_json_empty(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());
    char *json = context_build_ai_json("/tmp", 5);
    TEST_ASSERT_NOT_NULL(json);

    /* Should contain required keys */
    TEST_ASSERT_NOT_NULL(strstr(json, "\"session_id\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"recent_commands\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"error_summary\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"cwd\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"/tmp\""));

    free(json);
    context_shutdown();
}

void test_build_ai_json_with_commands(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());
    TEST_ASSERT_EQUAL_INT(0, context_add("make build", 0, 1200.5, "/home/user/project", ""));
    TEST_ASSERT_EQUAL_INT(0, context_add("gcc main.c", 1, 350.0, "/home/user/project", "error: undefined ref"));

    char *json = context_build_ai_json("/tmp", 10);
    TEST_ASSERT_NOT_NULL(json);

    /* Commands should appear in the JSON */
    TEST_ASSERT_NOT_NULL(strstr(json, "make build"));
    TEST_ASSERT_NOT_NULL(strstr(json, "gcc main.c"));
    TEST_ASSERT_NOT_NULL(strstr(json, "error: undefined ref"));

    /* Should have exit codes */
    TEST_ASSERT_NOT_NULL(strstr(json, "\"exit_code\": 0"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"exit_code\": 1"));

    /* error_summary should show at least 1 failure (from gcc command) */
    TEST_ASSERT_NOT_NULL(strstr(json, "\"recent_failures\":"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"out_of_last\": 10"));

    free(json);
    context_shutdown();
}

void test_build_ai_json_includes_session_id(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());
    const char *sid = context_session_id();
    TEST_ASSERT_NOT_NULL(sid);

    char *json = context_build_ai_json("/tmp", 0);
    TEST_ASSERT_NOT_NULL(json);

    /* The session ID should appear in the JSON output */
    TEST_ASSERT_NOT_NULL(strstr(json, sid));

    free(json);
    context_shutdown();
}

void test_build_ai_json_default_max_entries(void)
{
    TEST_ASSERT_EQUAL_INT(0, context_init());

    /* Add 25 commands */
    int i;
    for (i = 0; i < 25; i++) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "cmd_%d", i);
        TEST_ASSERT_EQUAL_INT(0, context_add(cmd, 0, 1.0, "/tmp", ""));
    }

    /* max_entries=0 should use default (20), so cmd_0 through cmd_4 should be excluded */
    char *json = context_build_ai_json("/tmp", 0);
    TEST_ASSERT_NOT_NULL(json);

    /* cmd_24 (most recent within top 20) should be present */
    TEST_ASSERT_NOT_NULL(strstr(json, "cmd_24"));
    /* cmd_5 should be present (5th oldest = within the 20 most recent) */
    TEST_ASSERT_NOT_NULL(strstr(json, "cmd_5"));

    free(json);
    context_shutdown();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_and_shutdown);
    RUN_TEST(test_double_init_is_safe);
    RUN_TEST(test_add_before_init);
    RUN_TEST(test_add_command);
    RUN_TEST(test_add_with_null_fields);
    RUN_TEST(test_session_id_unique);
    RUN_TEST(test_build_ai_json_not_initialized);
    RUN_TEST(test_build_ai_json_empty);
    RUN_TEST(test_build_ai_json_with_commands);
    RUN_TEST(test_build_ai_json_includes_session_id);
    RUN_TEST(test_build_ai_json_default_max_entries);
    return UNITY_END();
}
