#include "server/game.h"
#include "net/session.h"
#include "db/database.h"
#include "common/protocol.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main() {
    Database db;
    assert(db.Open(":memory:"));

    auto s1 = std::make_shared<Session>(INVALID_SOCK);
    auto s2 = std::make_shared<Session>(INVALID_SOCK);

    Game game(s1, s2, db);
    game.SetTurnTimeoutSeconds(1);

    // 1. Начало игры
    game.ProcessMessage(1, MessageType::PLACE_RANDOM, {});
    game.ProcessMessage(2, MessageType::PLACE_RANDOM, {});
    assert(game.GetPhase() == GamePhase::BATTLE);
    assert(game.GetCurrentTurn() == 1);
    std::cout << "Battle started, turn 1\n";

    // 2. До дедлайна таймаут не срабатывает 
    game.CheckTurnTimeout();
    assert(game.GetCurrentTurn() == 1);
    std::cout << "No timeout before deadline: OK\n";

    // 3. После дедлайна ход передается
    std::this_thread::sleep_for(1100ms);
    game.CheckTurnTimeout();
    assert(game.GetPhase() == GamePhase::BATTLE);
    assert(game.GetCurrentTurn() == 2);
    std::cout << "Turn passed on timeout: OK\n";

    // 4. Таймер взведён заново: немедленный вызов - no-op
    game.CheckTurnTimeout();
    assert(game.GetCurrentTurn() == 2);
    std::cout << "Timer re-armed after pass: OK\n";

    // 5. Оба AFK: серия из 3 таймаутов одного игрока -> техпоражение
    int guard = 0;
    while (game.GetPhase() == GamePhase::BATTLE && guard < 10) {
        std::this_thread::sleep_for(1100ms);
        game.CheckTurnTimeout();
        ++guard;
    }
    assert(game.GetPhase() == GamePhase::FINISHED);
    std::cout << "Tech loss after timeout streak: OK\n";

    std::cout << "All timer test passed!\n";
    return 0;
}