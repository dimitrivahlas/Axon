#include "storage.h"

/* Stubs — real SQLite implementation comes in Step 2 */

Storage::Storage() {}
Storage::~Storage() {}

int Storage::open(const std::string &) { return -1; }
void Storage::close() {}
int Storage::create_schema() { return -1; }

int Storage::insert_command(const std::string &, const std::string &,
                            int, double, const std::string &,
                            const std::string &)
{
    return -1;
}

std::vector<CommandRow> Storage::query_recent(int) { return {}; }
std::vector<CommandRow> Storage::query_recent_by_cwd(const std::string &, int) { return {}; }
int Storage::count_recent_failures(int) { return 0; }

int Storage::insert_session(const std::string &, int64_t, const std::string &) { return -1; }
int Storage::end_session(const std::string &, int64_t) { return -1; }
int64_t Storage::get_session_start(const std::string &) { return 0; }
