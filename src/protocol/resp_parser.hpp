#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include "protocol/stream_entry.hpp"

namespace credis::protocol {

struct ParsedCommand {
    std::vector<std::string> args;
    size_t consumed;
};

auto parse_resp(std::string_view input) -> std::expected<std::vector<std::string>, std::string>;
auto parse_one(std::string_view input) -> std::expected<ParsedCommand, std::string>;

auto encode_simple_string(std::string_view s) -> std::string;
auto encode_bulk_string(std::string_view s) -> std::string;
auto encode_null_bulk_string() -> std::string;
auto encode_integer(int64_t n) -> std::string;
auto encode_array(const std::vector<std::string>& elements) -> std::string;
auto encode_raw_array(const std::vector<std::string>& raw_elements) -> std::string;
auto encode_entries(std::span<const credis::store::StreamEntry> entries) -> std::string;
auto encode_error(std::string_view s) -> std::string;
auto encode_null_array() -> std::string;
auto encode_stream_entries(
    const std::vector<std::pair<std::string, std::span<const credis::store::StreamEntry>>>& streams) -> std::string;

} // namespace credis::protocol
