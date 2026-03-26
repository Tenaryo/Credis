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

Redis::List* Store::get_list(const std::string& key) {
    Entry* entry = find_valid_entry(key);
    if (!entry || !std::holds_alternative<Redis::List>(entry->value)) {
        return nullptr;
    }
    return &std::get<Redis::List>(entry->value);
}

Redis::List* Store::get_or_create_list(const std::string& key) {
    Entry* entry = find_valid_entry(key);
    if (!entry) {
        data_[key] = Entry{.value = Redis::List{}};
        entry = &data_[key];
    }
    if (!std::holds_alternative<Redis::List>(entry->value)) {
        entry->value = Redis::List{};
    }
    return &std::get<Redis::List>(entry->value);
}

Redis::Stream* Store::get_stream(const std::string& key) {
    Entry* entry = find_valid_entry(key);
    if (!entry || !std::holds_alternative<Redis::Stream>(entry->value)) {
        return nullptr;
    }
    return &std::get<Redis::Stream>(entry->value);
}

Redis::Stream* Store::get_or_create_stream(const std::string& key) {
    Entry* entry = find_valid_entry(key);
    if (!entry) {
        data_[key] = Entry{.value = Redis::Stream{}};
        entry = &data_[key];
    }
    if (!std::holds_alternative<Redis::Stream>(entry->value)) {
        entry->value = Redis::Stream{};
    }
    return &std::get<Redis::Stream>(entry->value);
}

bool Store::parse_entry_id(const std::string& id, int64_t& timestamp, int64_t& sequence) {
    auto dash_pos = id.find('-');
    if (dash_pos == std::string::npos) {
        return false;
    }
    try {
        timestamp = std::stoll(id.substr(0, dash_pos));
        sequence = std::stoll(id.substr(dash_pos + 1));
        return true;
    } catch (...) {
        return false;
    }
}

bool Store::compare_entry_id(const std::string& a, const std::string& b) {
    int64_t ts_a, seq_a, ts_b, seq_b;
    if (!parse_entry_id(a, ts_a, seq_a) || !parse_entry_id(b, ts_b, seq_b)) {
        return false;
    }
    if (ts_a != ts_b) {
        return ts_a < ts_b;
    }
    return seq_a < seq_b;
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
    if (!entry || !std::holds_alternative<Redis::String>(entry->value)) {
        return std::nullopt;
    }
    return std::get<Redis::String>(entry->value);
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

std::vector<std::string> Store::lpop(const std::string& key, int64_t count) {
    std::vector<std::string> result;
    auto* list = get_list(key);
    if (!list || list->empty()) {
        return result;
    }

    if (count <= 0) {
        return result;
    }

    int64_t actual_count = std::min(count, static_cast<int64_t>(list->size()));
    result.reserve(actual_count);

    for (int64_t i = 0; i < actual_count; ++i) {
        result.push_back(std::move(list->front()));
        list->pop_front();
    }

    return result;
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

std::string Store::get_type(const std::string& key) {
    auto* entry = find_valid_entry(key);
    if (!entry) {
        return "none";
    }
    if (std::holds_alternative<Redis::String>(entry->value)) {
        return "string";
    }
    if (std::holds_alternative<Redis::List>(entry->value)) {
        return "list";
    }
    if (std::holds_alternative<Redis::Stream>(entry->value)) {
        return "stream";
    }
    return "none";
}

std::string Store::xadd(const std::string& key,
                        const std::string& id,
                        const std::vector<std::pair<std::string, std::string>>& fields) {
    int64_t timestamp, sequence;
    std::string final_id;

    bool auto_full_id = (id == "*");
    bool auto_seq = !auto_full_id && id.size() >= 2 && id.substr(id.size() - 2) == "-*";

    if (auto_full_id) {
        auto now = std::chrono::system_clock::now();
        timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        auto* stream = get_or_create_stream(key);

        if (stream->empty()) {
            sequence = 0;
        } else {
            int64_t last_ts, last_seq;
            parse_entry_id(stream->back().id, last_ts, last_seq);
            if (last_ts == timestamp) {
                sequence = last_seq + 1;
            } else {
                sequence = 0;
            }
        }

        final_id = std::to_string(timestamp) + "-" + std::to_string(sequence);
    } else if (auto_seq) {
        auto dash_pos = id.find('-');
        if (dash_pos == std::string::npos) {
            return "ERR Invalid stream ID specified";
        }
        try {
            timestamp = std::stoll(id.substr(0, dash_pos));
        } catch (...) {
            return "ERR Invalid stream ID specified";
        }

        auto* stream = get_or_create_stream(key);

        if (stream->empty()) {
            sequence = (timestamp == 0) ? 1 : 0;
        } else {
            int64_t last_ts, last_seq;
            parse_entry_id(stream->back().id, last_ts, last_seq);
            if (last_ts == timestamp) {
                sequence = last_seq + 1;
            } else {
                sequence = (timestamp == 0) ? 1 : 0;
            }
        }

        final_id = std::to_string(timestamp) + "-" + std::to_string(sequence);
    } else {
        if (!parse_entry_id(id, timestamp, sequence)) {
            return "ERR Invalid stream ID specified";
        }
        final_id = id;
    }

    if (timestamp == 0 && sequence == 0) {
        return "ERR The ID specified in XADD must be greater than 0-0";
    }

    auto* stream = get_or_create_stream(key);

    if (!stream->empty()) {
        const std::string& last_id = stream->back().id;
        if (!compare_entry_id(last_id, final_id)) {
            return "ERR The ID specified in XADD is equal or smaller than the target stream top "
                   "item";
        }
    }

    Redis::StreamEntry entry;
    entry.id = final_id;
    entry.fields = fields;
    stream->push_back(std::move(entry));

    return final_id;
}
std::vector<Redis::StreamEntry>
Store::xrange(const std::string& key, const std::string& start, const std::string& end) {
    std::vector<Redis::StreamEntry> result;

    auto* stream = get_stream(key);
    if (!stream || stream->empty()) {
        return result;
    }

    std::string start_id = start;
    std::string end_id = end;

    auto start_dash = start.find('-');
    if (start_dash == std::string::npos) {
        start_id = start + "-0";
    }

    auto end_dash = end.find('-');
    if (end_dash == std::string::npos) {
        end_id = end + "-9223372036854775807";
    }

    for (const auto& entry : *stream) {
        bool gte_start = !compare_entry_id(entry.id, start_id);
        bool lte_end = !compare_entry_id(end_id, entry.id);

        if (gte_start && lte_end) {
            result.push_back(entry);
        }
    }

    return result;
}

std::vector<Redis::StreamEntry> Store::xread(const std::string& key, const std::string& id) {
    std::vector<Redis::StreamEntry> result;

    auto* stream = get_stream(key);
    if (!stream || stream->empty()) {
        return result;
    }

    std::string threshold_id = id;
    auto dash = id.find('-');
    if (dash == std::string::npos) {
        threshold_id = id + "-0";
    }

    for (const auto& entry : *stream) {
        if (compare_entry_id(threshold_id, entry.id)) {
            result.push_back(entry);
        }
    }

    return result;
}
