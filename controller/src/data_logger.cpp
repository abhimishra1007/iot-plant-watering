#include "data_logger.hpp"

#include <sqlite3.h>

#include <stdexcept>

namespace {
const char *kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS readings (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    ts           INTEGER NOT NULL,          -- unix seconds, UTC
    raw          INTEGER NOT NULL,          -- MCP3008 counts, 0-1023
    moisture_pct REAL    NOT NULL,
    pump_on      INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_readings_ts ON readings(ts);

CREATE TABLE IF NOT EXISTS watering_events (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    started_at     INTEGER NOT NULL,
    ended_at       INTEGER,
    duration_s     INTEGER,
    start_moisture REAL    NOT NULL,
    end_moisture   REAL,
    stop_reason    TEXT
);
CREATE INDEX IF NOT EXISTS idx_events_started ON watering_events(started_at);
)SQL";

class Statement {
public:
    Statement(sqlite3 *db, const char *sql)
    {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("prepare: ") + sqlite3_errmsg(db));
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    sqlite3_stmt *get() { return stmt_; }

    void run(sqlite3 *db)
    {
        if (sqlite3_step(stmt_) != SQLITE_DONE) {
            throw std::runtime_error(std::string("step: ") + sqlite3_errmsg(db));
        }
    }

private:
    sqlite3_stmt *stmt_ = nullptr;
};
}  // namespace

DataLogger::DataLogger(const std::string &path)
{
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("open database " + path + ": " + msg);
    }
    sqlite3_busy_timeout(db_, 2000);
    exec("PRAGMA journal_mode=WAL;");
    exec(kSchema);
}

DataLogger::~DataLogger()
{
    sqlite3_close(db_);
}

void DataLogger::exec(const char *sql)
{
    char *err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("sqlite: " + msg);
    }
}

void DataLogger::log_reading(int64_t ts, int raw, double moisture_pct, bool pump_on)
{
    Statement st(db_, "INSERT INTO readings (ts, raw, moisture_pct, pump_on) VALUES (?, ?, ?, ?)");
    sqlite3_bind_int64(st.get(), 1, ts);
    sqlite3_bind_int(st.get(), 2, raw);
    sqlite3_bind_double(st.get(), 3, moisture_pct);
    sqlite3_bind_int(st.get(), 4, pump_on ? 1 : 0);
    st.run(db_);
}

int64_t DataLogger::start_watering(int64_t ts, double moisture_pct)
{
    Statement st(db_, "INSERT INTO watering_events (started_at, start_moisture) VALUES (?, ?)");
    sqlite3_bind_int64(st.get(), 1, ts);
    sqlite3_bind_double(st.get(), 2, moisture_pct);
    st.run(db_);
    return sqlite3_last_insert_rowid(db_);
}

void DataLogger::finish_watering(int64_t event_id, int64_t ts, double moisture_pct, const std::string &reason)
{
    Statement st(db_,
                 "UPDATE watering_events SET ended_at = ?, duration_s = ? - started_at, "
                 "end_moisture = ?, stop_reason = ? WHERE id = ?");
    sqlite3_bind_int64(st.get(), 1, ts);
    sqlite3_bind_int64(st.get(), 2, ts);
    sqlite3_bind_double(st.get(), 3, moisture_pct);
    sqlite3_bind_text(st.get(), 4, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st.get(), 5, event_id);
    st.run(db_);
}
