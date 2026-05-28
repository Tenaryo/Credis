#include "command_handler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>

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
    static constexpr auto kWriteCommands = std::array{
        "SET"sv, "DEL"sv, "INCR"sv, "RPUSH"sv, "LPUSH"sv, "LPOP"sv, "RPOP"sv, "XADD"sv, "ZADD"sv, "ZREM"sv, "GEOADD"sv};
    return std::ranges::find(kWriteCommands, cmd) != kWriteCommands.end();
}

} // namespace

namespace credis::handler {

CommandHandler::CommandHandler(credis::store::Store& store, credis::server::ServerConfig config)
    : store_(store), config_(std::move(config)) {
    if (config_.master_replid.empty()) {
        config_.generate_replid();
    }
    register_commands();
}

void CommandHandler::remove_connection(int fd) {
    authenticated_fds_.erase(fd);
    transactions_.erase(fd);
}

void CommandHandler::register_commands() {
    command_table_ = {
        {"PING",
         {[](CommandContext& ctx,
             const std::vector<std::string>&,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_ping(ctx, fd));
          },
          1}},
        {"ECHO",
         {[](CommandContext&,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_echo(args[1]));
          },
          2}},
        {"SET",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_set(ctx, args));
           },
           3}},
        {"MSET",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_mset(ctx, args));
          },
          3}},
        {"GET",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_get(ctx, args[1]));
          },
          2}},
        {"INCR",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_incr(ctx, args[1]));
          },
          2}},
        {"RPUSH",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>& send) -> ProcessResult {
              if (send) {
                  return handle_rpush_with_blocking(ctx, args, send);
              }
              return ProcessResult::normal(handle_rpush(ctx, args));
          },
          3}},
        {"LPUSH",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>& send) -> ProcessResult {
              if (send) {
                  return handle_lpush_with_blocking(ctx, args, send);
              }
              return ProcessResult::normal(handle_lpush(ctx, args));
          },
          3}},
        {"LLEN",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(credis::protocol::encode_integer(ctx.store.llen(args[1])));
          },
          2}},
        {"LPOP",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_lpop(ctx, args));
           },
           2}},
        {"RPOP",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_rpop(ctx, args));
          },
          2}},
        {"LRANGE",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_lrange(ctx, args));
          },
          4}},
        {"BLPOP",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return handle_blpop(ctx, fd, args);
          },
          3}},
        {"TYPE",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(credis::protocol::encode_simple_string(ctx.store.get_type(args[1])));
          },
          2}},
        {"KEYS",
         {[](CommandContext& ctx,
             const std::vector<std::string>&,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(credis::protocol::encode_array(ctx.store.keys()));
          },
          2}},
        {"XADD",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>& send) -> ProcessResult {
              return handle_xadd_with_blocking(ctx, args, send);
          },
          4}},
        {"ZADD",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_zadd(ctx, args));
          },
          4}},
        {"ZRANK",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_zrank(ctx, args));
          },
          3}},
        {"ZRANGE",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_zrange(ctx, args));
          },
          4}},
        {"ZCARD",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_zcard(ctx, args[1]));
          },
          2}},
        {"ZSCORE",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_zscore(ctx, args));
          },
          3}},
        {"ZREM",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_zrem(ctx, args));
          },
          3}},
        {"GEOADD",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_geoadd(ctx, args));
          },
          5}},
        {"GEOPOS",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_geopos(ctx, args));
          },
          3}},
        {"GEODIST",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_geodist(ctx, args));
          },
          4}},
        {"GEOSEARCH",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_geosearch(ctx, args));
          },
          8}},
        {"XRANGE",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_xrange(ctx, args));
          },
          4}},
        {"XREAD",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int fd,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return handle_xread_with_blocking(ctx, fd, args);
          },
          4}},
        {"INFO",
         {[](CommandContext& ctx,
             const std::vector<std::string>& args,
             int,
             const std::function<void(int, const std::string&)>&) -> ProcessResult {
              return ProcessResult::normal(handle_info(ctx, args));
          },
          1}},
    };
}

auto CommandHandler::process(std::string_view input) -> std::string {
    auto result = process_with_fd(-1, input, nullptr);
    if (std::holds_alternative<ProcessResult::Normal>(result.state)) [[likely]] {
        return std::get<ProcessResult::Normal>(result.state).response;
    }
    return std::get<ProcessResult::ReplicaHandshake>(result.state).response;
}

