#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace credis::handler {

struct ProcessResult {
    struct Normal {
        std::string response;
    };
    struct Block {};
    struct ReplicaHandshake {
        std::string response;
    };
    struct Wait {
        int64_t numreplicas{0};
        int64_t timeout_ms{0};
    };

    std::variant<Normal, Block, ReplicaHandshake, Wait> state;
    std::vector<std::string> propagate_cmds;
    size_t consumed{0};

    static auto normal(std::string resp) -> ProcessResult {
        return {Normal{std::move(resp)}, {}};
    }
    static auto block() -> ProcessResult {
        return {Block{}, {}};
    }
    static auto replica_handshake(std::string resp) -> ProcessResult {
        return {ReplicaHandshake{std::move(resp)}, {}};
    }
    static auto wait(int64_t num, int64_t timeout) -> ProcessResult {
        return {Wait{num, timeout}, {}};
    }
};

struct TransactionState {
    bool in_multi{false};
    std::vector<std::vector<std::string>> queued_commands;
    std::unordered_map<std::string, uint64_t> watched_keys;
};

} // namespace credis::handler
