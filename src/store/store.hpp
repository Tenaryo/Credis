#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "protocol/stream_id.hpp"

namespace Redis {
using String = std::string;
using List = std::deque<std::string>;

struct StreamEntry {
    std::string id;
    std::vector<std::pair<std::string, std::string>> fields;
};

using Stream = std::deque<StreamEntry>;

using Value = std::variant<String, List, Stream>;
} // namespace Redis

class Store {
    struct Entry {
        Redis::Value value;
        std::optional<std::chrono::steady_clock::time_point> expiry;
    };

    std::unordered_map<std::string, Entry> data_;

    static std::chrono::steady_clock::time_point get_current_time();
    bool is_expired(const Entry& entry) const;
    Entry* find_valid_entry(std::string_view key);
    Redis::List* get_list(std::string_view key);
    Redis::List* get_or_create_list(std::string key);
    Redis::Stream* get_stream(std::string_view key);
    Redis::Stream* get_or_create_stream(std::string key);

    static size_t lower_bound(const Redis::Stream& stream, const StreamId& target);
    static size_t upper_bound(const Redis::Stream& stream, const StreamId& target);
  public:
    void set(std::string key, std::string value, std::optional<uint64_t> ttl_ms = std::nullopt);
    std::optional<std::string> get(std::string_view key);
    std::optional<int64_t> incr(std::string_view key);
    bool exists(std::string_view key);
    bool del(std::string_view key);

    int64_t rpush(std::string key, std::string value);
    int64_t lpush(std::string key, std::string value);
    int64_t llen(std::string_view key);
    std::optional<std::string> lpop(std::string_view key);
    std::vector<std::string> lpop(std::string_view key, int64_t count);
    std::vector<std::string> lrange(std::string_view key, int64_t start, int64_t stop);

    std::string xadd(std::string key,
                     std::string id,
                     const std::vector<std::pair<std::string, std::string>>& fields);

    std::vector<Redis::StreamEntry>
    xrange(std::string_view key, std::string start, std::string end);

    std::vector<Redis::StreamEntry> xread(std::string_view key, std::string id);

    std::optional<std::string> get_stream_max_id(std::string_view key);

    std::string get_type(std::string_view key);
};
