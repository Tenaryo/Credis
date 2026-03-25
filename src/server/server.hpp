#pragma once

#include <expected>
#include <string>

class Server {
    int server_fd_{-1};
    int port_;
  public:
    explicit Server(int port);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;

    std::expected<int, std::string> accept_connection() const;
    [[nodiscard]] int fd() const noexcept { return server_fd_; }
  private:
    std::expected<void, std::string> create_socket();
    std::expected<void, std::string> set_reuse_addr();
    std::expected<void, std::string> bind_socket();
    std::expected<void, std::string> listen_socket();
};
