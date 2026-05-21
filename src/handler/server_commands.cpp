#include "handler/server_commands.hpp"

#include <string>

#include "protocol/resp_parser.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "rdb/rdb_constants.hpp"
#include "server/acl_manager.hpp"
#include "server/server_config.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::handler {

auto handle_ping(CommandContext& ctx, int fd) -> std::string {
    if (ctx.pubsub_manager && fd >= 0 && ctx.pubsub_manager->get().is_subscribed(fd)) {
        return credis::protocol::encode_array({"pong", ""});
    }
    return credis::protocol::encode_simple_string("PONG");
}

auto handle_echo(std::string_view message) -> std::string {
    return credis::protocol::encode_bulk_string(message);
}

auto handle_info(CommandContext& ctx, const std::vector<std::string>& /* args */) -> std::string {
    const auto* role = ctx.config.is_replica() ? "slave" : "master";
    auto info = "# Replication\r\nrole:" + std::string(role) + "\r\nmaster_replid:" + ctx.config.master_replid
                + "\r\nmaster_repl_offset:" + std::to_string(ctx.config.master_repl_offset) + "\r\n";
    return credis::protocol::encode_bulk_string(info);
}

auto handle_config_get(CommandContext& ctx, const std::string& param) -> std::string {
    auto upper = credis::util::to_upper(param);
    if (upper == "DIR") {
        return "*2\r\n$3\r\ndir\r\n" + credis::protocol::encode_bulk_string(ctx.config.dir);
    }
    if (upper == "DBFILENAME") {
        auto value = ctx.config.dbfilename.empty() ? credis::protocol::encode_null_bulk_string()
                                                   : credis::protocol::encode_bulk_string(ctx.config.dbfilename);
        return "*2\r\n$10\r\ndbfilename\r\n" + value;
    }
    if (upper == "APPENDONLY") {
        return "*2\r\n$10\r\nappendonly\r\n" + credis::protocol::encode_bulk_string(ctx.config.appendonly);
    }
    if (upper == "APPENDDIRNAME") {
        return "*2\r\n$13\r\nappenddirname\r\n" + credis::protocol::encode_bulk_string(ctx.config.appenddirname);
    }
    if (upper == "APPENDFILENAME") {
        return "*2\r\n$14\r\nappendfilename\r\n" + credis::protocol::encode_bulk_string(ctx.config.appendfilename);
    }
    if (upper == "APPENDFSYNC") {
        return "*2\r\n$11\r\nappendfsync\r\n" + credis::protocol::encode_bulk_string(ctx.config.appendfsync);
    }
    return credis::protocol::encode_array({});
}

auto handle_acl_whoami() -> std::string {
    return credis::protocol::encode_bulk_string("default");
}

auto handle_acl(CommandContext& ctx, const std::vector<std::string>& args) -> std::string {
    if (args.size() < 2) {
        return credis::protocol::encode_error("ERR unknown subcommand for 'ACL'. Try ACL HELP.");
    }
    auto subcmd = credis::util::to_upper(args[1]);
    if (subcmd == "WHOAMI") {
        return handle_acl_whoami();
    }
    if (subcmd == "GETUSER") {
        if (args.size() < 3) {
            return credis::protocol::encode_error("ERR wrong number of arguments for 'acl|getuser' command");
        }
        return handle_acl_getuser(ctx, args);
    }
    if (subcmd == "SETUSER") {
        if (args.size() < 3) {
            return credis::protocol::encode_error("ERR wrong number of arguments for 'acl|setuser' command");
        }
        return handle_acl_setuser(ctx, args);
    }
    return credis::protocol::encode_error("ERR unknown subcommand for 'ACL'. Try ACL HELP.");
}

auto handle_acl_getuser(CommandContext& ctx, const std::vector<std::string>& args) -> std::string {
    const auto* user = ctx.acl_manager->get_user(args[2]);
    if (user == nullptr) {
        return credis::protocol::encode_null_array();
    }
    std::vector<std::string> flags;
    if (user->nopass) {
        flags.emplace_back("nopass");
    }
    return credis::protocol::encode_raw_array({credis::protocol::encode_bulk_string("flags"),
                                               credis::protocol::encode_array(flags),
                                               credis::protocol::encode_bulk_string("passwords"),
                                               credis::protocol::encode_array(user->passwords)});
}

auto handle_acl_setuser(CommandContext& ctx, const std::vector<std::string>& args) -> std::string {
    const auto& username = args[2];
    for (size_t i = 3; i < args.size(); ++i) {
        const auto& rule = args[i];
        if (!rule.empty() && rule[0] == '>') {
            ctx.acl_manager->set_password(username, rule.substr(1));
        }
    }
    return credis::protocol::encode_simple_string("OK");
}

auto handle_auth(CommandContext& ctx, int fd, const std::vector<std::string>& args) -> std::string {
    if (args.size() < 3) {
        return credis::protocol::encode_error("ERR wrong number of arguments for 'auth' command");
    }
    if (ctx.acl_manager->authenticate(args[1], args[2])) {
        ctx.authenticated_fds->insert(fd);
        return credis::protocol::encode_simple_string("OK");
    }
    return credis::protocol::encode_error("WRONGPASS invalid username-password pair or user is disabled.");
}

auto handle_replconf(const std::vector<std::string>& args) -> std::string {
    if (args.size() >= 2 && credis::util::to_upper(args[1]) == "GETACK") {
        return credis::protocol::encode_array({"REPLCONF", "ACK", "0"});
    }
    return credis::protocol::encode_simple_string("OK");
}

auto handle_psync(CommandContext& ctx) -> ProcessResult {
    auto response
        = "+FULLRESYNC " + ctx.config.master_replid + " " + std::to_string(ctx.config.master_repl_offset) + "\r\n";
    response += "$88\r\n";
    response.append(credis::rdb::kEmptyRdb.begin(), credis::rdb::kEmptyRdb.end());
    return ProcessResult::replica_handshake(response);
}

auto handle_wait(const std::vector<std::string>& args) -> ProcessResult {
    if (args.size() < 3) {
        return ProcessResult::normal(
            credis::protocol::encode_error("ERR wrong number of arguments for 'wait' command"));
    }
    auto numreplicas = credis::util::parse_int<int64_t>(args[1]);
    auto timeout = credis::util::parse_int<int64_t>(args[2]);
    if (!numreplicas || !timeout) {
        return ProcessResult::normal(credis::protocol::encode_error("ERR value is not an integer or out of range"));
    }
    return ProcessResult::wait(*numreplicas, *timeout);
}

} // namespace credis::handler
