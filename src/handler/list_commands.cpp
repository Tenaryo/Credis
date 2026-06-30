#include "handler/list_commands.hpp"

#include <algorithm>
#include <chrono>

#include "blocking_manager/blocking_manager.hpp"
#include "protocol/resp_codec.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"

namespace credis::handler {

void handle_rpush(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const std::string_view key = args[1];
    if (!ctx.store.key_is_absent_or_holds<credis::store::List>(key)) {
        credis::protocol::encode_error_into(*ctx.out,
                                            "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    int64_t count = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        count = ctx.store.rpush(std::string(key), std::string(args[i]));
    }
    credis::protocol::encode_integer_into(*ctx.out, count);
}

void handle_lpush(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const std::string_view key = args[1];
    if (!ctx.store.key_is_absent_or_holds<credis::store::List>(key)) {
        credis::protocol::encode_error_into(*ctx.out,
                                            "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    int64_t count = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        count = ctx.store.lpush(std::string(key), std::string(args[i]));
    }
    credis::protocol::encode_integer_into(*ctx.out, count);
}

void handle_lpop(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const std::string_view key = args[1];

    if (args.size() == 2) {
        auto element = ctx.store.lpop(key);
        if (element) {
            credis::protocol::encode_bulk_string_into(*ctx.out, *element);
        } else {
            credis::protocol::encode_null_bulk_string_into(*ctx.out);
        }
        return;
    }

    auto parsed = credis::util::parse_int<int64_t>(args[2]);
    if (parsed) {
        int64_t count_val = *parsed;
        if (count_val <= 0) {
            *ctx.out += "*0\r\n";
            return;
        }
        auto elements = ctx.store.lpop(key, count_val);
        credis::protocol::encode_array_into(*ctx.out, std::vector<std::string>{elements.begin(), elements.end()});
        return;
    }
    credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
}

void handle_rpop(CommandContext& ctx, const std::vector<std::string_view>& args) {
    if (args.size() == 2) {
        auto element = ctx.store.rpop(args[1]);
        if (element) {
            credis::protocol::encode_bulk_string_into(*ctx.out, *element);
        } else {
            credis::protocol::encode_null_bulk_string_into(*ctx.out);
        }
        return;
    }

    auto parsed = credis::util::parse_int<int64_t>(args[2]);
    if (!parsed) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
        return;
    }
    int64_t count_val = *parsed;

    if (count_val <= 0) {
        *ctx.out += "*0\r\n";
        return;
    }
    auto elements = ctx.store.rpop(args[1], count_val);
    credis::protocol::encode_array_into(*ctx.out, std::vector<std::string>{elements.begin(), elements.end()});
}

void handle_lrange(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const std::string_view key = args[1];

    auto start_opt = credis::util::parse_int<int64_t>(args[2]);
    auto stop_opt = credis::util::parse_int<int64_t>(args[3]);
    if (!start_opt || !stop_opt) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
        return;
    }

    auto elements = ctx.store.lrange(key, *start_opt, *stop_opt);
    credis::protocol::encode_array_into(*ctx.out, elements);
}

auto handle_blpop(CommandContext& ctx, int fd, const std::vector<std::string_view>& args) -> ProcessResult {
    const std::string_view key = args[1];

    auto timeout_opt = credis::util::parse_double(args[2]);
    if (!timeout_opt) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
        return ProcessResult::normal();
    }
    if (*timeout_opt < 0) {
        credis::protocol::encode_error_into(*ctx.out, "ERR timeout is negative");
        return ProcessResult::normal();
    }
    double timeout_sec = *timeout_opt;

    auto elements = ctx.store.lpop(key, 1);
    if (!elements.empty()) {
        credis::protocol::encode_array_into(*ctx.out, std::vector<std::string>{std::string(key), elements[0]});
        return ProcessResult::normal();
    }

    if (ctx.blocking_manager) {
        auto timeout_ms = std::chrono::milliseconds(static_cast<int64_t>(timeout_sec * 1000));
        ctx.blocking_manager->get().block_client(fd, std::string(key), timeout_ms);
        return ProcessResult::block();
    }

    credis::protocol::encode_error_into(*ctx.out, "ERR blocking not available");
    return ProcessResult::normal();
}

auto handle_rpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string_view>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult {
    const std::string_view key = args[1];
    int64_t count = 0;

    for (size_t i = 2; i < args.size(); ++i) {
        if (ctx.blocking_manager) {
            auto blocked = ctx.blocking_manager->get().wake_client(std::string(key));
            if (blocked) {
                count = ctx.store.rpush(std::string(key), std::string(args[i]));
                auto elements = ctx.store.lpop(key, 1);
                if (!elements.empty()) {
                    send_to_client(blocked->fd, credis::protocol::encode_array({std::string(key), elements[0]}));
                }
                continue;
            }
        }
        count = ctx.store.rpush(std::string(key), std::string(args[i]));
    }
    credis::protocol::encode_integer_into(*ctx.out, count);
    return ProcessResult::normal();
}

auto handle_lpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string_view>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult {
    const std::string_view key = args[1];
    int64_t count = 0;

    for (size_t i = 2; i < args.size(); ++i) {
        if (ctx.blocking_manager) {
            auto blocked = ctx.blocking_manager->get().wake_client(std::string(key));
            if (blocked) {
                count = ctx.store.lpush(std::string(key), std::string(args[i]));
                auto elements = ctx.store.lpop(key, 1);
                if (!elements.empty()) {
                    send_to_client(blocked->fd, credis::protocol::encode_array({std::string(key), elements[0]}));
                }
                continue;
            }
        }
        count = ctx.store.lpush(std::string(key), std::string(args[i]));
    }
    credis::protocol::encode_integer_into(*ctx.out, count);
    return ProcessResult::normal();
}

} // namespace credis::handler
