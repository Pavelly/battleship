#include "db/database.h"
#include <sqlite3.h>
#include <iostream>

bool Database::Open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "[DB] Open failed: "
                  << (db_ ? sqlite3_errmsg(db_) : "unknown error") << std::endl;
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    if (!ExecUnlocked("PRAGMA foreign_keys = ON;")) return false;
    return InitSchemaUnlocked();
}

void Database::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::CreateUser(const std::string &username, const std::string& password_hash, const std::string& salt, int64_t& out_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO users (username, password_hash, salt) VALUES (?1, ?2, ?3);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return false;
    out_id = sqlite3_last_insert_rowid(db_);
    return true;
}

std::optional<UserRecord> Database::GetUserByName(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = 
        "SELECT id, username, password_hash, salt, wins, losses "
        "FROM users WHERE username = ?1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<UserRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord u;
        u.id            = sqlite3_column_int64(stmt, 0);
        u.username      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        u.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u.salt          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        u.wins          = sqlite3_column_int(stmt, 4);
        u.losses        = sqlite3_column_int(stmt, 5);
        result = u;
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::RecordGameResult(int64_t player1_id, int64_t player2_id, int64_t winner_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const int64_t loser_id = (winner_id == player1_id) ? player2_id : player1_id;

    if (!ExecUnlocked("BEGIN;")) return false;

    bool ok = true;

    sqlite3_stmt* stmt = nullptr;
    const char* insert_sql = "INSERT INTO games (player1_id, player2_id, winner_id) VALUES (?1, ?2, ?3);";
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, player1_id);
        sqlite3_bind_int64(stmt, 2, player2_id);
        sqlite3_bind_int64(stmt, 3, winner_id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    } else
        ok = false;

    ok = ok && ExecBindInt64Unlocked(
        "UPDATE users SET wins = wins + 1 WHERE id = ?1;", winner_id
    );
    ok = ok && ExecBindInt64Unlocked(
        "UPDATE users SET losses = losses + 1 WHERE id = ?1;", loser_id
    );

    ExecUnlocked(ok ? "COMMIT;" : "ROLLBACK;");
    return ok;
}

std::vector<GameRecord> Database::GetUserHistory(int64_t user_id, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GameRecord> out;
    if (!db_)
        return out;

    if (limit <= 0) limit = 10;
    if (limit > 50) limit = 50;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT g.id, g.player1_id, g.player2_id, g.winner_id, g.finished_at, "
        "       u1.username, u2.username "
        "FROM games g "
        "JOIN users u1 ON u1.id = g.player1_id "
        "JOIN users u2 ON u2.id = g.player2_id "
        "WHERE g.player1_id = ?1 OR g.player2_id = ?1 "
        "ORDER BY g.id DESC "
        "LIMIT ?2;";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    sqlite3_bind_int64(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GameRecord r;
        r.game_id       = sqlite3_column_int64(stmt, 0);
        r.player1_id    = sqlite3_column_int64(stmt, 1);
        r.player2_id    = sqlite3_column_int64(stmt, 2);
        r.winner_id     = sqlite3_column_int64(stmt, 3);

        const auto* dt  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const auto* n1  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const auto* n2  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        r.finished_at   = dt ? dt : "";
        r.player1_name  = n1 ? n1 : "?";
        r.player2_name  = n2 ? n2 : "?";
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return out;
}

int64_t Database::CountGames() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return -1;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM games;", -1, &stmt, nullptr) != SQLITE_OK)
        return -1;
    int64_t n = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

bool Database::ExecUnlocked(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] Exec failed: " << (err ? err: "?") << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::ExecBindInt64Unlocked(const char* sql, int64_t value) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, value);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::InitSchemaUnlocked() {
    static const char* schema =
        "CREATE TABLE IF NOT EXISTS users ("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username      TEXT NOT NULL UNIQUE COLLATE NOCASE,"
        "  password_hash TEXT NOT NULL,"
        "  salt          TEXT NOT NULL,"
        "  created_at    TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),"
        "  wins          INTEGER NOT NULL DEFAULT 0,"
        "  losses        INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS games ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  player1_id  INTEGER NOT NULL REFERENCES users(id),"
        "  player2_id  INTEGER NOT NULL REFERENCES users(id),"
        "  winner_id   INTEGER NOT NULL REFERENCES users(id),"
        "  finished_at TEXT NOT NULL DEFAULT (datetime('now', 'localtime')));";
    return ExecUnlocked(schema);
}
