#include "resp_codec.hpp"

#include <charconv>

namespace credis::protocol {

using credis::util::Error;
using credis::util::ErrorCode;

auto parse_one(std::string_view input) -> std::expected<ParsedCommand, Error> {
    if (input.empty() || input[0] != '*') {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: expected array"));
    }

    size_t pos = 1;
    auto crlf = input.find("\r\n", pos);
    if (crlf == std::string_view::npos) {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Incomplete RESP: missing CRLF after array count"));
    }

    int count = 0;
    auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + crlf, count);
    if (ec != std::errc{} || count < 0) {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: invalid array count"));
    }

    pos = crlf + 2;

    std::vector<std::string> args;
    for (int i = 0; i < count; ++i) {
        if (pos >= input.size() || input[pos] != '$') {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: expected bulk string"));
        }

        crlf = input.find("\r\n", pos + 1);
        if (crlf == std::string_view::npos) {
            return std::unexpected(
                Error(ErrorCode::kProtocolError, "Incomplete RESP: missing CRLF after bulk string length"));
        }

        int len = 0;
        auto [ptr2, ec2] = std::from_chars(input.data() + pos + 1, input.data() + crlf, len);
        if (ec2 != std::errc{} || len < 0) {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: invalid bulk string length"));
        }

        pos = crlf + 2;

        if (pos + static_cast<size_t>(len) > input.size()) {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Incomplete RESP: bulk string truncated"));
        }

        args.emplace_back(input.substr(pos, static_cast<size_t>(len)));
        pos += static_cast<size_t>(len) + 2;
    }

    return ParsedCommand{std::move(args), pos};
}

auto encode_simple_string(std::string_view s) -> std::string {
    return "+" + std::string(s) + "\r\n";
}

auto encode_bulk_string(std::string_view s) -> std::string {
    return "$" + std::to_string(s.size()) + "\r\n" + std::string(s) + "\r\n";
}

auto encode_null_bulk_string() -> std::string {
    return "$-1\r\n";
}

auto encode_integer(int64_t n) -> std::string {
    return ":" + std::to_string(n) + "\r\n";
}

auto encode_array(const std::vector<std::string>& elements) -> std::string {
    std::string result = "*" + std::to_string(elements.size()) + "\r\n";
    for (const auto& elem : elements) {
        result += encode_bulk_string(elem);
    }
    return result;
}

auto encode_raw_array(const std::vector<std::string>& raw_elements) -> std::string {
    std::string result = "*" + std::to_string(raw_elements.size()) + "\r\n";
    for (const auto& elem : raw_elements) {
        result += elem;
    }
    return result;
}

auto encode_entries(std::span<const credis::store::StreamEntry> entries) -> std::string {
    std::string result = "*" + std::to_string(entries.size()) + "\r\n";
    for (const auto& entry : entries) {
        result += "*2\r\n";
        result += encode_bulk_string(entry.id);
        result += "*" + std::to_string(entry.fields.size() * 2) + "\r\n";
        for (const auto& [field, value] : entry.fields) {
            result += encode_bulk_string(field);
            result += encode_bulk_string(value);
        }
    }
    return result;
}

auto encode_error(std::string_view s) -> std::string {
    return "-" + std::string(s) + "\r\n";
}

auto encode_null_array() -> std::string {
    return "*-1\r\n";
}

auto encode_stream_entries(
    const std::vector<std::pair<std::string, std::span<const credis::store::StreamEntry>>>& streams) -> std::string {
    std::string result = "*" + std::to_string(streams.size()) + "\r\n";
    for (const auto& [key, entries] : streams) {
        result += "*2\r\n";
        result += encode_bulk_string(key);
        result += encode_entries(entries);
    }
    return result;
}

} // namespace credis::protocol
