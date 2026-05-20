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

    static auto encode_simple_string(std::string_view s) -> std::string;
    static auto encode_bulk_string(std::string_view s) -> std::string;
    static auto encode_null_bulk_string() -> std::string;
    static auto encode_integer(int64_t n) -> std::string;
    static auto encode_array(const std::vector<std::string>& elements) -> std::string;
    static auto encode_raw_array(const std::vector<std::string>& raw_elements) -> std::string;
    static auto encode_entries(std::span<const Redis::StreamEntry> entries) -> std::string;
    static auto encode_error(std::string_view s) -> std::string;
    static auto encode_null_array() -> std::string;
    static auto encode_stream_entries(
        const std::vector<std::pair<std::string, std::span<const Redis::StreamEntry>>>& streams) -> std::string;
};
