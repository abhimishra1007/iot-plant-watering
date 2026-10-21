#pragma once

#include <cstdint>
#include <string>

struct sqlite3;

// Writes sensor readings and pump runs to SQLite. The Flask dashboard reads
// the same file (WAL mode lets it read while the controller writes).
class DataLogger {
public:
    explicit DataLogger(const std::string &path);
    ~DataLogger();
    DataLogger(const DataLogger &) = delete;
    DataLogger &operator=(const DataLogger &) = delete;

    void log_reading(int64_t ts, int raw, double moisture_pct, bool pump_on);
    // Returns the row id of the new watering event.
    int64_t start_watering(int64_t ts, double moisture_pct);
    void finish_watering(int64_t event_id, int64_t ts, double moisture_pct, const std::string &reason);

private:
    void exec(const char *sql);
    sqlite3 *db_ = nullptr;
};
