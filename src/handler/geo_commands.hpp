#pragma once

#include <string>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_geoadd(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_geopos(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_geodist(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_geosearch(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;

} // namespace credis::handler
