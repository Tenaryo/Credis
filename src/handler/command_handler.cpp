#include "command_handler.hpp"
#include "block_manager/blocking_manager.hpp"
#include "geo/geo_score.hpp"
#include "protocol/resp_parser.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace {
using namespace std::string_view_literals;

bool is_write_command(std::string_view cmd) {
    static constexpr auto kWriteCommands = std::array{"SET"sv,
                                                      "DEL"sv,
                                                      "INCR"sv,
                                                      "RPUSH"sv,
                                                      "LPUSH"sv,
                                                      "LPOP"sv,
                                                      "XADD"sv,
                                                      "ZADD"sv,
                                                      "ZREM"sv,
                                                      "GEOADD"sv};
    return std::ranges::find(kWriteCommands, cmd) != kWriteCommands.end();
}

static constexpr std::array<char, 88> kEmptyRdb{
    {'R',    'E',    'D',    'I',    'S',    '0',    '0',    '1',    '1',    '\xfa', '\x09',
     'r',    'e',    'd',    'i',    's',    '-',    'v',    'e',    'r',    '\x05', '7',
     '.',    '2',    '.',    '0',    '\xfa', '\x0a', 'r',    'e',    'd',    'i',    's',
     '-',    'b',    'i',    't',    's',    '\xc0', '\x40', '\xfa', '\x05', 'c',    't',
     'i',    'm',    'e',    '\xc2', '\x6d', '\x08', '\xbc', '\x65', '\xfa', '\x08', 'u',
     's',    'e',    'd',    '-',    'm',    'e',    'm',    '\xc2', '\xb0', '\xc4', '\x10',
     '\x00', '\xfa', '\x08', 'a',    'o',    'f',    '-',    'b',    'a',    's',    'e',
     '\xc0', '\x00', '\xff', '\xf0', '\x6e', '\x3b', '\xfe', '\xc0', '\xff', '\x5a', '\xa2'}};
} // namespace

CommandHandler::CommandHandler(Store& store, const ServerConfig& config)
    : store_(store), config_(config) {}

void CommandHandler::remove_connection(int fd) {
    authenticated_fds_.erase(fd);
    transactions_.erase(fd);
}

std::string CommandHandler::process(std::string_view input) {
    auto result = process_with_fd(-1, input, nullptr);
    return result.response;
}

