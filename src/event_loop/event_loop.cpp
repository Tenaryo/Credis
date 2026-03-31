#include "event_loop.hpp"
#include <iostream>
#include <unistd.h>

EventLoop::EventLoop() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "Failed to create epoll instance\n";
    }
}

EventLoop::~EventLoop() {
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
}

EventLoop::EventLoop(EventLoop&& other) noexcept : epoll_fd_(other.epoll_fd_) {
    other.epoll_fd_ = -1;
}

EventLoop& EventLoop::operator=(EventLoop&& other) noexcept {
    if (this != &other) {
        if (epoll_fd_ >= 0) {
            close(epoll_fd_);
        }
        epoll_fd_ = other.epoll_fd_;
        other.epoll_fd_ = -1;
    }
    return *this;
}

void EventLoop::add_fd(int fd, uint32_t events) {
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        std::cerr << "Failed to add fd to epoll\n";
    }
}

void EventLoop::remove_fd(int fd) { epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr); }

void EventLoop::run(int /* server_fd */,
                    std::function<void(int)> on_data,
                    std::function<std::chrono::milliseconds()> get_timeout) {
    struct epoll_event events[MAX_EVENTS];

    while (running_) {
        auto timeout_ms = get_timeout();
        int timeout = timeout_ms.count() < 0 ? -1 : static_cast<int>(timeout_ms.count());

        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "epoll_wait failed\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            on_data(fd);
        }
    }
}