auto CommandHandler::process_with_fd(int fd,
                                     std::string_view input,
                                     std::function<void(int, const std::string&)> send_to_client) -> ProcessResult {
    size_t total_consumed = 0;
    ProcessResult result = ProcessResult::normal("");

    while (total_consumed < input.size()) {
        auto parsed = credis::protocol::parse_one(input.substr(total_consumed));
        if (!parsed) [[unlikely]] {
            break;
        }

        total_consumed += parsed->consumed;

        auto cmd_result = process_single_command(fd, std::move(parsed->args), send_to_client);

        if (!cmd_result.propagate_args.empty()) [[unlikely]] {
            result.propagate_args.insert(result.propagate_args.end(),
                                         std::make_move_iterator(cmd_result.propagate_args.begin()),
                                         std::make_move_iterator(cmd_result.propagate_args.end()));
        }

        if (!std::holds_alternative<ProcessResult::Normal>(cmd_result.state)) [[unlikely]] {
            cmd_result.propagate_args = std::move(result.propagate_args);
            cmd_result.consumed = total_consumed;
            return cmd_result;
        }

        if (send_to_client) [[likely]] {
            send_to_client(fd, std::get<ProcessResult::Normal>(cmd_result.state).response);
        }
        result.state = std::move(cmd_result.state);
    }

    result.consumed = total_consumed;
    return result;
}

