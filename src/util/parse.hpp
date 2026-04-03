#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

template <typename T>
requires std::is_integral_v<T> std::optional<T> parse_int(std::string_view sv) {
    T value{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{} || ptr != sv.data() + sv.size())
        return std::nullopt;
    return value;
}

inline std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}
