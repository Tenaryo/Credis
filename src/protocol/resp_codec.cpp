#include "resp_codec.hpp"

#include <charconv>

namespace credis::protocol {

using credis::util::Error;
using credis::util::ErrorCode;

namespace {

constexpr auto digit_count(size_t n) -> size_t {
    if (n == 0) {
        return 1;
    }
    size_t c = 0;
    while (n > 0) {
        n /= 10;
        ++c;
    }
    return c;
}

} // namespace

auto parse_one(std::string_view input) -> std::expected<ParsedCommand, Error> {
    if (input.empty() || input[0] != '*') [[unlikely]] {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: expected array"));
    }

    size_t pos = 1;
    auto crlf = input.find("\r\n", pos);
    if (crlf == std::string_view::npos) {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Incomplete RESP: missing CRLF after array count"));
    }

    int count = 0;
    auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + crlf, count);
    if (ec != std::errc{} || count < 0) [[unlikely]] {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: invalid array count"));
    }

    pos = crlf + 2;

    // TODO: hand-roll single-pass without string_view::find for \r\n scanning;
    // pre-compute count to reserve args vector and avoid reallocations
    std::vector<std::string_view> args;
    args.reserve(static_cast<size_t>(count));
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
        if (ec2 != std::errc{} || len < 0) [[unlikely]] {
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
    std::string result;
    result.reserve(1 + s.size() + 2);
    result += '+';
    result += s;
    result += "\r\n";
    return result;
}

auto encode_bulk_string(std::string_view s) -> std::string {
    auto len_str = std::to_string(s.size());
    std::string result;
    result.reserve(1 + len_str.size() + 2 + s.size() + 2);
    result += '$';
    result += len_str;
    result += "\r\n";
    result += s;
    result += "\r\n";
    return result;
}

auto encode_null_bulk_string() -> const std::string& {
    static const std::string kNullBulk = "$-1\r\n";
    return kNullBulk;
}

auto encode_integer(int64_t n) -> std::string {
    auto n_str = std::to_string(n);
    std::string result;
    result.reserve(1 + n_str.size() + 2);
    result += ':';
    result += n_str;
    result += "\r\n";
    return result;
}

auto encode_array(const std::vector<std::string>& elements) -> std::string {
    auto count_str = std::to_string(elements.size());
    size_t total = 1 + count_str.size() + 2;
    for (const auto& elem : elements) {
        total += 1 + digit_count(elem.size()) + 2 + elem.size() + 2;
    }
    std::string result;
    result.reserve(total);
    result += '*';
    result += count_str;
    result += "\r\n";
    for (const auto& elem : elements) {
        result += '$';
        result += std::to_string(elem.size());
        result += "\r\n";
        result += elem;
        result += "\r\n";
    }
    return result;
}

auto encode_raw_array(const std::vector<std::string>& raw_elements) -> std::string {
    auto count_str = std::to_string(raw_elements.size());
    size_t total = 1 + count_str.size() + 2;
    for (const auto& elem : raw_elements) {
        total += elem.size();
    }
    std::string result;
    result.reserve(total);
    result += '*';
    result += count_str;
    result += "\r\n";
    for (const auto& elem : raw_elements) {
        result += elem;
    }
    return result;
}

auto encode_entries(std::span<const credis::store::StreamEntry> entries) -> std::string {
    auto count_str = std::to_string(entries.size());
    size_t total = 1 + count_str.size() + 2;
    for (const auto& entry : entries) {
        total += 3;
        total += 1 + digit_count(entry.id.size()) + 2 + entry.id.size() + 2;
        total += 1 + digit_count(entry.fields.size() * 2) + 2;
        for (const auto& [field, value] : entry.fields) {
            total += 1 + digit_count(field.size()) + 2 + field.size() + 2;
            total += 1 + digit_count(value.size()) + 2 + value.size() + 2;
        }
    }
    std::string result;
    result.reserve(total);
    result += '*';
    result += count_str;
    result += "\r\n";
    for (const auto& entry : entries) {
        result += "*2\r\n";
        result += '$';
        result += std::to_string(entry.id.size());
        result += "\r\n";
        result += entry.id;
        result += "\r\n";
        result += '*';
        result += std::to_string(entry.fields.size() * 2);
        result += "\r\n";
        for (const auto& [field, value] : entry.fields) {
            result += '$';
            result += std::to_string(field.size());
            result += "\r\n";
            result += field;
            result += "\r\n";
            result += '$';
            result += std::to_string(value.size());
            result += "\r\n";
            result += value;
            result += "\r\n";
        }
    }
    return result;
}

auto encode_error(std::string_view s) -> std::string {
    std::string result;
    result.reserve(1 + s.size() + 2);
    result += '-';
    result += s;
    result += "\r\n";
    return result;
}

auto encode_null_array() -> const std::string& {
    static const std::string kNullArray = "*-1\r\n";
    return kNullArray;
}

auto encode_stream_entries(
    const std::vector<std::pair<std::string, std::span<const credis::store::StreamEntry>>>& streams) -> std::string {
    auto count_str = std::to_string(streams.size());
    size_t total = 1 + count_str.size() + 2;
    for (const auto& [key, entries] : streams) {
        total += 3;
        total += 1 + digit_count(key.size()) + 2 + key.size() + 2;
        total += 1 + digit_count(entries.size()) + 2;
        for (const auto& entry : entries) {
            total += 3;
            total += 1 + digit_count(entry.id.size()) + 2 + entry.id.size() + 2;
            total += 1 + digit_count(entry.fields.size() * 2) + 2;
            for (const auto& [field, value] : entry.fields) {
                total += 1 + digit_count(field.size()) + 2 + field.size() + 2;
                total += 1 + digit_count(value.size()) + 2 + value.size() + 2;
            }
        }
    }
    std::string result;
    result.reserve(total);
    result += '*';
    result += count_str;
    result += "\r\n";
    for (const auto& [key, entries] : streams) {
        result += "*2\r\n";
        result += '$';
        result += std::to_string(key.size());
        result += "\r\n";
        result += key;
        result += "\r\n";
        result += '*';
        result += std::to_string(entries.size());
        result += "\r\n";
        for (const auto& entry : entries) {
            result += "*2\r\n";
            result += '$';
            result += std::to_string(entry.id.size());
            result += "\r\n";
            result += entry.id;
            result += "\r\n";
            result += '*';
            result += std::to_string(entry.fields.size() * 2);
            result += "\r\n";
            for (const auto& [field, value] : entry.fields) {
                result += '$';
                result += std::to_string(field.size());
                result += "\r\n";
                result += field;
                result += "\r\n";
                result += '$';
                result += std::to_string(value.size());
                result += "\r\n";
                result += value;
                result += "\r\n";
            }
        }
    }
    return result;
}

} // namespace credis::protocol
