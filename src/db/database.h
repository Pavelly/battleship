#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

struct UserRecord {
    int64_t id = 0;
    std::string username;
    std::string password_hash;
    std::string salt;
    int wins = 0;
    int losses = 0;
};

struct GameRecord {
    int64_t game_id = 0;
    int64_t player1_id = 0;
    int64_t player2_id = 0;
    int64_t winner_id = 0;
    std::string player1_name;
    std::string player2_name;
    std::string finished_at;    // "YYYY-MM-DD HH:MM:SS"
};

class Database {
public:
    Database() = default;
    ~Database() { Close(); }

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool Open(const std::string& path);
    void Close();

    bool CreateUser(const std::string& username,
                    const std::string& password_hash,
                    const std::string& salt,
                    int64_t& out_id);
    
    std::optional<UserRecord> GetUserByName(const std::string& username);
    bool RecordGameResult(int64_t player1_id, int64_t player2_id, int64_t winner_id);
    std::vector<GameRecord> GetUserHistory(int64_t user_id, int limit);

    int64_t CountGames();
private:
    bool ExecUnlocked(const std::string& sql);
    bool ExecBindInt64Unlocked(const char* sql, int64_t value);
    bool InitSchemaUnlocked();

    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};