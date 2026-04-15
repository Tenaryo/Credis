#include "resp_parser.hpp"
#include <charconv>

std::expected<std::vector<std::string>, std::string> RespParser::parse(std::string_view input) {
    auto result = parse_one(input);
    if (!result)
        return std::unexpected(std::move(result.error()));
    return std::move(result->args);
}

auto RespParser::parse_one(std::string_view input) -> std::expected<ParsedCommand, std::string> {
    if (input.empty() || input[0] != '*') {
        return std::unexpected("Invalid RESP: expected array");
    }

    size_t pos = 1;
    auto crlf = input.find("\r\n", pos);
    if (crlf == std::string_view::npos) {
        return std::unexpected("Incomplete RESP: missing CRLF after array count");
    }

    int count = 0;
    auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + crlf, count);
    if (ec != std::errc{} || count < 0) {
        return std::unexpected("Invalid RESP: invalid array count");
    }

    pos = crlf + 2;

    std::vector<std::string> args;
    for (int i = 0; i < count; ++i) {
        if (pos >= input.size() || input[pos] != '$') {
            return std::unexpected("Invalid RESP: expected bulk string");
        }

        crlf = input.find("\r\n", pos + 1);
        if (crlf == std::string_view::npos) {
            return std::unexpected("Incomplete RESP: missing CRLF after bulk string length");
        }

        int len = 0;
        auto [ptr2, ec2] = std::from_chars(input.data() + pos + 1, input.data() + crlf, len);
        if (ec2 != std::errc{} || len < 0) {
            return std::unexpected("Invalid RESP: invalid bulk string length");
        }

        pos = crlf + 2;

        if (pos + len > input.size()) {
            return std::unexpected("Incomplete RESP: bulk string truncated");
        }

        args.emplace_back(input.substr(pos, len));
        pos += len + 2;
    }

    return ParsedCommand{std::move(args), pos};
}

std::string RespParser::encode_simple_string(std::string_view s) {
    return "+" + std::string(s) + "\r\n";
}

std::string RespParser::encode_bulk_string(std::string_view s) {
    return "$" + std::to_string(s.size()) + "\r\n" + std::string(s) + "\r\n";
}

std::string RespParser::encode_null_bulk_string() { return "$-1\r\n"; }

std::string RespParser::encode_integer(int64_t n) { return ":" + std::to_string(n) + "\r\n"; }

std::string RespParser::encode_array(const std::vector<std::string>& elements) {
    std::string result = "*" + std::to_string(elements.size()) + "\r\n";
    for (const auto& elem : elements) {
        result += encode_bulk_string(elem);
    }
    return result;
}

std::string RespParser::encode_raw_array(std::vector<std::string> raw_elements) {
    std::string result = "*" + std::to_string(raw_elements.size()) + "\r\n";
    for (auto& elem : raw_elements) {
        result += std::move(elem);
    }
    return result;
}

std::string RespParser::encode_entries(std::span<const Redis::StreamEntry> entries) {
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

std::string RespParser::encode_error(std::string_view s) { return "-" + std::string(s) + "\r\n"; }

std::string RespParser::encode_null_array() { return "*-1\r\n"; }

std::string RespParser::encode_stream_entries(
    const std::vector<std::pair<std::string, std::span<const Redis::StreamEntry>>>& streams) {
    std::string result = "*" + std::to_string(streams.size()) + "\r\n";
    for (const auto& [key, entries] : streams) {
        result += "*2\r\n";
        result += encode_bulk_string(key);
        result += encode_entries(entries);
    }
    return result;
}
