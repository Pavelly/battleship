#pragma once

#include "server/room.h"
#include "common/protocol.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

class Database;
class Game;

class RoomManager {
public:
    enum class JoinResult : uint8_t {
        OK                  = 0,
        NOT_FOUND           = 0x01,
        FULL_OR_IN_GAME     = 0x02,
        ALREADY_IN_ROOM     = 0x03,
        OWN_ROOM            = 0x04
    };

    struct MatchRecord {
        std::shared_ptr<Game> game;
        int player_number;
    };

    explicit RoomManager(Database& db);
    ~RoomManager();

    uint16_t CreateRoom(const std::shared_ptr<Session>& session, const std::string& name, bool is_private);
    JoinResult JoinRoom(const std::shared_ptr<Session>& session, uint16_t room_id);
    
    std::vector<uint8_t> BuildRoomListPayload() const;

    void OnWaitingPlayerDisconnected(const std::shared_ptr<Session>& session);
    std::optional<MatchRecord> GetMatch(int session_id) const;
    void RemoveMatch(const std::shared_ptr<Session>& session);

    void StartTurnWatchdog();
    void StopTurnWatchdog();
private:
    void StartGameInRoom(Room& room, const std::shared_ptr<Session>& guest);
    void WatchdogLoop();
    bool IsBusyLocked(const std::shared_ptr<Session>& session) const;

    Database& db_;
    mutable std::mutex mutex_;
    std::unordered_map<uint16_t, std::shared_ptr<Room>> rooms_;
    std::unordered_map<int, MatchRecord> active_matches_;
    uint16_t next_room_id_ = 1;

    std::thread watchdog_;
    std::atomic<bool> watchdog_running_{false};
};