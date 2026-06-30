#pragma once

#include <string_view>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

void handle_set(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_get(CommandContext& ctx, std::string_view key);
void handle_incr(CommandContext& ctx, std::string_view key);
void handle_mset(CommandContext& ctx, const std::vector<std::string_view>& args);

} // namespace credis::handler