ProcessResult
CommandHandler::process_with_fd(int fd,
                                std::string_view input,
                                std::function<void(int, const std::string&)> send_to_client) {
    auto parsed = RespParser::parse(input);
    if (!parsed) {
        return {false, RespParser::encode_error("ERR " + parsed.error())};
    }

    auto& args = *parsed;
    if (args.empty()) {
        return {false, RespParser::encode_error("ERR empty command")};
    }

    std::string& cmd = args[0];
    cmd = to_upper(std::move(cmd));

    if (pubsub_manager_ && pubsub_manager_->is_subscribed(fd)) {
        static constexpr auto kSubscribedAllowed = std::array{"SUBSCRIBE"sv,
                                                              "UNSUBSCRIBE"sv,
                                                              "PSUBSCRIBE"sv,
                                                              "PUNSUBSCRIBE"sv,
                                                              "PING"sv,
                                                              "QUIT"sv,
                                                              "RESET"sv};
        if (std::ranges::find(kSubscribedAllowed, cmd) == kSubscribedAllowed.end()) {
            return {false,
                    RespParser::encode_error("ERR Can't execute '" + cmd + "' in subscribed mode")};
        }
    }

    if (cmd != "AUTH" && !authenticated_fds_.contains(fd)) {
        auto* user = acl_manager_.get_user("default");
        if (user && user->nopass) {
            authenticated_fds_.insert(fd);
        } else {
            return {false, RespParser::encode_error("NOAUTH Authentication required.")};
        }
    }

    if (cmd == "MULTI") {
        transactions_[fd].in_multi = true;
        return {false, RespParser::encode_simple_string("OK")};
    }

    if (cmd == "EXEC") {
        auto it = transactions_.find(fd);
        if (it == transactions_.end() || !it->second.in_multi) {
            return {false, RespParser::encode_error("ERR EXEC without MULTI")};
        }

        auto& tx = it->second;

        bool dirty = false;
        for (const auto& [key, version] : tx.watched_keys) {
            if (store_.get_key_version(key) != version) {
                dirty = true;
                break;
            }
        }

        if (dirty) {
            transactions_.erase(it);
            return {false, RespParser::encode_null_array()};
        }

        std::vector<std::string> results;
        results.reserve(tx.queued_commands.size());
        for (const auto& queued_args : tx.queued_commands) {
            auto cmd_result = execute_command(queued_args, fd, send_to_client);
            results.push_back(std::move(cmd_result.response));
        }
        transactions_.erase(it);
        return {false, RespParser::encode_raw_array(std::move(results))};
    }

    if (cmd == "DISCARD") {
        auto dit = transactions_.find(fd);
        if (dit == transactions_.end() || !dit->second.in_multi) {
            return {false, RespParser::encode_error("ERR DISCARD without MULTI")};
        }
        transactions_.erase(dit);
        return {false, RespParser::encode_simple_string("OK")};
    }

    if (cmd == "WATCH") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'watch' command")};
        }
        auto it = transactions_.find(fd);
        if (it != transactions_.end() && it->second.in_multi) {
            return {false, RespParser::encode_error("ERR WATCH inside MULTI is not allowed")};
        }
        auto& tx = transactions_[fd];
        for (size_t i = 1; i < args.size(); ++i) {
            tx.watched_keys[args[i]] = store_.get_key_version(args[i]);
        }
        return {false, RespParser::encode_simple_string("OK")};
    }

    if (cmd == "UNWATCH") {
        auto uit = transactions_.find(fd);
        if (uit != transactions_.end()) {
            uit->second.watched_keys.clear();
        }
        return {false, RespParser::encode_simple_string("OK")};
    }

    auto it = transactions_.find(fd);
    if (it != transactions_.end() && it->second.in_multi) {
        it->second.queued_commands.push_back(args);
        return {false, RespParser::encode_simple_string("QUEUED")};
    }

    auto result = execute_command(args, fd, send_to_client);
    if (is_write_command(cmd)) {
        result.propagate_args = args;
    }
    return result;
}

