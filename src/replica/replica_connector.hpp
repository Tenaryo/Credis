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

    bool send_and_expect(const std::vector<std::string>& args, std::string_view expected_response);

    template <typename Pred> bool send_and_check(const std::vector<std::string>& args, Pred&& pred);
  public:
    ReplicaConnector(std::string host, int port);
    ~ReplicaConnector();

    ReplicaConnector(const ReplicaConnector&) = delete;
    ReplicaConnector& operator=(const ReplicaConnector&) = delete;
    ReplicaConnector(ReplicaConnector&&) noexcept;
    ReplicaConnector& operator=(ReplicaConnector&&) noexcept;

    bool connect_to_master();
    bool send_ping();
    bool send_replconf(int listening_port);
    bool send_psync();
    std::optional<std::string> receive_rdb();

    void set_handler(CommandHandler& handler) { handler_ = &handler; }
    auto process_propagated_commands() -> std::optional<std::string>;
    void send_response(std::string_view data);
    [[nodiscard]] auto master_fd() const noexcept -> int { return fd_; }
};
