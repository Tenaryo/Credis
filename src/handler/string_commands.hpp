#pragma once

#include <string>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_set(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_get(CommandContext& ctx, const std::string& key) -> std::string;
auto handle_incr(CommandContext& ctx, const std::string& key) -> std::string;

} // namespace credis::handler