ProcessResult
CommandHandler::execute_command(const std::vector<std::string>& args,
                                int fd,
                                std::function<void(int, const std::string&)> send_to_client) {
    const std::string& cmd = args[0];

    if (cmd == "PING") {
        if (pubsub_manager_ && pubsub_manager_->is_subscribed(fd)) {
            return {false, RespParser::encode_array({"pong", ""})};
        }
        return {false, handle_ping()};
    }
    if (cmd == "ECHO") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'echo' command")};
        }
        return {false, handle_echo(args[1])};
    }
    if (cmd == "SET") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'set' command")};
        }
        return {false, handle_set(args)};
    }
    if (cmd == "GET") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'get' command")};
        }
        return {false, handle_get(args[1])};
    }
    if (cmd == "INCR") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'incr' command")};
        }
        return {false, handle_incr(args[1])};
    }
    if (cmd == "RPUSH") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'rpush' command")};
        }
        if (send_to_client) {
            return handle_rpush_with_blocking(args, send_to_client);
        }
        return {false, handle_rpush(args)};
    }
    if (cmd == "LPUSH") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'lpush' command")};
        }
        if (send_to_client) {
            return handle_lpush_with_blocking(args, send_to_client);
        }
        return {false, handle_lpush(args)};
    }
    if (cmd == "LLEN") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'llen' command")};
        }
        return {false, RespParser::encode_integer(store_.llen(args[1]))};
    }
    if (cmd == "LPOP") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'lpop' command")};
        }
        return {false, handle_lpop(args)};
    }
    if (cmd == "LRANGE") {
        if (args.size() < 4) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'lrange' command")};
        }
        return {false, handle_lrange(args)};
    }
    if (cmd == "BLPOP") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'blpop' command")};
        }
        return handle_blpop(fd, args);
    }
    if (cmd == "TYPE") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'type' command")};
        }
        return {false, RespParser::encode_simple_string(store_.get_type(args[1]))};
    }
    if (cmd == "KEYS") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'keys' command")};
        }
        return {false, RespParser::encode_array(store_.keys())};
    }
    if (cmd == "XADD") {
        if (args.size() < 4 || (args.size() - 3) % 2 != 0) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'xadd' command")};
        }
        return handle_xadd_with_blocking(args, send_to_client);
    }
    if (cmd == "ZADD") {
        if (args.size() < 4) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'zadd' command")};
        }
        return {false, handle_zadd(args)};
    }
    if (cmd == "ZRANK") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'zrank' command")};
        }
        return {false, handle_zrank(args)};
    }
    if (cmd == "ZRANGE") {
        if (args.size() < 4) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'zrange' command")};
        }
        return {false, handle_zrange(args)};
    }
    if (cmd == "ZCARD") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'zcard' command")};
        }
        return {false, handle_zcard(args[1])};
    }
    if (cmd == "ZSCORE") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'zscore' command")};
        }
        return {false, handle_zscore(args)};
    }
    if (cmd == "ZREM") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'zrem' command")};
        }
        return {false, handle_zrem(args)};
    }
    if (cmd == "GEOADD") {
        if (args.size() < 5) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'geoadd' command")};
        }
        return {false, handle_geoadd(args)};
    }
    if (cmd == "GEOPOS") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'geopos' command")};
        }
        return {false, handle_geopos(args)};
    }
    if (cmd == "GEODIST") {
        if (args.size() < 4) {
            return {
                false,
                RespParser::encode_error("ERR wrong number of arguments for 'geodist' command")};
        }
        return {false, handle_geodist(args)};
    }
    if (cmd == "GEOSEARCH") {
        if (args.size() < 8) {
            return {
                false,
                RespParser::encode_error("ERR wrong number of arguments for 'geosearch' command")};
        }
        return {false, handle_geosearch(args)};
    }
    if (cmd == "XRANGE") {
        if (args.size() < 4) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'xrange' command")};
        }
        return {false, handle_xrange(args)};
    }
    if (cmd == "XREAD") {
        if (args.size() < 4) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'xread' command")};
        }
        return handle_xread_with_blocking(fd, args);
    }
    if (cmd == "INFO") {
        return {false, handle_info(args)};
    }
    if (cmd == "CONFIG") {
        if (args.size() < 3 || to_upper(args[1]) != "GET") {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'config' command")};
        }
        return {false, handle_config_get(args[2])};
    }
    if (cmd == "ACL") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error("ERR unknown subcommand for 'ACL'. Try ACL HELP.")};
        }
        auto subcmd = to_upper(args[1]);
        if (subcmd == "WHOAMI") {
            return {false, handle_acl_whoami()};
        }
        if (subcmd == "GETUSER") {
            if (args.size() < 3) {
                return {false,
                        RespParser::encode_error(
                            "ERR wrong number of arguments for 'acl|getuser' command")};
            }
            return {false, handle_acl_getuser(args)};
        }
        if (subcmd == "SETUSER") {
            if (args.size() < 3) {
                return {false,
                        RespParser::encode_error(
                            "ERR wrong number of arguments for 'acl|setuser' command")};
            }
            return {false, handle_acl_setuser(args)};
        }
        return {false, RespParser::encode_error("ERR unknown subcommand for 'ACL'. Try ACL HELP.")};
    }
    if (cmd == "AUTH") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'auth' command")};
        }
        if (acl_manager_.authenticate(args[1], args[2])) {
            authenticated_fds_.insert(fd);
            return {false, RespParser::encode_simple_string("OK")};
        }
        return {false,
                RespParser::encode_error(
                    "WRONGPASS invalid username-password pair or user is disabled.")};
    }
    if (cmd == "REPLCONF") {
        if (args.size() >= 2 && to_upper(args[1]) == "GETACK") {
            return {false, RespParser::encode_array({"REPLCONF", "ACK", "0"})};
        }
        return {false, RespParser::encode_simple_string("OK")};
    }
    if (cmd == "WAIT") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'wait' command")};
        }
        auto numreplicas = parse_int<int64_t>(args[1]);
        auto timeout = parse_int<int64_t>(args[2]);
        if (!numreplicas || !timeout) {
            return {false, RespParser::encode_error("ERR value is not an integer or out of range")};
        }
        ProcessResult result;
        result.is_wait = true;
        result.wait_numreplicas = *numreplicas;
        result.wait_timeout_ms = *timeout;
        return result;
    }
    if (cmd == "PSYNC") {
        auto response = "+FULLRESYNC " + config_.master_replid + " " +
                        std::to_string(config_.master_repl_offset) + "\r\n";
        response += "$88\r\n";
        response.append(kEmptyRdb.begin(), kEmptyRdb.end());
        ProcessResult psync_result(false, response);
        psync_result.is_replica_handshake = true;
        return psync_result;
    }
    if (cmd == "SUBSCRIBE") {
        if (args.size() < 2) {
            return {
                false,
                RespParser::encode_error("ERR wrong number of arguments for 'subscribe' command")};
        }
        size_t count = pubsub_manager_ ? pubsub_manager_->subscribe(fd, args[1]) : 1;
        auto resp = "*3\r\n" + RespParser::encode_bulk_string("subscribe") +
                    RespParser::encode_bulk_string(args[1]) +
                    RespParser::encode_integer(static_cast<int64_t>(count));
        return {false, std::move(resp)};
    }
    if (cmd == "UNSUBSCRIBE") {
        if (args.size() < 2) {
            return {false,
                    RespParser::encode_error(
                        "ERR wrong number of arguments for 'unsubscribe' command")};
        }
        size_t count = pubsub_manager_ ? pubsub_manager_->unsubscribe(fd, args[1]) : 0;
        auto resp = "*3\r\n" + RespParser::encode_bulk_string("unsubscribe") +
                    RespParser::encode_bulk_string(args[1]) +
                    RespParser::encode_integer(static_cast<int64_t>(count));
        return {false, std::move(resp)};
    }
    if (cmd == "PUBLISH") {
        if (args.size() < 3) {
            return {
                false,
                RespParser::encode_error("ERR wrong number of arguments for 'publish' command")};
        }
        const auto& channel = args[1];
        const auto& message = args[2];
        if (pubsub_manager_) {
            const auto& subs = pubsub_manager_->get_subscribers(channel);
            if (send_to_client) {
                auto msg = RespParser::encode_array({"message", channel, message});
                std::ranges::for_each(subs, [&](int fd) { send_to_client(fd, msg); });
            }
            return {false, RespParser::encode_integer(static_cast<int64_t>(subs.size()))};
        }
        return {false, RespParser::encode_integer(0)};
    }

    return {false, RespParser::encode_error("ERR unknown command '" + cmd + "'")};
}