auto CommandHandler::process_single_command(
    int fd,
    std::vector<std::string> args,
    const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult {
    if (args.empty()) [[unlikely]] {
        return ProcessResult::normal(credis::protocol::encode_error("ERR empty command"));
    }

    std::string& cmd = args[0];
    cmd = credis::util::to_upper(std::move(cmd));

    if (pubsub_manager_ && pubsub_manager_->get().is_subscribed(fd)) [[unlikely]] {
        static constexpr auto kSubscribedAllowed = std::array{
            "SUBSCRIBE"sv, "UNSUBSCRIBE"sv, "PSUBSCRIBE"sv, "PUNSUBSCRIBE"sv, "PING"sv, "QUIT"sv, "RESET"sv};
        if (std::ranges::find(kSubscribedAllowed, cmd) == kSubscribedAllowed.end()) [[unlikely]] {
            return ProcessResult::normal(
                credis::protocol::encode_error("ERR Can't execute '" + cmd + "' in subscribed mode"));
        }
    }

    if (cmd != "AUTH" && !authenticated_fds_.contains(fd)) [[unlikely]] {
        const auto* user = acl_manager_.get_user("default");
        if ((user != nullptr) && user->nopass) [[likely]] {
            authenticated_fds_.insert(fd);
        } else {
            return ProcessResult::normal(credis::protocol::encode_error("NOAUTH Authentication required."));
        }
    }

    if (cmd == "MULTI") [[unlikely]] {
        if (transactions_[fd].in_multi) [[unlikely]] {
            return ProcessResult::normal(credis::protocol::encode_error("ERR MULTI calls can not be nested"));
        }
        transactions_[fd].in_multi = true;
        return ProcessResult::normal(credis::protocol::kRespOk);
    }

    if (cmd == "EXEC") [[unlikely]] {
        auto it = transactions_.find(fd);
        if (it == transactions_.end() || !it->second.in_multi) [[unlikely]] {
            return ProcessResult::normal(credis::protocol::encode_error("ERR EXEC without MULTI"));
        }

        auto& tx = it->second;

        bool dirty = false;
        for (const auto& [key, version] : tx.watched_keys) {
            if (store_.get_key_version(key) != version) [[unlikely]] {
                dirty = true;
                break;
            }
        }

        if (dirty) [[unlikely]] {
            transactions_.erase(it);
            return ProcessResult::normal(credis::protocol::encode_null_array());
        }

        std::vector<std::string> results;
        results.reserve(tx.queued_commands.size());
        for (const auto& queued_args : tx.queued_commands) {
            auto cmd_result = execute_command(queued_args, fd, send_to_client);
            if (auto* normal = std::get_if<ProcessResult::Normal>(&cmd_result.state)) [[likely]] {
                results.push_back(std::move(normal->response));
            } else {
                results.push_back(credis::protocol::encode_error("ERR command in EXEC not allowed"));
            }
        }
        transactions_.erase(it);
        return ProcessResult::normal(credis::protocol::encode_raw_array(results));
    }

    if (cmd == "DISCARD") [[unlikely]] {
        auto dit = transactions_.find(fd);
        if (dit == transactions_.end() || !dit->second.in_multi) [[unlikely]] {
            return ProcessResult::normal(credis::protocol::encode_error("ERR DISCARD without MULTI"));
        }
        transactions_.erase(dit);
        return ProcessResult::normal(credis::protocol::kRespOk);
    }

    if (cmd == "WATCH") [[unlikely]] {
        if (args.size() < 2) [[unlikely]] {
            return ProcessResult::normal(
                credis::protocol::encode_error("ERR wrong number of arguments for 'watch' command"));
        }
        auto it = transactions_.find(fd);
        if (it != transactions_.end() && it->second.in_multi) [[unlikely]] {
            return ProcessResult::normal(credis::protocol::encode_error("ERR WATCH inside MULTI is not allowed"));
        }
        auto& tx = transactions_[fd];
        for (size_t i = 1; i < args.size(); ++i) {
            tx.watched_keys[args[i]] = store_.get_key_version(args[i]);
        }
        return ProcessResult::normal(credis::protocol::kRespOk);
    }

    if (cmd == "UNWATCH") [[unlikely]] {
        auto uit = transactions_.find(fd);
        if (uit != transactions_.end()) {
            uit->second.watched_keys.clear();
        }
        return ProcessResult::normal(credis::protocol::kRespOk);
    }

    auto it = transactions_.find(fd);
    if (it != transactions_.end() && it->second.in_multi) [[unlikely]] {
        it->second.queued_commands.push_back(args);
        return ProcessResult::normal(credis::protocol::kRespQueued);
    }

    auto result = execute_command(args, fd, send_to_client);
    if (is_write_command(cmd)) [[likely]] {
        result.propagate_args = args;
    }
    return result;
}

template <typename SendFn>
auto CommandHandler::execute_command(const std::vector<std::string>& args,
                                     int fd,
                                     SendFn&& send_to_client) -> ProcessResult {
    CommandContext ctx{store_,
                       config_,
                       blocking_manager_,
                       pubsub_manager_,
                       &acl_manager_,
                       replica_count_fn_,
                       &transactions_,
                       &authenticated_fds_};
    const std::string& cmd = args[0];

    if (cmd == "CONFIG") [[unlikely]] {
        if (args.size() < 3 || credis::util::to_upper(args[1]) != "GET") [[unlikely]] {
            return ProcessResult::normal(
                credis::protocol::encode_error("ERR wrong number of arguments for 'config' command"));
        }
        return ProcessResult::normal(handle_config_get(ctx, args[2]));
    }
    if (cmd == "ACL") [[unlikely]] {
        return ProcessResult::normal(handle_acl(ctx, args));
    }
    if (cmd == "AUTH") [[unlikely]] {
        return ProcessResult::normal(handle_auth(ctx, fd, args));
    }
    if (cmd == "REPLCONF") [[unlikely]] {
        return ProcessResult::normal(handle_replconf(args));
    }
    if (cmd == "WAIT") [[unlikely]] {
        return handle_wait(args);
    }
    if (cmd == "PSYNC") [[unlikely]] {
        return handle_psync(ctx);
    }
    if (cmd == "SUBSCRIBE") [[unlikely]] {
        if (args.size() < 2) [[unlikely]] {
            return ProcessResult::normal(
                credis::protocol::encode_error("ERR wrong number of arguments for 'subscribe' command"));
        }
        return ProcessResult::normal(handle_subscribe(ctx, fd, args[1]));
    }
    if (cmd == "UNSUBSCRIBE") [[unlikely]] {
        if (args.size() < 2) [[unlikely]] {
            return ProcessResult::normal(
                credis::protocol::encode_error("ERR wrong number of arguments for 'unsubscribe' command"));
        }
        return ProcessResult::normal(handle_unsubscribe(ctx, fd, args[1]));
    }
    if (cmd == "PUBLISH") [[unlikely]] {
        if (args.size() < 3) [[unlikely]] {
            return ProcessResult::normal(
                credis::protocol::encode_error("ERR wrong number of arguments for 'publish' command"));
        }
        return ProcessResult::normal(handle_publish(ctx, args[1], args[2], send_to_client));
    }

    auto it = command_table_.find(cmd);
    if (it == command_table_.end()) [[unlikely]] {
        return ProcessResult::normal(credis::protocol::encode_error("ERR unknown command '" + cmd + "'"));
    }
    if (args.size() < it->second.min_args) [[unlikely]] {
        return ProcessResult::normal(credis::protocol::encode_error("ERR wrong number of arguments for '"
                                                                    + credis::util::to_lower(cmd) + "' command"));
    }
    return it->second.handler(ctx, args, fd, send_to_client);
}

template ProcessResult CommandHandler::execute_command<std::function<void(int, const std::string&)>>(
    const std::vector<std::string>&,
    int,
    std::function<void(int, const std::string&)>&&);

} // namespace credis::handler
