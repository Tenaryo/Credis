#include "command_handler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>

#include "aof/aof_manager.hpp"
#include "connection/connection_pool.hpp"
#include "handler/geo_commands.hpp"
#include "handler/list_commands.hpp"
#include "handler/pubsub_commands.hpp"
#include "handler/server_commands.hpp"
#include "handler/stream_commands.hpp"
#include "handler/string_commands.hpp"
#include "handler/zset_commands.hpp"
#include "protocol/resp_codec.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "store/store.hpp"
#include "util/string_utils.hpp"

namespace {

using namespace std::string_view_literals;

auto is_write_command(std::string_view cmd) -> bool {
    static constexpr auto kWriteCommands = std::array{"SET"sv,
                                                      "DEL"sv,
                                                      "INCR"sv,
                                                      "MSET"sv,
                                                      "RPUSH"sv,
                                                      "LPUSH"sv,
                                                      "LPOP"sv,
                                                      "RPOP"sv,
                                                      "XADD"sv,
                                                      "ZADD"sv,
                                                      "ZREM"sv,
                                                      "GEOADD"sv};
    return std::ranges::find(kWriteCommands, cmd) != kWriteCommands.end();
}

} // namespace

namespace credis::handler {

CommandHandler::CommandHandler(credis::store::Store& store, credis::server::ServerConfig config)
    : ctx_{store, std::move(config)} {
    if (ctx_.config.master_replid.empty()) {
        ctx_.config.generate_replid();
    }
    register_commands();
}

void CommandHandler::remove_connection(int fd) {
}

void CommandHandler::register_commands() {
    command_table_ = {
        {"PING",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>&,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_ping(ctx, fd);
              return ProcessResult::normal();
          },
          1}},
        {"ECHO",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_echo(ctx, args[1]);
              return ProcessResult::normal();
          },
          2}},
        {"SET",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_set(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"MSET",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_mset(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"GET",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_get(ctx, args[1]);
              return ProcessResult::normal();
          },
          2}},
        {"INCR",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_incr(ctx, args[1]);
              return ProcessResult::normal();
          },
          2}},
        {"RPUSH",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>& send) -> ProcessResult {
              if (send) {
                  return handle_rpush_with_blocking(ctx, args, send);
              }
              handle_rpush(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"LPUSH",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>& send) -> ProcessResult {
              if (send) {
                  return handle_lpush_with_blocking(ctx, args, send);
              }
              handle_lpush(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"LLEN",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              credis::protocol::encode_integer_into(*ctx.out, ctx.store.llen(args[1]));
              return ProcessResult::normal();
          },
          2}},
        {"LPOP",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_lpop(ctx, args);
              return ProcessResult::normal();
          },
          2}},
        {"RPOP",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_rpop(ctx, args);
              return ProcessResult::normal();
          },
          2}},
        {"LRANGE",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_lrange(ctx, args);
              return ProcessResult::normal();
          },
          4}},
        {"BLPOP",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return handle_blpop(ctx, fd, args);
          },
          3}},
        {"TYPE",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              credis::protocol::encode_simple_string_into(*ctx.out, ctx.store.get_type(args[1]));
              return ProcessResult::normal();
          },
          2}},
        {"KEYS",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>&,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              credis::protocol::encode_array_into(*ctx.out, ctx.store.keys());
              return ProcessResult::normal();
          },
          2}},
        {"XADD",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>& send) -> ProcessResult {
              return handle_xadd_with_blocking(ctx, args, send);
          },
          4}},
        {"ZADD",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_zadd(ctx, args);
              return ProcessResult::normal();
          },
          4}},
        {"ZRANK",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_zrank(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"ZRANGE",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_zrange(ctx, args);
              return ProcessResult::normal();
          },
          4}},
        {"ZCARD",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_zcard(ctx, args[1]);
              return ProcessResult::normal();
          },
          2}},
        {"ZSCORE",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_zscore(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"ZREM",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_zrem(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"GEOADD",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_geoadd(ctx, args);
              return ProcessResult::normal();
          },
          5}},
        {"GEOPOS",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_geopos(ctx, args);
              return ProcessResult::normal();
          },
          3}},
        {"GEODIST",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_geodist(ctx, args);
              return ProcessResult::normal();
          },
          4}},
        {"GEOSEARCH",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_geosearch(ctx, args);
              return ProcessResult::normal();
          },
          8}},
        {"XRANGE",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_xrange(ctx, args);
              return ProcessResult::normal();
          },
          4}},
        {"XREAD",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return handle_xread_with_blocking(ctx, fd, args);
          },
          4}},
        {"INFO",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_info(ctx, args);
              return ProcessResult::normal();
          },
          1}},
        {"BGREWRITEAOF",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>&,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_bgrewriteaof(ctx);
              return ProcessResult::normal();
          },
          1}},
        {"CONFIG",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              if (args.size() < 3) {
                  credis::protocol::encode_error_into(*ctx.out, "ERR wrong number of arguments for 'config' command");
                  return ProcessResult::normal();
              }
              auto subcmd = credis::util::to_upper(args[1]);
              if (subcmd == "GET") {
                  if (args.size() < 3) {
                      credis::protocol::encode_error_into(*ctx.out,
                                                          "ERR wrong number of arguments for 'config|get' command");
                      return ProcessResult::normal();
                  }
                  handle_config_get(ctx, args[2]);
                  return ProcessResult::normal();
              }
              if (subcmd == "SET") {
                  if (args.size() < 4) {
                      credis::protocol::encode_error_into(*ctx.out,
                                                          "ERR wrong number of arguments for 'config|set' command");
                      return ProcessResult::normal();
                  }
                  handle_config_set(ctx, args[2], args[3]);
                  return ProcessResult::normal();
              }
              credis::protocol::encode_error_into(*ctx.out, "ERR unsupported CONFIG subcommand");
              return ProcessResult::normal();
          },
          3}},
        {"ACL",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_acl(ctx, args);
              return ProcessResult::normal();
          },
          2}},
        {"AUTH",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_auth(ctx, fd, args);
              return ProcessResult::normal();
          },
          2}},
        {"REPLCONF",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              handle_replconf(ctx, args);
              return ProcessResult::normal();
          },
          2}},
        {"WAIT",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult { return handle_wait(ctx, args); },
          3}},
        {"PSYNC",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>&,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult { return handle_psync(ctx); },
          1}},
        {"SUBSCRIBE",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              if (args.size() < 2) {
                  credis::protocol::encode_error_into(*ctx.out,
                                                      "ERR wrong number of arguments for 'subscribe' command");
                  return ProcessResult::normal();
              }
              handle_subscribe(ctx, fd, args[1]);
              return ProcessResult::normal();
          },
          2}},
        {"UNSUBSCRIBE",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              if (args.size() < 2) {
                  credis::protocol::encode_error_into(*ctx.out,
                                                      "ERR wrong number of arguments for 'unsubscribe' command");
                  return ProcessResult::normal();
              }
              handle_unsubscribe(ctx, fd, args[1]);
              return ProcessResult::normal();
          },
          2}},
        {"PUBLISH",
         {[](CommandContext& ctx,
             const std::vector<std::string_view>& args,
             int,
             const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult {
              if (args.size() < 3) {
                  credis::protocol::encode_error_into(*ctx.out, "ERR wrong number of arguments for 'publish' command");
                  return ProcessResult::normal();
              }
              handle_publish(ctx, args[1], args[2], send_to_client);
              return ProcessResult::normal();
          },
          3}},
    };
}

