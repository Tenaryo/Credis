#include "resp_codec.hpp"

#include <charconv>

namespace credis::protocol {

using credis::util::Error;
using credis::util::ErrorCode;

namespace {

inline void append_digit(std::string& out, size_t n) {
    if (n < 10) [[likely]] {
        out += static_cast<char>('0' + n);
    } else {
        char buf[11];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), n);
        out.append(buf, static_cast<size_t>(ptr - buf));
    }
}

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

auto parse_int_until_crlf(const char*& p, const char* end) -> std::optional<int> {
    if (p >= end) [[unlikely]] {
        return std::nullopt;
    }
    bool neg = false;
    if (*p == '-') {
        neg = true;
        ++p;
    }
    int val = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        val = val * 10 + (*p++ - '0');
    }
    if (p + 1 >= end || *p != '\r' || *(p + 1) != '\n') [[unlikely]] {
        return std::nullopt;
    }
    p += 2;
    return neg ? -val : val;
}

} // namespace

auto parse_one(std::string_view input) -> std::expected<ParsedCommand, Error> {
    if (input.empty() || input[0] != '*') [[unlikely]] {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: expected array"));
    }

    const char* p = input.data() + 1;
    const char* end = input.data() + input.size();

    auto count_opt = parse_int_until_crlf(p, end);
    if (!count_opt || *count_opt < 0) [[unlikely]] {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: invalid array count"));
    }
    int count = *count_opt;

    std::vector<std::string_view> args;
    args.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (p >= end || *p != '$') [[unlikely]] {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: expected bulk string"));
        }
        ++p;

        auto len_opt = parse_int_until_crlf(p, end);
        if (!len_opt || *len_opt < 0) [[unlikely]] {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: invalid bulk string length"));
        }
        int len = *len_opt;

        if (p + len > end) [[unlikely]] {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Incomplete RESP: bulk string truncated"));
        }
        args.emplace_back(p, static_cast<size_t>(len));
        p += static_cast<size_t>(len) + 2;
    }

    size_t consumed = static_cast<size_t>(p - input.data());
    return ParsedCommand{std::move(args), consumed};
}

auto parse_one_into(std::vector<std::string_view>& args, std::string_view input) -> std::expected<size_t, Error> {
    if (input.empty() || input[0] != '*') [[unlikely]] {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: expected array"));
    }

    const char* p = input.data() + 1;
    const char* end = input.data() + input.size();

    auto count_opt = parse_int_until_crlf(p, end);
    if (!count_opt || *count_opt < 0) [[unlikely]] {
        return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: invalid array count"));
    }
    int count = *count_opt;

    args.clear();
    args.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (p >= end || *p != '$') [[unlikely]] {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: expected bulk string"));
        }
        ++p;

        auto len_opt = parse_int_until_crlf(p, end);
        if (!len_opt || *len_opt < 0) [[unlikely]] {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Invalid RESP: invalid bulk string length"));
        }
        int len = *len_opt;

        if (p + len > end) [[unlikely]] {
            return std::unexpected(Error(ErrorCode::kProtocolError, "Incomplete RESP: bulk string truncated"));
        }
        args.emplace_back(p, static_cast<size_t>(len));
        p += static_cast<size_t>(len) + 2;
    }

    return static_cast<size_t>(p - input.data());
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

auto encode_array(std::span<const std::string_view> elements) -> std::string {
    char buf[11];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), elements.size());
    size_t total = 1 + static_cast<size_t>(ptr - buf) + 2;
    for (const auto& elem : elements) {
        total += 1 + digit_count(elem.size()) + 2 + elem.size() + 2;
    }
    std::string result;
    result.reserve(total);
    result += '*';
    result.append(buf, static_cast<size_t>(ptr - buf));
    result += "\r\n";
    for (const auto& elem : elements) {
        auto [p2, ec2] = std::to_chars(buf, buf + sizeof(buf), elem.size());
        result += '$';
        result.append(buf, static_cast<size_t>(p2 - buf));
        result += "\r\n";
        result += elem;
        result += "\r\n";
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

void encode_simple_string_into(std::string& out, std::string_view s) {
    out += '+';
    out += s;
    out += "\r\n";
}

void encode_bulk_string_into(std::string& out, std::string_view s) {
    out += '$';
    auto len = s.size();
    if (len < 10) [[likely]] {
        out += static_cast<char>('0' + len);
    } else {
        char buf[11];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), len);
        out.append(buf, static_cast<size_t>(ptr - buf));
    }
    out += "\r\n";
    out += s;
    out += "\r\n";
}

