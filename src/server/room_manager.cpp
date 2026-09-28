#include "server/room_manager.h"
#include "server/game.h"
#include "db/database.h"
#include <chrono>
#include <iostream>
#include <unordered_set>

RoomManager::RoomManager(Database &db) : db_(db) {}
RoomManager::~RoomManager() { StopTurnWatchdog(); }

uint16_t RoomManager::CreateRoom(const std::shared_ptr<Session>& session, const std::string& name, bool is_private) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsBusyLocked(session))
        return 0;

    const uint16_t id = next_room_id_++;
    rooms_[id] = std::make_shared<Room>(id, name, is_private, session);
    std::cout << "[Rooms] Room #" << id << " '" << name << "' created by "
              << session->GetUsername() << (is_private ? " (private)" : "") << std::endl;
    return id;
}

RoomManager::JoinResult RoomManager::JoinRoom(const std::shared_ptr<Session>& session, uint16_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end())
        return JoinResult::NOT_FOUND;

    Room& room = *it->second;
    if (room.GetOwner() == session)
        return JoinResult::OWN_ROOM;
    if (room.GetState() != RoomState::WAITING)
        return JoinResult::FULL_OR_IN_GAME;
    if (IsBusyLocked(session))
        return JoinResult::ALREADY_IN_ROOM;

    StartGameInRoom(room, session);
    return JoinResult::OK;
}

std::vector<uint8_t> RoomManager::BuildRoomListPayload() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const Room*> open;
    for (const auto& [id, room] : rooms_)
        if (room->GetState() == RoomState::WAITING && !room->IsPrivate())
            open.push_back(room.get());

    MessageWriter w(MessageType::ROOM_LIST);
    w.WriteUInt8(static_cast<uint8_t>(open.size()));
    for (const Room* r : open) {
        w.WriteUInt16(r->GetId());
        const auto& rname = r->GetName();
        w.WriteUInt8(static_cast<uint8_t>(rname.size()));
        w.WriteBytes(rname.data(), rname.size());
        const auto& oname = r->GetOwner()->GetUsername();
        w.WriteUInt8(static_cast<uint8_t>(oname.size()));
        w.WriteBytes(oname.data(), oname.size());
    }
    return w.Finish();        
}

void RoomManager::OnWaitingPlayerDisconnected(const std::shared_ptr<Session>& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = rooms_.begin(); it != rooms_.end(); ++it)
        if (it->second->GetState() == RoomState::WAITING && it->second->GetOwner() == session) {
            std::cout << "[Rooms] Room #" << it->first << " closed (owner left)\n";
            rooms_.erase(it);
            return;
        }
}

std::optional<RoomManager::MatchRecord> RoomManager::GetMatch(int session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_matches_.find(session_id);
    if (it == active_matches_.end())
        return std::nullopt;
    return it->second;
}

void RoomManager::RemoveMatch(const std::shared_ptr<Session>& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_matches_.erase(session->GetId());

    for (auto it = rooms_.begin(); it != rooms_.end(); ++it) {
        auto& room = *it->second;
        if (room.GetState() != RoomState::IN_GAME || !room.HasPlayer(session))
            continue;
        auto other = (room.GetOwner() == session) ? room.GetGuest() : room.GetOwner();
        if (!active_matches_.count(other->GetId())) {
            std::cout << "[Rooms] Room #" << room.GetId() << " closed" << std::endl;
            rooms_.erase(it);
        }
        break;
    }
}

void RoomManager::StartTurnWatchdog() {
    if (watchdog_running_.load())
        return;
    watchdog_running_.store(true);
    watchdog_ = std::thread(&RoomManager::WatchdogLoop, this);
    std::cout << "[Rooms] Turn watchdog started\n";
}

void RoomManager::StopTurnWatchdog() {
    if (!watchdog_running_.exchange(false))
        return;
    if (watchdog_.joinable())
        watchdog_.join();
    std::cout << "[Rooms] Turn watchdog stopped\n";    
}

void RoomManager::StartGameInRoom(Room &room, const std::shared_ptr<Session>& guest) {
    auto owner = room.GetOwner();
    room.SetGuest(guest);

    auto game = std::make_shared<Game>(owner, guest, db_);
    room.SetGame(game);

    std::weak_ptr<Game> weak = game;
    owner->SetMessageHandler([weak](MessageType t, const std::vector<uint8_t>& p) {
        if (auto g = weak.lock()) 
            g->ProcessMessage(1, t, p);
    });
    guest->SetMessageHandler([weak](MessageType t, const std::vector<uint8_t>& p) {
        if (auto g = weak.lock()) 
            g->ProcessMessage(2, t, p);
    });

    active_matches_[owner->GetId()] = MatchRecord{game, 1};
    active_matches_[guest->GetId()] = MatchRecord{game, 2};

    const std::string& owner_name = owner->GetUsername();
    const std::string& guest_name = guest->GetUsername();

    MessageWriter f1(MessageType::MATCH_FOUND);
    f1.WriteUInt8(static_cast<uint8_t>(guest_name.size()));
    f1.WriteBytes(guest_name.data(), guest_name.size());
    owner->SendSessionMessage(f1.Finish());

    MessageWriter f2(MessageType::MATCH_FOUND);
    f2.WriteUInt8(static_cast<uint8_t>(owner_name.size()));
    f2.WriteBytes(owner_name.data(), owner_name.size());
    guest->SendSessionMessage(f2.Finish());

    MessageWriter n1(MessageType::PLAYER_NUMBER);
    n1.WriteUInt8(0x01);
    owner->SendSessionMessage(n1.Finish());

    MessageWriter n2(MessageType::PLAYER_NUMBER);
    n2.WriteUInt8(0x02);
    guest->SendSessionMessage(n2.Finish());

    std::cout << "[Room] Game started in room #" << room.GetId() << ": "
              << owner_name << " vs " << guest_name << std::endl;
}

void RoomManager::WatchdogLoop() {
    while (watchdog_running_.load()) {
        std::vector<std::shared_ptr<Game>> games;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::unordered_set<Game*> seen;
            for (const auto& [session_id, rec] : active_matches_)
                if (seen.insert(rec.game.get()).second)
                    games.push_back(rec.game);
        }
        for (auto& game : games)
            game->CheckTurnTimeout();

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

bool RoomManager::IsBusyLocked(const std::shared_ptr<Session>& session) const
{
    if (active_matches_.count(session->GetId()))
        return true;
    for (const auto& [id, room] : rooms_)
        if (room->GetState() == RoomState::WAITING && room->GetOwner() == session)
            return true;
    return false;
}
