#pragma once

#include <string>
#include <string_view>
#include <vector>

class Store;

class CommandHandler {
    Store& store_;
  public:
    explicit CommandHandler(Store& store);

    std::string process(std::string_view input);
  private:
    static std::string handle_ping();
    static std::string handle_echo(std::string_view args);
    std::string handle_set(const std::vector<std::string>& args);
    std::string handle_get(const std::string& key);
    std::string handle_rpush(const std::vector<std::string>& args);
    std::string handle_lpush(const std::vector<std::string>& args);
    std::string handle_lpop(const std::vector<std::string>& args);
    std::string handle_lrange(const std::vector<std::string>& args);
};
