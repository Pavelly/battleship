#include "net/session.h"
#include "server/game.h"
#include <iostream>
#include <cstring>

std::atomic<int> Session::next_id_{1};

Session::Session(SocketType socket) 
    : id_(next_id_++)
    , socket_(socket)
    , alive_(true) {
    read_buffer_.reserve(1024);
    std::cout << "[Session " << id_ << "] Created\n";
}

Session::~Session() {
    CloseSocket(socket_);
    std::cout << "[Session " << id_ << "] Destroyed\n";
}

void Session::Run() {
    char temp_buffer[1024];

    while (alive_.load()) {
        int bytes_received = recv(socket_, temp_buffer, sizeof(temp_buffer), 0);

        if (bytes_received <= 0) {
            std::cout << "[Session " << id_ << "] Disconnected\n";
            alive_.store(false);
            break;
        }

        read_buffer_.insert(read_buffer_.end(), temp_buffer, temp_buffer + bytes_received);
        ProcessIncomingData();
    }
}

void Session::SendSessionMessage(const std::vector<uint8_t> &message) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    if (!alive_.load()) {
        std::cout << "[Session " << id_ << "] Dead";
        return;
    }
    
    int sent = send(socket_, reinterpret_cast<const char*>(message.data()), static_cast<int>(message.size()), 0);
    if (sent <= 0) {
        std::cerr << "[Session " << id_ << "] Send failed\n";
        alive_.store(false);
    }
}

void Session::SetGame(std::shared_ptr<Game> game) {
    game_ = game;
}

std::shared_ptr<Game> Session::GetGame() const {
    return game_;
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

// void Session::HandleMessage(MessageType type, const std::vector<uint8_t>& payload) {
//     std::cout << "[Session " << id_ << "] Received message type: "
//               << static_cast<int>(type) 
//               << ", payload size: " << payload.size() << std::endl;
    
//     switch (type) {
//         case MessageType::SHOT:
//             if (payload.size() >= 2) {
//                 uint8_t col = payload[0];
//                 uint8_t row = payload[1];
//                 std::cout << "[Session " << id_ << "] Shot at col = "
//                           << static_cast<int>(col)
//                           << ", row = " << static_cast<int>(row) << std::endl;
//             } else {
//                 std::cerr << "[Session " << id_ << "] Invalid SHOT payload\n";
//             }
//             break;
//         case MessageType::PLACE_SHIPS:
//             std::cout << "[Session " << id_ << "] Place ships request\n";
//             // TODO: Logic
//             break;
//         default:
//             std::cout << "[Session " << id_ << "] Unknown message type\n";
//             break;
//     }
// }

void Session::HandleMessage(MessageType type, const std::vector<uint8_t>& payload) {
    std::cout << "[Session " << id_ << "] Received message type: "
              << static_cast<int>(type) 
              << ", payload size: " << payload.size() << std::endl;

    if (game_) {
        game_->ProcessMessage(player_number_, type, payload);
        return;
    }

    std::cout << "[Session " << id_ << "] No game yet, ignoring\n";
}