#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace credis::connection {

class Connection {
    int fd_{-1};
    std::vector<char> buffer_;
    size_t read_pos_{0};
    size_t data_len_{0};
    std::string pending_write_;

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
    auto handle_read() -> std::optional<std::string_view>;
    void consume(size_t n);
    void send_data(const char* data, size_t len);
    void flush();
    [[nodiscard]] auto pending_data() const -> std::string_view { return pending_write_; }
    void clear_pending() { pending_write_.clear(); }
    void trim_pending(size_t n) { pending_write_.erase(0, n); }

  private:
    void close();
    void compact();

    static constexpr size_t kInitialBufferSize = 4096;
    static constexpr size_t kMaxBufferSize = 512ULL * 1024 * 1024;
};

} // namespace credis::connection
