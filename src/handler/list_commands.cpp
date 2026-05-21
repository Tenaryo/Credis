#include "handler/list_commands.hpp"

#include <algorithm>
#include <chrono>

#include "blocking_manager/blocking_manager.hpp"
#include "protocol/resp_parser.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"

namespace credis::handler {

auto handle_rpush(CommandContext& ctx, const std::vector<std::string>& args) -> std::string {
    const std::string& key = args[1];
    if (!ctx.store.key_is_absent_or_holds<credis::store::List>(key)) {
        return credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    int64_t count = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        count = ctx.store.rpush(key, args[i]);
    }
    return credis::protocol::encode_integer(count);
}

auto handle_lpush(CommandContext& ctx, const std::vector<std::string>& args) -> std::string {
    const std::string& key = args[1];
    if (!ctx.store.key_is_absent_or_holds<credis::store::List>(key)) {
        return credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    int64_t count = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        count = ctx.store.lpush(key, args[i]);
    }
    return credis::protocol::encode_integer(count);
}

auto handle_lpop(CommandContext& ctx, const std::vector<std::string>& args) -> std::string {
    const std::string& key = args[1];

    if (args.size() == 2) {
        auto elements = ctx.store.lpop(key, 1);
        if (elements.empty()) {
            return credis::protocol::encode_null_bulk_string();
        }
        return credis::protocol::encode_bulk_string(elements[0]);
    }

    auto parsed = credis::util::parse_int<int64_t>(args[2]);
    if (!parsed) {
        return credis::protocol::encode_error("ERR value is not an integer or out of range");
    }
    int64_t count = *parsed;

    if (count <= 0) {
        return credis::protocol::encode_array({});
    }

    auto elements = ctx.store.lpop(key, count);
    return credis::protocol::encode_array(elements);
}

auto handle_lrange(CommandContext& ctx, const std::vector<std::string>& args) -> std::string {
    const std::string& key = args[1];

    auto start_opt = credis::util::parse_int<int64_t>(args[2]);
    auto stop_opt = credis::util::parse_int<int64_t>(args[3]);
    if (!start_opt || !stop_opt) {
        return credis::protocol::encode_error("ERR value is not an integer or out of range");
    }

    auto elements = ctx.store.lrange(key, *start_opt, *stop_opt);
    return credis::protocol::encode_array(elements);
}

auto handle_blpop(CommandContext& ctx, int fd, const std::vector<std::string>& args) -> ProcessResult {
    const std::string& key = args[1];

    auto timeout_opt = credis::util::parse_double(args[2]);
    if (!timeout_opt) {
        return ProcessResult::normal(credis::protocol::encode_error("ERR value is not an integer or out of range"));
    }
    if (*timeout_opt < 0) {
        return ProcessResult::normal(credis::protocol::encode_error("ERR timeout is negative"));
    }
    double timeout_sec = *timeout_opt;

    auto elements = ctx.store.lpop(key, 1);
    if (!elements.empty()) {
        return ProcessResult::normal(credis::protocol::encode_array({key, elements[0]}));
    }

    if (ctx.blocking_manager) {
        auto timeout_ms = std::chrono::milliseconds(static_cast<int64_t>(timeout_sec * 1000));
        ctx.blocking_manager->get().block_client(fd, key, timeout_ms);
        return ProcessResult::block();
    }

    return ProcessResult::normal(credis::protocol::encode_error("ERR blocking not available"));
}

auto handle_rpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult {
    const std::string& key = args[1];
    int64_t count = 0;

    for (size_t i = 2; i < args.size(); ++i) {
        if (ctx.blocking_manager) {
            auto blocked = ctx.blocking_manager->get().wake_client(key);
            if (blocked) {
                count = ctx.store.rpush(key, args[i]);
                auto elements = ctx.store.lpop(key, 1);
                if (!elements.empty()) {
                    send_to_client(blocked->fd, credis::protocol::encode_array({key, elements[0]}));
                }
                continue;
            }
        }
        count = ctx.store.rpush(key, args[i]);
    }
    return ProcessResult::normal(credis::protocol::encode_integer(count));
}

auto handle_lpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult {
    const std::string& key = args[1];
    int64_t count = ctx.store.llen(key);

    for (size_t i = 2; i < args.size(); ++i) {
        if (ctx.blocking_manager) {
            auto blocked = ctx.blocking_manager->get().wake_client(key);
            if (blocked) {
                send_to_client(blocked->fd, credis::protocol::encode_array({key, args[i]}));
                ++count;
                continue;
            }
        }
        count = ctx.store.lpush(key, args[i]);
    }
    return ProcessResult::normal(credis::protocol::encode_integer(count));
}

} // namespace credis::handler
