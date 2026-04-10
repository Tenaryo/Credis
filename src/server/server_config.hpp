#pragma once

#include <optional>
#include <string>

struct ReplicaOfConfig {
    std::string host;
    int port;
};

struct ServerConfig {
    std::optional<ReplicaOfConfig> replicaof;
    std::string master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
    int64_t master_repl_offset = 0;
    std::string dir;
    std::string dbfilename;
    bool is_replica() const { return replicaof.has_value(); }
};