std::string CommandHandler::handle_ping() { return RespParser::encode_simple_string("PONG"); }

std::string CommandHandler::handle_echo(std::string_view args) {
    return RespParser::encode_bulk_string(args);
}

std::string CommandHandler::handle_set(const std::vector<std::string>& args) {
    const std::string& key = args[1];
    const std::string& value = args[2];

    std::optional<uint64_t> ttl_ms;

    for (size_t i = 3; i < args.size(); ++i) {
        auto option = to_upper(args[i]);

        if (option == "EX" || option == "PX") {
            if (i + 1 >= args.size()) {
                return RespParser::encode_error("ERR syntax error");
            }

            auto parsed = parse_int<uint64_t>(args[i + 1]);
            if (!parsed) {
                return RespParser::encode_error("ERR value is not an integer or out of range");
            }

            ttl_ms = (option == "EX") ? *parsed * 1000 : *parsed;
            ++i;
        }
    }

    store_.set(key, value, ttl_ms);
    return RespParser::encode_simple_string("OK");
}

std::string CommandHandler::handle_get(const std::string& key) {
    auto value = store_.get(key);
    return value ? RespParser::encode_bulk_string(*value) : RespParser::encode_null_bulk_string();
}

std::string CommandHandler::handle_incr(const std::string& key) {
    auto result = store_.incr(key);
    if (!result) {
        return RespParser::encode_error("ERR value is not an integer or out of range");
    }
    return RespParser::encode_integer(*result);
}

std::string CommandHandler::handle_rpush(const std::vector<std::string>& args) {
    const std::string& key = args[1];
    int64_t count = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        count = store_.rpush(key, args[i]);
    }
    return RespParser::encode_integer(count);
}

std::string CommandHandler::handle_lpush(const std::vector<std::string>& args) {
    const std::string& key = args[1];
    int64_t count = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        count = store_.lpush(key, args[i]);
    }
    return RespParser::encode_integer(count);
}

