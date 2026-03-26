#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

struct StreamEntry {
    std::string id;
    std::vector<std::pair<std::string, std::string>> fields;
};

using Stream = std::deque<StreamEntry>;

class Store {
    struct Entry {
        std::variant<std::string, std::deque<std::string>, Stream> value;
        std::optional<std::chrono::steady_clock::time_point> expiry;
    };

    std::unordered_map<std::string, Entry> data_;

    static std::chrono::steady_clock::time_point get_current_time();
    bool is_expired(const Entry& entry) const;
    Entry* find_valid_entry(const std::string& key);
    std::deque<std::string>* get_list(const std::string& key);
    std::deque<std::string>* get_or_create_list(const std::string& key);
    Stream* get_or_create_stream(const std::string& key);
    static bool parse_entry_id(const std::string& id, int64_t& timestamp, int64_t& sequence);
    static bool compare_entry_id(const std::string& a, const std::string& b);
  public:
    void set(const std::string& key,
             const std::string& value,
             std::optional<uint64_t> ttl_ms = std::nullopt);
    std::optional<std::string> get(const std::string& key);
    bool exists(const std::string& key);
    bool del(const std::string& key);

    int64_t rpush(const std::string& key, const std::string& value);
    int64_t lpush(const std::string& key, const std::string& value);
    int64_t llen(const std::string& key);
    std::optional<std::string> lpop(const std::string& key);
    std::vector<std::string> lpop(const std::string& key, int64_t count);
    std::vector<std::string> lrange(const std::string& key, int64_t start, int64_t stop);

    std::string xadd(const std::string& key,
                     const std::string& id,
                     const std::vector<std::pair<std::string, std::string>>& fields);

    std::string get_type(const std::string& key);
};
