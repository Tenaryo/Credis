#pragma once

#include <string_view>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_set(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string;
auto handle_get(CommandContext& ctx, std::string_view key) -> std::string;
auto handle_incr(CommandContext& ctx, std::string_view key) -> std::string;
auto handle_mset(CommandContext& ctx, const std::vector<std::string_view>& args) -> std::string;

} // namespace credis::handler
