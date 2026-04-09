#include "server_runner.hpp"
#include "protocol/resp_parser.hpp"
#include "replica/replica_connector.hpp"
#include "util/parse.hpp"

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

    if (wait_state_ && std::chrono::steady_clock::now() >= wait_state_->deadline) {
        finish_wait(count_acked_replicas());
    }

    auto now = std::chrono::steady_clock::now();
    std::optional<std::chrono::steady_clock::time_point> earliest;

    if (auto blocking_deadline = blocking_manager_.get_next_deadline())
        earliest = *blocking_deadline;

    if (wait_state_ && (!earliest || wait_state_->deadline < *earliest))
        earliest = wait_state_->deadline;

    if (!earliest)
        return std::chrono::milliseconds(-1);
    return *earliest <= now
               ? std::chrono::milliseconds(0)
               : std::chrono::duration_cast<std::chrono::milliseconds>(*earliest - now);
}

void RedisApp::send_to_blocked(int fd, const std::string& response) {
    if (auto it = connections_.find(fd); it != connections_.end()) {
        it->second->send_data(response.c_str(), response.size());
    }
}

void RedisApp::handle_replica_ack(int fd) {
    char buf[4096];
    auto n = ::read(fd, buf, sizeof(buf));
    if (n <= 0) {
        replica_fds_.erase(fd);
        replica_offsets_.erase(fd);
        replica_buffers_.erase(fd);
        event_loop_.remove_fd(fd);
        connections_.erase(fd);
        return;
    }
    replica_buffers_[fd].append(buf, static_cast<size_t>(n));

    auto& buffer = replica_buffers_[fd];
    while (auto result = RespParser::parse_one(buffer)) {
        auto& args = result->args;
        if (args.size() >= 3 && to_upper(args[0]) == "REPLCONF" && to_upper(args[1]) == "ACK") {
            replica_offsets_[fd] = std::stoll(args[2]);
        }
        buffer.erase(0, result->consumed);
    }

    if (wait_state_) {
        int acked = count_acked_replicas();
        if (acked >= wait_state_->numreplicas) {
            finish_wait(acked);
        }
    }
}

int RedisApp::count_acked_replicas() const {
    if (!wait_state_)
        return 0;
    return count_acked_replicas_for(wait_state_->target_offset);
}

int RedisApp::count_acked_replicas_for(int64_t target) const {
    int count = 0;
    for (int rfd : replica_fds_) {
        auto it = replica_offsets_.find(rfd);
        if (it != replica_offsets_.end() && it->second >= target) {
            ++count;
        }
    }
    return count;
}

void RedisApp::finish_wait(int count) {
    if (!wait_state_)
        return;
    int client_fd = wait_state_->client_fd;
    auto response = RespParser::encode_integer(count);
    if (auto it = connections_.find(client_fd); it != connections_.end()) {
        it->second->send_data(response.c_str(), response.size());
    }
    wait_state_.reset();
}

void RedisApp::on_event(int fd) {
    if (replica_connector_ && fd == replica_connector_->master_fd()) {
        auto result = replica_connector_->process_propagated_commands();
        if (!result.has_value()) {
            event_loop_.remove_fd(replica_connector_->master_fd());
            replica_connector_.reset();
        } else if (!result->empty()) {
            replica_connector_->send_response(*result);
        }
        return;
    }

    if (replica_fds_.contains(fd)) {
        handle_replica_ack(fd);
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

        if (result.is_wait) {
            int acked = count_acked_replicas_for(master_offset_);
            if (master_offset_ == 0 || acked >= result.wait_numreplicas) {
                acked = static_cast<int>(replica_fds_.size());
                auto resp = RespParser::encode_integer(acked);
                conn.send_data(resp.c_str(), resp.size());
            } else {
                auto deadline = result.wait_timeout_ms == 0
                                    ? std::chrono::steady_clock::now()
                                    : std::chrono::steady_clock::now() +
                                          std::chrono::milliseconds(result.wait_timeout_ms);
                wait_state_.emplace(
                    WaitState{fd, master_offset_, result.wait_numreplicas, deadline});

                for (int rfd : replica_fds_) {
                    if (auto rit = connections_.find(rfd); rit != connections_.end()) {
                        auto getack = RespParser::encode_array({"REPLCONF", "GETACK", "*"});
                        rit->second->send_data(getack.c_str(), getack.size());
                    }
                }
            }
        } else {
            if (!result.should_block) {
                conn.send_data(result.response.c_str(), result.response.size());
            }
        }

        if (result.is_replica_handshake) {
            replica_fds_.insert(fd);
        }
        if (!result.propagate_args.empty()) {
            auto msg = RespParser::encode_array(result.propagate_args);
            master_offset_ += static_cast<int64_t>(msg.size());
            for (int rfd : replica_fds_) {
                if (auto rit = connections_.find(rfd); rit != connections_.end()) {
                    rit->second->send_data(msg.c_str(), msg.size());
                }
            }
        }
    } else {
        replica_fds_.erase(fd);
        replica_offsets_.erase(fd);
        replica_buffers_.erase(fd);
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

        auto responses = replica_connector_->process_pending_buffer();
        if (!responses.empty()) {
            replica_connector_->send_response(responses);
        }
    }

    handler_.set_blocking_manager(&blocking_manager_);
    handler_.set_replica_count_fn([this]() { return replica_fds_.size(); });

    event_loop_.run(
        server_.fd(),
        [this](int fd) { on_event(fd); },
        [this]() -> std::chrono::milliseconds { return compute_timeout(); });

    return 0;
}
