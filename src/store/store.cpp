#include "store.hpp"

#include "util/parse.hpp"

namespace credis::store {

using credis::protocol::StreamId;
using credis::util::parse_int;

auto Store::get_current_time() -> std::chrono::steady_clock::time_point {
    return std::chrono::steady_clock::now();
}

auto Store::is_expired(const Entry& entry) -> bool {
    return entry.expiry && get_current_time() >= *entry.expiry;
}

auto Store::find_valid_entry(std::string_view key) -> Store::Entry* {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return nullptr;
    }
    if (is_expired(it->second)) [[unlikely]] {
        data_.erase(it);
        return nullptr;
    }
    return &it->second;
}

auto Store::lower_bound(const Stream& stream, const credis::protocol::StreamId& target) -> size_t {
    size_t lo = 0;
    size_t hi = stream.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        auto sid = StreamId::parse(stream[mid].id);
        if (sid && *sid < target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

auto Store::upper_bound(const Stream& stream, const credis::protocol::StreamId& target) -> size_t {
    size_t lo = 0;
    size_t hi = stream.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        auto sid = StreamId::parse(stream[mid].id);
        if (sid && !(target < *sid)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

void Store::set(std::string key, std::string value, std::optional<uint64_t> ttl_ms) {
    touch_key(key);
    Entry entry;
    entry.value = std::move(value);
    if (ttl_ms) {
        entry.expiry = get_current_time() + std::chrono::milliseconds(*ttl_ms);
    }
    data_[std::move(key)] = std::move(entry);
}

auto Store::get(std::string_view key) -> std::optional<std::string> {
    Entry* entry = find_valid_entry(key);
    if ((entry == nullptr) || !std::holds_alternative<String>(entry->value)) {
        return std::nullopt;
    }
    return std::get<String>(entry->value);
}

auto Store::incr(std::string_view key) -> std::optional<int64_t> {
    touch_key(key);
    Entry* entry = find_valid_entry(key);

    if (entry == nullptr) {
        data_[std::string(key)] = Entry{String("1"), {}};
        return 1;
    }

    if (!std::holds_alternative<String>(entry->value)) {
        return std::nullopt;
    }

    const std::string& str_value = std::get<String>(entry->value);
    auto parsed = parse_int<int64_t>(str_value);
    if (!parsed || *parsed == INT64_MAX) {
        return std::nullopt;
    }

    int64_t new_value = *parsed + 1;
    entry->value = String(std::to_string(new_value));
    return new_value;
}

auto Store::exists(std::string_view key) -> bool {
    return find_valid_entry(key) != nullptr;
}

auto Store::del(std::string_view key) -> bool {
    touch_key(key);
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    data_.erase(it);
    return true;
}

auto Store::rpush(std::string key, std::string value) -> int64_t {
    touch_key(key);
    auto* list = get_or_create_list(std::move(key));
    list->push_back(std::move(value));
    return static_cast<int64_t>(list->size());
}

auto Store::lpush(std::string key, std::string value) -> int64_t {
    touch_key(key);
    auto* list = get_or_create_list(std::move(key));
    list->push_front(std::move(value));
    return static_cast<int64_t>(list->size());
}

auto Store::llen(std::string_view key) -> int64_t {
    auto* list = get_list(key);
    return (list != nullptr) ? static_cast<int64_t>(list->size()) : 0;
}

auto Store::lpop(std::string_view key) -> std::optional<std::string> {
    touch_key(key);
    auto* list = get_list(key);
    if ((list == nullptr) || list->empty()) {
        return std::nullopt;
    }
    auto val = std::move(list->front());
    list->pop_front();
    return val;
}

auto Store::lpop(std::string_view key, int64_t count) -> std::vector<std::string> {
    touch_key(key);
    std::vector<std::string> result;
    auto* list = get_list(key);
    if ((list == nullptr) || list->empty()) {
        return result;
    }

    if (count <= 0) {
        return result;
    }

    int64_t actual_count = std::min(count, static_cast<int64_t>(list->size()));
    result.reserve(static_cast<size_t>(actual_count));

    for (int64_t i = 0; i < actual_count; ++i) {
        result.push_back(std::move(list->front()));
        list->pop_front();
    }

    return result;
}

auto Store::rpop(std::string_view key) -> std::optional<std::string> {
    touch_key(key);
    auto* list = get_list(key);
    if ((list == nullptr) || list->empty()) {
        return std::nullopt;
    }
    auto val = std::move(list->back());
    list->pop_back();
    return val;
}

auto Store::rpop(std::string_view key, int64_t count) -> std::vector<std::string> {
    touch_key(key);
    std::vector<std::string> result;
    auto* list = get_list(key);
    if ((list == nullptr) || list->empty()) {
        return result;
    }

    if (count <= 0) {
        return result;
    }

    int64_t actual_count = std::min(count, static_cast<int64_t>(list->size()));
    result.reserve(static_cast<size_t>(actual_count));

    for (int64_t i = 0; i < actual_count; ++i) {
        result.push_back(std::move(list->back()));
        list->pop_back();
    }

    return result;
}

auto Store::lrange(std::string_view key, int64_t start, int64_t stop) -> std::vector<std::string> {
    std::vector<std::string> result;
    auto* list = get_list(key);
    if (list == nullptr) {
        return result;
    }

    auto len = static_cast<int64_t>(list->size());

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
        result.push_back((*list)[static_cast<size_t>(i)]);
    }

    return result;
}

auto Store::get_type(std::string_view key) -> std::string {
    auto* entry = find_valid_entry(key);
    if (entry == nullptr) {
        return "none";
    }
    if (std::holds_alternative<String>(entry->value)) {
        return "string";
    }
    if (std::holds_alternative<List>(entry->value)) {
        return "list";
    }
    if (std::holds_alternative<Stream>(entry->value)) {
        return "stream";
    }
    if (std::holds_alternative<SortedSet>(entry->value)) {
        return "zset";
    }
    return "none";
}

auto Store::xadd(std::string key,
                 const std::string& id,
                 const std::vector<std::pair<std::string, std::string>>& fields) -> std::string {
    touch_key(key);
    auto* stream = get_or_create_stream(std::move(key));

    int64_t timestamp{};
    int64_t sequence{};
    std::string final_id;

    bool auto_full_id = (id == "*");
    bool auto_seq = !auto_full_id && id.size() >= 2 && id.substr(id.size() - 2) == "-*";

    if (auto_full_id) {
        auto now = std::chrono::system_clock::now();
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        if (stream->empty()) {
            sequence = 0;
        } else {
            auto last = StreamId::parse(stream->back().id);
            sequence = (last && last->timestamp == timestamp) ? last->sequence + 1 : 0;
        }

        final_id = StreamId{timestamp, sequence}.to_string();
    } else if (auto_seq) {
        auto dash_pos = id.find('-');
        if (dash_pos == std::string::npos) {
            return "ERR Invalid stream ID specified";
        }
        auto ts = parse_int<int64_t>(id.substr(0, dash_pos));
        if (!ts) {
            return "ERR Invalid stream ID specified";
        }
        timestamp = *ts;

        if (stream->empty()) {
            sequence = (timestamp == 0) ? 1 : 0;
        } else {
            auto last = StreamId::parse(stream->back().id);
            if (last && last->timestamp == timestamp) {
                sequence = last->sequence + 1;
            } else {
                sequence = (timestamp == 0) ? 1 : 0;
            }
        }

        final_id = StreamId{timestamp, sequence}.to_string();
    } else {
        auto sid = StreamId::parse(id);
        if (!sid) {
            return "ERR Invalid stream ID specified";
        }
        timestamp = sid->timestamp;
        sequence = sid->sequence;
        final_id = id;
    }

    if (timestamp == 0 && sequence == 0) {
        return "ERR The ID specified in XADD must be greater than 0-0";
    }

    if (!stream->empty()) {
        auto last = StreamId::parse(stream->back().id);
        if (!last || !(last < StreamId{timestamp, sequence})) {
            return "ERR The ID specified in XADD is equal or smaller than the target stream top "
                   "item";
        }
    }

    stream->push_back(StreamEntry{final_id, fields});
    return final_id;
}

auto Store::xrange(std::string_view key, std::string_view start, std::string_view end) -> std::span<const StreamEntry> {
    auto* stream = get_stream(key);
    if ((stream == nullptr) || stream->empty()) {
        return {};
    }

    auto start_sid = start == "-" ? StreamId{0, 0}
                     : start.find('-') == std::string_view::npos
                         ? StreamId{parse_int<int64_t>(start).value_or(0), 0}
                         : StreamId::parse(start).value_or(StreamId{INT64_MAX, INT64_MAX});
    auto end_sid = end == "+"                                ? StreamId{INT64_MAX, INT64_MAX}
                   : end.find('-') == std::string_view::npos ? StreamId{parse_int<int64_t>(end).value_or(0), INT64_MAX}
                                                             : StreamId::parse(end).value_or(StreamId{-1, -1});

    auto lo = lower_bound(*stream, start_sid);
    auto hi = upper_bound(*stream, end_sid);

    if (hi <= lo) {
        return {};
    }
    return {stream->data() + lo, hi - lo};
}

auto Store::xread(std::string_view key, std::string_view id) -> std::span<const StreamEntry> {
    auto* stream = get_stream(key);
    if ((stream == nullptr) || stream->empty()) {
        return {};
    }

    auto threshold_sid = id.find('-') == std::string_view::npos
                             ? StreamId{parse_int<int64_t>(id).value_or(0), 0}
                             : StreamId::parse(id).value_or(StreamId{INT64_MAX, INT64_MAX});

    auto lo = upper_bound(*stream, threshold_sid);

    if (lo >= stream->size()) {
        return {};
    }
    return {stream->data() + lo, stream->size() - lo};
}

auto Store::get_stream_max_id(std::string_view key) -> std::optional<std::string> {
    auto* stream = get_stream(key);
    if ((stream == nullptr) || stream->empty()) {
        return std::nullopt;
    }

    return stream->back().id;
}

auto Store::zadd(std::string key, double score, std::string member) -> int64_t {
    touch_key(key);
    auto* zset = get_or_create_zset(std::move(key));
    return zset->add(score, std::move(member));
}

auto Store::zrank(std::string_view key, std::string_view member) -> std::optional<int64_t> {
    auto* zset = get_zset(key);
    if (zset == nullptr) {
        return std::nullopt;
    }

    auto it = zset->member_scores.find(std::string(member));
    if (it == zset->member_scores.end()) {
        return std::nullopt;
    }

    auto entry_it = zset->entries.find({it->second, std::string(member)});
    if (entry_it == zset->entries.end()) {
        return std::nullopt;
    }

    return static_cast<int64_t>(std::distance(zset->entries.begin(), entry_it));
}

auto Store::zrange(std::string_view key, int64_t start, int64_t stop) -> std::vector<std::string> {
    auto* zset = get_zset(key);
    if (zset == nullptr) {
        return {};
    }

    auto len = static_cast<int64_t>(zset->entries.size());

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
        return {};
    }
    if (stop >= len) {
        stop = len - 1;
    }

    auto it = std::next(zset->entries.begin(), start);
    auto end_it = std::next(zset->entries.begin(), stop + 1);

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(stop - start + 1));
    for (; it != end_it; ++it) {
        result.push_back(it->second);
    }
    return result;
}

auto Store::zcard(std::string_view key) -> int64_t {
    auto* zset = get_zset(key);
    return (zset != nullptr) ? static_cast<int64_t>(zset->member_scores.size()) : 0;
}

auto Store::zscore(std::string_view key, std::string_view member) -> std::optional<double> {
    auto* zset = get_zset(key);
    if (zset == nullptr) {
        return std::nullopt;
    }

    auto it = zset->member_scores.find(std::string(member));
    if (it == zset->member_scores.end()) {
        return std::nullopt;
    }

    return it->second;
}

auto Store::zrem(std::string_view key, std::string_view member) -> int64_t {
    touch_key(key);
    auto* zset = get_zset(key);
    if (zset == nullptr) {
        return 0;
    }
    return zset->remove(member);
}

auto Store::zgetall(std::string_view key) -> std::vector<std::pair<std::string, double>> {
    auto* zset = get_zset(key);
    if (zset == nullptr) {
        return {};
    }
    std::vector<std::pair<std::string, double>> result;
    result.reserve(zset->member_scores.size());
    for (const auto& [member, score] : zset->member_scores) {
        result.emplace_back(member, score);
    }
    return result;
}

auto Store::keys() -> std::vector<std::string> {
    std::vector<std::string> result;
    for (auto it = data_.begin(); it != data_.end();) {
        if (is_expired(it->second)) {
            it = data_.erase(it);
        } else {
            result.push_back(it->first);
            ++it;
        }
    }
    return result;
}

} // namespace credis::store
