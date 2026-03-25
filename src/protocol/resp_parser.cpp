#include "resp_parser.hpp"
#include <charconv>

std::expected<std::vector<std::string>, std::string> RespParser::parse(std::string_view input) {
    std::vector<std::string> args;

    if (input.empty() || input[0] != '*') {
        return std::unexpected("Invalid RESP: expected array");
    }

    size_t pos = 1;
    auto crlf = input.find("\r\n", pos);
    if (crlf == std::string_view::npos) {
        return std::unexpected("Invalid RESP: missing CRLF after array count");
    }

    int count = 0;
    auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + crlf, count);
    if (ec != std::errc{} || count < 0) {
        return std::unexpected("Invalid RESP: invalid array count");
    }

    pos = crlf + 2;

    for (int i = 0; i < count; ++i) {
        if (pos >= input.size() || input[pos] != '$') {
            return std::unexpected("Invalid RESP: expected bulk string");
        }

        crlf = input.find("\r\n", pos + 1);
        if (crlf == std::string_view::npos) {
            return std::unexpected("Invalid RESP: missing CRLF after bulk string length");
        }

        int len = 0;
        auto [ptr2, ec2] = std::from_chars(input.data() + pos + 1, input.data() + crlf, len);
        if (ec2 != std::errc{} || len < 0) {
            return std::unexpected("Invalid RESP: invalid bulk string length");
        }

        pos = crlf + 2;

        if (pos + len > input.size()) {
            return std::unexpected("Invalid RESP: bulk string truncated");
        }

        args.emplace_back(input.substr(pos, len));
        pos += len + 2;
    }

    return args;
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

std::string RespParser::encode_error(std::string_view s) { return "-" + std::string(s) + "\r\n"; }
