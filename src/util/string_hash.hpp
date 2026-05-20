#pragma once

#include <string>
#include <string_view>

struct StringHash {
    using is_transparent = void;
    using hash_type = std::hash<std::string_view>;

    auto operator()(std::string_view sv) const noexcept -> size_t {
        return hash_type{}(sv);
    }
    auto operator()(const std::string& s) const noexcept -> size_t {
        return hash_type{}(s);
    }
};
