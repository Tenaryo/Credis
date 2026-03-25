#pragma once

#include <string>
#include <string_view>

class CommandHandler {
  public:
    std::string process(std::string_view input);
  private:
    static std::string handle_ping();
    static std::string handle_echo(std::string_view args);
};
