#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "store/stream_entry.hpp"
#include "store/stream_id.hpp"
#include "util/string_hash.hpp"

namespace credis::store {

using String = std::string;
using List = std::deque<std::string>;

using Stream = std::vector<StreamEntry>;

struct SortedSet {
    std::set<std::pair<double, std::string>> entries;
    std::unordered_map<std::string_view, decltype(entries)::iterator, credis::util::StringHash, std::equal_to<>>
        member_scores;

    auto add(double score, std::string member) -> int64_t {
        auto it = member_scores.find(member);
        if (it != member_scores.end()) {
            auto nh = entries.extract(it->second);
            nh.value().first = score;
            auto [new_iter, inserted, _] = entries.insert(std::move(nh));
            it->second = new_iter;
            return 0;
        }
        auto [entry_it, _] = entries.emplace(score, std::move(member));
        member_scores.emplace(entry_it->second, entry_it);
        return 1;
    }

    auto remove(std::string_view member) -> int64_t {
        auto it = member_scores.find(member);
        if (it == member_scores.end()) {
            return 0;
        }
        auto entry_it = it->second;
        member_scores.erase(it);
        entries.erase(entry_it);
        return 1;
    }
};

using Value = std::variant<String, List, Stream, SortedSet>;

class Store {
  public:
    struct Entry {
        Value value;
        std::optional<std::chrono::steady_clock::time_point> expiry;
    };

  private:
    std::unordered_map<std::string, Entry, credis::util::StringHash, std::equal_to<>> data_;

    uint64_t version_counter_{0};
    std::unordered_map<std::string, uint64_t, credis::util::StringHash, std::equal_to<>> key_versions_;

    void touch_key(std::string_view key) {
        auto it = key_versions_.find(key);
        if (it != key_versions_.end()) {
            it->second = ++version_counter_;
        } else {
            key_versions_.emplace(std::string(key), ++version_counter_);
        }
    }

    static auto get_current_time() -> std::chrono::steady_clock::time_point;
    static auto is_expired(const Entry& entry) -> bool;
    auto find_valid_entry(std::string_view key) -> Entry*;
    template <typename T>
    auto get_typed(std::string_view key) -> T* {
        Entry* entry = find_valid_entry(key);
        if (!entry) {
            return nullptr;
        }
        return std::get_if<T>(&entry->value);
    }

    template <typename T>
    auto get_or_create_typed(std::string key) -> T* {
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

    auto get_list(std::string_view key) -> List* {
        return get_typed<List>(key);
    }
    auto get_or_create_list(std::string key) -> List* {
        return get_or_create_typed<List>(std::move(key));
    }
    auto get_stream(std::string_view key) -> Stream* {
        return get_typed<Stream>(key);
    }
    auto get_or_create_stream(std::string key) -> Stream* {
        return get_or_create_typed<Stream>(std::move(key));
    }
    auto get_zset(std::string_view key) -> SortedSet* {
        return get_typed<SortedSet>(key);
    }
    auto get_or_create_zset(std::string key) -> SortedSet* {
        return get_or_create_typed<SortedSet>(std::move(key));
    }

    static auto lower_bound(const Stream& stream, const credis::protocol::StreamId& target) -> size_t;
    static auto upper_bound(const Stream& stream, const credis::protocol::StreamId& target) -> size_t;

  public:
    void set(std::string key, std::string value, std::optional<uint64_t> ttl_ms = std::nullopt);
    void mset(const std::vector<std::pair<std::string, std::string>>& pairs);
    auto get(std::string_view key) -> std::optional<std::string>;
    auto incr(std::string_view key) -> std::optional<int64_t>;
    auto exists(std::string_view key) -> bool;
    auto del(std::string_view key) -> bool;

    auto rpush(std::string key, std::string value) -> int64_t;
    auto lpush(std::string key, std::string value) -> int64_t;
    auto llen(std::string_view key) -> int64_t;
    auto lpop(std::string_view key) -> std::optional<std::string>;
    auto lpop(std::string_view key, int64_t count) -> std::vector<std::string>;
    auto rpop(std::string_view key) -> std::optional<std::string>;
    auto rpop(std::string_view key, int64_t count) -> std::vector<std::string>;
    auto lrange(std::string_view key, int64_t start, int64_t stop) -> std::vector<std::string>;

    auto xadd(std::string key, const std::string& id, const std::vector<std::pair<std::string, std::string>>& fields)
        -> std::string;

    auto xrange(std::string_view key, std::string_view start, std::string_view end) -> std::span<const StreamEntry>;

    auto xread(std::string_view key, std::string_view id) -> std::span<const StreamEntry>;

    auto get_stream_max_id(std::string_view key) -> std::optional<std::string>;

    auto zadd(std::string key, double score, std::string member) -> int64_t;
    auto zrank(std::string_view key, std::string_view member) -> std::optional<int64_t>;
    auto zrange(std::string_view key, int64_t start, int64_t stop) -> std::vector<std::string>;
    auto zcard(std::string_view key) -> int64_t;
    auto zscore(std::string_view key, std::string_view member) -> std::optional<double>;
    auto zrem(std::string_view key, std::string_view member) -> int64_t;
    auto zgetall(std::string_view key) -> std::vector<std::pair<std::string, double>>;

    auto get_type(std::string_view key) -> std::string;
    auto keys() -> std::vector<std::string>;

    template <typename T>
    [[nodiscard]] auto key_is_absent_or_holds(std::string_view key) const -> bool {
        auto it = data_.find(key);
        if (it == data_.end()) {
            return true;
        }
        if (it->second.expiry && get_current_time() > *it->second.expiry) {
            return true;
        }
        return std::holds_alternative<T>(it->second.value);
    }

    auto get_key_version(std::string_view key) const -> uint64_t {
        auto it = key_versions_.find(key);
        return it != key_versions_.end() ? it->second : 0;
    }

    void for_each_valid_entry(const std::function<void(std::string_view, const Entry&)>& fn) const;
};

} // namespace credis::store
