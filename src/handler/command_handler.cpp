#include "command_handler.hpp"
#include "protocol/resp_parser.hpp"
#include "store/store.hpp"
#include <algorithm>
#include <cctype>

CommandHandler::CommandHandler(Store& store) : store_(store) {}

std::string CommandHandler::process(std::string_view input) {
    auto parsed = RespParser::parse(input);
    if (!parsed) {
        return RespParser::encode_error("ERR " + parsed.error());
    }

    const auto& args = *parsed;
    if (args.empty()) {
        return RespParser::encode_error("ERR empty command");
    }

    std::string cmd = args[0];
    std::transform(
        cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return std::toupper(c); });

    if (cmd == "PING") {
        return handle_ping();
    } else if (cmd == "ECHO") {
        if (args.size() < 2) {
            return RespParser::encode_error("ERR wrong number of arguments for 'echo' command");
        }
        return handle_echo(args[1]);
    } else if (cmd == "SET") {
        if (args.size() < 3) {
            return RespParser::encode_error("ERR wrong number of arguments for 'set' command");
        }
        return handle_set(args);
    } else if (cmd == "GET") {
        if (args.size() < 2) {
            return RespParser::encode_error("ERR wrong number of arguments for 'get' command");
        }
        return handle_get(args[1]);
    } else if (cmd == "RPUSH") {
        if (args.size() < 3) {
            return RespParser::encode_error("ERR wrong number of arguments for 'rpush' command");
        }
        return handle_rpush(args);
    } else if (cmd == "LPUSH") {
        if (args.size() < 3) {
            return RespParser::encode_error("ERR wrong number of arguments for 'lpush' command");
        }
        return handle_lpush(args);
    } else if (cmd == "LLEN") {
        if (args.size() < 2) {
            return RespParser::encode_error("ERR wrong number of arguments for 'llen' command");
        }
        return RespParser::encode_integer(store_.llen(args[1]));
    } else if (cmd == "LPOP") {
        if (args.size() < 2) {
            return RespParser::encode_error("ERR wrong number of arguments for 'lpop' command");
        }
        return handle_lpop(args[1]);
    } else if (cmd == "LRANGE") {
        if (args.size() < 4) {
            return RespParser::encode_error("ERR wrong number of arguments for 'lrange' command");
        }
        return handle_lrange(args);
    }

    return RespParser::encode_error("ERR unknown command '" + cmd + "'");
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

            if (option == "EX") {
                ttl_ms = num * 1000;
            } else {
                ttl_ms = num;
            }
            ++i;
        }
    }

    store_.set(key, value, ttl_ms);
    return RespParser::encode_simple_string("OK");
}

std::string CommandHandler::handle_get(const std::string& key) {
    auto value = store_.get(key);
    if (value) {
        return RespParser::encode_bulk_string(*value);
    }
    return RespParser::encode_null_bulk_string();
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

std::string CommandHandler::handle_lpop(const std::string& key) {
    auto value = store_.lpop(key);
    if (value) {
        return RespParser::encode_bulk_string(*value);
    }
    return RespParser::encode_null_bulk_string();
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
