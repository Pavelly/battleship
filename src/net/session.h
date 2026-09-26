#pragma once

#include "net/platform.h"
#include "common/protocol.h"
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <functional>

class Session : public std::enable_shared_from_this<Session> {
public:
    using IncomingMessageHandler =
        std::function<void(MessageType, const std::vector<uint8_t>&)>;

    explicit Session(SocketType socket);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void Run();
    void SendSessionMessage(const std::vector<uint8_t>& message);

    void SetMessageHandler(IncomingMessageHandler handler);

    int GetId() const { return id_; }
    bool IsAlive() const { return alive_.load(); }
private:
    static std::atomic<int> next_id_;

    int id_;
    SocketType socket_;
    std::atomic<bool> alive_;
    std::mutex send_mutex_;
    std::vector<uint8_t> read_buffer_;

    std::mutex handler_mutex_;
    IncomingMessageHandler handler_;

    void ProcessIncomingData();
    void HandleMessage(MessageType type, const std::vector<uint8_t>& payload);
};