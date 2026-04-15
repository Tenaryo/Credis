#pragma once

#include <string>
#include <string_view>

struct StringHash {
    using is_transparent = void;
    using hash_type = std::hash<std::string_view>;

    size_t operator()(std::string_view sv) const noexcept { return hash_type{}(sv); }
    size_t operator()(const std::string& s) const noexcept { return hash_type{}(s); }
};
