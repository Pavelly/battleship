#include "server/lobby.h"
#include "server/game.h"
#include "common/protocol.h"
#include <iostream>
#include <chrono>
#include <unordered_set>
#include "lobby.h"

Lobby::Lobby(Database &db)
    : db_(db) {}

bool Lobby::TryMatch(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (waiting_queue_.empty()) {
        waiting_queue_.push(session);
        std::cout << "[Lobby] Player " << session->GetId() << " added to queue\n";

        session->SetMessageHandler([](MessageType t, const std::vector<uint8_t>&) {
            std::cout << "[Lobby] Ignoring message type " << static_cast<int>(t)
                    << ": player is in queue\n";
        });

        MessageWriter msg(MessageType::WAITING);
        session->SendSessionMessage(msg.Finish());
        return false;
    }


    auto player1 = waiting_queue_.front();
    waiting_queue_.pop();
    auto player2 = session;

    const std::string& name1 = player1->GetUsername();
    const std::string& name2 = player2->GetUsername();

    std::cout << "[Lobby] Match found! " << player1->GetUsername()
              << " (session " << player1->GetId() << ") vs "
              << player2->GetUsername()
              << " (session " << player2->GetId() << ")\n";

    auto game = std::make_shared<Game>(player1, player2, db_);
    std::weak_ptr<Game> game_weak = game;

    player1->SetMessageHandler([game_weak](MessageType t, const std::vector<uint8_t>& p) {
        if (auto g = game_weak.lock()) g->ProcessMessage(1, t, p);
    });
    player2->SetMessageHandler([game_weak](MessageType t, const std::vector<uint8_t>& p) {
        if (auto g = game_weak.lock()) g->ProcessMessage(2, t, p);
    });

    active_matches_[player1->GetId()] = MatchRecord{game, 1};
    active_matches_[player2->GetId()] = MatchRecord{game, 2};

    MessageWriter found1(MessageType::MATCH_FOUND);
    found1.WriteUInt8(static_cast<uint8_t>(name2.size()));
    found1.WriteBytes(name2.data(), name2.size());
    player1->SendSessionMessage(found1.Finish());

    MessageWriter found2(MessageType::MATCH_FOUND);
    found2.WriteUInt8(static_cast<uint8_t>(name1.size()));
    found2.WriteBytes(name1.data(), name1.size());
    player2->SendSessionMessage(found2.Finish());

    // MessageWriter found(MessageType::MATCH_FOUND);
    // player1->SendSessionMessage(found.Finish());
    // player2->SendSessionMessage(found.Finish());

    MessageWriter num1(MessageType::PLAYER_NUMBER);
    num1.WriteUInt8(1);
    player1->SendSessionMessage(num1.Finish());

    MessageWriter num2(MessageType::PLAYER_NUMBER);
    num2.WriteUInt8(2);
    player2->SendSessionMessage(num2.Finish());

    return true;
}

void Lobby::RemoveFromQueue(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<std::shared_ptr<Session>> fresh;
    while (!waiting_queue_.empty()) {
        auto s = waiting_queue_.front();
        waiting_queue_.pop();
        if (s != session)
            fresh.push(s);
    }
    waiting_queue_ = std::move(fresh);
    
    std::cout << "[Lobby] Player " << session->GetId()
              << " removed from queue\n";
}

std::optional<Lobby::MatchRecord> Lobby::GetMatch(int session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_matches_.find(session_id);
    if (it == active_matches_.end())
        return std::nullopt;
    return it->second;
}

void Lobby::RemoveMatch(int session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_matches_.erase(session_id);
}

void Lobby::StartTurnWatchdog() {
    if (watchdog_running_.load())
        return;
    watchdog_running_.store(true);
    watchdog_ = std::thread(&Lobby::WatchdogLoop, this);
    std::cout << "[Lobby] Turn watchdog started\n";
}

void Lobby::StopTurnWatchdog() {
    if (!watchdog_running_.exchange(false))
        return;
    if (watchdog_.joinable())
        watchdog_.join();
    std::cout << "[Lobby] Turn watchdog stopped\n";
}

void Lobby::WatchdogLoop() {
    while (watchdog_running_.load()) {
        std::vector<std::shared_ptr<Game>> games;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::unordered_set<Game*> seen;
            for (const auto& [session_id, rec] : active_matches_)
                if (seen.insert(rec.game.get()).second)
                    games.push_back(rec.game);
        }
        for (auto& game : games)
            game->CheckTurnTimeout();

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
