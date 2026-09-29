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

std::vector<OnlineRegistry::OnlineInfo> OnlineRegistry::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<OnlineInfo> out;
    for (const auto& [user_id, weak] : online_)
        if (auto s = weak.lock())
            if (s->IsAlive())
                out.push_back(OnlineInfo{user_id, s->GetId(), s->GetUsername()});
    return out;
}

std::shared_ptr<Session> OnlineRegistry::FindSession(int session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [user_id, weak] : online_)
        if (auto s = weak.lock())
            if (s->GetId() == session_id && s->IsAlive())
                return s;
    return nullptr;
}

bool OnlineRegistry::IsStale(const std::weak_ptr<Session>& entry) const
{
    auto session = entry.lock();
    return !session || !session->IsAlive();
}
