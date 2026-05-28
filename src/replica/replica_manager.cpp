#include "replica_manager.hpp"

#include <algorithm>

#include "protocol/resp_codec.hpp"
#include "util/string_utils.hpp"

namespace credis::replica {

void ReplicaManager::remove_replica(int fd) {
    replica_fds_.erase(fd);
    replica_offsets_.erase(fd);
    replica_buffers_.erase(fd);
}

auto ReplicaManager::count_acked_for(int64_t target) const -> int64_t {
    int64_t count = 0;
    for (int rfd : replica_fds_) {
        auto it = replica_offsets_.find(rfd);
        if (it != replica_offsets_.end() && it->second >= target) [[likely]] {
            ++count;
        }
    }
    return count;
}

auto ReplicaManager::process_ack(int fd, std::string_view data) -> std::optional<WaitResult> {
    auto& buffer = replica_buffers_[fd];
    buffer.append(data);

    while (auto result = credis::protocol::parse_one(buffer)) [[likely]] {
        auto& args = result->args;
        if (args.size() >= 3 && credis::util::to_upper(args[0]) == "REPLCONF"
            && credis::util::to_upper(args[1]) == "ACK") [[unlikely]] {
            replica_offsets_[fd] = std::stoll(args[2]);
        }
        buffer.erase(0, result->consumed);
    }

    if (wait_state_) [[unlikely]] {
        int64_t acked = count_acked_for(wait_state_->target_offset);
        if (acked >= wait_state_->numreplicas) [[unlikely]] {
            WaitResult wr{wait_state_->client_fd, acked};
            wait_state_.reset();
            return wr;
        }
    }
    return std::nullopt;
}

void ReplicaManager::start_wait(int client_fd, int64_t numreplicas, int64_t timeout_ms) {
    auto deadline = timeout_ms == 0 ? std::chrono::steady_clock::now()
                                    : std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    wait_state_.emplace(WaitState{client_fd, numreplicas, master_offset_, deadline});
}

auto ReplicaManager::check_wait_timeout() -> std::optional<WaitResult> {
    if (!wait_state_) [[likely]] {
        return std::nullopt;
    }
    if (std::chrono::steady_clock::now() < wait_state_->deadline) [[likely]] {
        return std::nullopt;
    }
    int64_t count = count_acked_for(wait_state_->target_offset);
    WaitResult wr{wait_state_->client_fd, count};
    wait_state_.reset();
    return wr;
}

auto ReplicaManager::next_deadline() const -> std::optional<std::chrono::steady_clock::time_point> {
    return wait_state_ ? std::optional(wait_state_->deadline) : std::nullopt;
}

} // namespace credis::replica
