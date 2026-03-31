#pragma once

#include <chrono>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "protocol/stream_id.hpp"

struct BlockedClient {
    int fd;
    std::string key;
    std::chrono::steady_clock::time_point deadline;
    StreamId last_id;

    bool is_indefinite() const { return deadline == std::chrono::steady_clock::time_point::max(); }
};

class BlockingManager {
    std::unordered_map<std::string, std::list<BlockedClient>> blocked_clients_;
    std::unordered_map<int, std::list<BlockedClient>::iterator> fd_to_client_;
  public:
    void block_client(int fd, std::string key, std::chrono::milliseconds timeout);
    void block_client_for_stream(int fd,
                                 std::string key,
                                 StreamId last_id,
                                 std::chrono::milliseconds timeout);
    std::optional<BlockedClient> wake_client(const std::string& key);
    std::optional<BlockedClient> wake_client_for_stream(const std::string& key,
                                                        const std::string& new_entry_id);
    std::vector<int> get_expired_clients();
    void unblock_client(int fd);
    std::optional<std::chrono::steady_clock::time_point> get_next_deadline() const;
    bool is_blocked(int fd) const;
    size_t blocked_count() const;
};
