#include "server.h"
#include "game.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <memory>

Server::Server(uint16_t port) 
    : port_(port)
    , listen_socket_(INVALID_SOCK)
    , running_(false) {}

Server::~Server() {
    Stop();
}

bool Server::Start() {
    if (!InitNetwork()) {
        std::cerr << "Failed to initialize network\n";
        return false;
    }

    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ == INVALID_SOCK) {
        std::cerr << "Failed to create socket. Error: " << GetLastSocketError() << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(
            listen_socket_, 
            SOL_SOCKET, 
            SO_REUSEADDR, 
            reinterpret_cast<const char*>(&opt), 
            sizeof(opt)
        ) < 0
    ) {
        std::cerr << "setsockopt failed\n";
        CloseSocket(listen_socket_);
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed. Error: " << GetLastSocketError() << std::endl;
        std::cerr << "Port " << port_ << " might be in use\n";
        CloseSocket(listen_socket_);
        return false;
    }

    if (listen(listen_socket_, 10) < 0) {
        std::cerr << "Listen failed\n";
        CloseSocket(listen_socket_);
        return false;
    }

    std::cout << "Server listening on port " << port_ << "...\n";
    running_ = true;
    return true;
}

void Server::Stop() {
    if (listen_socket_ != INVALID_SOCK) {
        CloseSocket(listen_socket_);
        listen_socket_ = INVALID_SOCK;
    }
    running_ = false;
    CleanupNetwork();
}

void Server::Run() {
    if (!running_) {
        std::cerr << "Server not started\n";
        return;
    }

    std::cout << "Waiting for connection...\n";
    
    while (running_) {
        SocketType client_socket = AcceptClient();
        if (client_socket == INVALID_SOCK) {
            if (running_) {
                std::cerr << "Accept failed\n";
            }
            continue;
        }
        std::cout << "Client connected!\n";
        std::thread client_thread(&Server::HandleClient, this, client_socket);
        client_thread.detach();
    }
}

SocketType Server::AcceptClient() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    return accept(listen_socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
}

void Server::HandleClient(SocketType client_socket) {
    auto session = std::make_shared<Session>(client_socket);

    MessageWriter welcome(MessageType::GAME_START);
    welcome.WriteUInt8(1);
    session->SendSessionMessage(welcome.Finish());

    lobby_.TryMatch(session);
    session->Run();
    lobby_.RemoveFromQueue(session);

    if (auto match = lobby_.GetMatch(session->GetId())) {
        match->game->OnPlayerDisconnect(match->player_number);
        lobby_.RemoveMatch(session->GetId());
    }

    std::cout << "[Server] Client handler finished\n";
}

void Server::HandleMessage(SocketType client_socket, MessageType type, const std::vector<uint8_t> &payload) {
    std::cout << "Received message type: " << static_cast<int>(type)
              << ", payload size: " << payload.size() << std::endl;

    switch (type) {
    case MessageType::SHOT: {
        if (payload.size() >= 2) {
            uint8_t col = payload[0];
            uint8_t row = payload[1];
            std::cout << "Shot at column " << static_cast<int>(col)
                      << ", row " << static_cast<int>(row) << std::endl;
            
            MessageWriter result(MessageType::SHOT_RESULT);
            result.WriteUInt8(static_cast<uint8_t>(ShotResult::HIT));
            auto result_msg = result.Finish();
            send(client_socket, reinterpret_cast<const char*>(result_msg.data()), static_cast<int>(result_msg.size()), 0);
        }
        break;
    }
    default:
        std::cerr << "Unknown message type: " << static_cast<int>(type) << std::endl;
        break;
    }
}
