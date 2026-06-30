#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

void handle_xadd(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_xrange(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_xread(CommandContext& ctx, const std::vector<std::string_view>& args);
auto handle_xread_with_blocking(CommandContext& ctx, int fd, const std::vector<std::string_view>& args)
    -> ProcessResult;
auto handle_xadd_with_blocking(CommandContext& ctx,
                               const std::vector<std::string_view>& args,
                               const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;

} // namespace credis::handler
