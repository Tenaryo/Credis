#include "connection/connection.hpp"
#include "event_loop/event_loop.hpp"
#include "handler/command_handler.hpp"
#include "server/server.hpp"
#include <iostream>
#include <memory>
#include <unordered_map>

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    constexpr int PORT = 6379;

    Server server(PORT);
    if (server.fd() < 0) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    std::cout << "Waiting for clients to connect...\n";

    EventLoop event_loop;
    if (event_loop.fd() < 0) {
        std::cerr << "Failed to create event loop\n";
        return 1;
    }

    event_loop.add_fd(server.fd());

    std::unordered_map<int, std::unique_ptr<Connection>> connections;
    CommandHandler handler;

    event_loop.run(server.fd(), [&](int fd) {
        if (fd == server.fd()) {
            auto client_result = server.accept_connection();
            if (client_result) {
                int client_fd = *client_result;
                std::cout << "Client connected\n";
                connections[client_fd] = std::make_unique<Connection>(client_fd);
                event_loop.add_fd(client_fd);
            }
        } else {
            auto it = connections.find(fd);
            if (it != connections.end()) {
                Connection& conn = *it->second;
                if (conn.handle_read()) {
                    std::string response = handler.process("");
                    conn.send_data(response.c_str(), response.size());
                } else {
                    std::cout << "Client disconnected\n";
                    event_loop.remove_fd(fd);
                    connections.erase(it);
                }
            }
        }
    });

    return 0;
}
