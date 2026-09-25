#include "game.h"
#include "protocol.h"
#include <iostream>

Game::Game(std::shared_ptr<Session> player1, std::shared_ptr<Session> player2) 
    : player1_(player1)
    , player2_(player2) 
    , phase_(GamePhase::PLACEMENT) 
    , current_turn_(1)
    , p1_ready_(false)
    , p2_ready_(false) {
    std::cout << "[Game] Created between player " << player1->GetId()
              << " and " << player2->GetId() << std::endl;
}

void Game::ProcessMessage(int player_num, MessageType type, const std::vector<uint8_t> &payload) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (phase_ == GamePhase::FINISHED)
        return;

    std::cout << "[Game] Player " << player_num
              << " sent message type " << static_cast<int>(type) << std::endl;

    switch (type) {
        case MessageType::PLACE_SHIPS:
            HandlePlaceShips(player_num, payload);
            break;
        case MessageType::SHOT:
            HandleShot(player_num, payload);
            break;

        default:
            std::cerr << "[Game] Unknown message type: "
                      << static_cast<int>(type) << std::endl;
            break;
    }
}

void Game::OnPlayerDisconnect(int player_num) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[Game] Player " << player_num << " disconnected\n";

    if (phase_ == GamePhase::FINISHED)
        return;
    
    int winner = (player_num == 1) ? 2 : 1;
    EndGame(winner, 1);
}

void Game::HandlePlaceShips(int player_num, const std::vector<uint8_t> &payload) {
    if (phase_ != GamePhase::PLACEMENT) {
        std::cerr << "[Game] PLACE_SHIPS received in wrong phase\n";
        return;
    }

    if ((player_num == 1 && p1_ready_) || (player_num == 2 && p2_ready_)) {
        std::cerr << "[Game] Player " << player_num << " already placed ships\n";
        return;
    }

    if (payload.empty()) {
        std::cerr << "[Game] Empty PLACE_SHIPS payload\n";
        return;
    }

    uint8_t count = payload[0];
    if (count != 10 || payload.size() != 1 + 10 * 4) {
        std::cerr << "[Game] Invalid PLACE_SHIPS payload size\n";
        return;
    }

    Board& board = (player_num == 1) ? board1_ : board2_;
    board.Reset();

    bool valid = true;
    for (uint8_t i = 0; i < count; ++i) {
        size_t offset = 1 + i * 4;
        uint8_t row = payload[offset];
        uint8_t col = payload[offset + 1];
        uint8_t size = payload[offset + 2];
        Orientation orientation = static_cast<Orientation>(payload[offset + 3]);

        PlacementResult result = board.PlaceShip(row, col, size, orientation);
        if (result != PlacementResult::OK) {
            std::cerr << "[Game] Invalid ship placement: row = " << static_cast<int>(row)
                      << ", col = " << static_cast<int>(col)
                      << ", size = " << static_cast<int>(size)
                      << ", error = " << static_cast<int>(result) << std::endl;
            valid = false;
            break;
        }
    }

    if (!valid) {
        MessageWriter error_msg(MessageType::ERROR_MSG);
        error_msg.WriteUInt8(1);
        SendToPlayer(player_num, error_msg.Finish());
        board.Reset();
        return;
    }

    if (player_num == 1) 
        p1_ready_ = true;
    else 
        p2_ready_ = true;
    
    std::cout << "[Game] Player " << player_num << " placed ships successfully\n";

    MessageWriter ready_msg(MessageType::PLACEMENT_READY);
    SendToPlayer(player_num, ready_msg.Finish());

    if (p1_ready_ && p2_ready_)
        StartBattle();
}

void Game::HandleShot(int player_num, const std::vector<uint8_t> &payload) {
    if (phase_ != GamePhase::BATTLE) {
        std::cerr << "[Game] SHOT received in wrong phase\n";
        return;
    }

    if (current_turn_ != player_num) {
        std::cerr << "[Game] Not player " << player_num << "'s turn\n";
        MessageWriter error_msg(MessageType::ERROR_MSG);
        error_msg.WriteUInt8(2);
        SendToPlayer(player_num, error_msg.Finish());
        return; 
    }

    if (payload.size() < 2) {
        std::cerr << "[Game] Invalid SHOT payload\n";
        return;
    }

    uint8_t row = payload[0];
    uint8_t col = payload[1];

    Board& enemy_board = (player_num == 1) ? board2_ : board1_;
    int enemy_num = (player_num == 1) ? 2 : 1;

    ShotResult result = enemy_board.Shoot(row, col);

    std::cout << "[Game] Player " << player_num
                                  << " shot at (" << static_cast<int>(row) << ", "
                                  << static_cast<int>(col) << ") = "
                                  << static_cast<int>(result) << std::endl;
    
    MessageWriter result_msg(MessageType::SHOT_RESULT);
    result_msg.WriteUInt8(row);
    result_msg.WriteUInt8(col);
    result_msg.WriteUInt8(static_cast<uint8_t>(result));
    SendToPlayer(player_num, result_msg.Finish());

    MessageWriter enemy_msg(MessageType::ENEMY_SHOT);
    enemy_msg.WriteUInt8(row);
    enemy_msg.WriteUInt8(col);
    enemy_msg.WriteUInt8(static_cast<int>(result));
    SendToPlayer(enemy_num, enemy_msg.Finish());

    if (enemy_board.AllShipsSunk()) {
        EndGame(player_num, 0);
        return;
    }

    if (result == ShotResult::MISS) {
        current_turn_ = enemy_num;

        MessageWriter your_turn(MessageType::YOUR_TURN);
        SendToPlayer(current_turn_, your_turn.Finish());

        MessageWriter enemy_turn(MessageType::ENEMY_TURN);
        SendToPlayer(player_num, enemy_turn.Finish());
    } else {
        MessageWriter your_turn(MessageType::YOUR_TURN);
        SendToPlayer(player_num, your_turn.Finish());
    }
}

void Game::SendToPlayer(int player_num, const std::vector<uint8_t> &msg) {
    auto player = GetPlayer(player_num);
    if (player && player->IsAlive())
        player->SendSessionMessage(msg);
}

void Game::SendToBoth(const std::vector<uint8_t> &msg) {
    SendToPlayer(1, msg);
    SendToPlayer(2, msg);
}

void Game::StartBattle() {
    phase_ = GamePhase::BATTLE;
    current_turn_ = 1;

    std::cout << "[Game] Battle started! Player 1 goes first\n";

    MessageWriter start_msg(MessageType::BATTLE_START);
    SendToBoth(start_msg.Finish());

    MessageWriter your_turn(MessageType::YOUR_TURN);
    SendToPlayer(1, your_turn.Finish());

    MessageWriter enemy_turn(MessageType::ENEMY_TURN);
    SendToPlayer(2, enemy_turn.Finish());
}

void Game::EndGame(int winner, uint8_t reason) {
    phase_ = GamePhase::FINISHED;

    std::cout << "[Game] Game over! Player " << winner << " wins (reason = "
              << static_cast<int>(reason) << std::endl;

    MessageWriter game_over(MessageType::GAME_OVER);
    game_over.WriteUInt8(static_cast<uint8_t>(winner));
    game_over.WriteUInt8(reason);
    SendToBoth(game_over.Finish());
}
