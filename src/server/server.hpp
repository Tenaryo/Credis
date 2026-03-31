#pragma once

#include <expected>
#include <string>

class Server {
    int server_fd_{-1};
    int port_;
    Server(int port, int fd) : server_fd_(fd), port_(port) {}
  public:
    static std::expected<Server, std::string> create(int port);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;

    std::expected<int, std::string> accept_connection() const;
    [[nodiscard]] int fd() const noexcept { return server_fd_; }
};