std::string CommandHandler::handle_lpop(const std::vector<std::string>& args) {
    const std::string& key = args[1];

    if (args.size() == 2) {
        auto elements = store_.lpop(key, 1);
        if (elements.empty()) {
            return RespParser::encode_null_bulk_string();
        }
        return RespParser::encode_bulk_string(elements[0]);
    }

    auto parsed = parse_int<int64_t>(args[2]);
    if (!parsed) {
        return RespParser::encode_error("ERR value is not an integer or out of range");
    }
    int64_t count = *parsed;

    if (count <= 0) {
        return RespParser::encode_array({});
    }

    auto elements = store_.lpop(key, count);
    return RespParser::encode_array(elements);
}

std::string CommandHandler::handle_lrange(const std::vector<std::string>& args) {
    const std::string& key = args[1];

    auto start_opt = parse_int<int64_t>(args[2]);
    auto stop_opt = parse_int<int64_t>(args[3]);
    if (!start_opt || !stop_opt) {
        return RespParser::encode_error("ERR value is not an integer or out of range");
    }

    auto elements = store_.lrange(key, *start_opt, *stop_opt);
    return RespParser::encode_array(elements);
}

std::string CommandHandler::handle_info(const std::vector<std::string>& /* args */) {
    auto role = config_.is_replica() ? "slave" : "master";
    auto info = "# Replication\r\nrole:" + std::string(role) +
                "\r\nmaster_replid:" + config_.master_replid +
                "\r\nmaster_repl_offset:" + std::to_string(config_.master_repl_offset) + "\r\n";
    return RespParser::encode_bulk_string(info);
}

ProcessResult CommandHandler::handle_blpop(int fd, const std::vector<std::string>& args) {
    const std::string& key = args[1];

    double timeout_sec = 0;
    try {
        timeout_sec = std::stod(args[2]);
        if (timeout_sec < 0) {
            return {false, RespParser::encode_error("ERR timeout is negative")};
        }
    } catch (...) {
        return {false, RespParser::encode_error("ERR value is not an integer or out of range")};
    }

    auto elements = store_.lpop(key, 1);
    if (!elements.empty()) {
        return {false, RespParser::encode_array({key, elements[0]})};
    }

    if (blocking_manager_) {
        auto timeout_ms = std::chrono::milliseconds(static_cast<int64_t>(timeout_sec * 1000));
        blocking_manager_->block_client(fd, key, timeout_ms);
        return {true, ""};
    }

    return {false, RespParser::encode_error("ERR blocking not available")};
}

ProcessResult CommandHandler::handle_rpush_with_blocking(
    const std::vector<std::string>& args,
    std::function<void(int, const std::string&)> send_to_client) {
    const std::string& key = args[1];
    int64_t count = 0;

    for (size_t i = 2; i < args.size(); ++i) {
        if (blocking_manager_) {
            auto blocked = blocking_manager_->wake_client(key);
            if (blocked) {
                count = store_.rpush(key, args[i]);
                auto elements = store_.lpop(key, 1);
                if (!elements.empty()) {
                    send_to_client(blocked->fd, RespParser::encode_array({key, elements[0]}));
                }
                continue;
            }
        }
        count = store_.rpush(key, args[i]);
    }
    return {false, RespParser::encode_integer(count)};
}

ProcessResult CommandHandler::handle_lpush_with_blocking(
    const std::vector<std::string>& args,
    std::function<void(int, const std::string&)> send_to_client) {
    const std::string& key = args[1];
    int64_t count = store_.llen(key);

    for (size_t i = 2; i < args.size(); ++i) {
        if (blocking_manager_) {
            auto blocked = blocking_manager_->wake_client(key);
            if (blocked) {
                send_to_client(blocked->fd, RespParser::encode_array({key, args[i]}));
                ++count;
                continue;
            }
        }
        count = store_.lpush(key, args[i]);
    }
    return {false, RespParser::encode_integer(count)};
}

std::string CommandHandler::handle_zadd(const std::vector<std::string>& args) {
    const std::string& key = args[1];
    double score;
    try {
        score = std::stod(args[2]);
    } catch (...) {
        return RespParser::encode_error("ERR value is not a valid float");
    }
    auto added = store_.zadd(key, score, args[3]);
    return RespParser::encode_integer(added);
}

