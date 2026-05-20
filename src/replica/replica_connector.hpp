#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "server/server_config.hpp"

namespace credis::replica {

struct ProcessedBuffer {
    std::string ack_responses;
    std::vector<std::string_view> commands;
};

class ReplicaConnector {
    std::string host_;
    int port_;
    int fd_{-1};
    std::string pending_buffer_;
    int64_t offset_{0};

    auto send_and_expect(const std::vector<std::string>& args, std::string_view expected_response) -> bool;
    auto process_buffer_impl() -> ProcessedBuffer;

    template <typename Pred>
    auto send_and_check(const std::vector<std::string>& args, Pred&& pred) -> bool;

  public:
    ReplicaConnector(std::string host, int port);
    ~ReplicaConnector();

    ReplicaConnector(const ReplicaConnector&) = delete;
    auto operator=(const ReplicaConnector&) -> ReplicaConnector& = delete;
    ReplicaConnector(ReplicaConnector&&) noexcept;
    auto operator=(ReplicaConnector&&) noexcept -> ReplicaConnector&;

    auto connect_to_master() -> bool;
    auto send_ping() -> bool;
    auto send_replconf(int listening_port) -> bool;
    auto send_psync() -> bool;
    auto receive_rdb() -> std::optional<std::string>;

    auto process_propagated_commands() -> std::optional<ProcessedBuffer>;
    auto process_pending_buffer() -> ProcessedBuffer;
    void send_response(std::string_view data) const;
    [[nodiscard]] auto master_fd() const noexcept -> int {
        return fd_;
    }
};

auto connect_if_replica(const credis::server::ServerConfig& config,
                        int listening_port) -> std::optional<ReplicaConnector>;

} // namespace credis::replica
