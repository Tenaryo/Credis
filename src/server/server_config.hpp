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
    std::string master_replid;
    std::string dir;
    std::string dbfilename;
    [[nodiscard]] auto is_replica() const -> bool {
        return replica.has_value();
    }
    void generate_replid();
};

} // namespace credis::server
