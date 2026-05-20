#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "connection/connection.hpp"

namespace credis::connection {

class ConnectionPool {
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;

  public:
    void add(int fd);
    void remove(int fd);
    auto read_from(int fd) -> std::optional<std::string_view>;
    auto send_to(int fd, std::string_view response) -> bool;
    [[nodiscard]] auto contains(int fd) const -> bool;
};

} // namespace credis::connection
