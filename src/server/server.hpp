#pragma once

#include <expected>
#include <string>

#include "util/error.hpp"

namespace credis::server {

class TcpListener {
    int server_fd_{-1};
    TcpListener(int fd) : server_fd_(fd) {
    }

  public:
    static auto create(int port) -> std::expected<TcpListener, credis::util::Error>;
    ~TcpListener();

    TcpListener(const TcpListener&) = delete;
    auto operator=(const TcpListener&) -> TcpListener& = delete;
    TcpListener(TcpListener&&) noexcept;
    auto operator=(TcpListener&&) noexcept -> TcpListener&;

    [[nodiscard]] auto accept_connection() const -> std::expected<int, credis::util::Error>;
    [[nodiscard]] auto fd() const noexcept -> int {
        return server_fd_;
    }
};

} // namespace credis::server
