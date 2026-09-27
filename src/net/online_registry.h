#pragma once

#include "net/session.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

class OnlineRegistry {
public:
    bool TryAcquire(int64_t user_id, const std::shared_ptr<Session>& session);
    void Release(int64_t user_id, const std::shared_ptr<Session>& session);
    size_t Size() const;
private:
    bool IsStale(const std::weak_ptr<Session>& entry) const;
    
    mutable std::mutex mutex_;
    std::unordered_map<int64_t, std::weak_ptr<Session>> online_;
};