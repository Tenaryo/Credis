#include "handler/pubsub_commands.hpp"

#include <algorithm>

#include "protocol/resp_codec.hpp"
#include "pubsub/pubsub_manager.hpp"

namespace credis::handler {

void handle_subscribe(CommandContext& ctx, int fd, std::string_view channel) {
    size_t count = ctx.pubsub_manager ? ctx.pubsub_manager->get().subscribe(fd, std::string(channel)) : 1;
    *ctx.out += "*3\r\n";
    credis::protocol::encode_bulk_string_into(*ctx.out, "subscribe");
    credis::protocol::encode_bulk_string_into(*ctx.out, channel);
    credis::protocol::encode_integer_into(*ctx.out, static_cast<int64_t>(count));
}

void handle_unsubscribe(CommandContext& ctx, int fd, std::string_view channel) {
    size_t count = ctx.pubsub_manager ? ctx.pubsub_manager->get().unsubscribe(fd, channel) : 0;
    *ctx.out += "*3\r\n";
    credis::protocol::encode_bulk_string_into(*ctx.out, "unsubscribe");
    credis::protocol::encode_bulk_string_into(*ctx.out, channel);
    credis::protocol::encode_integer_into(*ctx.out, static_cast<int64_t>(count));
}

void handle_publish(CommandContext& ctx,
                    std::string_view channel,
                    std::string_view message,
                    const std::function<void(int, const std::string&)>& send_to_client) {
    if (ctx.pubsub_manager) {
        const auto& subs = ctx.pubsub_manager->get().get_subscribers(channel);
        if (send_to_client) {
            auto msg = credis::protocol::encode_array({"message", std::string(channel), std::string(message)});
            std::ranges::for_each(subs, [&](int cfd) { send_to_client(cfd, msg); });
        }
        credis::protocol::encode_integer_into(*ctx.out, static_cast<int64_t>(subs.size()));
        return;
    }
    credis::protocol::encode_integer_into(*ctx.out, 0);
}

} // namespace credis::handler
