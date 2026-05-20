#pragma once

#include <chrono>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "store/stream_id.hpp"

namespace credis::blocking {

struct BlockedClient {
    int fd;
    std::string key;
    std::chrono::steady_clock::time_point deadline;
    credis::protocol::StreamId last_id;

    [[nodiscard]] auto is_indefinite() const -> bool {
        return deadline == std::chrono::steady_clock::time_point::max();
    }
};

class BlockingManager {
    std::unordered_map<std::string, std::list<BlockedClient>> blocked_clients_;
    std::unordered_map<int, std::list<BlockedClient>::iterator> fd_to_client_;

  public:
    void block_client(int fd, const std::string& key, std::chrono::milliseconds timeout);
    void block_client_for_stream(int fd,
                                 const std::string& key,
                                 credis::protocol::StreamId last_id,
                                 std::chrono::milliseconds timeout);
    auto wake_client(const std::string& key) -> std::optional<BlockedClient>;
    auto wake_client_for_stream(const std::string& key,
                                const std::string& new_entry_id) -> std::optional<BlockedClient>;
    auto get_expired_clients() -> std::vector<int>;
    void unblock_client(int fd);
    auto get_next_deadline() const -> std::optional<std::chrono::steady_clock::time_point>;
    auto is_blocked(int fd) const -> bool;
    auto blocked_count() const -> size_t;
};

} // namespace credis::blocking
