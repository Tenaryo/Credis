#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class CommandHandler;

class ReplicaConnector {
    std::string host_;
    int port_;
    int fd_{-1};
    std::string pending_buffer_;
    int64_t offset_{0};
    CommandHandler* handler_{nullptr};

    auto send_and_expect(const std::vector<std::string>& args, std::string_view expected_response) -> bool;
    auto process_buffer_impl() -> std::string;

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

    void set_handler(CommandHandler& handler) {
        handler_ = &handler;
    }
    auto process_propagated_commands() -> std::optional<std::string>;
    auto process_pending_buffer() -> std::string;
    void send_response(std::string_view data) const;
    [[nodiscard]] auto master_fd() const noexcept -> int {
        return fd_;
    }
};
