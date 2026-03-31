#pragma once

#include <expected>
#include <string>
#include <vector>

#include "store/store.hpp"

class RespParser {
  public:
    static std::expected<std::vector<std::string>, std::string> parse(std::string_view input);

    static std::string encode_simple_string(std::string_view s);
    static std::string encode_bulk_string(std::string_view s);
    static std::string encode_null_bulk_string();
    static std::string encode_integer(int64_t n);
    static std::string encode_array(const std::vector<std::string>& elements);
    static std::string encode_error(std::string_view s);
    static std::string encode_null_array();
    static std::string encode_stream_entries(
        const std::vector<std::pair<std::string, std::vector<Redis::StreamEntry>>>& streams);
};
