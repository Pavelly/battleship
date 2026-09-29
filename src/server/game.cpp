#include "server/game.h"
#include "db/database.h"
#include "game/random_fleet.h"
#include "common/protocol.h"
#include "common/logger.h"
#include <iostream>
#include <random>

Game::Game(std::shared_ptr<Session> player1, std::shared_ptr<Session> player2, Database& db) 
    : player1_(player1)
    , player2_(player2) 
    , db_(db)
    , phase_(GamePhase::PLACEMENT) 
    , current_turn_(1)
    , p1_ready_(false)
    , p2_ready_(false) {
    LOG_INFO << "[Game] Created between player " << player1->GetId()
              << " and " << player2->GetId();
}

void Game::ProcessMessage(int player_num, MessageType type, const std::vector<uint8_t> &payload) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (phase_ == GamePhase::FINISHED) {
        switch (type) {
            case MessageType::REMATCH_REQUEST:
                HandleRematchRequest(player_num);
                break;
            case MessageType::REMATCH_DECLINE:
                HandleRematchDecline(player_num);
                break;
            
            default:
                LOG_WARN << "[Game] Ignoring message type "
                          << static_cast<int>(type) << " in FINISHED phase";
                return;
        }
    }

    LOG_DEBUG << "[Game] Player " << player_num
              << " sent message type " << static_cast<int>(type);

    switch (type) {
        case MessageType::PLACE_SHIPS:
            HandlePlaceShips(player_num, payload);
            break;
        case MessageType::PLACE_RANDOM:
            HandlePlaceRandom(player_num);
            break;
        case MessageType::SHOT:
            HandleShot(player_num, payload);
            break;

        default:
            LOG_ERROR << "[Game] Unknown message type: "
                      << static_cast<int>(type);
            break;
    }
}

void Game::OnPlayerDisconnect(int player_num) {
    std::lock_guard<std::mutex> lock(mutex_);

    LOG_INFO << "[Game] Player " << player_num << " disconnected";

    if (phase_ == GamePhase::FINISHED)
        return;
    
    int winner = (player_num == 1) ? 2 : 1;
    EndGame(winner, 1);
}

void Game::HandlePlaceShips(int player_num, const std::vector<uint8_t> &payload) {
    if (phase_ != GamePhase::PLACEMENT) {
        LOG_ERROR << "[Game] PLACE_SHIPS received in wrong phase";
        return;
    }

    if ((player_num == 1 && p1_ready_) || (player_num == 2 && p2_ready_)) {
        LOG_ERROR << "[Game] Player " << player_num << " already placed ships";
        return;
    }

    if (payload.empty()) {
        LOG_ERROR << "[Game] Empty PLACE_SHIPS payload";
        return;
    }

    uint8_t count = payload[0];
    if (count != 10 || payload.size() != 1 + 10 * 4) {
        LOG_ERROR << "[Game] Invalid PLACE_SHIPS payload size";
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
            LOG_ERROR << "[Game] Invalid ship placement: row = " << static_cast<int>(row)
                      << ", col = " << static_cast<int>(col)
                      << ", size = " << static_cast<int>(size)
                      << ", error = " << static_cast<int>(result);
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
    
    LOG_INFO << "[Game] Player " << player_num << " placed ships successfully";

    MessageWriter ready_msg(MessageType::PLACEMENT_READY);
    SendToPlayer(player_num, ready_msg.Finish());

    if (p1_ready_ && p2_ready_)
        StartBattle();
    else
        LOG_INFO << "[Game] Fleet accepted from player " << player_num
                  << ", waiting for the second fleet...";
}

void Game::HandleShot(int player_num, const std::vector<uint8_t> &payload) {
    if (phase_ != GamePhase::BATTLE) {
        LOG_ERROR << "[Game] SHOT received in wrong phase";
        return;
    }

    if (current_turn_ != player_num) {
        LOG_ERROR << "[Game] Not player " << player_num << "'s turn";
        MessageWriter error_msg(MessageType::ERROR_MSG);
        error_msg.WriteUInt8(2);
        SendToPlayer(player_num, error_msg.Finish());
        return; 
    }

    if (payload.size() < 2) {
        LOG_ERROR << "[Game] Invalid SHOT payload";
        return;
    }

    timeout_streak_[player_num - 1] = 0;

    uint8_t row = payload[0];
    uint8_t col = payload[1];

    Board& enemy_board = (player_num == 1) ? board2_ : board1_;
    int enemy_num = (player_num == 1) ? 2 : 1;

    ShotResult result = enemy_board.Shoot(row, col);

    std::vector<Coord> outlined;
    if (result == ShotResult::SINK) {
        outlined = enemy_board.MarkOutlineAround(row, col);
        LOG_DEBUG << "[Game] Ship sunk, outlined " << outlined.size()
                  << " cells";
    }

    LOG_INFO << "[Game] Player " << player_num
                                  << " shot at (" << static_cast<int>(row) << ", "
                                  << static_cast<int>(col) << ") = "
                                  << static_cast<int>(result);
    
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

    if (!outlined.empty()) {
        MessageWriter upd(MessageType::BOARD_UPDATE);
        upd.WriteUInt8(static_cast<uint8_t>(enemy_num));
        upd.WriteUInt8(static_cast<uint8_t>(outlined.size()));
        for (const auto& cell : outlined) {
            upd.WriteUInt8(cell.row);
            upd.WriteUInt8(cell.col);
            upd.WriteUInt8(static_cast<uint8_t>(CellState::MISS));
        }
        SendToBoth(upd.Finish());
    }

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
        
        ArmTurnTimer();
    } else {
        MessageWriter your_turn(MessageType::YOUR_TURN);
        SendToPlayer(player_num, your_turn.Finish());

        ArmTurnTimer();
    }
}

