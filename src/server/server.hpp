#pragma once

#include <expected>
#include <string>

namespace credis::server {

class TcpListener {
    int server_fd_{-1};
    int port_;
    TcpListener(int port, int fd) : server_fd_(fd), port_(port) {
    }

  public:
    static std::expected<TcpListener, std::string> create(int port);
    ~TcpListener();

    TcpListener(const TcpListener&) = delete;
    auto operator=(const TcpListener&) -> TcpListener& = delete;
    TcpListener(TcpListener&&) noexcept;
    auto operator=(TcpListener&&) noexcept -> TcpListener&;

    [[nodiscard]] std::expected<int, std::string> accept_connection() const;
    [[nodiscard]] auto fd() const noexcept -> int {
        return server_fd_;
    }
};

} // namespace credis::server
