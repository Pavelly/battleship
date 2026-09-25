#pragma once

#include "session.h"
#include "board.h"
#include <memory>
#include <mutex>

enum class GamePhase {
    PLACEMENT,
    BATTLE,
    FINISHED
};

class Game : public std::enable_shared_from_this<Game> {
public:
    Game(std::shared_ptr<Session> player1, std::shared_ptr<Session> player2);

    void ProcessMessage(int player_num, MessageType type, const std::vector<uint8_t>& payload);
    void OnPlayerDisconnect(int player_num);
    std::shared_ptr<Session> GetPlayer1() const { return player1_; }
    std::shared_ptr<Session> GetPlayer2() const { return player2_; }
    GamePhase GetPhase() const { return phase_; }
private:
    std::shared_ptr<Session> player1_;
    std::shared_ptr<Session> player2_;

    Board board1_;
    Board board2_;

    GamePhase phase_;
    int current_turn_;
    bool p1_ready_;
    bool p2_ready_;

    std::mutex mutex_;

    void HandlePlaceShips(int player_num, const std::vector<uint8_t>& payload);
    void HandleShot(int player_num, const std::vector<uint8_t>& payload);

    void SendToPlayer(int player_num, const std::vector<uint8_t>& msg);
    void SendToBoth(const std::vector<uint8_t>& msg);
    void StartBattle();
    void EndGame(int winner, uint8_t reason);

    std::shared_ptr<Session> GetPlayer(int num) { return num == 1 ? player1_ : player2_; }
};