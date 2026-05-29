#include "replica_manager.hpp"

#include "protocol/resp_codec.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::replica {

auto ReplicaManager::find(int fd) -> ReplicaState* {
    for (auto& r : replicas_) {
        if (r.fd == fd) {
            return &r;
        }
    }
    return nullptr;
}

auto ReplicaManager::find(int fd) const -> const ReplicaState* {
    for (const auto& r : replicas_) {
        if (r.fd == fd) {
            return &r;
        }
    }
    return nullptr;
}

void ReplicaManager::remove_replica(int fd) {
    for (auto it = replicas_.begin(); it != replicas_.end(); ++it) {
        if (it->fd == fd) {
            std::swap(*it, replicas_.back());
            replicas_.pop_back();
            return;
        }
    }
}

auto ReplicaManager::count_acked_for(int64_t target) const -> int64_t {
    int64_t count = 0;
    for (const auto& r : replicas_) {
        if (r.offset >= target) {
            ++count;
        }
    }
    return count;
}

auto ReplicaManager::process_ack(int fd, std::string_view data) -> AckResult {
    auto* r = find(fd);
    if (!r) {
        return {};
    }

    size_t consumed = 0;
    while (consumed < data.size()) {
        auto result = credis::protocol::parse_one(data.substr(consumed));
        if (!result) {
            break;
        }
        consumed += result->consumed;

        auto& args = result->args;
        if (args.size() >= 3 && credis::util::to_upper(args[0]) == "REPLCONF"
            && credis::util::to_upper(args[1]) == "ACK") {
            if (auto offset = credis::util::parse_int<int64_t>(args[2])) {
                r->offset = *offset;
            }
        }
    }

    AckResult ack_result;
    ack_result.consumed = consumed;

    if (wait_state_) {
        int64_t acked = count_acked_for(wait_state_->target_offset);
        if (acked >= wait_state_->numreplicas) {
            ack_result.wait.emplace(WaitResult{wait_state_->client_fd, acked});
            wait_state_.reset();
        }
    }
    return ack_result;
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
