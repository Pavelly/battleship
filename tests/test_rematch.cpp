#include "server/game.h"
#include "net/session.h"
#include "db/database.h"
#include "common/protocol.h"
#include <cassert>
#include <iostream>

int main() {
    Database db;
    assert(db.Open(":memory:"));

    auto s1 = std::make_shared<Session>(INVALID_SOCK);
    auto s2 = std::make_shared<Session>(INVALID_SOCK);
    s1->SetUser(1, "Alice");
    s2->SetUser(2, "Bob");

    Game game(s1, s2, db);
    game.SetTurnTimeoutSeconds(1);

    game.ProcessMessage(1, MessageType::PLACE_RANDOM, {});
    game.ProcessMessage(2, MessageType::PLACE_RANDOM, {});
    assert(game.GetPhase() == GamePhase::BATTLE);

    // Завершаем партию дисконнектом
    game.OnPlayerDisconnect(1);
    assert(game.GetPhase() == GamePhase::FINISHED);

    // Игровые сообщения в FINISHED игнорируются
    game.ProcessMessage(2, MessageType::SHOT, {0, 0});
    assert(game.GetPhase() == GamePhase::FINISHED);

    // Одиночная заявка не начинает реванш
    game.ProcessMessage(2, MessageType::REMATCH_REQUEST, {});
    assert(game.GetPhase() == GamePhase::FINISHED);

    // Отказ сбрасывает заявку
    game.ProcessMessage(1, MessageType::REMATCH_DECLINE, {});
    assert(game.GetPhase() == GamePhase::FINISHED);

    // Обе заявки → новая расстановка
    game.ProcessMessage(1, MessageType::REMATCH_REQUEST, {});
    assert(game.GetPhase() == GamePhase::FINISHED);
    game.ProcessMessage(2, MessageType::REMATCH_REQUEST, {});
    assert(game.GetPhase() == GamePhase::PLACEMENT);

    // Второй раунд играбелен
    game.ProcessMessage(1, MessageType::PLACE_RANDOM, {});
    game.ProcessMessage(2, MessageType::PLACE_RANDOM, {});
    assert(game.GetPhase() == GamePhase::BATTLE);
    std::cout << "Second round in battle: OK\n";

    std::cout << "All rematch tests passed!\n";
    return 0;
}