#include "handler/string_commands.hpp"

#include <optional>

#include "protocol/resp_codec.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::handler {

void handle_set(CommandContext& ctx, const std::vector<std::string_view>& args) {
    std::string_view key = args[1];
    std::string_view value = args[2];

    std::optional<uint64_t> ttl_ms;
    for (size_t i = 3; i < args.size(); ++i) {
        auto option = credis::util::to_upper(args[i]);
        if (option == "EX" || option == "PX") {
            if (i + 1 >= args.size()) {
                credis::protocol::encode_error_into(*ctx.out, "ERR syntax error");
                return;
            }
            auto parsed = credis::util::parse_int<uint64_t>(args[i + 1]);
            if (!parsed) {
                credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
                return;
            }
            ttl_ms = (option == "EX") ? *parsed * 1000 : *parsed;
            ++i;
        }
    }

    if (!ctx.store.set_if_valid_type(key, std::string(value), ttl_ms)) {
        credis::protocol::encode_error_into(*ctx.out,
                                            "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    *ctx.out += credis::protocol::kRespOk;
}

void handle_get(CommandContext& ctx, std::string_view key) {
    auto value = ctx.store.get(key);
    if (value) {
        credis::protocol::encode_bulk_string_into(*ctx.out, *value);
    } else {
        credis::protocol::encode_null_bulk_string_into(*ctx.out);
    }
}

void handle_incr(CommandContext& ctx, std::string_view key) {
    auto result = ctx.store.incr_if_valid_type(key);
    if (!result) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
        return;
    }
    credis::protocol::encode_integer_into(*ctx.out, *result);
}

void handle_mset(CommandContext& ctx, const std::vector<std::string_view>& args) {
    if ((args.size() - 1) % 2 != 0) {
        credis::protocol::encode_error_into(*ctx.out, "ERR wrong number of arguments for MSET");
        return;
    }
    for (size_t i = 1; i < args.size(); i += 2) {
        if (!ctx.store.key_is_absent_or_holds<credis::store::String>(args[i])) {
            credis::protocol::encode_error_into(*ctx.out,
                                                "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
    }
    for (size_t i = 1; i < args.size(); i += 2) {
        ctx.store.set(std::string(args[i]), std::string(args[i + 1]));
    }
    *ctx.out += credis::protocol::kRespOk;
}

} // namespace credis::handler
