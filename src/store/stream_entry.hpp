#pragma once

#include <string>
#include <utility>
#include <vector>

#include "store/stream_id.hpp"

namespace credis::store {
struct StreamEntry {
    std::string id;
    credis::protocol::StreamId parsed_id;
    std::vector<std::pair<std::string, std::string>> fields;
};
} // namespace credis::store
