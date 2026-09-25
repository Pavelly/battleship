#pragma once

#include "platform.h"
#include "protocol.h"
#include "lobby.h"
#include <cstdint>
#include <string>

class Server {
public:
    explicit Server(uint16_t port);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool Start();
    void Stop();
    void Run();
private:
    uint16_t port_;
    SocketType listen_socket_;
    bool running_;

    Lobby lobby_;

    SocketType AcceptClient();
    void HandleClient(SocketType client_socket);
    void HandleMessage(SocketType client_socket, MessageType type, const std::vector<uint8_t>& payload);
};