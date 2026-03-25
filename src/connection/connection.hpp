#pragma once

#include <vector>

class Connection {
    int fd_{-1};
    std::vector<char> buffer_;
  public:
    explicit Connection(int fd);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    [[nodiscard]] int fd() const noexcept { return fd_; }
    void close();
    bool handle_read();
    void send_data(const char* data, size_t len);

    static constexpr size_t BUFFER_SIZE = 4096;
};
