#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

void handle_rpush(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_lpush(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_lpop(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_rpop(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_lrange(CommandContext& ctx, const std::vector<std::string_view>& args);
auto handle_blpop(CommandContext& ctx, int fd, const std::vector<std::string_view>& args) -> ProcessResult;
auto handle_rpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string_view>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;
auto handle_lpush_with_blocking(CommandContext& ctx,
                                const std::vector<std::string_view>& args,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;

} // namespace credis::handler
