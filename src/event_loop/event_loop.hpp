#pragma once

#include <sys/epoll.h>

#include <chrono>
#include <csignal>
#include <functional>

namespace credis::event_loop {

class EventLoop {
    int epoll_fd_{-1};
    int max_events_;
    bool running_{true};
    static inline EventLoop* instance_{nullptr};

    static void handle_signal(int) {
        if (instance_ != nullptr) {
            instance_->stop();
        }
    }

    static constexpr int kDefaultMaxEvents = 1024;

  public:
    explicit EventLoop(int max_events = kDefaultMaxEvents);
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    auto operator=(const EventLoop&) -> EventLoop& = delete;
    EventLoop(EventLoop&&) noexcept;
    auto operator=(EventLoop&&) noexcept -> EventLoop&;

    void add_fd(int fd, uint32_t events = EPOLLIN) const;
    void remove_fd(int fd) const;
    void run(const std::function<void(int)>& on_event,
             const std::function<std::chrono::milliseconds()>& get_timeout) const;
    void stop() {
        running_ = false;
    }

    // TODO: graceful shutdown — drain in-flight requests, flush RDB/AOF, notify replicas
    // before returning from run(). Currently SIGINT/SIGTERM just stops the event loop;
    // RAII destructors clean up fds and connections.
};

} // namespace credis::event_loop
