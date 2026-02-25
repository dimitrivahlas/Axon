#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "../context/storage.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  test_%s: ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

static const char *tmp_db()
{
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/axon_test_%d.db", getpid());
    return path;
}

static void cleanup_db()
{
    unlink(tmp_db());
    /* WAL and SHM files */
    char wal[270], shm[270];
    snprintf(wal, sizeof(wal), "%s-wal", tmp_db());
    snprintf(shm, sizeof(shm), "%s-shm", tmp_db());
    unlink(wal);
    unlink(shm);
}

void test_open_and_close()
{
    tests_run++;
    Storage s;
    assert(!s.is_open());
    assert(s.open(tmp_db()) == 0);
    assert(s.is_open());
    s.close();
    assert(!s.is_open());
    cleanup_db();
}

void test_create_schema()
{
    tests_run++;
    Storage s;
    assert(s.open(tmp_db()) == 0);
    assert(s.create_schema() == 0);
    /* Calling again should be idempotent */
    assert(s.create_schema() == 0);
    s.close();
    cleanup_db();
}

void test_insert_and_query()
{
    tests_run++;
    Storage s;
    assert(s.open(tmp_db()) == 0);
    assert(s.create_schema() == 0);

    assert(s.insert_command("sess1", "ls -la", 0, 1.5, "/tmp", "") == 0);
    assert(s.insert_command("sess1", "gcc main.c", 1, 200.0, "/tmp", "error: ...") == 0);
    assert(s.insert_command("sess1", "echo hello", 0, 0.5, "/home", "") == 0);

    auto rows = s.query_recent(10);
    assert(rows.size() == 3);
    /* Should be in chronological order (oldest first) */
    assert(rows[0].command == "ls -la");
    assert(rows[1].command == "gcc main.c");
    assert(rows[2].command == "echo hello");

    /* Check fields */
    assert(rows[0].exit_code == 0);
    assert(rows[1].exit_code == 1);
    assert(rows[1].stderr_out == "error: ...");
    assert(rows[0].session_id == "sess1");

    s.close();
    cleanup_db();
}

void test_query_limit()
{
    tests_run++;
    Storage s;
    assert(s.open(tmp_db()) == 0);
    assert(s.create_schema() == 0);

    for (int i = 0; i < 10; i++) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "cmd_%d", i);
        assert(s.insert_command("sess1", cmd, 0, 1.0, "/tmp", "") == 0);
    }

    auto rows = s.query_recent(3);
    assert(rows.size() == 3);
    /* Should be the 3 most recent, in chronological order */
    assert(rows[0].command == "cmd_7");
    assert(rows[1].command == "cmd_8");
    assert(rows[2].command == "cmd_9");

    s.close();
    cleanup_db();
}

void test_query_by_cwd()
{
    tests_run++;
    Storage s;
    assert(s.open(tmp_db()) == 0);
    assert(s.create_schema() == 0);

    assert(s.insert_command("sess1", "make build", 0, 100.0, "/project", "") == 0);
    assert(s.insert_command("sess1", "ls", 0, 1.0, "/home", "") == 0);
    assert(s.insert_command("sess1", "make test", 0, 50.0, "/project", "") == 0);

    auto rows = s.query_recent_by_cwd("/project", 10);
    assert(rows.size() == 2);
    assert(rows[0].command == "make build");
    assert(rows[1].command == "make test");

    s.close();
    cleanup_db();
}

void test_count_failures()
{
    tests_run++;
    Storage s;
    assert(s.open(tmp_db()) == 0);
    assert(s.create_schema() == 0);

    assert(s.insert_command("sess1", "true", 0, 1.0, "/tmp", "") == 0);
    assert(s.insert_command("sess1", "false", 1, 1.0, "/tmp", "") == 0);
    assert(s.insert_command("sess1", "gcc bad.c", 1, 1.0, "/tmp", "error") == 0);
    assert(s.insert_command("sess1", "ls", 0, 1.0, "/tmp", "") == 0);

    assert(s.count_recent_failures(10) == 2);
    assert(s.count_recent_failures(2) == 1); /* last 2: gcc bad.c (fail), ls (ok) */

    s.close();
    cleanup_db();
}

void test_sessions()
{
    tests_run++;
    Storage s;
    assert(s.open(tmp_db()) == 0);
    assert(s.create_schema() == 0);

    assert(s.insert_session("abc123", 1700000000, "/home/user") == 0);
    assert(s.get_session_start("abc123") == 1700000000);
    assert(s.get_session_start("nonexistent") == 0);
    assert(s.end_session("abc123", 1700003600) == 0);

    s.close();
    cleanup_db();
}

void test_empty_query()
{
    tests_run++;
    Storage s;
    assert(s.open(tmp_db()) == 0);
    assert(s.create_schema() == 0);

    auto rows = s.query_recent(10);
    assert(rows.empty());

    assert(s.count_recent_failures(10) == 0);

    s.close();
    cleanup_db();
}

void test_operations_without_open()
{
    tests_run++;
    Storage s;
    /* All operations should fail gracefully when DB is not open */
    assert(s.create_schema() == -1);
    assert(s.insert_command("s", "cmd", 0, 1.0, "/", "") == -1);
    assert(s.query_recent(10).empty());
    assert(s.count_recent_failures(10) == 0);
    assert(s.insert_session("s", 0, "/") == -1);
    assert(s.end_session("s", 0) == -1);
    assert(s.get_session_start("s") == 0);
}

int main()
{
    printf("=== Storage Tests ===\n");

    TEST(open_and_close);
    TEST(create_schema);
    TEST(insert_and_query);
    TEST(query_limit);
    TEST(query_by_cwd);
    TEST(count_failures);
    TEST(sessions);
    TEST(empty_query);
    TEST(operations_without_open);

    printf("\n-----------------------\n");
    printf("%d Tests %d Failures 0 Ignored\n", tests_passed, tests_run - tests_passed);
    printf("OK\n");

    return 0;
}
