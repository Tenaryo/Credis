#pragma once

#include <functional>
#include <string>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_xadd(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_xrange(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_xread(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_xread_with_blocking(CommandContext& ctx, int fd, const std::vector<std::string>& args) -> ProcessResult;
auto handle_xadd_with_blocking(CommandContext& ctx,
                               const std::vector<std::string>& args,
                               const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;

} // namespace credis::handler
