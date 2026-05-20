#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace credis::util {

inline auto to_upper(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

inline auto to_lower(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace credis::util
