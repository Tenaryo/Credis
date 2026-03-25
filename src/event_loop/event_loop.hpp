#pragma once

#include <functional>
#include <sys/epoll.h>

class EventLoop {
    int epoll_fd_{-1};
    static constexpr int MAX_EVENTS = 64;
  public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop(EventLoop&&) noexcept;
    EventLoop& operator=(EventLoop&&) noexcept;

    void add_fd(int fd, uint32_t events = EPOLLIN);
    void remove_fd(int fd);
    void run(int server_fd, std::function<void(int)> on_event);

    [[nodiscard]] int fd() const noexcept { return epoll_fd_; }
};
