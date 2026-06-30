#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

void handle_zadd(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_zrank(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_zrange(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_zcard(CommandContext& ctx, std::string_view key);
void handle_zscore(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_zrem(CommandContext& ctx, const std::vector<std::string_view>& args);

} // namespace credis::handler
