#pragma once

#include <optional>
#include <string>

namespace credis::server {

struct ReplicaConfig {
    std::string host;
    int port;
};

struct ServerConfig {
    std::optional<ReplicaConfig> replica;
    std::string master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
    int64_t master_repl_offset = 0;
    std::string dir;
    std::string dbfilename;
    std::string appendonly = "no";
    std::string appenddirname = "appendonlydir";
    std::string appendfilename = "appendonly.aof";
    std::string appendfsync = "everysec";
    [[nodiscard]] auto is_replica() const -> bool {
        return replica.has_value();
    }
};

} // namespace credis::server
