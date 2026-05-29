#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_zadd(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string;
auto handle_zrank(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string;
auto handle_zrange(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string;
auto handle_zcard(CommandContext& ctx, std::string_view key) -> std::string;
auto handle_zscore(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string;
auto handle_zrem(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string;

} // namespace credis::handler
