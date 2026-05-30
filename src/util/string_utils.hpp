#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace credis::util {

inline auto to_upper(std::string_view sv) -> std::string {
    std::string result(sv);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::toupper(c); });
    return result;
}

inline auto to_lower(std::string_view sv) -> std::string {
    std::string result(sv);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace credis::util
