#include "handler/server_commands.hpp"

#include <string>

#include "aof/aof_manager.hpp"
#include "protocol/resp_codec.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "rdb/rdb_constants.hpp"
#include "server/acl_manager.hpp"
#include "server/server_config.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::handler {

namespace {
const std::string kEmpty;
} // namespace

void handle_ping(CommandContext& ctx, int fd) {
    if (ctx.pubsub_manager && fd >= 0 && ctx.pubsub_manager->get().is_subscribed(fd)) {
        credis::protocol::encode_array_into(*ctx.out, std::vector<std::string>{"pong", ""});
        return;
    }
    *ctx.out += credis::protocol::kRespPong;
}

void handle_echo(CommandContext& ctx, std::string_view message) {
    credis::protocol::encode_bulk_string_into(*ctx.out, message);
}

void handle_info(CommandContext& ctx, const std::vector<std::string_view>& /* args */) {
    const auto* role = ctx.config.is_replica() ? "slave" : "master";
    auto info = "# Replication\r\nrole:" + std::string(role) + "\r\nmaster_replid:" + ctx.config.master_replid
                + "\r\nmaster_repl_offset:" + std::to_string(ctx.offset_fn ? ctx.offset_fn() : 0) + "\r\n";
    credis::protocol::encode_bulk_string_into(*ctx.out, info);
}

void handle_config_get(CommandContext& ctx, std::string_view param) {
    auto upper = credis::util::to_upper(param);
    if (upper == "DIR") {
        *ctx.out += "*2\r\n$3\r\ndir\r\n";
        credis::protocol::encode_bulk_string_into(*ctx.out, ctx.config.dir);
        return;
    }
    if (upper == "DBFILENAME") {
        *ctx.out += "*2\r\n$10\r\ndbfilename\r\n";
        if (ctx.config.dbfilename.empty()) {
            credis::protocol::encode_null_bulk_string_into(*ctx.out);
        } else {
            credis::protocol::encode_bulk_string_into(*ctx.out, ctx.config.dbfilename);
        }
        return;
    }
    if (upper == "APPENDONLY") {
        *ctx.out += "*2\r\n$10\r\nappendonly\r\n";
        credis::protocol::encode_bulk_string_into(*ctx.out,
                                                  ctx.aof_manager != nullptr ? ctx.aof_manager->appendonly() : kEmpty);
        return;
    }
    if (upper == "APPENDDIRNAME") {
        *ctx.out += "*2\r\n$13\r\nappenddirname\r\n";
        credis::protocol::encode_bulk_string_into(
            *ctx.out, ctx.aof_manager != nullptr ? ctx.aof_manager->appenddirname() : kEmpty);
        return;
    }
    if (upper == "APPENDFILENAME") {
        *ctx.out += "*2\r\n$14\r\nappendfilename\r\n";
        credis::protocol::encode_bulk_string_into(
            *ctx.out, ctx.aof_manager != nullptr ? ctx.aof_manager->appendfilename() : kEmpty);
        return;
    }
    if (upper == "APPENDFSYNC") {
        *ctx.out += "*2\r\n$11\r\nappendfsync\r\n";
        credis::protocol::encode_bulk_string_into(*ctx.out,
                                                  ctx.aof_manager != nullptr ? ctx.aof_manager->appendfsync() : kEmpty);
        return;
    }
    *ctx.out += "*0\r\n";
}

void handle_config_set(CommandContext& ctx, std::string_view param, std::string_view value) {
    auto upper = credis::util::to_upper(param);
    if (upper == "APPENDONLY") {
        if (ctx.aof_manager == nullptr) {
            credis::protocol::encode_error_into(*ctx.out, "ERR AOF is not configured");
            return;
        }
        auto val = credis::util::to_lower(value);
        if (val != "yes" && val != "no") {
            credis::protocol::encode_error_into(*ctx.out, "ERR invalid value for appendonly");
            return;
        }
        auto was = ctx.aof_manager->appendonly();
        ctx.aof_manager->set_appendonly(val);
        if (val == "yes" && was != "yes") {
            ctx.aof_manager->ensure_directory(ctx.config.dir);
            ctx.aof_manager->ensure_file(ctx.config.dir);
            ctx.aof_manager->ensure_manifest(ctx.config.dir);
            ctx.aof_manager->open(ctx.config.dir);
        } else if (val == "no" && was != "no") {
            ctx.aof_manager->close();
        }
        *ctx.out += credis::protocol::kRespOk;
        return;
    }
    if (upper == "APPENDFSYNC") {
        if (ctx.aof_manager == nullptr) {
            credis::protocol::encode_error_into(*ctx.out, "ERR AOF is not configured");
            return;
        }
        auto val = credis::util::to_lower(value);
        if (val != "always" && val != "everysec" && val != "no") {
            credis::protocol::encode_error_into(*ctx.out, "ERR invalid value for appendfsync");
            return;
        }
        ctx.aof_manager->set_appendfsync(val);
        *ctx.out += credis::protocol::kRespOk;
        return;
    }
    credis::protocol::encode_error_into(*ctx.out, "ERR unsupported CONFIG parameter");
}

