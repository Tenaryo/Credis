#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Redis {
struct StreamEntry {
    std::string id;
    std::vector<std::pair<std::string, std::string>> fields;
};
} // namespace Redis
