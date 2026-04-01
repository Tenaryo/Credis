#pragma once

#include <string>

class ReplicaConnector {
    std::string host_;
    int port_;
    int fd_{-1};
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
};
