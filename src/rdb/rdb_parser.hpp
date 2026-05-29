#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "store/store.hpp"

namespace credis::rdb {

struct RdbEntry {
    credis::store::Value value;
    std::optional<uint64_t> expire_ms;
};

auto parse_rdb(const std::vector<uint8_t>& data) -> std::unordered_map<std::string, RdbEntry>;
auto load_rdb_file(const std::string& path) -> std::unordered_map<std::string, RdbEntry>;
void load_into_store(credis::store::Store& store, const std::string& dir, const std::string& dbfilename);

} // namespace credis::rdb
