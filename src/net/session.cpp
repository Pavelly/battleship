#include "net/session.h"
#include "server/game.h"
#include "common/logger.h"
#include <iostream>
#include <cstring>

std::atomic<int> Session::next_id_{1};

Session::Session(SocketType socket) 
    : id_(next_id_++)
    , socket_(socket)
    , alive_(true) {
    read_buffer_.reserve(1024);
    LOG_INFO << "[Session " << id_ << "] Created";
}

Session::~Session() {
    if (socket_ != INVALID_SOCK) CloseSocket(socket_);
    LOG_DEBUG << "[Session " << id_ << "] Destroyed";
}

void Session::Run() {
    char temp_buffer[1024];

    while (alive_.load()) {
        int bytes_received = recv(socket_, temp_buffer, sizeof(temp_buffer), 0);

        if (bytes_received <= 0) {
            LOG_INFO << "[Session " << id_ << "] Disconnected";
            alive_.store(false);
            break;
        }

        read_buffer_.insert(read_buffer_.end(), temp_buffer, temp_buffer + bytes_received);
        ProcessIncomingData();
    }
}

void Session::SendSessionMessage(const std::vector<uint8_t> &message) {
    if (socket_ == INVALID_SOCK)
        return;
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    if (!alive_.load()) {
        LOG_INFO << "[Session " << id_ << "] Dead";
        return;
    }
    
    int sent = send(socket_, reinterpret_cast<const char*>(message.data()), static_cast<int>(message.size()), 0);
    if (sent <= 0) {
        std::cerr << "[Session " << id_ << "] Send failed";
        alive_.store(false);
    }
}

void Session::SetMessageHandler(IncomingMessageHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    handler_ = std::move(handler);
}

void Session::SetUser(int64_t id, std::string username) {
    user_id_ = id;
    username_ = std::move(username);
}

void Session::Kick() {
    alive_.store(false);
    if (socket_ != INVALID_SOCK)
        ShutdownSocket(socket_);
    LOG_INFO << "[Session " << id_ << "] Kicked by server";
}

void Session::ProcessIncomingData() {
    while (true) {
        MessageType type;
        std::vector<uint8_t> payload;
        size_t msg_size = Protocol::TryReadMessage(read_buffer_, type, payload);

        if (msg_size == 0) 
            break;
        
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + msg_size);
        HandleMessage(type, payload);
    }
}

void Session::HandleMessage(MessageType type, const std::vector<uint8_t>& payload) {
    IncomingMessageHandler handler;
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        handler = handler_;
    }

    if (handler)
        handler(type, payload);
    else 
        LOG_WARN << "[Session " << id_ << "] Message type "
                  << static_cast<int>(type) << " ignored no handler yet";
}