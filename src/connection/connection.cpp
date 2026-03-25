#include "connection.hpp"
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd) : fd_(fd), buffer_(BUFFER_SIZE) {}

Connection::~Connection() { close(); }

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_), buffer_(std::move(other.buffer_)) {
    other.fd_ = -1;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        buffer_ = std::move(other.buffer_);
        other.fd_ = -1;
    }
    return *this;
}

void Connection::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Connection::handle_read() {
    ssize_t bytes_read = ::read(fd_, buffer_.data(), buffer_.size());
    if (bytes_read <= 0) {
        return false;
    }
    return true;
}

void Connection::send_data(const char* data, size_t len) { ::send(fd_, data, len, 0); }
