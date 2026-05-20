#pragma once

#include <string>
#include <utility>
#include <vector>

namespace credis::store {
struct StreamEntry {
    std::string id;
    std::vector<std::pair<std::string, std::string>> fields;
};
} // namespace credis::store
