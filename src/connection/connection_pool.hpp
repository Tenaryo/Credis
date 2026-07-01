#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "connection/connection.hpp"

namespace credis::connection {

class ConnectionPool {
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    std::vector<int> dirty_fds_;

    void mark_dirty(int fd, Connection& conn);

  public:
    void add(int fd);
    void remove(int fd);
    auto read_from(int fd) -> std::optional<std::string_view>;
    void consume(int fd, size_t n);
    void send_to(int fd, std::string_view response);
    [[nodiscard]] auto get_pending_write(int fd) -> std::string&;
    void flush_all();
    [[nodiscard]] auto contains(int fd) const -> bool;
    [[nodiscard]] auto get_connection(int fd) -> Connection&;
    [[nodiscard]] auto dirty_count() const noexcept -> size_t {
        return dirty_fds_.size();
    }
};

} // namespace credis::connection
