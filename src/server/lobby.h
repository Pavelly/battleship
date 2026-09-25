#pragma once

#include "session.h"
#include <queue>
#include <mutex>
#include <memory>

class Game;

class Lobby {
public:
    Lobby();

    bool TryMatch(std::shared_ptr<Session> session);
    void RemoveFromQueue(std::shared_ptr<Session> session);
private:
    std::mutex mutex_;
    std::queue<std::shared_ptr<Session>> waiting_queue_;
};