std::string CommandHandler::handle_zrank(const std::vector<std::string>& args) {
    auto rank = store_.zrank(args[1], args[2]);
    return rank ? RespParser::encode_integer(*rank) : RespParser::encode_null_bulk_string();
}

std::string CommandHandler::handle_zrange(const std::vector<std::string>& args) {
    const std::string& key = args[1];
    auto start_opt = parse_int<int64_t>(args[2]);
    auto stop_opt = parse_int<int64_t>(args[3]);
    if (!start_opt || !stop_opt) {
        return RespParser::encode_error("ERR value is not an integer or out of range");
    }
    auto elements = store_.zrange(key, *start_opt, *stop_opt);
    return RespParser::encode_array(elements);
}

std::string CommandHandler::handle_zcard(const std::string& key) {
    return RespParser::encode_integer(store_.zcard(key));
}

std::string CommandHandler::handle_zscore(const std::vector<std::string>& args) {
    auto score = store_.zscore(args[1], args[2]);
    if (!score)
        return RespParser::encode_null_bulk_string();

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", *score);
    return RespParser::encode_bulk_string(buf);
}

std::string CommandHandler::handle_zrem(const std::vector<std::string>& args) {
    auto removed = store_.zrem(args[1], args[2]);
    return RespParser::encode_integer(removed);
}

std::string CommandHandler::handle_geoadd(const std::vector<std::string>& args) {
    double lon;
    double lat;
    try {
        lon = std::stod(args[2]);
        lat = std::stod(args[3]);
    } catch (...) {
        return RespParser::encode_error("ERR value is not a valid float");
    }

    bool lon_invalid = !std::isfinite(lon) || lon < geo::kLonMin || lon > geo::kLonMax;
    bool lat_invalid = !std::isfinite(lat) || lat < geo::kLatMin || lat > geo::kLatMax;

    if (lon_invalid || lat_invalid) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "ERR invalid longitude,latitude pair %.6f,%.6f", lon, lat);
        return RespParser::encode_error(buf);
    }

    auto score = static_cast<double>(geo::encode(lat, lon));
    auto added = store_.zadd(args[1], score, args[4]);
    return RespParser::encode_integer(added);
}

std::string CommandHandler::handle_geopos(const std::vector<std::string>& args) {
    const auto& key = args[1];
    auto count = args.size() - 2;

    std::string resp;
    resp.reserve(count * 64);
    resp += "*" + std::to_string(count) + "\r\n";

    for (size_t i = 2; i < args.size(); ++i) {
        auto score = store_.zscore(key, args[i]);
        if (score) {
            auto coords = geo::decode(static_cast<uint64_t>(*score));
            char lon_buf[32];
            char lat_buf[32];
            std::snprintf(lon_buf, sizeof(lon_buf), "%.17g", coords.lon);
            std::snprintf(lat_buf, sizeof(lat_buf), "%.17g", coords.lat);
            resp += "*2\r\n";
            resp += RespParser::encode_bulk_string(lon_buf);
            resp += RespParser::encode_bulk_string(lat_buf);
        } else {
            resp += "*-1\r\n";
        }
    }

    return resp;
}

std::string CommandHandler::handle_geodist(const std::vector<std::string>& args) {
    auto score1 = store_.zscore(args[1], args[2]);
    auto score2 = store_.zscore(args[1], args[3]);
    if (!score1 || !score2)
        return RespParser::encode_null_bulk_string();
    auto c1 = geo::decode(static_cast<uint64_t>(*score1));
    auto c2 = geo::decode(static_cast<uint64_t>(*score2));
    auto dist = geo::distance(c1.lat, c1.lon, c2.lat, c2.lon);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", dist);
    return RespParser::encode_bulk_string(buf);
}

