#include "block_manager/blocking_manager.hpp"
#include "cli/cli_parser.hpp"
#include "connection/connection.hpp"
#include "event_loop/event_loop.hpp"
#include "handler/command_handler.hpp"
#include "protocol/resp_parser.hpp"
#include "server/server.hpp"
#include "store/store.hpp"
#include <iostream>
#include <memory>
#include <unordered_map>

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    Server server(parse_port(argc, argv));
    EventLoop event_loop;
    event_loop.add_fd(server.fd());

    Store store;
    CommandHandler handler(store);
    BlockingManager blocking_manager;
    handler.set_blocking_manager(&blocking_manager);

    std::unordered_map<int, std::unique_ptr<Connection>> connections;

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
                } else {
                    blocking_manager.unblock_client(fd);
                    event_loop.remove_fd(fd);
                    connections.erase(it);
                }
            }
        },
        get_timeout);

    return 0;
}
