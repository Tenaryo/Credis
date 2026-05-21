#include "event_loop.hpp"

#include <unistd.h>

#include <vector>

#include "util/logger.hpp"

namespace credis::event_loop {

EventLoop::EventLoop(int max_events) : max_events_(max_events) {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) [[unlikely]] {
        LOG_ERROR("Failed to create epoll instance");
    }
}

EventLoop::~EventLoop() {
    if (epoll_fd_ >= 0) [[likely]] {
        close(epoll_fd_);
    }
}

EventLoop::EventLoop(EventLoop&& other) noexcept : epoll_fd_(other.epoll_fd_) {
    other.epoll_fd_ = -1;
}

auto EventLoop::operator=(EventLoop&& other) noexcept -> EventLoop& {
    if (this != &other) [[likely]] {
        if (epoll_fd_ >= 0) [[likely]] {
            close(epoll_fd_);
        }
        epoll_fd_ = other.epoll_fd_;
        other.epoll_fd_ = -1;
    }
    return *this;
}

void EventLoop::add_fd(int fd, uint32_t events) const {
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) [[unlikely]] {
        LOG_ERROR("Failed to add fd to epoll");
    }
}

void EventLoop::remove_fd(int fd) const {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

void EventLoop::run(int /* server_fd */,
                    const std::function<void(int)>& on_event,
                    const std::function<std::chrono::milliseconds()>& get_timeout) const {
    std::vector<struct epoll_event> events(static_cast<size_t>(max_events_));

    while (running_) [[likely]] {
        auto timeout_ms = get_timeout();
        int timeout = timeout_ms.count() < 0 ? -1 : static_cast<int>(timeout_ms.count());

        int n = epoll_wait(epoll_fd_, events.data(), max_events_, timeout);
        if (n < 0) [[unlikely]] {
            if (errno == EINTR) [[likely]] {
                continue;
            }
            LOG_ERROR("epoll_wait failed");
            break;
        }

        for (int i = 0; i < n; ++i) [[likely]] {
            int fd = events[i].data.fd;
            on_event(fd);
        }
    }
}

} // namespace credis::event_loop
