#include "db/database.h"
#include <cassert>
#include <iostream>

int main() {
    Database db;
    assert(db.Open(":memory:"));

    int64_t alice = 0, bob = 0, carol = 0, dave = 0;
    assert(db.CreateUser("Alice", "h", "s", alice));
    assert(db.CreateUser("Bob", "h", "s", bob));
    assert(db.CreateUser("Carol", "h", "s", carol));
    assert(db.CreateUser("Dave", "h", "s", dave));

    // Четыре партии: Alice участвует в трёх
    assert(db.RecordGameResult(alice, bob, alice));      // g1
    assert(db.RecordGameResult(alice, carol, carol));    // g2
    assert(db.RecordGameResult(bob, carol, bob));        // g3 (без Alice)
    assert(db.RecordGameResult(alice, bob, bob));        // g4

    const auto h = db.GetUserHistory(alice, 10);
    assert(h.size() == 3);
    // Свежие первыми: g4, g2, g1
    assert(h[0].game_id == 4 && h[0].winner_id == bob);
    assert(h[0].player1_name == "Alice" && h[0].player2_name == "Bob");
    assert(h[1].game_id == 2 && h[1].winner_id == carol);
    assert(h[2].game_id == 1 && h[2].winner_id == alice);
    assert(!h[0].finished_at.empty());
    std::cout << "Order & names: OK\n";

    // LIMIT работает
    assert(db.GetUserHistory(alice, 2).size() == 2);
    assert(db.GetUserHistory(alice, 50).size() == 3);    // больше партий просто нет

    // У кого не было партий — пустая история
    assert(db.GetUserHistory(dave, 10).empty());
    std::cout << "Limits & empty history: OK\n";

    std::cout << "All history tests passed!\n";
    return 0;
}