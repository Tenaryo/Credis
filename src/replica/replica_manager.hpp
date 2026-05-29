#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace credis::replica {

struct ReplicaState {
    int fd;
    int64_t offset{0};
};

struct WaitResult {
    int client_fd;
    int64_t count;
};

struct AckResult {
    std::optional<WaitResult> wait;
    size_t consumed{0};
};

struct WaitState {
    int client_fd;
    int64_t numreplicas;
    int64_t target_offset;
    std::chrono::steady_clock::time_point deadline;
};

class ReplicaManager {
    std::vector<ReplicaState> replicas_;
    int64_t master_offset_{0};
    std::optional<WaitState> wait_state_;

    auto find(int fd) -> ReplicaState*;
    [[nodiscard]] auto find(int fd) const -> const ReplicaState*;

  public:
    auto count_acked_for(int64_t target) const -> int64_t;

    void add_replica(int fd) {
        replicas_.push_back({fd, 0});
    }
    void remove_replica(int fd);
    [[nodiscard]] auto count() const -> size_t {
        return replicas_.size();
    }
    [[nodiscard]] auto contains(int fd) const -> bool {
        return find(fd) != nullptr;
    }
    [[nodiscard]] auto replicas() const -> const std::vector<ReplicaState>& {
        return replicas_;
    }

    auto process_ack(int fd, std::string_view data) -> AckResult;

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
