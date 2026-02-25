#ifndef AXON_STORAGE_H
#define AXON_STORAGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <sqlite3.h>

struct CommandRow {
    int id;
    std::string session_id;
    int64_t timestamp;
    std::string command;
    int exit_code;
    double elapsed_ms;
    std::string cwd;
    std::string stderr_out;
};

class Storage {
public:
    Storage();
    ~Storage();

    int open(const std::string &db_path);
    void close();
    int create_schema();

    int insert_command(const std::string &session_id, const std::string &command,
                       int exit_code, double elapsed_ms, const std::string &cwd,
                       const std::string &stderr_out);

    std::vector<CommandRow> query_recent(int max_entries);
    std::vector<CommandRow> query_recent_by_cwd(const std::string &cwd, int max_entries);
    int count_recent_failures(int last_n);

    int insert_session(const std::string &session_id, int64_t started_at,
                       const std::string &initial_cwd);
    int end_session(const std::string &session_id, int64_t ended_at);
    int64_t get_session_start(const std::string &session_id);

    bool is_open() const { return db_ != nullptr; }

private:
    sqlite3 *db_;
};

#endif
