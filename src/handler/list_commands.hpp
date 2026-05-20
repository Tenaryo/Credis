#pragma once

#include <functional>
#include <string>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_rpush(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_lpush(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_lpop(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_lrange(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_blpop(CommandContext& ctx, int fd, const std::vector<std::string>& args) -> ProcessResult;
auto handle_rpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;
auto handle_lpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;

} // namespace credis::handler
