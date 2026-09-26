#include "server/lobby.h"
#include "server/game.h"
#include "common/protocol.h"
#include <iostream>

bool Lobby::TryMatch(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (waiting_queue_.empty()) {
        waiting_queue_.push(session);
        std::cout << "[Lobby] Player " << session->GetId() << " added to queue\n";

        MessageWriter msg(MessageType::WAITING);
        session->SendSessionMessage(msg.Finish());
        return false;
    }

    auto player1 = waiting_queue_.front();
    waiting_queue_.pop();
    auto player2 = session;

    std::cout << "[Lobby] Match found! Player " << player1->GetId()
              << " vs Player " << player2->GetId() << std::endl;

    auto game = std::make_shared<Game>(player1, player2);
    std::weak_ptr<Game> game_weak = game;

    player1->SetMessageHandler([game_weak](MessageType t, const std::vector<uint8_t>& p) {
        if (auto g = game_weak.lock()) g->ProcessMessage(1, t, p);
    });
    player2->SetMessageHandler([game_weak](MessageType t, const std::vector<uint8_t>& p) {
        if (auto g = game_weak.lock()) g->ProcessMessage(2, t, p);
    });

    active_matches_[player1->GetId()] = MatchRecord{game, 1};
    active_matches_[player2->GetId()] = MatchRecord{game, 2};

    MessageWriter found(MessageType::MATCH_FOUND);
    player1->SendSessionMessage(found.Finish());
    player2->SendSessionMessage(found.Finish());

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
