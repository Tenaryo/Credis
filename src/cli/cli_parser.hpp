#pragma once

#include <string_view>

#include "server/server_config.hpp"
#include "util/parse.hpp"

namespace credis::cli {

inline auto parse_port(int argc, char* argv[]) -> int {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--port" && i + 1 < argc) {
            auto port = credis::util::parse_int<int>(argv[i + 1]);
            if (port) {
                return *port;
            }
        }
    }
    return 6379;
}

inline auto parse_replicaof(int argc, char* argv[]) -> std::optional<credis::server::ReplicaConfig> {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--replicaof" && i + 1 < argc) {
            std::string val = argv[i + 1];
            auto space = val.find(' ');
            if (space != std::string::npos) {
                auto port = credis::util::parse_int<int>(val.substr(space + 1));
                if (port) {
                    return credis::server::ReplicaConfig{val.substr(0, space), *port};
                }
            }
        }
    }
    return std::nullopt;
}

inline auto parse_dir(int argc, char* argv[]) -> std::string {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--dir" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return "";
}

inline auto parse_dbfilename(int argc, char* argv[]) -> std::string {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--dbfilename" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return "";
}

inline void apply_aof_overrides(credis::server::ServerConfig& config, int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);
        if (i + 1 < argc) [[likely]] {
            if (arg == "--appendonly") {
                config.appendonly = argv[i + 1];
            } else if (arg == "--appenddirname") {
                config.appenddirname = argv[i + 1];
            } else if (arg == "--appendfilename") {
                config.appendfilename = argv[i + 1];
            } else if (arg == "--appendfsync") {
                config.appendfsync = argv[i + 1];
            }
        }
    }
}

} // namespace credis::cli
