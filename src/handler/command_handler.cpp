#include "command_handler.hpp"
#include "block_manager/blocking_manager.hpp"
#include "protocol/resp_parser.hpp"
#include "store/store.hpp"
#include <algorithm>
#include <cctype>

CommandHandler::CommandHandler(Store& store) : store_(store) {}

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

    const auto& args = *parsed;
    if (args.empty()) {
        return {false, RespParser::encode_error("ERR empty command")};
    }

    std::string cmd = args[0];
    std::transform(
        cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return std::toupper(c); });

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
        return {false, handle_xadd(args)};
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
        return {false, handle_xread(args)};
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
        std::string option = args[i];
        std::transform(option.begin(), option.end(), option.begin(), [](unsigned char c) {
            return std::toupper(c);
        });

        if (option == "EX" || option == "PX") {
            if (i + 1 >= args.size()) {
                return RespParser::encode_error("ERR syntax error");
            }

            uint64_t num = 0;
            try {
                num = std::stoull(args[i + 1]);
            } catch (...) {
                return RespParser::encode_error("ERR value is not an integer or out of range");
            }

            ttl_ms = (option == "EX") ? num * 1000 : num;
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

    int64_t count = 0;
    try {
        count = std::stoll(args[2]);
    } catch (...) {
        return RespParser::encode_error("ERR value is not an integer or out of range");
    }

    if (count <= 0) {
        return RespParser::encode_array({});
    }

    auto elements = store_.lpop(key, count);
    return RespParser::encode_array(elements);
}

std::string CommandHandler::handle_lrange(const std::vector<std::string>& args) {
    const std::string& key = args[1];

    int64_t start = 0;
    int64_t stop = 0;

    try {
        start = std::stoll(args[2]);
        stop = std::stoll(args[3]);
    } catch (...) {
        return RespParser::encode_error("ERR value is not an integer or out of range");
    }

    auto elements = store_.lrange(key, start, stop);
    return RespParser::encode_array(elements);
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

    std::string result = "*" + std::to_string(entries.size()) + "\r\n";
    for (const auto& entry : entries) {
        result += "*2\r\n";
        result += RespParser::encode_bulk_string(entry.id);
        result += "*" + std::to_string(entry.fields.size() * 2) + "\r\n";
        for (const auto& [field, value] : entry.fields) {
            result += RespParser::encode_bulk_string(field);
            result += RespParser::encode_bulk_string(value);
        }
    }

    return result;
}

std::string CommandHandler::handle_xread(const std::vector<std::string>& args) {
    size_t streams_idx = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        std::string arg = args[i];
        std::transform(
            arg.begin(), arg.end(), arg.begin(), [](unsigned char c) { return std::toupper(c); });
        if (arg == "STREAMS") {
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

    std::string result = "*" + std::to_string(results.size()) + "\r\n";
    for (const auto& [key, entries] : results) {
        result += "*2\r\n";
        result += RespParser::encode_bulk_string(key);
        result += "*" + std::to_string(entries.size()) + "\r\n";
        for (const auto& entry : entries) {
            result += "*2\r\n";
            result += RespParser::encode_bulk_string(entry.id);
            result += "*" + std::to_string(entry.fields.size() * 2) + "\r\n";
            for (const auto& [field, value] : entry.fields) {
                result += RespParser::encode_bulk_string(field);
                result += RespParser::encode_bulk_string(value);
            }
        }
    }

    return result;
}
