#pragma once

#include <string>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

void handle_geoadd(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_geopos(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_geodist(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_geosearch(CommandContext& ctx, const std::vector<std::string_view>& args);

} // namespace credis::handler