void Game::HandlePlaceRandom(int player_num) {
    if (phase_ != GamePhase::PLACEMENT) {
        LOG_ERROR << "[Game] PLACE_RANDOM in wrong phase";
        return;
    }
    if ((player_num == 1 && p1_ready_) || (player_num == 2 && p2_ready_)) {
        LOG_ERROR << "[Game] Player " << player_num << " already ready";
        return;
    }

    Board& board = (player_num == 1) ? board1_ : board2_;

    std::mt19937 rng(std::random_device{}());
    const auto specs = PlaceRandomFleet(board, rng);

    if (specs.empty()) {
        LOG_ERROR << "[Game] Random fleet generation failed";
        MessageWriter err(MessageType::ERROR_MSG);
        err.WriteUInt8(1);
        SendToPlayer(player_num, err.Finish());
        return;
    }

    MessageWriter ships(MessageType::OWN_SHIPS);
    ships.WriteUInt8(static_cast<uint8_t>(specs.size()));
    for (const auto& s : specs) {
        ships.WriteUInt8(s.row);
        ships.WriteUInt8(s.col);
        ships.WriteUInt8(s.size);
        ships.WriteUInt8(static_cast<uint8_t>(s.orientation));
    }
    SendToPlayer(player_num, ships.Finish());

    if (player_num == 1) 
        p1_ready_ = true;
    else 
        p2_ready_ = true;

    LOG_INFO << "[Game] Player " << player_num << " placed random fleet";

    MessageWriter ready(MessageType::PLACEMENT_READY);
    SendToPlayer(player_num, ready.Finish());

    if (p1_ready_ && p2_ready_)
        StartBattle();
    else
        LOG_INFO << "[Game] Fleet accepted from player " << player_num
                  << ", waiting for the second fleet...";
}

void Game::HandleRematchRequest(int player_num) {
    const int opponent = 3 - player_num;
    auto opp_session = GetPlayer(opponent);

    if (!opp_session || !opp_session->IsAlive()) {
        MessageWriter declined(MessageType::REMATCH_DECLINED);
        SendToPlayer(player_num, declined.Finish());
        return;
    }

    if (player_num == 1) p1_rematch_ = true;
    else                 p2_rematch_ = true;

    LOG_INFO << "[Game] Player " << player_num << " requests rematch";

    if (p1_rematch_ && p2_rematch_) {
        StartRematch();
        return;
    }

    MessageWriter offer(MessageType::REMATCH_OFFER);
    SendToPlayer(opponent, offer.Finish());
}

