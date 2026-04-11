#pragma once

#include "block_manager/blocking_manager.hpp"
#include "connection/connection.hpp"
#include "event_loop/event_loop.hpp"
#include "handler/command_handler.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "replica/replica_connector.hpp"
#include "server/server.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

struct AppConfig {
    int port{6379};
    ServerConfig server_config;
};

struct WaitState {
    int client_fd;
    int64_t target_offset;
    int64_t numreplicas;
    std::chrono::steady_clock::time_point deadline;
};

class RedisApp {
    Server server_;
    EventLoop event_loop_;
    Store store_;
    CommandHandler handler_;
    BlockingManager blocking_manager_;
    PubSubManager pubsub_manager_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    std::unordered_set<int> replica_fds_;
    std::unique_ptr<ReplicaConnector> replica_connector_;
    int listening_port_;
    int64_t master_offset_{0};
    std::unordered_map<int, int64_t> replica_offsets_;
    std::unordered_map<int, std::string> replica_buffers_;
    std::optional<WaitState> wait_state_;

    void on_event(int fd);
    std::chrono::milliseconds compute_timeout();
    void send_to_client(int fd, const std::string& response);
    bool perform_replica_handshake();
    void handle_replica_ack(int fd);
    int count_acked_replicas() const;
    int count_acked_replicas_for(int64_t target) const;
    void finish_wait(int count);
    void load_rdb();
  public:
    RedisApp(Server server, int listening_port, const ServerConfig& config);
    RedisApp(const RedisApp&) = delete;
    RedisApp& operator=(const RedisApp&) = delete;
    RedisApp(RedisApp&&) = delete;
    RedisApp& operator=(RedisApp&&) = delete;

    static std::expected<RedisApp, std::string> create(const AppConfig& config);
    int run();
};
