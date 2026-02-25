#include "context.h"
#include "storage.h"
#include "git_context.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static Storage g_storage;
static std::string g_session_id;
static bool g_initialized = false;

/* Ensure ~/.axon/ directory exists. Returns 0 on success. */
static int ensure_axon_dir(std::string &dir_out)
{
    const char *home = getenv("HOME");
    if (!home || home[0] == '\0')
        return -1;

    dir_out = std::string(home) + "/.axon";

    struct stat st;
    if (stat(dir_out.c_str(), &st) == 0)
        return 0; /* already exists */

    if (mkdir(dir_out.c_str(), 0755) != 0) {
        fprintf(stderr, "axon: failed to create %s\n", dir_out.c_str());
        return -1;
    }

    return 0;
}

/* Generate a simple hex session ID from /dev/urandom (8 bytes = 16 hex chars).
 * Falls back to time-based ID if urandom fails. */
static std::string generate_session_id()
{
    unsigned char buf[8];
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp) {
        size_t n = fread(buf, 1, sizeof(buf), fp);
        fclose(fp);
        if (n == sizeof(buf)) {
            char hex[17];
            for (int i = 0; i < 8; i++)
                snprintf(hex + i * 2, 3, "%02x", buf[i]);
            return std::string(hex);
        }
    }

    /* Fallback: pid + time */
    char fallback[32];
    snprintf(fallback, sizeof(fallback), "%x%lx",
             static_cast<unsigned>(getpid()), static_cast<unsigned long>(time(nullptr)));
    return std::string(fallback);
}

extern "C" {

int context_init(void)
{
    if (g_initialized)
        return 0;

    std::string axon_dir;
    if (ensure_axon_dir(axon_dir) != 0)
        return -1;

    std::string db_path = axon_dir + "/context.db";
    if (g_storage.open(db_path) != 0)
        return -1;

    if (g_storage.create_schema() != 0) {
        g_storage.close();
        return -1;
    }

    g_session_id = generate_session_id();

    /* Record session start */
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strncpy(cwd, "???", sizeof(cwd));

    int64_t now = static_cast<int64_t>(time(nullptr));
    g_storage.insert_session(g_session_id, now, cwd);

    g_initialized = true;
    return 0;
}

int context_add(const char *command, int exit_code, double elapsed_ms,
                const char *cwd, const char *stderr_output)
{
    if (!g_initialized || !g_storage.is_open())
        return -1;

    return g_storage.insert_command(
        g_session_id,
        command ? command : "",
        exit_code,
        elapsed_ms,
        cwd ? cwd : "",
        stderr_output ? stderr_output : ""
    );
}

char *context_build_ai_json(const char *, int)
{
    /* Stub — wired in Step 5 */
    const char *empty = "{}";
    char *result = static_cast<char *>(malloc(strlen(empty) + 1));
    if (result)
        strcpy(result, empty);
    return result;
}

void context_shutdown(void)
{
    if (g_initialized && g_storage.is_open()) {
        int64_t now = static_cast<int64_t>(time(nullptr));
        g_storage.end_session(g_session_id, now);
    }
    g_storage.close();
    g_session_id.clear();
    g_initialized = false;
}

const char *context_session_id(void)
{
    if (!g_initialized)
        return NULL;
    return g_session_id.c_str();
}

} /* extern "C" */
