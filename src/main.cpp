#include "block_manager/blocking_manager.hpp"
#include "cli/cli_parser.hpp"
#include "connection/connection.hpp"
#include "event_loop/event_loop.hpp"
#include "handler/command_handler.hpp"
#include "protocol/resp_parser.hpp"
#include "replica/replica_connector.hpp"
#include "server/server.hpp"
#include "store/store.hpp"
#include <iostream>
#include <memory>
#include <unordered_map>

#include <unordered_set>

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int server_port = parse_port(argc, argv);
    auto server_result = Server::create(server_port);
    if (!server_result) {
        std::cerr << server_result.error() << '\n';
        return 1;
    }
    auto server = std::move(*server_result);
    EventLoop event_loop;
    event_loop.add_fd(server.fd());

    ServerConfig config;
    config.replicaof = parse_replicaof(argc, argv);

    Store store;
    CommandHandler handler(store, config);
    BlockingManager blocking_manager;
    handler.set_blocking_manager(&blocking_manager);

    if (config.is_replica()) {
        ReplicaConnector connector(config.replicaof->host, config.replicaof->port);
        if (!connector.send_ping()) {
            std::cerr << "Failed to complete PING handshake with master\n";
            return 1;
        }
        if (!connector.send_replconf(server_port)) {
            std::cerr << "Failed to complete REPLCONF handshake with master\n";
            return 1;
        }
        if (!connector.send_psync()) {
            std::cerr << "Failed to complete PSYNC handshake with master\n";
            return 1;
        }
        auto rdb = connector.receive_rdb();
        if (!rdb) {
            std::cerr << "Failed to receive RDB file from master\n";
            return 1;
        }
    }

    std::unordered_map<int, std::unique_ptr<Connection>> connections;
    std::unordered_set<int> replica_fds;

    auto get_timeout = [&]() -> std::chrono::milliseconds {
        auto expired = blocking_manager.get_expired_clients();
        for (int fd : expired) {
            if (auto it = connections.find(fd); it != connections.end()) {
                auto response = RespParser::encode_null_array();
                it->second->send_data(response.c_str(), response.size());
            }
        }
        auto next = blocking_manager.get_next_deadline();
        if (!next)
            return std::chrono::milliseconds(-1);
        auto now = std::chrono::steady_clock::now();
        return *next <= now ? std::chrono::milliseconds(0)
                            : std::chrono::duration_cast<std::chrono::milliseconds>(*next - now);
    };

    auto send_to_blocked = [&](int fd, const std::string& response) {
        if (auto it = connections.find(fd); it != connections.end()) {
            it->second->send_data(response.c_str(), response.size());
        }
    };

    event_loop.run(
        server.fd(),
        [&](int fd) {
            if (fd == server.fd()) {
                if (auto client = server.accept_connection()) {
                    connections[*client] = std::make_unique<Connection>(*client);
                    event_loop.add_fd(*client);
                }
            } else if (auto it = connections.find(fd); it != connections.end()) {
                Connection& conn = *it->second;
                if (auto data = conn.handle_read()) {
                    auto result = handler.process_with_fd(fd, *data, send_to_blocked);
                    if (!result.should_block) {
                        conn.send_data(result.response.c_str(), result.response.size());
                    }
                    if (result.is_replica_handshake) {
                        replica_fds.insert(fd);
                    }
                    if (!result.propagate_args.empty()) {
                        auto msg = RespParser::encode_array(result.propagate_args);
                        for (int rfd : replica_fds) {
                            if (auto rit = connections.find(rfd); rit != connections.end()) {
                                rit->second->send_data(msg.c_str(), msg.size());
                            }
                        }
                    }
                } else {
                    replica_fds.erase(fd);
                    blocking_manager.unblock_client(fd);
                    event_loop.remove_fd(fd);
                    connections.erase(it);
                }
            }
        },
        get_timeout);

    return 0;
}
