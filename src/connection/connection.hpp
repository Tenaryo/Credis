#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace credis::connection {

class Connection {
    int fd_{-1};
    std::vector<char> buffer_;
    ssize_t bytes_read_{0};

  public:
    explicit Connection(int fd);
    ~Connection();

    Connection(const Connection&) = delete;
    auto operator=(const Connection&) -> Connection& = delete;
    Connection(Connection&&) noexcept;
    auto operator=(Connection&&) noexcept -> Connection&;

    [[nodiscard]] auto fd() const noexcept -> int {
        return fd_;
    }
    void close();
    auto handle_read() -> std::optional<std::string_view>;
    void send_data(const char* data, size_t len) const;

    static constexpr size_t kInitialBufferSize = 4096;
    static constexpr size_t kMaxBufferSize = 512 * 1024 * 1024;
};

} // namespace credis::connection
