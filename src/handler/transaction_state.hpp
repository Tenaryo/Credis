#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace credis::handler {

struct TransactionState {
    bool in_multi{false};
    std::vector<std::vector<std::string>> queued_commands;
    std::unordered_map<std::string, uint64_t> watched_keys;
};

} // namespace credis::handler
