#pragma once

#include "server/server_config.hpp"
#include "util/parse.hpp"
#include <string_view>

inline int parse_port(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--port" && i + 1 < argc) {
            auto port = parse_int<int>(argv[i + 1]);
            if (port)
                return *port;
        }
    }
    return 6379;
}

inline std::optional<ReplicaOfConfig> parse_replicaof(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--replicaof" && i + 1 < argc) {
            std::string val = argv[i + 1];
            auto space = val.find(' ');
            if (space != std::string::npos) {
                auto port = parse_int<int>(val.substr(space + 1));
                if (port)
                    return ReplicaOfConfig{val.substr(0, space), *port};
            }
        }
    }
    return std::nullopt;
}

inline std::string parse_dir(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--dir" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return "";
}

inline std::string parse_dbfilename(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--dbfilename" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return "";
}
