#pragma once

#include "net/platform.h"
#include "net/online_registry.h"
#include "server/admin_console.h"
#include "server/room_manager.h"
#include "db/auth_service.h"
#include <cstdint>

struct ServerSettings {
    uint16_t port = 9090;
    int turn_timeout_seconds = 30;
    int max_turn_timeouts = 3;
    bool admin_enabled = true;
};

class Server {
public:
    explicit Server(const ServerSettings& settings, Database& db);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool Start();
    void Stop();
    void Run();
private:
    uint16_t port_;
    Database& db_;
    AuthService auth_;
    OnlineRegistry online_;
    RoomManager rooms_;
    AdminConsole admin_;
    bool admin_enabled_;
    SocketType listen_socket_;
    bool running_;

    SocketType AcceptClient();
    void HandleClient(SocketType client_socket);
    void HandleAuthMessage(std::shared_ptr<Session> session, MessageType type, const std::vector<uint8_t>& payload);
    void HandleLobbyMessage(std::shared_ptr<Session> session, MessageType type, const std::vector<uint8_t>& payload);
    void SendAuthFail(const std::shared_ptr<Session>& session, uint16_t code);
};