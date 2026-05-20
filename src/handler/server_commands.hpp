#pragma once

#include <string>
#include <vector>

#include "handler/command_handler.hpp"

namespace credis::handler {

auto handle_ping(CommandContext& ctx, int fd = -1) -> std::string;
auto handle_echo(std::string_view message) -> std::string;
auto handle_info(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_config_get(CommandContext& ctx, const std::string& param) -> std::string;
auto handle_acl(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_acl_whoami() -> std::string;
auto handle_acl_getuser(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_acl_setuser(CommandContext& ctx, const std::vector<std::string>& args) -> std::string;
auto handle_auth(CommandContext& ctx, int fd, const std::vector<std::string>& args) -> std::string;
auto handle_replconf(const std::vector<std::string>& args) -> std::string;
auto handle_psync(CommandContext& ctx) -> ProcessResult;
auto handle_wait(const std::vector<std::string>& args) -> ProcessResult;

} // namespace credis::handler
