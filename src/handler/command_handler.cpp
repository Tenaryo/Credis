#include "command_handler.hpp"
#include "block_manager/blocking_manager.hpp"
#include "protocol/resp_parser.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"
#include <array>
#include <string_view>

namespace {
using namespace std::string_view_literals;

bool is_write_command(std::string_view cmd) {
    static constexpr auto kWriteCommands =
        std::array{"SET"sv, "DEL"sv, "INCR"sv, "RPUSH"sv, "LPUSH"sv, "LPOP"sv, "XADD"sv};
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

std::string CommandHandler::process(std::string_view input) {
    auto result = process_with_fd(-1, input, nullptr);
    return result.response;
}

ProcessResult
CommandHandler::process_with_fd(int fd,
                                std::string_view input,
                                std::function<void(int, const std::string&)> send_to_blocked) {
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
        std::vector<std::string> results;
        results.reserve(tx.queued_commands.size());
        for (const auto& queued_args : tx.queued_commands) {
            auto cmd_result = execute_command(queued_args, fd, send_to_blocked);
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

    auto it = transactions_.find(fd);
    if (it != transactions_.end() && it->second.in_multi) {
        it->second.queued_commands.push_back(args);
        return {false, RespParser::encode_simple_string("QUEUED")};
    }

    auto result = execute_command(args, fd, send_to_blocked);
    if (is_write_command(cmd)) {
        result.propagate_args = args;
    }
    return result;
}

ProcessResult
CommandHandler::execute_command(const std::vector<std::string>& args,
                                int fd,
                                std::function<void(int, const std::string&)> send_to_blocked) {
    const std::string& cmd = args[0];

    if (cmd == "PING") {
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
        if (send_to_blocked) {
            return handle_rpush_with_blocking(args, send_to_blocked);
        }
        return {false, handle_rpush(args)};
    }
    if (cmd == "LPUSH") {
        if (args.size() < 3) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'lpush' command")};
        }
        if (send_to_blocked) {
            return handle_lpush_with_blocking(args, send_to_blocked);
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
    if (cmd == "XADD") {
        if (args.size() < 4 || (args.size() - 3) % 2 != 0) {
            return {false,
                    RespParser::encode_error("ERR wrong number of arguments for 'xadd' command")};
        }
        return handle_xadd_with_blocking(args, send_to_blocked);
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
    std::function<void(int, const std::string&)> send_to_blocked) {
    const std::string& key = args[1];
    int64_t count = 0;

    for (size_t i = 2; i < args.size(); ++i) {
        if (blocking_manager_) {
            auto blocked = blocking_manager_->wake_client(key);
            if (blocked) {
                count = store_.rpush(key, args[i]);
                auto elements = store_.lpop(key, 1);
                if (!elements.empty()) {
                    send_to_blocked(blocked->fd, RespParser::encode_array({key, elements[0]}));
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
    std::function<void(int, const std::string&)> send_to_blocked) {
    const std::string& key = args[1];
    int64_t count = store_.llen(key);

    for (size_t i = 2; i < args.size(); ++i) {
        if (blocking_manager_) {
            auto blocked = blocking_manager_->wake_client(key);
            if (blocked) {
                send_to_blocked(blocked->fd, RespParser::encode_array({key, args[i]}));
                ++count;
                continue;
            }
        }
        count = store_.lpush(key, args[i]);
    }
    return {false, RespParser::encode_integer(count)};
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
    std::function<void(int, const std::string&)> send_to_blocked) {
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
            send_to_blocked(blocked->fd, response);
        }
    }

    return {false, RespParser::encode_bulk_string(new_id)};
}
