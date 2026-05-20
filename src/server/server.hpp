#pragma once

#include <expected>
#include <string>

class Server {
    int server_fd_{-1};
    int port_;
    Server(int port, int fd) : server_fd_(fd), port_(port) {
    }

  public:
    static std::expected<Server, std::string> create(int port);
    ~Server();

    Server(const Server&) = delete;
    auto operator=(const Server&) -> Server& = delete;
    Server(Server&&) noexcept;
    auto operator=(Server&&) noexcept -> Server&;

    [[nodiscard]] std::expected<int, std::string> accept_connection() const;
    [[nodiscard]] auto fd() const noexcept -> int {
        return server_fd_;
    }
};