std::string CommandHandler::handle_geosearch(const std::vector<std::string>& args) {
    const auto& key = args[1];

    if (to_upper(args[2]) != "FROMLONLAT")
        return RespParser::encode_error("ERR syntax error");

    double search_lon, search_lat;
    try {
        search_lon = std::stod(args[3]);
        search_lat = std::stod(args[4]);
    } catch (...) {
        return RespParser::encode_error("ERR value is not a valid float");
    }

    if (to_upper(args[5]) != "BYRADIUS")
        return RespParser::encode_error("ERR syntax error");

    double radius;
    try {
        radius = std::stod(args[6]);
    } catch (...) {
        return RespParser::encode_error("ERR value is not a valid float");
    }

    auto unit = to_upper(args[7]);
    static constexpr std::pair<std::string_view, double> kUnitFactors[] = {
        {"M", 1.0}, {"KM", 1000.0}, {"MI", 1609.34}, {"FT", 0.3048}};
    auto factor_it =
        std::ranges::find_if(kUnitFactors, [&](const auto& p) { return p.first == unit; });
    if (factor_it == std::end(kUnitFactors))
        return RespParser::encode_error("ERR unsupported unit provided");

    double radius_m = radius * factor_it->second;

    auto all = store_.zgetall(key);
    std::vector<std::string> matched;
    for (const auto& [member, score] : all) {
        auto coords = geo::decode(static_cast<uint64_t>(score));
        auto dist = geo::distance(search_lat, search_lon, coords.lat, coords.lon);
        if (dist <= radius_m)
            matched.push_back(member);
    }

    return RespParser::encode_array(matched);
}

std::string CommandHandler::handle_xadd(const std::vector<std::string>& args) {
    const std::string& key = args[1];
    const std::string& id = args[2];

    std::vector<std::pair<std::string, std::string>> fields;
    for (size_t i = 3; i < args.size(); i += 2) {
        fields.emplace_back(args[i], args[i + 1]);
    }

    std::string result = store_.xadd(key, id, fields);

    if (result.starts_with("ERR")) {
        return RespParser::encode_error(result);
    }

    return RespParser::encode_bulk_string(result);
}

std::string CommandHandler::handle_xrange(const std::vector<std::string>& args) {
    const std::string& key = args[1];
    const std::string& start = args[2];
    const std::string& end = args[3];

    auto entries = store_.xrange(key, start, end);
    return RespParser::encode_entries(entries);
}

std::string CommandHandler::handle_xread(const std::vector<std::string>& args) {
    size_t streams_idx = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (to_upper(args[i]) == "STREAMS") {
            streams_idx = i;
            break;
        }
    }

    if (streams_idx == 0) {
        return RespParser::encode_error("ERR syntax error");
    }

    size_t num_pairs = args.size() - streams_idx - 1;
    if (num_pairs == 0 || num_pairs % 2 != 0) {
        return RespParser::encode_error("ERR wrong number of arguments for 'xread' command");
    }

    size_t num_streams = num_pairs / 2;
    std::vector<std::pair<std::string, std::vector<Redis::StreamEntry>>> results;

    for (size_t i = 0; i < num_streams; ++i) {
        const std::string& key = args[streams_idx + 1 + i];
        const std::string& id = args[streams_idx + 1 + num_streams + i];

        auto entries = store_.xread(key, id);
        results.emplace_back(key, std::move(entries));
    }

    return RespParser::encode_stream_entries(results);
}

