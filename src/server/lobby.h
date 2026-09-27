#pragma once

#include "net/session.h"
#include <queue>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <optional>

class Game;
class Database;

class Lobby {
public:
    struct MatchRecord {
        std::shared_ptr<Game> game;
        int player_number;
    };

    explicit Lobby(Database& db);

    bool TryMatch(std::shared_ptr<Session> session);
    void RemoveFromQueue(std::shared_ptr<Session> session);

    std::optional<MatchRecord> GetMatch(int session_id);
    void RemoveMatch(int session_id);
private:
    Database& db_;
    std::mutex mutex_;
    std::queue<std::shared_ptr<Session>> waiting_queue_;
    std::unordered_map<int, MatchRecord> active_matches_;
};