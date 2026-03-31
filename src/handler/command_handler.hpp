#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class BlockingManager;
class Store;

struct ProcessResult {
    bool should_block{false};
    std::string response;
};

struct TransactionState {
    bool in_multi{false};
    std::vector<std::vector<std::string>> queued_commands;
};

class CommandHandler {
    Store& store_;
    BlockingManager* blocking_manager_{nullptr};
    std::unordered_map<int, TransactionState> transactions_;
  public:
    explicit CommandHandler(Store& store);

    void set_blocking_manager(BlockingManager* manager) { blocking_manager_ = manager; }

    std::string process(std::string_view input);
    ProcessResult process_with_fd(int fd,
                                  std::string_view input,
                                  std::function<void(int, const std::string&)> send_to_blocked);
  private:
    ProcessResult execute_command(const std::vector<std::string>& args,
                                  int fd,
                                  std::function<void(int, const std::string&)> send_to_blocked);

    static std::string handle_ping();
    static std::string handle_echo(std::string_view args);
    std::string handle_set(const std::vector<std::string>& args);
    std::string handle_get(const std::string& key);
    std::string handle_incr(const std::string& key);
    std::string handle_rpush(const std::vector<std::string>& args);
    std::string handle_lpush(const std::vector<std::string>& args);
    std::string handle_lpop(const std::vector<std::string>& args);
    std::string handle_lrange(const std::vector<std::string>& args);
    std::string handle_info(const std::vector<std::string>& args);
    std::string handle_xadd(const std::vector<std::string>& args);
    std::string handle_xrange(const std::vector<std::string>& args);
    std::string handle_xread(const std::vector<std::string>& args);
    ProcessResult handle_xread_with_blocking(int fd, const std::vector<std::string>& args);
    ProcessResult
    handle_xadd_with_blocking(const std::vector<std::string>& args,
                              std::function<void(int, const std::string&)> send_to_blocked);

    ProcessResult handle_blpop(int fd, const std::vector<std::string>& args);
    ProcessResult
    handle_rpush_with_blocking(const std::vector<std::string>& args,
                               std::function<void(int, const std::string&)> send_to_blocked);
    ProcessResult
    handle_lpush_with_blocking(const std::vector<std::string>& args,
                               std::function<void(int, const std::string&)> send_to_blocked);
};
