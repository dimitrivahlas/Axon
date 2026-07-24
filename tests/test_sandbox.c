#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
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

/* --- Timeout enforcement (Step 8) --- */

void test_timeout_allows_fast_command(void)
{
    /* A command that finishes well within the limit runs normally */
    sandbox_opts_t opts = {0};
    opts.timeout_ms = 5000;

    cmd_result_t result;
    int rc = sandbox_execute("true", &opts, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_timeout_kills_slow_command(void)
{
    /*
     * A command that outlives its timeout is SIGKILLed. The invocation still
     * succeeds (rc 0); the timeout surfaces as exit code 124 plus a notice in
     * the captured stderr. elapsed_ms must be far below the command's own
     * 5 s sleep, proving it was cut short rather than run to completion.
     */
    sandbox_opts_t opts = {0};
    opts.timeout_ms = 200;

    cmd_result_t result;
    int rc = sandbox_execute("sleep 5", &opts, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(SANDBOX_TIMEOUT_EXIT, result.exit_code);
    TEST_ASSERT_NOT_NULL(strstr(result.stderr_capture, "timed out"));
    TEST_ASSERT_TRUE(result.elapsed_ms < 3000.0);
}

void test_timeout_zero_is_unlimited(void)
{
    /*
     * timeout_ms == 0 means no limit: a command that sleeps for a second
     * still runs to completion and exits 0 (never reported as a timeout).
     */
    sandbox_opts_t opts = {0};
    opts.timeout_ms = 0;

    cmd_result_t result;
    int rc = sandbox_execute("sleep 1", &opts, &result);
    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
    TEST_ASSERT_TRUE(result.elapsed_ms >= 1000.0);
}

/* --- Read-only working directory (Step 9) --- */

/*
 * The read-only checks run inside a throwaway directory that THIS process
 * owns, then chdir into it. That matters: the sandbox maps the caller's uid
 * to root inside the namespace, so a caller-owned cwd is writable there by
 * default — which is exactly what lets us prove the read-only mount (EROFS),
 * rather than accidentally observing a permission error (EACCES) on a
 * directory owned by some other uid. saved_cwd is restored in teardown.
 */
static char ro_tmpdir[PATH_MAX];
static char ro_saved_cwd[PATH_MAX];
static int ro_entered;

static int ro_enter_owned_cwd(void)
{
    strcpy(ro_tmpdir, "/tmp/axon_ro_XXXXXX");
    if (mkdtemp(ro_tmpdir) == NULL)
        return -1;
    if (getcwd(ro_saved_cwd, sizeof(ro_saved_cwd)) == NULL)
        return -1;
    if (chdir(ro_tmpdir) != 0)
        return -1;
    ro_entered = 1;
    return 0;
}

static void ro_leave_owned_cwd(void)
{
    if (!ro_entered)
        return;
    /* Remove any probe files a test may have left, then the dir itself. */
    remove("axon_probe");
    if (chdir(ro_saved_cwd) == 0)
        rmdir(ro_tmpdir);
    ro_entered = 0;
}

void test_readonly_blocks_write(void)
{
    /*
     * With read_only_cwd set, creating a file in the cwd must fail because
     * the filesystem is read-only. We assert on the EROFS message, not just a
     * nonzero exit, so the test can't pass on an unrelated permission error.
     */
    TEST_ASSERT_EQUAL_INT(0, ro_enter_owned_cwd());

    sandbox_opts_t opts = {0};
    opts.read_only_cwd = 1;

    cmd_result_t result;
    int rc = sandbox_execute("touch axon_probe", &opts, &result);

    if (!have_userns) {
        ro_leave_owned_cwd();
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    ro_leave_owned_cwd();
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.exit_code != 0);
    TEST_ASSERT_NOT_NULL(strstr(result.stderr_capture, "Read-only"));
}

void test_readonly_allows_read(void)
{
    /* Reads from the cwd still succeed under a read-only mount. */
    TEST_ASSERT_EQUAL_INT(0, ro_enter_owned_cwd());

    sandbox_opts_t opts = {0};
    opts.read_only_cwd = 1;

    cmd_result_t result;
    int rc = sandbox_execute("ls . > /dev/null", &opts, &result);

    if (!have_userns) {
        ro_leave_owned_cwd();
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    ro_leave_owned_cwd();
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_readonly_unset_allows_write(void)
{
    /* With the flag unset (default), writes to the cwd still work. */
    TEST_ASSERT_EQUAL_INT(0, ro_enter_owned_cwd());

    sandbox_opts_t opts = {0};
    opts.read_only_cwd = 0;

    cmd_result_t result;
    int rc = sandbox_execute("touch axon_probe", &opts, &result);

    if (!have_userns) {
        ro_leave_owned_cwd();
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    ro_leave_owned_cwd();
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
}

void test_readonly_host_cwd_unaffected(void)
{
    /*
     * The read-only bind-remount must stay confined to the child's private
     * mount namespace: after a read-only sandbox run, this (host) process
     * must still be able to write to its own cwd.
     */
    TEST_ASSERT_EQUAL_INT(0, ro_enter_owned_cwd());

    sandbox_opts_t opts = {0};
    opts.read_only_cwd = 1;

    cmd_result_t result;
    int rc = sandbox_execute("true", &opts, &result);
    if (have_userns)
        TEST_ASSERT_EQUAL_INT(0, rc);
    else
        TEST_ASSERT_EQUAL_INT(-1, rc);

    /* Host cwd must remain writable regardless of namespace support. */
    FILE *f = fopen("axon_probe", "w");
    int host_writable = (f != NULL);
    if (f != NULL)
        fclose(f);
    ro_leave_owned_cwd();
    TEST_ASSERT_TRUE(host_writable);
}

/* --- Resource limits (Step 10) --- */

void test_rlimit_fsize_caps_file(void)
{
    /*
     * RLIMIT_FSIZE caps a written file at 64 MB. Ask dd for 100 MB into a
     * directory we own (writable in the sandbox): the write must fail and the
     * resulting file must be no larger than the cap.
     */
    TEST_ASSERT_EQUAL_INT(0, ro_enter_owned_cwd());

    cmd_result_t result;
    int rc = sandbox_execute(
        "dd if=/dev/zero of=axon_probe bs=1048576 count=100", NULL, &result);

    if (!have_userns) {
        ro_leave_owned_cwd();
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }

    struct stat st;
    int have_st = (stat("axon_probe", &st) == 0);
    long size = have_st ? (long)st.st_size : -1;
    ro_leave_owned_cwd();

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.exit_code != 0);          /* write hit the cap */
    TEST_ASSERT_TRUE(have_st);
    TEST_ASSERT_TRUE(size <= 64 * 1024 * 1024);       /* 64 MB cap */
}

void test_rlimit_nofile_caps_fds(void)
{
    /*
     * RLIMIT_NOFILE caps open descriptors at 256. Opening past that must fail
     * before the loop reaches "ALLOPEN".
     */
    cmd_result_t result;
    int rc = sandbox_execute(
        "sh -c 'n=3; while [ $n -lt 300 ]; do eval \"exec ${n}>/dev/null\" "
        "|| exit 1; n=$((n+1)); done; echo ALLOPEN 1>&2'", NULL, &result);

    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.exit_code != 0);
    TEST_ASSERT_NULL(strstr(result.stderr_capture, "ALLOPEN"));
}

void test_rlimit_as_caps_memory(void)
{
    /*
     * RLIMIT_AS caps address space at 512 MB. dd asking for a 600 MB buffer
     * cannot allocate it and fails with an out-of-memory error.
     */
    cmd_result_t result;
    int rc = sandbox_execute(
        "dd if=/dev/zero of=/dev/null bs=600M count=1", NULL, &result);

    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.exit_code != 0);
    TEST_ASSERT_NOT_NULL(strstr(result.stderr_capture, "memory"));
}

void test_rlimit_nproc_value_set(void)
{
    /*
     * RLIMIT_NPROC is set to 64. It is best-effort (the kernel does not
     * enforce it inside a nested user namespace — see the caveat in
     * sandbox.c), so this asserts the limit VALUE is applied rather than
     * actual fork failure.
     */
    cmd_result_t result;
    int rc = sandbox_execute("echo nproc=$(ulimit -u) 1>&2", NULL, &result);

    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
    TEST_ASSERT_NOT_NULL(strstr(result.stderr_capture, "nproc=64"));
}

void test_capabilities_dropped(void)
{
    /*
     * The child sheds all capabilities before exec, so the command runs with
     * an empty effective set.
     */
    cmd_result_t result;
    int rc = sandbox_execute("grep CapEff /proc/self/status 1>&2", NULL, &result);

    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
    TEST_ASSERT_NOT_NULL(strstr(result.stderr_capture, "CapEff:\t0000000000000000"));
}

void test_rlimit_normal_command_succeeds(void)
{
    /* A modest command well within every limit still runs normally. */
    cmd_result_t result;
    int rc = sandbox_execute("for i in 1 2 3 4 5; do true; done; echo ok 1>&2",
                             NULL, &result);

    if (!have_userns) {
        TEST_ASSERT_EQUAL_INT(-1, rc);
        return;
    }
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(0, result.exit_code);
    TEST_ASSERT_NOT_NULL(strstr(result.stderr_capture, "ok"));
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
    RUN_TEST(test_timeout_allows_fast_command);
    RUN_TEST(test_timeout_kills_slow_command);
    RUN_TEST(test_timeout_zero_is_unlimited);
    RUN_TEST(test_readonly_blocks_write);
    RUN_TEST(test_readonly_allows_read);
    RUN_TEST(test_readonly_unset_allows_write);
    RUN_TEST(test_readonly_host_cwd_unaffected);
    RUN_TEST(test_rlimit_fsize_caps_file);
    RUN_TEST(test_rlimit_nofile_caps_fds);
    RUN_TEST(test_rlimit_as_caps_memory);
    RUN_TEST(test_rlimit_nproc_value_set);
    RUN_TEST(test_capabilities_dropped);
    RUN_TEST(test_rlimit_normal_command_succeeds);
    return UNITY_END();
}
