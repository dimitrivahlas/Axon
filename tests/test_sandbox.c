#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "unity.h"
#include "../sandbox/sandbox.h"

/*
 * Whether this kernel allows unprivileged user namespaces (probed in main).
 * When it does not (e.g. Docker's default seccomp profile), the sandbox
 * must FAIL CLOSED: sandbox_execute() returns -1 and the command never runs.
 * Every functional test asserts one branch or the other, so the suite is
 * meaningful in both environments.
 */
static int have_userns;

void setUp(void) {}
void tearDown(void) {}

/* Probe namespace support in a throwaway child so the test process
 * itself is not moved into new namespaces. */
static int userns_available(void)
{
    pid_t pid = fork();
    if (pid < 0)
        return 0;
    if (pid == 0)
        _exit(unshare(CLONE_NEWUSER | CLONE_NEWPID) == 0 ? 0 : 1);

    int status;
    if (waitpid(pid, &status, 0) < 0)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* --- Argument validation (environment-independent) --- */

void test_null_command_fails(void)
{
    cmd_result_t result;
    TEST_ASSERT_EQUAL_INT(-1, sandbox_execute(NULL, NULL, &result));
}

void test_null_result_fails(void)
{
    TEST_ASSERT_EQUAL_INT(-1, sandbox_execute("true", NULL, NULL));
}

/* --- Functional tests (assert fail-closed when namespaces unavailable) --- */

void test_true_exits_zero(void)
{
    cmd_result_t result;
    int rc = sandbox_execute("true", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_false_exits_one(void)
{
    cmd_result_t result;
    int rc = sandbox_execute("false", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, result.exit_code);
}

void test_exit_code_propagated(void)
{
    cmd_result_t result;
    int rc = sandbox_execute("exit 42", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(42, result.exit_code);
}

void test_stderr_is_captured(void)
{
    cmd_result_t result;
    int rc = sandbox_execute("echo sandbox_boom 1>&2", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(strstr(result.stderr_capture, "sandbox_boom"));
}

void test_runs_in_pid_namespace(void)
{
    /* Inside a new PID namespace the shell is PID 1 */
    cmd_result_t result;
    int rc = sandbox_execute("test $$ -eq 1", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_uid_mapped_to_root(void)
{
    /* The user namespace maps the caller's UID to 0 inside the sandbox */
    cmd_result_t result;
    int rc = sandbox_execute("test \"$(id -u)\" -eq 0", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_signal_death_reported(void)
{
    /*
     * Killed by SIGKILL -> exit code 128 + 9. The sandboxed shell is PID 1
     * in its namespace and PID 1 ignores unhandled signals, so the victim
     * must be a forked subshell. The trailing "exit $?" also prevents the
     * shell from exec-optimizing the subshell into PID 1.
     */
    cmd_result_t result;
    int rc = sandbox_execute("sh -c 'kill -9 $$'; exit $?", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(137, result.exit_code);
}

void test_network_isolated_by_default(void)
{
    /*
     * A fresh network namespace has no route to the host's real interface
     * (eth0). Some kernels auto-populate every new netns with zero-traffic
     * pseudo devices (tunl0, gre0, sit0, ...) via pernet init, so the
     * assertion is "no eth0", not "no extra devices at all". Checked via
     * /proc/net/dev (not /sys/class/net) because sysfs's net class listing
     * is bound to the netns the sysfs superblock was mounted in, not the
     * reading task's current netns, while /proc/net/dev is generated
     * per-task and reflects the sandboxed process's own namespace.
     * stdout is redirected to stderr since sandbox_execute only captures
     * stderr.
     */
    cmd_result_t result;
    int rc = sandbox_execute("cat /proc/net/dev 1>&2", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NULL(strstr(result.stderr_capture, "eth0"));
}

void test_network_allowed_when_opted_in(void)
{
    /* With allow_network set, the sandbox shares the host's netns */
    sandbox_opts_t opts = {0};
    opts.allow_network = 1;

    cmd_result_t result;
    int rc = sandbox_execute("true", &opts, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_mount_namespace_isolated(void)
{
    /* The sandboxed child must see a different mount namespace than us */
    char host_ns[64] = {0};
    ssize_t n = readlink("/proc/self/ns/mnt", host_ns, sizeof(host_ns) - 1);
    TEST_ASSERT_TRUE(n > 0);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "test \"$(readlink /proc/self/ns/mnt)\" != \"%s\"",
             host_ns);

    cmd_result_t result;
    int rc = sandbox_execute(cmd, NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_timing_is_captured(void)
{
    cmd_result_t result;
    int rc = sandbox_execute("true", NULL, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.elapsed_ms > 0.0);
    TEST_ASSERT_TRUE(result.elapsed_ms < 5000.0);
}

int main(void)
{
    have_userns = userns_available();
    if (!have_userns)
        printf("NOTE: unprivileged user namespaces unavailable — "
               "asserting fail-closed behavior only\n");

    UNITY_BEGIN();
    RUN_TEST(test_null_command_fails);
    RUN_TEST(test_null_result_fails);
    RUN_TEST(test_true_exits_zero);
    RUN_TEST(test_false_exits_one);
    RUN_TEST(test_exit_code_propagated);
    RUN_TEST(test_stderr_is_captured);
    RUN_TEST(test_runs_in_pid_namespace);
    RUN_TEST(test_uid_mapped_to_root);
    RUN_TEST(test_signal_death_reported);
    RUN_TEST(test_network_isolated_by_default);
    RUN_TEST(test_network_allowed_when_opted_in);
    RUN_TEST(test_mount_namespace_isolated);
    RUN_TEST(test_timing_is_captured);
    return UNITY_END();
}
