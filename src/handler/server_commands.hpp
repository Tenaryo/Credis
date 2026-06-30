#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

void handle_ping(CommandContext& ctx, int fd = -1);
void handle_echo(CommandContext& ctx, std::string_view message);
void handle_info(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_config_get(CommandContext& ctx, std::string_view param);
void handle_config_set(CommandContext& ctx, std::string_view param, std::string_view value);
void handle_acl(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_acl_whoami(CommandContext& ctx);
void handle_acl_getuser(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_acl_setuser(CommandContext& ctx, const std::vector<std::string_view>& args);
void handle_auth(CommandContext& ctx, int fd, const std::vector<std::string_view>& args);
void handle_replconf(CommandContext& ctx, const std::vector<std::string_view>& args);
auto handle_psync(CommandContext& ctx) -> ProcessResult;
auto handle_wait(CommandContext& ctx, const std::vector<std::string_view>& args) -> ProcessResult;
void handle_bgrewriteaof(CommandContext& ctx);

} // namespace credis::handler
