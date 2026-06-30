#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "handler/command_handler.hpp"

namespace credis::handler {

void handle_subscribe(CommandContext& ctx, int fd, std::string_view channel);
void handle_unsubscribe(CommandContext& ctx, int fd, std::string_view channel);
void handle_publish(CommandContext& ctx,
                    std::string_view channel,
                    std::string_view message,
                    const std::function<void(int, const std::string&)>& send_to_client);

} // namespace credis::handler
