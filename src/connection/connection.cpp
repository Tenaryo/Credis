#include "connection.hpp"

#include <sys/socket.h>
#include <unistd.h>

namespace credis::connection {

Connection::Connection(int fd) : fd_(fd), buffer_(kInitialBufferSize) {
}

Connection::~Connection() {
    close();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_), buffer_(std::move(other.buffer_)), bytes_read_(other.bytes_read_) {
    other.fd_ = -1;
}

auto Connection::operator=(Connection&& other) noexcept -> Connection& {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        buffer_ = std::move(other.buffer_);
        bytes_read_ = other.bytes_read_;
        other.fd_ = -1;
    }
    return *this;
}

void Connection::close() {
    if (fd_ >= 0) [[likely]] {
        ::close(fd_);
        fd_ = -1;
    }
}

auto Connection::handle_read() -> std::optional<std::string_view> {
    bytes_read_ = ::read(fd_, buffer_.data(), buffer_.size());
    if (bytes_read_ <= 0) [[unlikely]] {
        return std::nullopt;
    }
    if (static_cast<size_t>(bytes_read_) == buffer_.size() && buffer_.size() < kMaxBufferSize) [[unlikely]] {
        buffer_.resize(buffer_.size() * 2);
    }
    return std::string_view(buffer_.data(), static_cast<size_t>(bytes_read_));
}

void Connection::send_data(const char* data, size_t len) const {
    size_t sent = 0;
    while (sent < len) [[likely]] {
        auto n = ::send(fd_, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) [[unlikely]] {
            break;
        }
        sent += static_cast<size_t>(n);
    }
}

} // namespace credis::connection
