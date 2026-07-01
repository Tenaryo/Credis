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
#include "handler/transaction_state.hpp"
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

namespace credis::aof {
class AofManager;
} // namespace credis::aof

namespace credis::connection {
class ConnectionPool;
} // namespace credis::connection

namespace credis::handler {

struct CommandContext {
    credis::store::Store& store;
    credis::server::ServerConfig config;
    credis::aof::AofManager* aof_manager = nullptr;
    credis::connection::ConnectionPool* conn_pool = nullptr;
    std::string* out = nullptr;
    std::optional<std::reference_wrapper<credis::blocking::BlockingManager>> blocking_manager;
    std::optional<std::reference_wrapper<credis::pubsub::PubSubManager>> pubsub_manager;
    credis::server::AclManager acl_manager;
    std::function<size_t()> replica_count_fn;
    std::function<int64_t()> offset_fn;
    std::unordered_map<int, TransactionState> transactions;
    std::unordered_set<int> authenticated_fds;
};

class CommandHandler {
    CommandContext ctx_;

  public:
    explicit CommandHandler(credis::store::Store& store, credis::server::ServerConfig config = {});

    void set_blocking_manager(credis::blocking::BlockingManager& manager) {
        ctx_.blocking_manager.emplace(manager);
    }
    void set_pubsub_manager(credis::pubsub::PubSubManager& manager) {
        ctx_.pubsub_manager.emplace(manager);
    }
    void set_replica_count_fn(std::function<size_t()> fn) {
        ctx_.replica_count_fn = std::move(fn);
    }
    void set_offset_fn(std::function<int64_t()> fn) {
        ctx_.offset_fn = std::move(fn);
    }
    void set_aof_manager(credis::aof::AofManager& mgr) {
        ctx_.aof_manager = &mgr;
    }
    void set_connection_pool(credis::connection::ConnectionPool& pool) {
        ctx_.conn_pool = &pool;
    }
    [[nodiscard]] auto get_aof_manager() const noexcept -> credis::aof::AofManager* {
        return ctx_.aof_manager;
    }
    [[nodiscard]] auto config() const noexcept -> const credis::server::ServerConfig& {
        return ctx_.config;
    }
    [[nodiscard]] auto acl_manager() noexcept -> credis::server::AclManager& {
        return ctx_.acl_manager;
    }

    void remove_connection(int fd);

    auto process(std::string_view input) -> std::string;
    auto process_with_fd(int fd,
                         std::string_view input,
                         const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;

    void set_output(std::string& out) {
        ctx_.out = &out;
    }

  private:
    template <typename SendFn>
    auto execute_command(std::vector<std::string_view> args, std::string_view cmd, int fd, SendFn&& send_to_client)
        -> ProcessResult;

    auto process_single_command(int fd,
                                std::vector<std::string_view> args,
                                std::string_view cmd,
                                const std::function<void(int, const std::string&)>& send_to_client) -> ProcessResult;

    using CmdHandler = std::function<ProcessResult(CommandContext&,
                                                   const std::vector<std::string_view>&,
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
