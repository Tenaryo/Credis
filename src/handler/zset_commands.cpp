#include "handler/zset_commands.hpp"

#include <cstdio>

#include "protocol/resp_codec.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"

namespace credis::handler {

auto handle_zadd(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    if (!ctx.store.key_is_absent_or_holds<credis::store::SortedSet>(args[1])) {
        return credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    int64_t added = 0;
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        auto score = credis::util::parse_double(args[i]);
        if (!score) {
            return credis::protocol::encode_error("ERR value is not a valid float");
        }
        added += ctx.store.zadd(std::string(args[1]), *score, std::string(args[i + 1]));
    }
    return credis::protocol::encode_integer(added);
}

auto handle_zrank(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    auto rank = ctx.store.zrank(args[1], args[2]);
    return rank ? credis::protocol::encode_integer(*rank) : credis::protocol::encode_null_bulk_string();
}

auto handle_zrange(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    std::string_view key = args[1];
    auto start_opt = credis::util::parse_int<int64_t>(args[2]);
    auto stop_opt = credis::util::parse_int<int64_t>(args[3]);
    if (!start_opt || !stop_opt) {
        return credis::protocol::encode_error("ERR value is not an integer or out of range");
    }
    auto elements = ctx.store.zrange(key, *start_opt, *stop_opt);
    return credis::protocol::encode_array(elements);
}

auto handle_zcard(CommandContext& ctx, std::string_view key) -> std::string {
    return credis::protocol::encode_integer(ctx.store.zcard(key));
}

auto handle_zscore(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    auto score = ctx.store.zscore(args[1], args[2]);
    if (!score) {
        return credis::protocol::encode_null_bulk_string();
    }

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", *score);
    return credis::protocol::encode_bulk_string(buf);
}

auto handle_zrem(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    auto removed = ctx.store.zrem(args[1], args[2]);
    return credis::protocol::encode_integer(removed);
}

} // namespace credis::handler
