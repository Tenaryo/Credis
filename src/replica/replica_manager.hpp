#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace credis::replica {

struct WaitState {
    int client_fd;
    int64_t numreplicas;
    int64_t target_offset;
    std::chrono::steady_clock::time_point deadline;
};

struct WaitResult {
    int client_fd;
    int64_t count;
};

class ReplicaManager {
    std::unordered_set<int> replica_fds_;
    std::unordered_map<int, int64_t> replica_offsets_;
    std::unordered_map<int, std::string> replica_buffers_;
    int64_t master_offset_{0};
    std::optional<WaitState> wait_state_;

  public:
    auto count_acked_for(int64_t target) const -> int64_t;

    void add_replica(int fd) {
        replica_fds_.insert(fd);
    }
    void remove_replica(int fd);
    [[nodiscard]] auto count() const -> size_t {
        return replica_fds_.size();
    }
    [[nodiscard]] auto replica_fds() const -> const std::unordered_set<int>& {
        return replica_fds_;
    }

    auto process_ack(int fd, std::string_view data) -> std::optional<WaitResult>;

    void propagate(const std::string& msg) {
        master_offset_ += static_cast<int64_t>(msg.size());
    }

    void start_wait(int client_fd, int64_t numreplicas, int64_t timeout_ms);
    auto check_wait_timeout() -> std::optional<WaitResult>;
    [[nodiscard]] auto has_wait() const -> bool {
        return wait_state_.has_value();
    }
    [[nodiscard]] auto next_deadline() const -> std::optional<std::chrono::steady_clock::time_point>;
    [[nodiscard]] auto offset() const -> int64_t {
        return master_offset_;
    }
};

} // namespace credis::replica