void encode_error_into(std::string& out, std::string_view s) {
    out += '-';
    out += s;
    out += "\r\n";
}

void encode_integer_into(std::string& out, int64_t n) {
    out += ':';
    if (n >= 0 && n < 10) [[likely]] {
        out += static_cast<char>('0' + n);
    } else {
        char buf[21];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), n);
        out.append(buf, static_cast<size_t>(ptr - buf));
    }
    out += "\r\n";
}

void encode_null_bulk_string_into(std::string& out) {
    out += "$-1\r\n";
}

void encode_null_array_into(std::string& out) {
    out += "*-1\r\n";
}

void encode_array_into(std::string& out, const std::vector<std::string>& elements) {
    out += '*';
    append_digit(out, elements.size());
    out += "\r\n";
    for (const auto& elem : elements) {
        out += '$';
        append_digit(out, elem.size());
        out += "\r\n";
        out += elem;
        out += "\r\n";
    }
}

void encode_array_into(std::string& out, std::span<const std::string_view> elements) {
    out += '*';
    append_digit(out, elements.size());
    out += "\r\n";
    for (const auto& elem : elements) {
        out += '$';
        append_digit(out, elem.size());
        out += "\r\n";
        out += elem;
        out += "\r\n";
    }
}

void encode_raw_array_into(std::string& out, const std::vector<std::string>& raw_elements) {
    out += '*';
    append_digit(out, raw_elements.size());
    out += "\r\n";
    for (const auto& elem : raw_elements) {
        out += elem;
    }
}

void encode_entries_into(std::string& out, std::span<const credis::store::StreamEntry> entries) {
    out += '*';
    append_digit(out, entries.size());
    out += "\r\n";
    for (const auto& entry : entries) {
        out += "*2\r\n";
        out += '$';
        append_digit(out, entry.id.size());
        out += "\r\n";
        out += entry.id;
        out += "\r\n";

        out += '*';
        append_digit(out, entry.fields.size() * 2);
        out += "\r\n";
        for (const auto& [field, value] : entry.fields) {
            out += '$';
            append_digit(out, field.size());
            out += "\r\n";
            out += field;
            out += "\r\n";

            out += '$';
            append_digit(out, value.size());
            out += "\r\n";
            out += value;
            out += "\r\n";
        }
    }
}

void encode_stream_entries_into(
    std::string& out,
    const std::vector<std::pair<std::string, std::span<const credis::store::StreamEntry>>>& streams) {
    out += '*';
    append_digit(out, streams.size());
    out += "\r\n";
    for (const auto& [key, entries] : streams) {
        out += "*2\r\n";
        out += '$';
        append_digit(out, key.size());
        out += "\r\n";
        out += key;
        out += "\r\n";

        out += '*';
        append_digit(out, entries.size());
        out += "\r\n";
        for (const auto& entry : entries) {
            out += "*2\r\n";
            out += '$';
            append_digit(out, entry.id.size());
            out += "\r\n";
            out += entry.id;
            out += "\r\n";

            out += '*';
            append_digit(out, entry.fields.size() * 2);
            out += "\r\n";
            for (const auto& [field, value] : entry.fields) {
                out += '$';
                append_digit(out, field.size());
                out += "\r\n";
                out += field;
                out += "\r\n";

                out += '$';
                append_digit(out, value.size());
                out += "\r\n";
                out += value;
                out += "\r\n";
            }
        }
    }
}

} // namespace credis::protocol
