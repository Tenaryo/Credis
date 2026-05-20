#pragma once

#include <charconv>
#include <optional>
#include <string_view>
#include <type_traits>

namespace credis::util {

template <typename T>
    requires std::is_integral_v<T>
auto parse_int(std::string_view sv) -> std::optional<T> {
    T value{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) {
        return std::nullopt;
    }
    return value;
}

inline auto parse_double(std::string_view sv) -> std::optional<double> {
    double value{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) {
        return std::nullopt;
    }
    return value;
}

} // namespace credis::util
