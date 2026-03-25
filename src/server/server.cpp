#include "server.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(int port) : port_(port) {
    if (create_socket() && set_reuse_addr() && bind_socket() && listen_socket()) {
        return;
    }
}

Server::~Server() {
    if (server_fd_ >= 0) {
        close(server_fd_);
    }
}

Server::Server(Server&& other) noexcept : server_fd_(other.server_fd_), port_(other.port_) {
    other.server_fd_ = -1;
}

Server& Server::operator=(Server&& other) noexcept {
    if (this != &other) {
        if (server_fd_ >= 0) {
            close(server_fd_);
        }
        server_fd_ = other.server_fd_;
        port_ = other.port_;
        other.server_fd_ = -1;
    }
    return *this;
}

std::expected<void, std::string> Server::create_socket() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        return std::unexpected("Failed to create server socket");
    }
    return {};
}

std::expected<void, std::string> Server::set_reuse_addr() {
    int reuse = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        return std::unexpected("setsockopt failed");
    }
    return {};
}

std::expected<void, std::string> Server::bind_socket() {
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        return std::unexpected("Failed to bind to port " + std::to_string(port_));
    }
    return {};
}

std::expected<void, std::string> Server::listen_socket() {
    if (listen(server_fd_, 65535) != 0) {
        return std::unexpected("listen failed");
    }
    return {};
}

std::expected<int, std::string> Server::accept_connection() const {
    struct sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        return std::unexpected("accept failed");
    }
    return client_fd;
}