void handle_acl_whoami(CommandContext& ctx) {
    credis::protocol::encode_bulk_string_into(*ctx.out, "default");
}

void handle_acl(CommandContext& ctx, const std::vector<std::string_view>& args) {
    if (args.size() < 2) {
        credis::protocol::encode_error_into(*ctx.out, "ERR unknown subcommand for 'ACL'. Try ACL HELP.");
        return;
    }
    auto subcmd = credis::util::to_upper(args[1]);
    if (subcmd == "WHOAMI") {
        handle_acl_whoami(ctx);
        return;
    }
    if (subcmd == "GETUSER") {
        if (args.size() < 3) {
            credis::protocol::encode_error_into(*ctx.out, "ERR wrong number of arguments for 'acl|getuser' command");
            return;
        }
        handle_acl_getuser(ctx, args);
        return;
    }
    if (subcmd == "SETUSER") {
        if (args.size() < 3) {
            credis::protocol::encode_error_into(*ctx.out, "ERR wrong number of arguments for 'acl|setuser' command");
            return;
        }
        handle_acl_setuser(ctx, args);
        return;
    }
    credis::protocol::encode_error_into(*ctx.out, "ERR unknown subcommand for 'ACL'. Try ACL HELP.");
}

void handle_acl_getuser(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const auto* user = ctx.acl_manager.get_user(args[2]);
    if (user == nullptr) {
        credis::protocol::encode_null_array_into(*ctx.out);
        return;
    }
    std::vector<std::string> flags;
    if (user->nopass) {
        flags.emplace_back("nopass");
    }
    credis::protocol::encode_raw_array_into(*ctx.out,
                                            {credis::protocol::encode_bulk_string("flags"),
                                             credis::protocol::encode_array(flags),
                                             credis::protocol::encode_bulk_string("passwords"),
                                             credis::protocol::encode_array(user->passwords)});
}

void handle_acl_setuser(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const auto& username = args[2];
    for (size_t i = 3; i < args.size(); ++i) {
        const auto& rule = args[i];
        if (!rule.empty() && rule[0] == '>') {
            ctx.acl_manager.set_password(username, rule.substr(1));
        }
    }
    *ctx.out += credis::protocol::kRespOk;
}

void handle_auth(CommandContext& ctx, int fd, const std::vector<std::string_view>& args) {
    if (args.size() < 3) {
        credis::protocol::encode_error_into(*ctx.out, "ERR wrong number of arguments for 'auth' command");
        return;
    }
    if (ctx.acl_manager.authenticate(args[1], args[2])) {
        ctx.authenticated_fds.insert(fd);
        *ctx.out += credis::protocol::kRespOk;
        return;
    }
    credis::protocol::encode_error_into(*ctx.out, "WRONGPASS invalid username-password pair or user is disabled.");
}

void handle_replconf(CommandContext& ctx, const std::vector<std::string_view>& args) {
    if (args.size() >= 2 && credis::util::to_upper(args[1]) == "GETACK") {
        credis::protocol::encode_array_into(*ctx.out, std::vector<std::string>{"REPLCONF", "ACK", "0"});
        return;
    }
    *ctx.out += credis::protocol::kRespOk;
}

auto handle_psync(CommandContext& ctx) -> ProcessResult {
    *ctx.out += "+FULLRESYNC " + ctx.config.master_replid + " " + std::to_string(ctx.offset_fn ? ctx.offset_fn() : 0)
                + "\r\n";
    *ctx.out += "$88\r\n";
    ctx.out->append(credis::rdb::kEmptyRdb.begin(), credis::rdb::kEmptyRdb.end());
    return ProcessResult::replica_handshake();
}

auto handle_wait(CommandContext& ctx, const std::vector<std::string_view>& args) -> ProcessResult {
    if (args.size() < 3) {
        credis::protocol::encode_error_into(*ctx.out, "ERR wrong number of arguments for 'wait' command");
        return ProcessResult::normal();
    }
    auto numreplicas = credis::util::parse_int<int64_t>(args[1]);
    auto timeout = credis::util::parse_int<int64_t>(args[2]);
    if (!numreplicas || !timeout) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not an integer or out of range");
        return ProcessResult::normal();
    }
    return ProcessResult::wait(*numreplicas, *timeout);
}

void handle_bgrewriteaof(CommandContext& ctx) {
    if (ctx.aof_manager == nullptr || ctx.aof_manager->appendonly() != "yes") {
        credis::protocol::encode_error_into(*ctx.out, "ERR AOF is not enabled");
        return;
    }
    if (ctx.aof_manager->is_rewriting()) {
        credis::protocol::encode_error_into(*ctx.out, "ERR AOF rewrite already in progress");
        return;
    }
    if (!ctx.aof_manager->start_rewrite(ctx.store, ctx.config.dir)) {
        credis::protocol::encode_error_into(*ctx.out, "ERR AOF rewrite failed");
        return;
    }
    credis::protocol::encode_simple_string_into(*ctx.out, "Background append only file rewriting started");
}

} // namespace credis::handler