ProcessResult CommandHandler::handle_xread_with_blocking(int fd,
                                                         const std::vector<std::string>& args) {
    bool has_block = false;
    int64_t timeout_ms = 0;
    size_t start_idx = 1;

    if (args.size() > start_idx) {
        if (to_upper(args[start_idx]) == "BLOCK") {
            has_block = true;
            if (start_idx + 1 >= args.size()) {
                return {false, RespParser::encode_error("ERR syntax error")};
            }
            auto parsed = parse_int<int64_t>(args[start_idx + 1]);
            if (!parsed) {
                return {false,
                        RespParser::encode_error("ERR value is not an integer or out of range")};
            }
            timeout_ms = *parsed;
            start_idx += 2;
        }
    }

    size_t streams_idx = 0;
    for (size_t i = start_idx; i < args.size(); ++i) {
        if (to_upper(args[i]) == "STREAMS") {
            streams_idx = i;
            break;
        }
    }

    if (streams_idx == 0) {
        return {false, RespParser::encode_error("ERR syntax error")};
    }

    size_t num_pairs = args.size() - streams_idx - 1;
    if (num_pairs == 0 || num_pairs % 2 != 0) {
        return {false,
                RespParser::encode_error("ERR wrong number of arguments for 'xread' command")};
    }

    size_t num_streams = num_pairs / 2;
    if (has_block && num_streams != 1) {
        return {false, RespParser::encode_error("ERR BLOCK only supports single stream")};
    }

    std::vector<std::pair<std::string, std::vector<Redis::StreamEntry>>> results;

    for (size_t i = 0; i < num_streams; ++i) {
        const std::string& key = args[streams_idx + 1 + i];
        const std::string& id_arg = args[streams_idx + 1 + num_streams + i];

        std::string id = id_arg;
        if (id_arg == "$") {
            auto max_id = store_.get_stream_max_id(key);
            id = max_id.value_or("0-0");
        }

        auto entries = store_.xread(key, id);
        results.emplace_back(key, std::move(entries));
    }

    bool has_data = std::ranges::any_of(results, [](const auto& p) { return !p.second.empty(); });

    if (has_data || !has_block) {
        return {false, RespParser::encode_stream_entries(results)};
    }

    if (blocking_manager_) {
        const std::string& key = args[streams_idx + 1];
        const std::string& id_arg = args[streams_idx + 1 + num_streams];

        std::string id = id_arg;
        if (id_arg == "$") {
            auto max_id = store_.get_stream_max_id(key);
            id = max_id.value_or("0-0");
        }

        auto sid = StreamId::parse(id).value_or(StreamId{0, 0});
        blocking_manager_->block_client_for_stream(
            fd, key, sid, std::chrono::milliseconds(timeout_ms));
        return {true, ""};
    }

    return {false, RespParser::encode_error("ERR blocking not available")};
}

ProcessResult CommandHandler::handle_xadd_with_blocking(
    const std::vector<std::string>& args,
    std::function<void(int, const std::string&)> send_to_client) {
    const std::string& key = args[1];
    const std::string& id = args[2];

    std::vector<std::pair<std::string, std::string>> fields;
    for (size_t i = 3; i < args.size(); i += 2) {
        fields.emplace_back(args[i], args[i + 1]);
    }

    std::string new_id = store_.xadd(key, id, fields);

    if (new_id.starts_with("ERR")) {
        return {false, RespParser::encode_error(new_id)};
    }

    if (blocking_manager_) {
        while (auto blocked = blocking_manager_->wake_client_for_stream(key, new_id)) {
            auto entries = store_.xread(key, blocked->last_id.to_string());
            auto response = RespParser::encode_stream_entries({{key, std::move(entries)}});
            send_to_client(blocked->fd, response);
        }
    }

    return {false, RespParser::encode_bulk_string(new_id)};
}

std::string CommandHandler::handle_config_get(const std::string& param) {
    auto upper = to_upper(param);
    if (upper == "DIR") {
        auto value = config_.dir.empty() ? RespParser::encode_null_bulk_string()
                                         : RespParser::encode_bulk_string(config_.dir);
        return "*2\r\n$3\r\ndir\r\n" + value;
    }
    if (upper == "DBFILENAME") {
        auto value = config_.dbfilename.empty()
                         ? RespParser::encode_null_bulk_string()
                         : RespParser::encode_bulk_string(config_.dbfilename);
        return "*2\r\n$10\r\ndbfilename\r\n" + value;
    }
    return RespParser::encode_array({});
}

std::string CommandHandler::handle_acl_whoami() {
    return RespParser::encode_bulk_string("default");
}

std::string CommandHandler::handle_acl_getuser(const std::vector<std::string>& args) {
    auto* user = acl_manager_.get_user(args[2]);
    if (!user) {
        return RespParser::encode_null_array();
    }
    std::vector<std::string> flags;
    if (user->nopass)
        flags.emplace_back("nopass");
    return RespParser::encode_raw_array({RespParser::encode_bulk_string("flags"),
                                         RespParser::encode_array(flags),
                                         RespParser::encode_bulk_string("passwords"),
                                         RespParser::encode_array(user->passwords)});
}

std::string CommandHandler::handle_acl_setuser(const std::vector<std::string>& args) {
    const auto& username = args[2];
    for (size_t i = 3; i < args.size(); ++i) {
        const auto& rule = args[i];
        if (!rule.empty() && rule[0] == '>') {
            acl_manager_.set_password(username, rule.substr(1));
        }
    }
    return RespParser::encode_simple_string("OK");
}
