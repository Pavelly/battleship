#include "online_registry.h"

bool OnlineRegistry::TryAcquire(int64_t user_id, const std::shared_ptr<Session>& session) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = online_.find(user_id);
    if (it != online_.end() && !IsStale(it->second))
        return false;

    online_[user_id] = session;
    return true;
}

void OnlineRegistry::Release(int64_t user_id, const std::shared_ptr<Session>& session) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = online_.find(user_id);
    if (it == online_.end()) return;

    if (it->second.lock() == session)
        online_.erase(it);
}

size_t OnlineRegistry::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return online_.size();
}

bool OnlineRegistry::IsStale(const std::weak_ptr<Session> &entry) const
{
    auto session = entry.lock();
    return !session || !session->IsAlive();
}
