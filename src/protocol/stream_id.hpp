#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct StreamId {
    int64_t timestamp{};
    int64_t sequence{};

    static std::optional<StreamId> parse(std::string_view id) {
        auto dash = id.find('-');
        if (dash == std::string_view::npos)
            return std::nullopt;
        try {
            size_t pos{};
            auto ts = std::stoll(std::string(id.substr(0, dash)), &pos);
            if (pos != dash)
                return std::nullopt;
            auto seq = std::stoll(std::string(id.substr(dash + 1)), &pos);
            if (pos != id.size() - dash - 1)
                return std::nullopt;
            return StreamId{ts, seq};
        } catch (...) {
            return std::nullopt;
        }
    }

    std::string to_string() const {
        return std::to_string(timestamp) + "-" + std::to_string(sequence);
    }

    auto operator<=>(const StreamId&) const = default;
};