auto CommandHandler::process(std::string_view input) -> std::string {
    std::string response;
    response.reserve(input.size());
    ctx_.out = &response;
    process_with_fd(-1, input, nullptr);
    ctx_.out = nullptr;
    return response;
}

auto CommandHandler::process_with_fd(int fd,
                                     std::string_view input,
                                     const std::function<void(int, const std::string&)>& send_to_client)
    -> ProcessResult {
    size_t total_consumed = 0;
    ProcessResult result = ProcessResult::normal();
    std::string* saved_out = ctx_.out;
    std::string local_buf;

    if (send_to_client && ctx_.conn_pool) {
        ctx_.out = &ctx_.conn_pool->get_pending_write(fd);
    } else if (send_to_client) {
        local_buf.reserve(input.size());
        ctx_.out = &local_buf;
    } else if (!saved_out) {
        local_buf.reserve(input.size());
        ctx_.out = &local_buf;
    }

    std::vector<std::string_view> cmd_args;

    while (total_consumed < input.size()) {
        cmd_args.clear();
        auto consumed = credis::protocol::parse_one_into(cmd_args, input.substr(total_consumed));
        if (!consumed) {
            break;
        }

        size_t cmd_len = *consumed;
        size_t cmd_start = total_consumed;
        total_consumed += cmd_len;
        std::string cmd_name = credis::util::to_upper(cmd_args[0]);

        auto cmd_result = process_single_command(fd, std::move(cmd_args), cmd_name, send_to_client);

        if (is_write_command(cmd_name)) {
            if (ctx_.replica_count_fn && ctx_.replica_count_fn() > 0) {
                result.propagate_cmds.push_back(std::string(input.substr(cmd_start, cmd_len)));
            }
            if (ctx_.aof_manager != nullptr) {
                ctx_.aof_manager->append(input.substr(cmd_start, cmd_len));
                ctx_.aof_manager->append_to_rewrite_buffer(input.substr(cmd_start, cmd_len));
            }
        }

        if (!std::holds_alternative<ProcessResult::Normal>(cmd_result.state)) {
            ctx_.out = saved_out;
            if (send_to_client && !ctx_.conn_pool && !local_buf.empty()) {
                send_to_client(fd, local_buf);
            }
            cmd_result.propagate_cmds = std::move(result.propagate_cmds);
            cmd_result.consumed = total_consumed;
            return cmd_result;
        }
    }

    ctx_.out = saved_out;

    if (send_to_client && !ctx_.conn_pool && !local_buf.empty()) {
        send_to_client(fd, local_buf);
    }

    result.consumed = total_consumed;
    return result;
}

