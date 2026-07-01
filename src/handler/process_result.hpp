#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace credis::handler {

struct ProcessResult {
    struct Normal {};
    struct Block {};
    struct ReplicaHandshake {};
    struct Wait {
        int64_t numreplicas{0};
        int64_t timeout_ms{0};
    };

    std::variant<Normal, Block, ReplicaHandshake, Wait> state;
    std::vector<std::string> propagate_cmds;
    size_t consumed{0};

    static auto normal() -> ProcessResult {
        return {Normal{}, {}};
    }
    static auto block() -> ProcessResult {
        return {Block{}, {}};
    }
    static auto replica_handshake() -> ProcessResult {
        return {ReplicaHandshake{}, {}};
    }
    static auto wait(int64_t num, int64_t timeout) -> ProcessResult {
        return {Wait{num, timeout}, {}};
    }
};

} // namespace credis::handler
