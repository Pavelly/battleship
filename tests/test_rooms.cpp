#include "server/room_manager.h"
#include "net/session.h"
#include "db/database.h"
#include <cassert>
#include <iostream>

namespace {
std::shared_ptr<Session> MakeSession(int id_hint, const std::string& name) {
    auto s = std::make_shared<Session>(INVALID_SOCK);
    s->SetUser(id_hint, name);
    return s;
}
uint8_t ListCount(const std::vector<uint8_t>& msg) {
    return msg[3];   // type(1) + len(2) + payload[0] = count
}
}

int main() {
    Database db;
    assert(db.Open(":memory:"));
    RoomManager mgr(db, 1, 1);

    auto alice = MakeSession(1, "Alice");
    auto bob   = MakeSession(2, "Bob");
    auto carol = MakeSession(3, "Carol");

    // Создание и защита от второй комнаты
    const uint16_t id = mgr.CreateRoom(alice, "Alice room", false);
    assert(id != 0);
    assert(mgr.CreateRoom(alice, "second", false) == 0);

    // Список: одна открытая комната
    assert(ListCount(mgr.BuildRoomListPayload()) == 1);

    // Приватная комната в список не попадает
    const uint16_t pid = mgr.CreateRoom(carol, "secret", true);
    assert(ListCount(mgr.BuildRoomListPayload()) == 1);

    // Ошибки входа
    assert(mgr.JoinRoom(alice, id) == RoomManager::JoinResult::OWN_ROOM);
    assert(mgr.JoinRoom(bob, 999)  == RoomManager::JoinResult::NOT_FOUND);

    // Успешный вход → партия
    assert(mgr.JoinRoom(bob, id) == RoomManager::JoinResult::OK);
    auto m = mgr.GetMatch(alice->GetId());
    assert(m && m->player_number == 1);
    m = mgr.GetMatch(bob->GetId());
    assert(m && m->player_number == 2);

    // Комната в игре — третьему не войти
    assert(mgr.JoinRoom(carol, id) == RoomManager::JoinResult::FULL_OR_IN_GAME);

    // Занятый игрок не может войти куда-то ещё
    assert(mgr.JoinRoom(bob, pid) == RoomManager::JoinResult::ALREADY_IN_ROOM);

    // Вход в приватную по id работает
    auto dave = MakeSession(4, "Dave");
    assert(mgr.JoinRoom(dave, pid) == RoomManager::JoinResult::OK);

    // Владелец отключился до начала игры — комната закрыта
    auto eve = MakeSession(5, "Eve");
    const uint16_t eid = mgr.CreateRoom(eve, "temp", false);
    assert(ListCount(mgr.BuildRoomListPayload()) == 1);   // только "Alice room"? нет:
    // Alice room уже в игре → открытых нет, кроме... eve's. После создания eve: 1.
    mgr.OnWaitingPlayerDisconnected(eve);
    assert(ListCount(mgr.BuildRoomListPayload()) == 0);
    (void)eid;

    std::cout << "All room tests passed!\n";
    return 0;
}
