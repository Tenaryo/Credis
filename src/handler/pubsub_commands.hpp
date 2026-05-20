#pragma once

#include <functional>
#include <string>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_subscribe(CommandContext& ctx, int fd, const std::string& channel) -> std::string;
auto handle_unsubscribe(CommandContext& ctx, int fd, const std::string& channel) -> std::string;
auto handle_publish(CommandContext& ctx,
                    const std::string& channel,
                    const std::string& message,
                    const std::function<void(int, const std::string&)>& send_to_client) -> std::string;

} // namespace credis::handler
