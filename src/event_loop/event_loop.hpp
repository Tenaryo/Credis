#pragma once

#include <sys/epoll.h>

#include <chrono>
#include <functional>

namespace credis::event_loop {

class EventLoop {
    int epoll_fd_{-1};
    int max_events_;

    static constexpr int kDefaultMaxEvents = 1024;
    bool running_{true};

  public:
    explicit EventLoop(int max_events = kDefaultMaxEvents);
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    auto operator=(const EventLoop&) -> EventLoop& = delete;
    EventLoop(EventLoop&&) noexcept;
    auto operator=(EventLoop&&) noexcept -> EventLoop&;

    void add_fd(int fd, uint32_t events = EPOLLIN) const;
    void remove_fd(int fd) const;
    void run(
        int server_fd,
        const std::function<void(int)>& on_event,
        const std::function<std::chrono::milliseconds()>& get_timeout) const;
    void stop() {
        running_ = false;
    }
};

} // namespace credis::event_loop
