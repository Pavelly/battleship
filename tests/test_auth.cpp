#include "db/auth_service.h"
#include "db/database.h"
#include <cassert>
#include <iostream>

namespace {
std::vector<uint8_t> MakePayload(const std::string& n, const std::string& p) {
    std::vector<uint8_t> v;
    v.push_back(static_cast<uint8_t>(n.size()));
    for (char c : n) v.push_back(static_cast<uint8_t>(c));
    v.push_back(static_cast<uint8_t>(p.size()));
    for (char c : p) v.push_back(static_cast<uint8_t>(c));
    return v;
}
}

int main() {
    Database db;
    assert(db.Open(":memory:"));
    AuthService auth(db);

    // Разбор payload
    std::string n, p;
    assert(AuthService::ParseCredentials(MakePayload("Alice", "secret123"), n, p));
    assert(n == "Alice" && p == "secret123");
    assert(!AuthService::ParseCredentials({0x05, 'a'}, n, p));  // обрезанный payload

    // Регистрация
    auto out = auth.Register("Alice", "secret123");
    std::cout << "Register result: " << static_cast<int>(out.result) << "\n";
    // Легенда: 0=OK, 1=BAD_CREDENTIALS, 2=USERNAME_TAKEN, 3=USERNAME_INVALID, 4=INVALID_PAYLOAD
    assert(out.result == AuthService::Result::OK);
    assert(out.user.wins == 0 && out.user.losses == 0);

    // Дубликат (COLLATE NOCASE)
    assert(auth.Register("alice", "other").result == AuthService::Result::USERNAME_TAKEN);

    // Невалидные имена
    assert(auth.Register("Al", "secret123").result == AuthService::Result::USERNAME_INVALID);
    assert(auth.Register("bad name!", "secret123").result == AuthService::Result::USERNAME_INVALID);

    // Вход
    assert(auth.Login("Alice", "wrong").result == AuthService::Result::BAD_CREDENTIALS);
    assert(auth.Login("Bob", "x").result == AuthService::Result::BAD_CREDENTIALS);
    out = auth.Login("Alice", "secret123");
    std::cout << "Login result: " << static_cast<int>(out.result) << "\n";
    assert(out.result == AuthService::Result::OK);
    assert(out.user.username == "Alice");

    std::cout << "All auth tests passed!\n";
    return 0;
}