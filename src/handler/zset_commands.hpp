#pragma once

#include <string>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_zadd(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_zrank(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_zrange(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_zcard(CommandContext& ctx, const std::string& key) -> std::string;
auto handle_zscore(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_zrem(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;

} // namespace credis::handler
