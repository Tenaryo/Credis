#include "handler/pubsub_commands.hpp"

#include <algorithm>

#include "protocol/resp_codec.hpp"
#include "pubsub/pubsub_manager.hpp"

namespace credis::handler {

auto handle_subscribe(CommandContext& ctx, int fd, const std::string& channel) -> std::string {
    size_t count = ctx.pubsub_manager ? ctx.pubsub_manager->get().subscribe(fd, channel) : 1;
    auto resp = "*3\r\n" + credis::protocol::encode_bulk_string("subscribe")
                + credis::protocol::encode_bulk_string(channel)
                + credis::protocol::encode_integer(static_cast<int64_t>(count));
    return resp;
}

auto handle_unsubscribe(CommandContext& ctx, int fd, const std::string& channel) -> std::string {
    size_t count = ctx.pubsub_manager ? ctx.pubsub_manager->get().unsubscribe(fd, channel) : 0;
    auto resp = "*3\r\n" + credis::protocol::encode_bulk_string("unsubscribe")
                + credis::protocol::encode_bulk_string(channel)
                + credis::protocol::encode_integer(static_cast<int64_t>(count));
    return resp;
}

auto handle_publish(CommandContext& ctx,
                    const std::string& channel,
                    const std::string& message,
                    const std::function<void(int, const std::string&)>& send_to_client) -> std::string {
    if (ctx.pubsub_manager) {
        const auto& subs = ctx.pubsub_manager->get().get_subscribers(channel);
        if (send_to_client) {
            auto msg = credis::protocol::encode_array({"message", channel, message});
            std::ranges::for_each(subs, [&](int cfd) { send_to_client(cfd, msg); });
        }
        return credis::protocol::encode_integer(static_cast<int64_t>(subs.size()));
    }
    return credis::protocol::encode_integer(0);
}

} // namespace credis::handler
