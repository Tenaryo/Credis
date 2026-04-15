#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "server/acl_manager.hpp"
#include "server/server_config.hpp"

class BlockingManager;
class PubSubManager;
class Store;

struct ProcessResult {
    struct Normal {
        std::string response;
    };
    struct Block {};
    struct ReplicaHandshake {
        std::string response;
    };
    struct Wait {
        int64_t numreplicas{0};
        int64_t timeout_ms{0};
    };

    std::variant<Normal, Block, ReplicaHandshake, Wait> state;
    std::vector<std::string> propagate_args;

    static ProcessResult normal(std::string resp) { return {Normal{std::move(resp)}, {}}; }
    static ProcessResult block() { return {Block{}, {}}; }
    static ProcessResult replica_handshake(std::string resp) {
        return {ReplicaHandshake{std::move(resp)}, {}};
    }
    static ProcessResult wait(int64_t num, int64_t timeout) { return {Wait{num, timeout}, {}}; }
};

struct TransactionState {
    bool in_multi{false};
    std::vector<std::vector<std::string>> queued_commands;
    std::unordered_map<std::string, uint64_t> watched_keys;
};

class CommandHandler {
    Store& store_;
    ServerConfig config_;
    BlockingManager* blocking_manager_{nullptr};
    PubSubManager* pubsub_manager_{nullptr};
    std::function<size_t()> replica_count_fn_;
    AclManager acl_manager_;
    std::unordered_map<int, TransactionState> transactions_;
    std::unordered_set<int> authenticated_fds_;
  public:
    explicit CommandHandler(Store& store, const ServerConfig& config = {});

    void set_blocking_manager(BlockingManager* manager) { blocking_manager_ = manager; }
    void set_pubsub_manager(PubSubManager* manager) { pubsub_manager_ = manager; }
    void set_replica_count_fn(std::function<size_t()> fn) { replica_count_fn_ = std::move(fn); }
    const ServerConfig& config() const noexcept { return config_; }

    void remove_connection(int fd);

    std::string process(std::string_view input);
    ProcessResult process_with_fd(int fd,
                                  std::string_view input,
                                  std::function<void(int, const std::string&)> send_to_client);
  private:
    template <typename SendFn>
    ProcessResult
    execute_command(const std::vector<std::string>& args, int fd, SendFn&& send_to_client);

    using CmdHandler = std::function<ProcessResult(const std::vector<std::string>&,
                                                   int,
                                                   std::function<void(int, const std::string&)>)>;

    struct CommandEntry {
        CmdHandler handler;
        size_t min_args;
    };

    std::unordered_map<std::string_view, CommandEntry> command_table_;
    void register_commands();

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
    std::string handle_config_get(const std::string& param);
    std::string handle_acl_whoami();
    std::string handle_acl_getuser(const std::vector<std::string>& args);
    std::string handle_acl_setuser(const std::vector<std::string>& args);
    std::string handle_xadd(const std::vector<std::string>& args);
    std::string handle_xrange(const std::vector<std::string>& args);
    std::string handle_xread(const std::vector<std::string>& args);
    std::string handle_zadd(const std::vector<std::string>& args);
    std::string handle_zrank(const std::vector<std::string>& args);
    std::string handle_zrange(const std::vector<std::string>& args);
    std::string handle_zcard(const std::string& key);
    std::string handle_zscore(const std::vector<std::string>& args);
    std::string handle_zrem(const std::vector<std::string>& args);
    std::string handle_geoadd(const std::vector<std::string>& args);
    std::string handle_geopos(const std::vector<std::string>& args);
    std::string handle_geodist(const std::vector<std::string>& args);
    std::string handle_geosearch(const std::vector<std::string>& args);
    ProcessResult handle_xread_with_blocking(int fd, const std::vector<std::string>& args);
    ProcessResult
    handle_xadd_with_blocking(const std::vector<std::string>& args,
                              std::function<void(int, const std::string&)> send_to_client);

    ProcessResult handle_blpop(int fd, const std::vector<std::string>& args);
    ProcessResult
    handle_rpush_with_blocking(const std::vector<std::string>& args,
                               std::function<void(int, const std::string&)> send_to_client);
    ProcessResult
    handle_lpush_with_blocking(const std::vector<std::string>& args,
                               std::function<void(int, const std::string&)> send_to_client);
};
