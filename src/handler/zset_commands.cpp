#include "handler/zset_commands.hpp"

#include <cstdio>

#include "protocol/resp_codec.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"

namespace credis::handler {

void handle_zadd(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const std::string_view key = args[1];
    auto score1 = credis::util::parse_double(args[2]);
    if (!score1) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not a valid float");
        return;
    }
    auto opt = ctx.store.zadd_if_valid_type(key, *score1, std::string(args[3]));
    if (!opt) {
        credis::protocol::encode_error_into(*ctx.out,
                                            "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    int64_t added = *opt;
    for (size_t i = 4; i < args.size(); i += 2) {
        auto score = credis::util::parse_double(args[i]);
        if (!score) {
            credis::protocol::encode_error_into(*ctx.out, "ERR value is not a valid float");
            return;
        }
        added += ctx.store.zadd(std::string(key), *score, std::string(args[i + 1]));
    }
    credis::protocol::encode_integer_into(*ctx.out, added);
}

void handle_zrank(CommandContext& ctx, const std::vector<std::string_view>& args) {
    auto rank = ctx.store.zrank(args[1], args[2]);
    if (rank) {
        credis::protocol::encode_integer_into(*ctx.out, *rank);
    } else {
        credis::protocol::encode_null_bulk_string_into(*ctx.out);
    }
}

void handle_zrange(CommandContext& ctx, const std::vector<std::string_view>& args) {
    std::string_view key = args[1];
    auto start_opt = credis::util::parse_int<int64_t>(args[2]);
    auto stop_opt = credis::util::parse_int<int64_t>(args[3]);
    if (!start_opt || !stop_opt) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
        return;
    }
    auto elements = ctx.store.zrange(key, *start_opt, *stop_opt);
    credis::protocol::encode_array_into(*ctx.out, elements);
}

void handle_zcard(CommandContext& ctx, std::string_view key) {
    credis::protocol::encode_integer_into(*ctx.out, ctx.store.zcard(key));
}

void handle_zscore(CommandContext& ctx, const std::vector<std::string_view>& args) {
    auto score = ctx.store.zscore(args[1], args[2]);
    if (!score) {
        credis::protocol::encode_null_bulk_string_into(*ctx.out);
        return;
    }

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", *score);
    credis::protocol::encode_bulk_string_into(*ctx.out, buf);
}

void handle_zrem(CommandContext& ctx, const std::vector<std::string_view>& args) {
    auto removed = ctx.store.zrem(args[1], args[2]);
    credis::protocol::encode_integer_into(*ctx.out, removed);
}

} // namespace credis::handler
