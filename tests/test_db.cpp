#include "db/database.h"
#include <cassert>
#include <iostream>

int main() {
    Database db;
    assert(db.Open(":memory:"));

    int64_t id1 = 0, id2 = 0;
    assert(db.CreateUser("Alice", "hash1", "salt1", id1));
    assert(db.CreateUser("Bob", "hash2", "salt2", id2));
    assert(id1 != id2);

    int64_t dup = 0;
    assert(!db.CreateUser("alice", "x", "y", dup));

    auto user = db.GetUserByName("ALICE");
    assert(user.has_value());
    assert(user->id == id1);
    assert(user->password_hash == "hash1");
    assert(user->wins == 0 && user->losses == 0);
    assert(!db.GetUserByName("Nobody").has_value());

    assert(db.RecordGameResult(id1, id2, id1));
    user = db.GetUserByName("Alice");
    assert(user->wins == 1 && user->losses == 0);
    auto bob = db.GetUserByName("Bob");
    assert(bob->wins == 0 && bob->losses == 1);

    assert(db.RecordGameResult(id1, id2, id2));
    bob = db.GetUserByName("Bob");
    assert(bob->wins == 1 && bob->losses == 1);

    std::cout << "All db tests passed!\n";
    return 0;
}