#include "connection.hpp"

#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace credis::connection {

Connection::Connection(int fd) : fd_(fd), buffer_(kInitialBufferSize) {
}

Connection::~Connection() {
    close();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_), buffer_(std::move(other.buffer_)), read_pos_(other.read_pos_), data_len_(other.data_len_) {
    other.fd_ = -1;
}

auto Connection::operator=(Connection&& other) noexcept -> Connection& {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        buffer_ = std::move(other.buffer_);
        read_pos_ = other.read_pos_;
        data_len_ = other.data_len_;
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

void Connection::compact() {
    if (read_pos_ > 0) {
        size_t unconsumed = data_len_ - read_pos_;
        if (unconsumed > 0) {
            std::memmove(buffer_.data(), buffer_.data() + read_pos_, unconsumed);
        }
        data_len_ = unconsumed;
        read_pos_ = 0;
    }
}

auto Connection::handle_read() -> std::optional<std::string_view> {
    compact();

    if (data_len_ == buffer_.size() && buffer_.size() < kMaxBufferSize) [[unlikely]] {
        buffer_.resize(buffer_.size() * 2);
    }

    // TODO: compact may memmove large buffers (rare, only huge values); could use ring buffer
    ssize_t n = ::read(fd_, buffer_.data() + data_len_, buffer_.size() - data_len_);
    if (n <= 0) [[unlikely]] {
        return std::nullopt;
    }

    data_len_ += static_cast<size_t>(n);
    return std::string_view(buffer_.data() + read_pos_, data_len_ - read_pos_);
}

void Connection::consume(size_t n) {
    read_pos_ += n;
}

void Connection::send_data(const char* data, size_t len) const {
    size_t sent = 0;
    while (sent < len) {
        auto n = ::send(fd_, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            break;
        }
        sent += static_cast<size_t>(n);
    }
}

} // namespace credis::connection
