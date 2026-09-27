#pragma once

#include "net/platform.h"
#include "net/online_registry.h"
#include "server/lobby.h"
#include "db/auth_service.h"
#include <cstdint>

class Server {
public:
    explicit Server(uint16_t port, Database& db);
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
    Lobby lobby_;
    SocketType listen_socket_;
    bool running_;

    SocketType AcceptClient();
    void HandleClient(SocketType client_socket);
    void HandleAuthMessage(std::shared_ptr<Session> session, MessageType type, const std::vector<uint8_t>& payload);
    void SendAuthFail(const std::shared_ptr<Session>& session, uint16_t code);
};