#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "store/store.hpp"

struct RdbEntry {
    Redis::Value value;
    std::optional<uint64_t> expire_ms;
};

class RdbParser {
  public:
    static auto parse(const std::vector<uint8_t>& data) -> std::unordered_map<std::string, RdbEntry>;
    static auto load_file(const std::string& path) -> std::unordered_map<std::string, RdbEntry>;
};
