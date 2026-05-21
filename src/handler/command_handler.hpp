#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "handler/process_result.hpp"
#include "server/acl_manager.hpp"
#include "server/server_config.hpp"

namespace credis::store {
class Store;
} // namespace credis::store

namespace credis::blocking {
class BlockingManager;
} // namespace credis::blocking

namespace credis::pubsub {
class PubSubManager;
} // namespace credis::pubsub

namespace credis::handler {

struct CommandContext {
    credis::store::Store& store;
    const credis::server::ServerConfig& config;
    std::optional<std::reference_wrapper<credis::blocking::BlockingManager>> blocking_manager;
    std::optional<std::reference_wrapper<credis::pubsub::PubSubManager>> pubsub_manager;
    credis::server::AclManager* acl_manager{nullptr};
    std::function<size_t()> replica_count_fn;
    std::unordered_map<int, TransactionState>* transactions{nullptr};
    std::unordered_set<int>* authenticated_fds{nullptr};
};

class CommandHandler {
    credis::store::Store& store_;
    credis::server::ServerConfig config_;
    std::optional<std::reference_wrapper<credis::blocking::BlockingManager>> blocking_manager_;
    std::optional<std::reference_wrapper<credis::pubsub::PubSubManager>> pubsub_manager_;
    std::function<size_t()> replica_count_fn_;
    credis::server::AclManager acl_manager_;
    std::unordered_map<int, TransactionState> transactions_;
    std::unordered_set<int> authenticated_fds_;

  public:
    explicit CommandHandler(credis::store::Store& store, credis::server::ServerConfig config = {});

    void set_blocking_manager(credis::blocking::BlockingManager& manager) {
        blocking_manager_.emplace(manager);
    }
    void set_pubsub_manager(credis::pubsub::PubSubManager& manager) {
        pubsub_manager_.emplace(manager);
    }
    void set_replica_count_fn(std::function<size_t()> fn) {
        replica_count_fn_ = std::move(fn);
    }
    [[nodiscard]] auto config() const noexcept -> const credis::server::ServerConfig& {
        return config_;
    }
    [[nodiscard]] auto acl_manager() noexcept -> credis::server::AclManager& {
        return acl_manager_;
    }

    void remove_connection(int fd);

    auto process(std::string_view input) -> std::string;
    auto process_with_fd(int fd,
                         std::string_view input,
                         std::function<void(int, const std::string&)> send_to_client) -> ProcessResult;

  private:
    template <typename SendFn>
    auto execute_command(const std::vector<std::string>& args, int fd, SendFn&& send_to_client) -> ProcessResult;

    using CmdHandler = std::function<ProcessResult(CommandContext&,
                                                   const std::vector<std::string>&,
                                                   int,
                                                   std::function<void(int, const std::string&)>)>;

    struct CommandEntry {
        CmdHandler handler;
        size_t min_args;
    };

    std::unordered_map<std::string_view, CommandEntry> command_table_;
    void register_commands();
};

} // namespace credis::handler