auto CommandHandler::process_single_command(int fd,
                                            std::vector<std::string_view> args,
                                            std::string_view cmd,
                                            const std::function<void(int, const std::string&)>& send_to_client)
    -> ProcessResult {
    if (args.empty()) {
        credis::protocol::encode_error_into(*ctx_.out, "ERR empty command");
        return ProcessResult::normal();
    }

    if (ctx_.pubsub_manager && ctx_.pubsub_manager->get().is_subscribed(fd)) {
        static constexpr auto kSubscribedAllowed = std::array{
            "SUBSCRIBE"sv, "UNSUBSCRIBE"sv, "PSUBSCRIBE"sv, "PUNSUBSCRIBE"sv, "PING"sv, "QUIT"sv, "RESET"sv};
        if (std::ranges::find(kSubscribedAllowed, cmd) == kSubscribedAllowed.end()) {
            credis::protocol::encode_error_into(*ctx_.out,
                                                "ERR Can't execute '" + std::string(cmd) + "' in subscribed mode");
            return ProcessResult::normal();
        }
    }

    if (cmd != "AUTH" && ctx_.conn_pool) {
        auto& conn = ctx_.conn_pool->get_connection(fd);
        if (!conn.authenticated()) {
            const auto* user = ctx_.acl_manager.get_user("default");
            if ((user != nullptr) && user->nopass) {
                conn.set_authenticated(true);
            } else {
                credis::protocol::encode_error_into(*ctx_.out, "NOAUTH Authentication required.");
                return ProcessResult::normal();
            }
        }
    } else if (cmd != "AUTH" && !ctx_.authenticated_fds.contains(fd)) {
        const auto* user = ctx_.acl_manager.get_user("default");
        if ((user != nullptr) && user->nopass) {
            ctx_.authenticated_fds.insert(fd);
        } else {
            credis::protocol::encode_error_into(*ctx_.out, "NOAUTH Authentication required.");
            return ProcessResult::normal();
        }
    }

    auto* conn = ctx_.conn_pool ? &ctx_.conn_pool->get_connection(fd) : nullptr;

    auto tx_in_multi = [&]() -> bool {
        if (conn)
            return conn->tx() && conn->tx()->in_multi;
        auto it = ctx_.transactions.find(fd);
        return it != ctx_.transactions.end() && it->second.in_multi;
    };
    auto tx_set_in_multi = [&]() {
        if (conn)
            conn->tx_or_create().in_multi = true;
        else
            ctx_.transactions[fd].in_multi = true;
    };
    auto tx_get = [&]() -> TransactionState* {
        if (conn)
            return &conn->tx_or_create();
        return &ctx_.transactions[fd];
    };
    auto tx_have = [&]() -> bool {
        if (conn)
            return conn->tx() != nullptr;
        return ctx_.transactions.contains(fd);
    };
    auto tx_clear = [&]() {
        if (conn)
            conn->clear_tx();
        else
            ctx_.transactions.erase(fd);
    };

    if (cmd == "MULTI") {
        if (tx_in_multi()) {
            credis::protocol::encode_error_into(*ctx_.out, "ERR MULTI calls can not be nested");
            return ProcessResult::normal();
        }
        tx_set_in_multi();
        *ctx_.out += credis::protocol::kRespOk;
        return ProcessResult::normal();
    }

    if (cmd == "EXEC") {
        if (!tx_in_multi()) {
            credis::protocol::encode_error_into(*ctx_.out, "ERR EXEC without MULTI");
            return ProcessResult::normal();
        }

        auto& tx = *tx_get();

        bool dirty = false;
        for (const auto& [key, version] : tx.watched_keys) {
            if (ctx_.store.get_key_version(key) != version) {
                dirty = true;
                break;
            }
        }

        if (dirty) {
            tx_clear();
            credis::protocol::encode_null_array_into(*ctx_.out);
            return ProcessResult::normal();
        }

        auto* saved_out = ctx_.out;
        std::vector<std::string> results;
        results.reserve(tx.queued_commands.size());
        std::string queued_buf;
        for (auto& queued_args : tx.queued_commands) {
            queued_buf.clear();
            ctx_.out = &queued_buf;
            auto cmd_result = execute_command(std::vector<std::string_view>(queued_args.begin(), queued_args.end()),
                                              queued_args[0],
                                              fd,
                                              send_to_client);
            if (std::holds_alternative<ProcessResult::Normal>(cmd_result.state)) {
                results.push_back(std::move(queued_buf));
            } else {
                results.push_back(credis::protocol::encode_error("ERR command in EXEC not allowed"));
            }
        }
        ctx_.out = saved_out;
        tx_clear();
        credis::protocol::encode_raw_array_into(*ctx_.out, results);
        return ProcessResult::normal();
    }

    if (cmd == "DISCARD") {
        if (!tx_in_multi()) {
            credis::protocol::encode_error_into(*ctx_.out, "ERR DISCARD without MULTI");
            return ProcessResult::normal();
        }
        tx_clear();
        *ctx_.out += credis::protocol::kRespOk;
        return ProcessResult::normal();
    }

    if (cmd == "WATCH") {
        if (args.size() < 2) {
            credis::protocol::encode_error_into(*ctx_.out, "ERR wrong number of arguments for 'watch' command");
            return ProcessResult::normal();
        }
        if (tx_in_multi()) {
            credis::protocol::encode_error_into(*ctx_.out, "ERR WATCH inside MULTI is not allowed");
            return ProcessResult::normal();
        }
        auto& tx = *tx_get();
        for (size_t i = 1; i < args.size(); ++i) {
            tx.watched_keys[std::string(args[i])] = ctx_.store.get_key_version(args[i]);
        }
        *ctx_.out += credis::protocol::kRespOk;
        return ProcessResult::normal();
    }

    if (cmd == "UNWATCH") {
        if (tx_have()) {
            tx_get()->watched_keys.clear();
        }
        *ctx_.out += credis::protocol::kRespOk;
        return ProcessResult::normal();
    }

    if (tx_in_multi()) {
        tx_get()->queued_commands.emplace_back(args.begin(), args.end());
        *ctx_.out += credis::protocol::kRespQueued;
        return ProcessResult::normal();
    }

    return execute_command(std::move(args), cmd, fd, send_to_client);
}

template <typename SendFn>
auto CommandHandler::execute_command(std::vector<std::string_view> args,
                                     std::string_view cmd,
                                     int fd,
                                     SendFn&& send_to_client) -> ProcessResult {
    auto it = command_table_.find(cmd);
    if (it == command_table_.end()) {
        credis::protocol::encode_error_into(*ctx_.out, "ERR unknown command '" + std::string(cmd) + "'");
        return ProcessResult::normal();
    }
    if (args.size() < it->second.min_args) {
        credis::protocol::encode_error_into(*ctx_.out,
                                            "ERR wrong number of arguments for '" + std::string(cmd) + "' command");
        return ProcessResult::normal();
    }
    return it->second.handler(ctx_, args, fd, send_to_client);
}

} // namespace credis::handler
