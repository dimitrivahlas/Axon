#include "context.h"
#include "storage.h"
#include "git_context.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static Storage g_storage;

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

extern "C" {

int context_init(void)
{
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

    return 0;
}

int context_add(const char *, int, double, const char *, const char *)
{
    /* Stub — wired in Step 3 */
    return 0;
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
    g_storage.close();
}

const char *context_session_id(void)
{
    /* Stub — wired in Step 3 */
    return NULL;
}

} /* extern "C" */
