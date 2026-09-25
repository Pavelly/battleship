#include "lobby.h"
#include "game.h"
#include "protocol.h"
#include <iostream>

Lobby::Lobby() {}

bool Lobby::TryMatch(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!waiting_queue_.empty()) {
        auto player1 = waiting_queue_.front();
        waiting_queue_.pop();
        auto player2 = session;

        std::cout << "[Lobby] Match found! Player " << player1->GetId()
                  << " vs Player " << player2->GetId() << std::endl;
        
        auto game = std::make_shared<Game>(player1, player2);

        player1->SetGame(game);
        player2->SetGame(game);

        player1->SetPlayerNumber(1);
        player2->SetPlayerNumber(2);

        MessageWriter msg1(MessageType::MATCH_FOUND);
        player1->SendSessionMessage(msg1.Finish());
        MessageWriter msg2(MessageType::MATCH_FOUND);
        player2->SendSessionMessage(msg2.Finish());

        MessageWriter num1(MessageType::PLAYER_NUMBER);
        num1.WriteUInt8(0x01);
        player1->SendSessionMessage(num1.Finish());
        MessageWriter num2(MessageType::PLAYER_NUMBER);
        num2.WriteUInt8(0x02);
        player2->SendSessionMessage(num2.Finish());

        return true;
    } else {
        waiting_queue_.push(session);
        
        std::cout << "[Lobby] Player " << session->GetId()
                  << " added to queue. Waiting for opponent...\n";

        MessageWriter msg(MessageType::WAITING);
        session->SendSessionMessage(msg.Finish());

        return false;
    }
}

void Lobby::RemoveFromQueue(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::queue<std::shared_ptr<Session>> new_queue;
    while (!waiting_queue_.empty()) {
        auto s = waiting_queue_.front();
        waiting_queue_.pop();
        if (s != session)
            new_queue.push(s);
    }
    waiting_queue_ = std::move(new_queue);

    std::cout << "[Lobby] Player " << session->GetId()
              << " removed from queue\n";
}
