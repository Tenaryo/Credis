#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "protocol/stream_entry.hpp"
#include "protocol/stream_id.hpp"
#include "util/string_hash.hpp"

namespace Redis {
using String = std::string;
using List = std::deque<std::string>;

using Stream = std::vector<StreamEntry>;

struct SortedSet {
    std::set<std::pair<double, std::string>> entries;
    std::unordered_map<std::string, double> member_scores;

    int64_t add(double score, std::string member) {
        auto it = member_scores.find(member);
        if (it != member_scores.end()) {
            entries.erase({it->second, member});
            it->second = score;
            entries.emplace(score, member);
            return 0;
        }
        member_scores.emplace(member, score);
        entries.emplace(score, std::move(member));
        return 1;
    }

    int64_t remove(std::string_view member) {
        auto it = member_scores.find(std::string(member));
        if (it == member_scores.end())
            return 0;
        entries.erase({it->second, std::string(member)});
        member_scores.erase(it);
        return 1;
    }
};

using Value = std::variant<String, List, Stream, SortedSet>;
} // namespace Redis

class Store {
    struct Entry {
        Redis::Value value;
        std::optional<std::chrono::steady_clock::time_point> expiry;
    };

    std::unordered_map<std::string, Entry, StringHash, std::equal_to<>> data_;

    uint64_t version_counter_{0};
    std::unordered_map<std::string, uint64_t, StringHash, std::equal_to<>> key_versions_;

    void touch_key(std::string_view key) { key_versions_[std::string(key)] = ++version_counter_; }

    static std::chrono::steady_clock::time_point get_current_time();
    bool is_expired(const Entry& entry) const;
    Entry* find_valid_entry(std::string_view key);
    template <typename T> T* get_typed(std::string_view key) {
        Entry* entry = find_valid_entry(key);
        if (!entry)
            return nullptr;
        return std::get_if<T>(&entry->value);
    }

    template <typename T> T* get_or_create_typed(std::string key) {
        Entry* entry = find_valid_entry(key);
        if (!entry) {
            auto [it, _] = data_.emplace(std::move(key), Entry{T{}, {}});
            entry = &it->second;
        }
        if (!std::holds_alternative<T>(entry->value)) {
            entry->value = T{};
        }
        return &std::get<T>(entry->value);
    }

    Redis::List* get_list(std::string_view key) { return get_typed<Redis::List>(key); }
    Redis::List* get_or_create_list(std::string key) {
        return get_or_create_typed<Redis::List>(std::move(key));
    }
    Redis::Stream* get_stream(std::string_view key) { return get_typed<Redis::Stream>(key); }
    Redis::Stream* get_or_create_stream(std::string key) {
        return get_or_create_typed<Redis::Stream>(std::move(key));
    }
    Redis::SortedSet* get_zset(std::string_view key) { return get_typed<Redis::SortedSet>(key); }
    Redis::SortedSet* get_or_create_zset(std::string key) {
        return get_or_create_typed<Redis::SortedSet>(std::move(key));
    }

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

    std::span<const Redis::StreamEntry>
    xrange(std::string_view key, std::string_view start, std::string_view end);

    std::span<const Redis::StreamEntry> xread(std::string_view key, std::string_view id);

    std::optional<std::string> get_stream_max_id(std::string_view key);

    int64_t zadd(std::string key, double score, std::string member);
    std::optional<int64_t> zrank(std::string_view key, std::string_view member);
    std::vector<std::string> zrange(std::string_view key, int64_t start, int64_t stop);
    int64_t zcard(std::string_view key);
    std::optional<double> zscore(std::string_view key, std::string_view member);
    int64_t zrem(std::string_view key, std::string_view member);
    std::vector<std::pair<std::string, double>> zgetall(std::string_view key);

    std::string get_type(std::string_view key);
    std::vector<std::string> keys();

    uint64_t get_key_version(std::string_view key) const {
        auto it = key_versions_.find(key);
        return it != key_versions_.end() ? it->second : 0;
    }
};
