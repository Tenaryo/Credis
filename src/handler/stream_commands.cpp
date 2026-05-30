#include "handler/stream_commands.hpp"

#include <algorithm>
#include <chrono>
#include <span>
#include <utility>

#include "blocking_manager/blocking_manager.hpp"
#include "protocol/resp_codec.hpp"
#include "store/store.hpp"
#include "store/stream_id.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::handler {

auto handle_xadd(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    std::string key(args[1]);
    std::string id(args[2]);

    if (!ctx.store.key_is_absent_or_holds<credis::store::Stream>(key)) {
        return credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    if ((args.size() - 3) % 2 != 0) {
        return credis::protocol::encode_error("ERR wrong number of arguments for 'xadd' command");
    }

    std::vector<std::pair<std::string, std::string>> fields;
    for (size_t i = 3; i < args.size(); i += 2) {
        fields.emplace_back(std::string(args[i]), std::string(args[i + 1]));
    }

    std::string result = ctx.store.xadd(key, id, fields);

    if (result.starts_with("ERR")) {
        return credis::protocol::encode_error(result);
    }

    return credis::protocol::encode_bulk_string(result);
}

auto handle_xrange(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    std::string_view key = args[1];
    std::string_view start = args[2];
    std::string_view end = args[3];

    auto entries = ctx.store.xrange(key, start, end);
    return credis::protocol::encode_entries(entries);
}

auto handle_xread(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    size_t streams_idx = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (credis::util::to_upper(args[i]) == "STREAMS") {
            streams_idx = i;
            break;
        }
    }

    if (streams_idx == 0) {
        return credis::protocol::encode_error("ERR syntax error");
    }

    size_t num_pairs = args.size() - streams_idx - 1;
    if (num_pairs == 0 || num_pairs % 2 != 0) {
        return credis::protocol::encode_error("ERR wrong number of arguments for 'xread' command");
    }

    size_t num_streams = num_pairs / 2;
    std::vector<std::pair<std::string, std::span<const credis::store::StreamEntry>>> results;

    for (size_t i = 0; i < num_streams; ++i) {
        std::string_view key = args[streams_idx + 1 + i];
        std::string_view id = args[streams_idx + 1 + num_streams + i];

        auto entries = ctx.store.xread(key, id);
        results.emplace_back(std::string(key), entries);
    }

    return credis::protocol::encode_stream_entries(results);
}

auto handle_xread_with_blocking(CommandContext& ctx, int fd, const std::vector<std::string_view>& args)
    -> ProcessResult {
    bool has_block = false;
    int64_t timeout_ms = 0;
    size_t start_idx = 1;

    if (args.size() > start_idx) {
        if (credis::util::to_upper(args[start_idx]) == "BLOCK") {
            has_block = true;
            if (start_idx + 1 >= args.size()) {
                return ProcessResult::normal(credis::protocol::encode_error("ERR syntax error"));
            }
            auto parsed = credis::util::parse_int<int64_t>(args[start_idx + 1]);
            if (!parsed) {
                return ProcessResult::normal(
                    credis::protocol::encode_error("ERR value is not an integer or out of range"));
            }
            timeout_ms = *parsed;
            start_idx += 2;
        }
    }

    size_t streams_idx = 0;
    for (size_t i = start_idx; i < args.size(); ++i) {
        if (credis::util::to_upper(args[i]) == "STREAMS") {
            streams_idx = i;
            break;
        }
    }

    if (streams_idx == 0) {
        return ProcessResult::normal(credis::protocol::encode_error("ERR syntax error"));
    }

    size_t num_pairs = args.size() - streams_idx - 1;
    if (num_pairs == 0 || num_pairs % 2 != 0) {
        return ProcessResult::normal(
            credis::protocol::encode_error("ERR wrong number of arguments for 'xread' command"));
    }

    size_t num_streams = num_pairs / 2;
    if (has_block && num_streams != 1) {
        return ProcessResult::normal(credis::protocol::encode_error("ERR BLOCK only supports single stream"));
    }

    std::vector<std::pair<std::string, std::span<const credis::store::StreamEntry>>> results;

    for (size_t i = 0; i < num_streams; ++i) {
        std::string_view key = args[streams_idx + 1 + i];
        std::string_view id_arg = args[streams_idx + 1 + num_streams + i];

        std::string id(id_arg);
        if (id_arg == "$") {
            auto max_id = ctx.store.get_stream_max_id(key);
            id = max_id.value_or("0-0");
        }

        auto entries = ctx.store.xread(key, id);
        results.emplace_back(std::string(key), entries);
    }

    bool has_data = std::ranges::any_of(results, [](const auto& p) { return !p.second.empty(); });

    if (has_data || !has_block) {
        return ProcessResult::normal(credis::protocol::encode_stream_entries(results));
    }

    if (ctx.blocking_manager) {
        const std::string key(args[streams_idx + 1]);
        std::string_view id_arg = args[streams_idx + 1 + num_streams];

        std::string id(id_arg);
        if (id_arg == "$") {
            auto max_id = ctx.store.get_stream_max_id(key);
            id = max_id.value_or("0-0");
        }

        auto sid = credis::protocol::StreamId::parse(id).value_or(credis::protocol::StreamId{0, 0});
        ctx.blocking_manager->get().block_client_for_stream(fd, key, sid, std::chrono::milliseconds(timeout_ms));
        return ProcessResult::block();
    }

    return ProcessResult::normal(credis::protocol::encode_error("ERR blocking not available"));
}

auto handle_xadd_with_blocking(CommandContext& ctx,
                               const std::vector<std::string_view>& args,
                               const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult {
    std::string key(args[1]);
    std::string id(args[2]);

    if (!ctx.store.key_is_absent_or_holds<credis::store::Stream>(key)) {
        return ProcessResult::normal(
            credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value"));
    }

    if ((args.size() - 3) % 2 != 0) {
        return ProcessResult::normal(
            credis::protocol::encode_error("ERR wrong number of arguments for 'xadd' command"));
    }

    std::vector<std::pair<std::string, std::string>> fields;
    for (size_t i = 3; i < args.size(); i += 2) {
        fields.emplace_back(std::string(args[i]), std::string(args[i + 1]));
    }

    std::string new_id = ctx.store.xadd(key, id, fields);

    if (new_id.starts_with("ERR")) {
        return ProcessResult::normal(credis::protocol::encode_error(new_id));
    }

    if (ctx.blocking_manager) {
        while (auto blocked = ctx.blocking_manager->get().wake_client_for_stream(key, new_id)) {
            auto entries = ctx.store.xread(key, blocked->last_id.to_string());
            auto response = credis::protocol::encode_stream_entries({{key, entries}});
            send_to_client(blocked->fd, response);
        }
    }

    return ProcessResult::normal(credis::protocol::encode_bulk_string(new_id));
}

} // namespace credis::handler