void Game::HandleRematchDecline(int player_num) {
    p1_rematch_ = false;
    p2_rematch_ = false;

    LOG_INFO << "[Game] Player " << player_num << " declined rematch";

    MessageWriter declined(MessageType::REMATCH_DECLINED);
    SendToPlayer(3 - player_num, declined.Finish());
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
    timeout_streak_[0] = 0;
    timeout_streak_[1] = 0;
    ArmTurnTimer();

    LOG_INFO << "[Game] Battle started! Player 1 goes first";

    MessageWriter start_msg(MessageType::BATTLE_START);
    SendToBoth(start_msg.Finish());

    MessageWriter your_turn(MessageType::YOUR_TURN);
    SendToPlayer(1, your_turn.Finish());

    MessageWriter enemy_turn(MessageType::ENEMY_TURN);
    SendToPlayer(2, enemy_turn.Finish());
}

void Game::StartRematch() {
    board1_.Reset();
    board2_.Reset();
    p1_ready_   = false;
    p2_ready_   = false;
    p1_rematch_ = false;
    p2_rematch_ = false;
    timeout_streak_[0] = 0;
    timeout_streak_[1] = 0;
    phase_ = GamePhase::PLACEMENT;

    LOG_INFO << "[Game] Rematch started! Back to placement";
    
    MessageWriter start(MessageType::REMATCH_START);
    SendToBoth(start.Finish());
}

void Game::EndGame(int winner, uint8_t reason) {
    phase_ = GamePhase::FINISHED;

    LOG_INFO << "[Game] Game over! Player " << winner << " wins (reason = "
              << static_cast<int>(reason) << ")";

    MessageWriter game_over(MessageType::GAME_OVER);
    game_over.WriteUInt8(static_cast<uint8_t>(winner));
    game_over.WriteUInt8(reason);
    SendToBoth(game_over.Finish());

    const int64_t p1_id = player1_->GetUserId();
    const int64_t p2_id = player2_->GetUserId();

    if (p1_id == -1 || p2_id == -1) {
        LOG_ERROR << "[Game] Cannot record result: player not authenticated";
        return;
    }

    const int64_t winner_id = (winner == 1) ? p1_id : p2_id;

    if (db_.RecordGameResult(p1_id, p2_id, winner_id)) {
        LOG_INFO << "[Game] Result saved to DB (winner id = " << winner_id << ")";
    } else {
        LOG_ERROR << "[Game] Failed to save result to DB";
    }
}

void Game::ArmTurnTimer() {
    turn_deadline_ = std::chrono::steady_clock::now() 
        + std::chrono::seconds(turn_timeout_seconds_);
}

Game::GameSnapshot Game::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GameSnapshot{static_cast<int>(phase_), current_turn_};
}

void Game::CheckTurnTimeout()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (phase_ != GamePhase::BATTLE)
        return;
    if (std::chrono::steady_clock::now() < turn_deadline_)
        return;

    const int offender = current_turn_;
    const int opponent = (offender == 1) ? 2 : 1;
    timeout_streak_[offender - 1]++;

    LOG_INFO << "[Game] Turn timeout: player " << offender
              << " (streak " << timeout_streak_[offender - 1] << ")";
    
    if (timeout_streak_[offender - 1] >= static_cast<int>(max_turn_timeouts_)) {
        EndGame(opponent, 2);
        return;
    }

    current_turn_ = opponent;
    ArmTurnTimer();

    MessageWriter timeout_msg(MessageType::TURN_TIMEOUT);
    timeout_msg.WriteUInt8(static_cast<uint8_t>(offender));
    SendToBoth(timeout_msg.Finish());

    MessageWriter your_turn(MessageType::YOUR_TURN);
    SendToPlayer(current_turn_, your_turn.Finish());
    MessageWriter enemy_turn(MessageType::ENEMY_TURN);
    SendToPlayer(offender, enemy_turn.Finish());
}
