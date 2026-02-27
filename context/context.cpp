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

/* Escape a string for safe embedding in JSON. */
static std::string json_escape_str(const char *src)
{
    std::string out;
    if (!src)
        return out;
    for (const char *p = src; *p; p++) {
        switch (*p) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(*p) < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(*p));
                out += buf;
            } else {
                out += *p;
            }
        }
    }
    return out;
}

char *context_build_ai_json(const char *cwd, int max_entries)
{
    if (!g_initialized)
        return NULL;

    if (max_entries <= 0)
        max_entries = 20;

    std::string json;
    json += "{\n";

    /* session_id */
    json += "  \"session_id\": \"" + json_escape_str(g_session_id.c_str()) + "\",\n";

    /* recent_commands */
    auto rows = g_storage.query_recent(max_entries);
    json += "  \"recent_commands\": [";
    for (size_t i = 0; i < rows.size(); i++) {
        if (i > 0)
            json += ",";
        json += "\n    {";
        json += "\"command\": \"" + json_escape_str(rows[i].command.c_str()) + "\", ";
        json += "\"exit_code\": " + std::to_string(rows[i].exit_code) + ", ";
        char elapsed[64];
        snprintf(elapsed, sizeof(elapsed), "%.1f", rows[i].elapsed_ms);
        json += "\"elapsed_ms\": " + std::string(elapsed) + ", ";
        json += "\"cwd\": \"" + json_escape_str(rows[i].cwd.c_str()) + "\", ";
        json += "\"stderr\": \"" + json_escape_str(rows[i].stderr_out.c_str()) + "\"";
        json += "}";
    }
    if (!rows.empty())
        json += "\n  ";
    json += "],\n";

    /* error_summary */
    int failures = g_storage.count_recent_failures(10);
    json += "  \"error_summary\": {\"recent_failures\": " + std::to_string(failures)
          + ", \"out_of_last\": 10},\n";

    /* git context (only if in a git repo) */
    std::string cwd_str = cwd ? cwd : "";
    if (!cwd_str.empty()) {
        GitContext ctx = git_gather_context(cwd_str);
        if (ctx.is_git_repo) {
            json += "  \"git\": {\n";
            json += "    \"branch\": \"" + json_escape_str(ctx.branch.c_str()) + "\",\n";
            json += "    \"is_dirty\": " + std::string(ctx.is_dirty ? "true" : "false") + ",\n";

            json += "    \"modified_files\": [";
            for (size_t i = 0; i < ctx.modified_files.size(); i++) {
                if (i > 0) json += ", ";
                json += "\"" + json_escape_str(ctx.modified_files[i].c_str()) + "\"";
            }
            json += "],\n";

            json += "    \"staged_files\": [";
            for (size_t i = 0; i < ctx.staged_files.size(); i++) {
                if (i > 0) json += ", ";
                json += "\"" + json_escape_str(ctx.staged_files[i].c_str()) + "\"";
            }
            json += "],\n";

            json += "    \"recent_commits\": [";
            for (size_t i = 0; i < ctx.recent_commits.size(); i++) {
                if (i > 0) json += ", ";
                json += "\"" + json_escape_str(ctx.recent_commits[i].c_str()) + "\"";
            }
            json += "]\n";

            json += "  },\n";
        }
    }

    /* cwd */
    json += "  \"cwd\": \"" + json_escape_str(cwd) + "\"\n";

    json += "}";

    return strdup(json.c_str());
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
