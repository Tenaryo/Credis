#include "server_runner.hpp"
#include "protocol/resp_parser.hpp"
#include "replica/replica_connector.hpp"

#include <iostream>

RedisApp::RedisApp(Server server, int listening_port, const ServerConfig& config)
    : server_(std::move(server)), handler_(store_, config), listening_port_(listening_port) {}

std::expected<RedisApp, std::string> RedisApp::create(const AppConfig& config) {
    auto server = Server::create(config.port);
    if (!server)
        return std::unexpected(server.error());
    return std::expected<RedisApp, std::string>(
        std::in_place, std::move(*server), config.port, config.server_config);
}

bool RedisApp::perform_replica_handshake() {
    const auto& rc = handler_.config().replicaof;
    auto connector = std::make_unique<ReplicaConnector>(rc->host, rc->port);
    connector->set_handler(handler_);
    if (!connector->send_ping() || !connector->send_replconf(listening_port_) ||
        !connector->send_psync() || !connector->receive_rdb().has_value()) {
        return false;
    }
    replica_connector_ = std::move(connector);
    return true;
}

std::chrono::milliseconds RedisApp::compute_timeout() {
    for (int fd : blocking_manager_.get_expired_clients()) {
        if (auto it = connections_.find(fd); it != connections_.end()) {
            auto response = RespParser::encode_null_array();
            it->second->send_data(response.c_str(), response.size());
        }
    }
    auto next = blocking_manager_.get_next_deadline();
    if (!next)
        return std::chrono::milliseconds(-1);
    auto now = std::chrono::steady_clock::now();
    return *next <= now ? std::chrono::milliseconds(0)
                        : std::chrono::duration_cast<std::chrono::milliseconds>(*next - now);
}

void RedisApp::send_to_blocked(int fd, const std::string& response) {
    if (auto it = connections_.find(fd); it != connections_.end()) {
        it->second->send_data(response.c_str(), response.size());
    }
}

void RedisApp::on_event(int fd) {
    if (replica_connector_ && fd == replica_connector_->master_fd()) {
        if (!replica_connector_->process_propagated_commands()) {
            event_loop_.remove_fd(replica_connector_->master_fd());
            replica_connector_.reset();
        }
        return;
    }

    if (fd == server_.fd()) {
        if (auto client = server_.accept_connection()) {
            connections_[*client] = std::make_unique<Connection>(*client);
            event_loop_.add_fd(*client);
        }
        return;
    }

    auto it = connections_.find(fd);
    if (it == connections_.end())
        return;

    Connection& conn = *it->second;
    if (auto data = conn.handle_read()) {
        auto result = handler_.process_with_fd(
            fd, *data, [this](int fd, const std::string& resp) { send_to_blocked(fd, resp); });

        if (!result.should_block) {
            conn.send_data(result.response.c_str(), result.response.size());
        }
        if (result.is_replica_handshake) {
            replica_fds_.insert(fd);
        }
        if (!result.propagate_args.empty()) {
            auto msg = RespParser::encode_array(result.propagate_args);
            for (int rfd : replica_fds_) {
                if (auto rit = connections_.find(rfd); rit != connections_.end()) {
                    rit->second->send_data(msg.c_str(), msg.size());
                }
            }
        }
    } else {
        replica_fds_.erase(fd);
        blocking_manager_.unblock_client(fd);
        event_loop_.remove_fd(fd);
        connections_.erase(it);
    }
}

int RedisApp::run() {
    event_loop_.add_fd(server_.fd());

    if (handler_.config().is_replica()) {
        if (!perform_replica_handshake()) {
            std::cerr << "Failed to complete replica handshake\n";
            return 1;
        }
        event_loop_.add_fd(replica_connector_->master_fd());
    }

    handler_.set_blocking_manager(&blocking_manager_);

    event_loop_.run(
        server_.fd(),
        [this](int fd) { on_event(fd); },
        [this]() -> std::chrono::milliseconds { return compute_timeout(); });

    return 0;
}
