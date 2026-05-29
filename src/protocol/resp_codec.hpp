#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include "store/stream_entry.hpp"
#include "util/error.hpp"

namespace credis::protocol {

inline const std::string kRespOk = "+OK\r\n";
inline const std::string kRespQueued = "+QUEUED\r\n";
inline const std::string kRespPong = "+PONG\r\n";

struct ParsedCommand {
    std::vector<std::string_view> args;
    size_t consumed;
};

auto parse_one(std::string_view input) -> std::expected<ParsedCommand, credis::util::Error>;

auto encode_simple_string(std::string_view s) -> std::string;
auto encode_bulk_string(std::string_view s) -> std::string;
auto encode_null_bulk_string() -> const std::string&;
auto encode_integer(int64_t n) -> std::string;
auto encode_array(const std::vector<std::string>& elements) -> std::string;
auto encode_raw_array(const std::vector<std::string>& raw_elements) -> std::string;
auto encode_entries(std::span<const credis::store::StreamEntry> entries) -> std::string;
auto encode_error(std::string_view s) -> std::string;
auto encode_null_array() -> const std::string&;
auto encode_stream_entries(
    const std::vector<std::pair<std::string, std::span<const credis::store::StreamEntry>>>& streams) -> std::string;

} // namespace credis::protocol
