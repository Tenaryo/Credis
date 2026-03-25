#include "store.hpp"

std::chrono::steady_clock::time_point Store::get_current_time() {
    return std::chrono::steady_clock::now();
}

bool Store::is_expired(const Entry& entry) const {
    return entry.expiry && get_current_time() >= *entry.expiry;
}

Store::Entry* Store::find_valid_entry(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return nullptr;
    }
    if (is_expired(it->second)) {
        data_.erase(it);
        return nullptr;
    }
    return &it->second;
}

std::deque<std::string>* Store::get_list(const std::string& key) {
    Entry* entry = find_valid_entry(key);
    if (!entry || !std::holds_alternative<std::deque<std::string>>(entry->value)) {
        return nullptr;
    }
    return &std::get<std::deque<std::string>>(entry->value);
}

std::deque<std::string>* Store::get_or_create_list(const std::string& key) {
    Entry* entry = find_valid_entry(key);
    if (!entry) {
        data_[key] = Entry{.value = std::deque<std::string>{}};
        entry = &data_[key];
    }
    if (!std::holds_alternative<std::deque<std::string>>(entry->value)) {
        entry->value = std::deque<std::string>{};
    }
    return &std::get<std::deque<std::string>>(entry->value);
}

void Store::set(const std::string& key, const std::string& value, std::optional<uint64_t> ttl_ms) {
    Entry entry;
    entry.value = value;
    if (ttl_ms) {
        entry.expiry = get_current_time() + std::chrono::milliseconds(*ttl_ms);
    }
    data_[key] = std::move(entry);
}

std::optional<std::string> Store::get(const std::string& key) {
    Entry* entry = find_valid_entry(key);
    if (!entry || !std::holds_alternative<std::string>(entry->value)) {
        return std::nullopt;
    }
    return std::get<std::string>(entry->value);
}

bool Store::exists(const std::string& key) { return find_valid_entry(key) != nullptr; }

bool Store::del(const std::string& key) { return data_.erase(key) > 0; }

int64_t Store::rpush(const std::string& key, const std::string& value) {
    auto* list = get_or_create_list(key);
    list->push_back(value);
    return static_cast<int64_t>(list->size());
}

int64_t Store::lpush(const std::string& key, const std::string& value) {
    auto* list = get_or_create_list(key);
    list->push_front(value);
    return static_cast<int64_t>(list->size());
}

int64_t Store::llen(const std::string& key) {
    auto* list = get_list(key);
    return list ? static_cast<int64_t>(list->size()) : 0;
}

std::optional<std::string> Store::lpop(const std::string& key) {
    auto* list = get_list(key);
    if (!list || list->empty()) {
        return std::nullopt;
    }
    std::string value = std::move(list->front());
    list->pop_front();
    return value;
}

std::vector<std::string> Store::lrange(const std::string& key, int64_t start, int64_t stop) {
    std::vector<std::string> result;
    auto* list = get_list(key);
    if (!list) {
        return result;
    }

    int64_t len = static_cast<int64_t>(list->size());

    if (start < 0) {
        start = len + start;
    }
    if (stop < 0) {
        stop = len + stop;
    }

    if (start < 0) {
        start = 0;
    }
    if (stop < 0) {
        stop = 0;
    }

    if (start >= len || start > stop) {
        return result;
    }

    if (stop >= len) {
        stop = len - 1;
    }

    for (int64_t i = start; i <= stop; ++i) {
        result.push_back((*list)[i]);
    }

    return result;
}
