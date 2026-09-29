#pragma once

#include "net/online_registry.h"
#include "server/room_manager.h"
#include "db/database.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

class AdminConsole {
public:
    AdminConsole(const OnlineRegistry& online,
                 const RoomManager& rooms,
                 Database& db,
                 std::function<void()> shutdown_cb);
    ~AdminConsole();

    AdminConsole(const AdminConsole&) = delete;
    AdminConsole& operator=(const AdminConsole&) = delete;

    void Start();
private:
    void Loop();
    void HandleCommand(const std::string& line);
    void CmdHelp();
    void CmdStatus();
    void CmdRooms();
    void CmdOnline();
    void CmdGames();
    void CmdKick(const std::string& arg);
    void CmdShutdown();

    const OnlineRegistry& online_;
    const RoomManager& rooms_;
    Database& db_;
    std::function<void()> shutdown_cb_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point start_time_;
};