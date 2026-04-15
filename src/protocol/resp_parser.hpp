#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include "protocol/stream_entry.hpp"

class RespParser {
  public:
    struct ParsedCommand {
        std::vector<std::string> args;
        size_t consumed;
    };

    static std::expected<std::vector<std::string>, std::string> parse(std::string_view input);
    static auto parse_one(std::string_view input) -> std::expected<ParsedCommand, std::string>;

    static std::string encode_simple_string(std::string_view s);
    static std::string encode_bulk_string(std::string_view s);
    static std::string encode_null_bulk_string();
    static std::string encode_integer(int64_t n);
    static std::string encode_array(const std::vector<std::string>& elements);
    static std::string encode_raw_array(std::vector<std::string> raw_elements);
    static std::string encode_entries(std::span<const Redis::StreamEntry> entries);
    static std::string encode_error(std::string_view s);
    static std::string encode_null_array();
    static std::string encode_stream_entries(
        const std::vector<std::pair<std::string, std::span<const Redis::StreamEntry>>>& streams);
};
