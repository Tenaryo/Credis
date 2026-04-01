#pragma once

#include <string>
#include <string_view>
#include <vector>

class ReplicaConnector {
    std::string host_;
    int port_;
    int fd_{-1};

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
};
