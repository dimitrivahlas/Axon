#include "storage.h"
#include <cstdio>
#include <ctime>
#include <algorithm>

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS commands ("
    "    id          INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    session_id  TEXT NOT NULL,"
    "    timestamp   INTEGER NOT NULL,"
    "    command     TEXT NOT NULL,"
    "    exit_code   INTEGER NOT NULL,"
    "    elapsed_ms  REAL NOT NULL,"
    "    cwd         TEXT NOT NULL,"
    "    stderr_out  TEXT"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_commands_session ON commands(session_id);"
    "CREATE INDEX IF NOT EXISTS idx_commands_cwd ON commands(cwd);"
    "CREATE INDEX IF NOT EXISTS idx_commands_timestamp ON commands(timestamp);"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "    session_id  TEXT PRIMARY KEY,"
    "    started_at  INTEGER NOT NULL,"
    "    ended_at    INTEGER,"
    "    initial_cwd TEXT NOT NULL"
    ");";

Storage::Storage() : db_(nullptr) {}

Storage::~Storage()
{
    close();
}

int Storage::open(const std::string &db_path)
{
    if (db_)
        close();

    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "axon: sqlite3_open failed: %s\n", sqlite3_errmsg(db_));
        db_ = nullptr;
        return -1;
    }

    /* WAL mode for better concurrent access */
    char *err = nullptr;
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err);
    if (err)
        sqlite3_free(err);

    return 0;
}

void Storage::close()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

int Storage::create_schema()
{
    if (!db_)
        return -1;

    char *err = nullptr;
    int rc = sqlite3_exec(db_, SCHEMA_SQL, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "axon: schema creation failed: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    return 0;
}

int Storage::insert_command(const std::string &session_id, const std::string &command,
                            int exit_code, double elapsed_ms, const std::string &cwd,
                            const std::string &stderr_out)
{
    if (!db_)
        return -1;

    const char *sql =
        "INSERT INTO commands (session_id, timestamp, command, exit_code, elapsed_ms, cwd, stderr_out) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        return -1;

    int64_t now = static_cast<int64_t>(time(nullptr));

    sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, command.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, exit_code);
    sqlite3_bind_double(stmt, 5, elapsed_ms);
    sqlite3_bind_text(stmt, 6, cwd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, stderr_out.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

std::vector<CommandRow> Storage::query_recent(int max_entries)
{
    std::vector<CommandRow> results;
    if (!db_)
        return results;

    const char *sql =
        "SELECT id, session_id, timestamp, command, exit_code, elapsed_ms, cwd, stderr_out "
        "FROM commands ORDER BY timestamp DESC, id DESC LIMIT ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return results;

    sqlite3_bind_int(stmt, 1, max_entries);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CommandRow row;
        row.id = sqlite3_column_int(stmt, 0);
        row.session_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        row.timestamp = sqlite3_column_int64(stmt, 2);
        row.command = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        row.exit_code = sqlite3_column_int(stmt, 4);
        row.elapsed_ms = sqlite3_column_double(stmt, 5);
        row.cwd = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        const char *se = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        row.stderr_out = se ? se : "";
        results.push_back(row);
    }

    sqlite3_finalize(stmt);

    /* Reverse so oldest is first (chronological order) */
    std::reverse(results.begin(), results.end());
    return results;
}

std::vector<CommandRow> Storage::query_recent_by_cwd(const std::string &cwd, int max_entries)
{
    std::vector<CommandRow> results;
    if (!db_)
        return results;

    const char *sql =
        "SELECT id, session_id, timestamp, command, exit_code, elapsed_ms, cwd, stderr_out "
        "FROM commands WHERE cwd = ? ORDER BY timestamp DESC, id DESC LIMIT ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return results;

    sqlite3_bind_text(stmt, 1, cwd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, max_entries);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CommandRow row;
        row.id = sqlite3_column_int(stmt, 0);
        row.session_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        row.timestamp = sqlite3_column_int64(stmt, 2);
        row.command = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        row.exit_code = sqlite3_column_int(stmt, 4);
        row.elapsed_ms = sqlite3_column_double(stmt, 5);
        row.cwd = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        const char *se = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        row.stderr_out = se ? se : "";
        results.push_back(row);
    }

    sqlite3_finalize(stmt);

    std::reverse(results.begin(), results.end());
    return results;
}

int Storage::count_recent_failures(int last_n)
{
    if (!db_)
        return 0;

    const char *sql =
        "SELECT COUNT(*) FROM ("
        "  SELECT exit_code FROM commands ORDER BY timestamp DESC, id DESC LIMIT ?"
        ") WHERE exit_code != 0;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, last_n);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return count;
}

int Storage::insert_session(const std::string &session_id, int64_t started_at,
                            const std::string &initial_cwd)
{
    if (!db_)
        return -1;

    const char *sql =
        "INSERT INTO sessions (session_id, started_at, initial_cwd) VALUES (?, ?, ?);";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return -1;

    sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, started_at);
    sqlite3_bind_text(stmt, 3, initial_cwd.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int Storage::end_session(const std::string &session_id, int64_t ended_at)
{
    if (!db_)
        return -1;

    const char *sql = "UPDATE sessions SET ended_at = ? WHERE session_id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return -1;

    sqlite3_bind_int64(stmt, 1, ended_at);
    sqlite3_bind_text(stmt, 2, session_id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int64_t Storage::get_session_start(const std::string &session_id)
{
    if (!db_)
        return 0;

    const char *sql = "SELECT started_at FROM sessions WHERE session_id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);

    int64_t start = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        start = sqlite3_column_int64(stmt, 0);

    sqlite3_finalize(stmt);
    return start;
}
