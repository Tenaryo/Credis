#include "command_handler.hpp"
#include <algorithm>
#include <cctype>

std::string CommandHandler::process(std::string_view input) { return handle_ping(); }

std::string CommandHandler::handle_ping() { return "+PONG\r\n"; }

std::string CommandHandler::handle_echo(std::string_view args) {
    return "+" + std::string(args) + "\r\n";
}
