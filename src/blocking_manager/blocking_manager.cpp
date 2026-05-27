#include "blocking_manager.hpp"

namespace credis::blocking {

void BlockingManager::block_client(int fd, const std::string& key, std::chrono::milliseconds timeout) {
    block_client_for_stream(fd, key, credis::protocol::StreamId{}, timeout);
}

void BlockingManager::block_client_for_stream(int fd,
                                              const std::string& key,
                                              credis::protocol::StreamId last_id,
                                              std::chrono::milliseconds timeout) {
    auto deadline = timeout.count() == 0 ? std::chrono::steady_clock::time_point::max()
                                         : std::chrono::steady_clock::now() + timeout;

    BlockedClient client{fd, key, deadline, last_id};
    auto& queue = blocked_clients_[key];
    queue.push_back(std::move(client));
    fd_to_client_[fd] = std::prev(queue.end());
}

auto BlockingManager::wake_client(const std::string& key) -> std::optional<BlockedClient> {
    auto it = blocked_clients_.find(key);
    if (it == blocked_clients_.end() || it->second.empty()) {
        return std::nullopt;
    }

    BlockedClient client = std::move(it->second.front());
    it->second.pop_front();
    fd_to_client_.erase(client.fd);

    if (it->second.empty()) {
        blocked_clients_.erase(it);
    }

    return client;
}

auto BlockingManager::wake_client_for_stream(const std::string& key,
                                             const std::string& new_entry_id) -> std::optional<BlockedClient> {
    auto it = blocked_clients_.find(key);
    if (it == blocked_clients_.end() || it->second.empty()) {
        return std::nullopt;
    }

    auto new_sid = credis::protocol::StreamId::parse(new_entry_id);
    if (!new_sid) [[unlikely]] {
        return std::nullopt;
    }

    for (auto client_it = it->second.begin(); client_it != it->second.end(); ++client_it) {
        if (client_it->last_id < *new_sid) {
            BlockedClient client = std::move(*client_it);
            it->second.erase(client_it);
            fd_to_client_.erase(client.fd);

            if (it->second.empty()) {
                blocked_clients_.erase(it);
            }

            return client;
        }
    }

    return std::nullopt;
}

auto BlockingManager::get_expired_clients() -> std::vector<int> {
    std::vector<int> expired;
    auto now = std::chrono::steady_clock::now();

    for (auto it = blocked_clients_.begin(); it != blocked_clients_.end();) {
        auto& queue = it->second;
        for (auto client_it = queue.begin(); client_it != queue.end();) {
            auto& client = *client_it;
            if (!client.is_indefinite() && client.deadline <= now) {
                expired.push_back(client.fd);
                fd_to_client_.erase(client.fd);
                client_it = queue.erase(client_it);
            } else {
                ++client_it;
            }
        }
        if (queue.empty()) {
            it = blocked_clients_.erase(it);
        } else {
            ++it;
        }
    }

    return expired;
}

void BlockingManager::unblock_client(int fd) {
    auto it = fd_to_client_.find(fd);
    if (it == fd_to_client_.end()) {
        return;
    }

    std::string key = it->second->key;
    auto queue_it = blocked_clients_.find(key);
    if (queue_it != blocked_clients_.end()) [[likely]] {
        queue_it->second.erase(it->second);
        if (queue_it->second.empty()) {
            blocked_clients_.erase(queue_it);
        }
    }

    fd_to_client_.erase(it);
}

auto BlockingManager::get_next_deadline() const -> std::optional<std::chrono::steady_clock::time_point> {
    std::optional<std::chrono::steady_clock::time_point> earliest;
    for (const auto& [key, queue] : blocked_clients_) {
        for (const auto& client : queue) {
            if (!client.is_indefinite()) {
                if (!earliest || client.deadline < *earliest) {
                    earliest = client.deadline;
                }
            }
        }
    }
    return earliest;
}

auto BlockingManager::is_blocked(int fd) const -> bool {
    return fd_to_client_.contains(fd);
}

auto BlockingManager::blocked_count() const -> size_t {
    return fd_to_client_.size();
}

} // namespace credis::blocking
