#include "server.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace credis::server {

auto TcpListener::create(int port) -> std::expected<TcpListener, std::string> {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) [[unlikely]] {
        return std::unexpected("Failed to create server socket");
    }

    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) [[unlikely]] {
        close(fd);
        return std::unexpected("setsockopt failed");
    }

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) [[unlikely]] {
        close(fd);
        return std::unexpected("Failed to bind to port " + std::to_string(port));
    }

    if (listen(fd, 65535) != 0) [[unlikely]] {
        close(fd);
        return std::unexpected("listen failed");
    }

    return TcpListener(port, fd);
}

TcpListener::~TcpListener() {
    if (server_fd_ >= 0) [[likely]] {
        close(server_fd_);
    }
}

TcpListener::TcpListener(TcpListener&& other) noexcept : server_fd_(other.server_fd_), port_(other.port_) {
    other.server_fd_ = -1;
}

auto TcpListener::operator=(TcpListener&& other) noexcept -> TcpListener& {
    if (this != &other) [[likely]] {
        if (server_fd_ >= 0) [[likely]] {
            close(server_fd_);
        }
        server_fd_ = other.server_fd_;
        port_ = other.port_;
        other.server_fd_ = -1;
    }
    return *this;
}

auto TcpListener::accept_connection() const -> std::expected<int, std::string> {
    struct sockaddr_in client_addr {};
    socklen_t len = sizeof(client_addr);
    int fd = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
    if (fd < 0) [[unlikely]] {
        return std::unexpected("accept failed");
    }
    return fd;
}

} // namespace credis::server
