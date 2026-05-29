#include "handler/string_commands.hpp"

#include <optional>

#include "protocol/resp_codec.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::handler {

auto handle_set(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    std::string_view key = args[1];
    std::string_view value = args[2];

    if (!ctx.store.key_is_absent_or_holds<credis::store::String>(key)) {
        return credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    std::optional<uint64_t> ttl_ms;

    for (size_t i = 3; i < args.size(); ++i) {
        auto option = credis::util::to_upper(std::string(args[i]));

        if (option == "EX" || option == "PX") {
            if (i + 1 >= args.size()) {
                return credis::protocol::encode_error("ERR syntax error");
            }

            auto parsed = credis::util::parse_int<uint64_t>(args[i + 1]);
            if (!parsed) {
                return credis::protocol::encode_error("ERR value is not an integer or out of range");
            }

            ttl_ms = (option == "EX") ? *parsed * 1000 : *parsed;
            ++i;
        }
    }

    ctx.store.set(std::string(key), std::string(value), ttl_ms);
    return credis::protocol::kRespOk;
}

auto handle_get(CommandContext& ctx, std::string_view key) -> std::string {
    auto value = ctx.store.get(key);
    return value ? credis::protocol::encode_bulk_string(*value) : credis::protocol::encode_null_bulk_string();
}

auto handle_incr(CommandContext& ctx, std::string_view key) -> std::string {
    if (!ctx.store.key_is_absent_or_holds<credis::store::String>(key)) {
        return credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    auto result = ctx.store.incr(key);
    if (!result) {
        return credis::protocol::encode_error("ERR value is not an integer or out of range");
    }
    return credis::protocol::encode_integer(*result);
}

auto handle_mset(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string {
    if ((args.size() - 1) % 2 != 0) {
        return credis::protocol::encode_error("ERR wrong number of arguments for MSET");
    }
    for (size_t i = 1; i < args.size(); i += 2) {
        if (!ctx.store.key_is_absent_or_holds<credis::store::String>(args[i])) {
            return credis::protocol::encode_error("WRONGTYPE Operation against a key holding the wrong kind of value");
        }
        ctx.store.set(std::string(args[i]), std::string(args[i + 1]));
    }
    return credis::protocol::kRespOk;
}

} // namespace credis::handler
