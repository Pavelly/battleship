#pragma once

#include "platform.h"
#include "protocol.h"
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

class Game;

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(SocketType socket);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void Run();
    void SendSessionMessage(const std::vector<uint8_t>& message);
    void SetGame(std::shared_ptr<Game> game);
    std::shared_ptr<Game> GetGame() const;

    void SetPlayerNumber(int num) { player_number_ = num; }
    int GetPlayerNumber() const { return player_number_; }

    int GetId() const { return id_; }
    bool IsAlive() const { return alive_.load(); }
private:
    static std::atomic<int> next_id_;

    int id_;
    SocketType socket_;
    std::atomic<bool> alive_;
    std::mutex send_mutex_;
    std::shared_ptr<Game> game_;
    std::vector<uint8_t> read_buffer_;
    int player_number_ = 0;

    void ProcessIncomingData();
    void HandleMessage(MessageType type, const std::vector<uint8_t>& payload);
};