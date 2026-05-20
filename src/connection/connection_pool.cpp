#include "connection_pool.hpp"

namespace credis::connection {

void ConnectionPool::add(int fd) {
    connections_[fd] = std::make_unique<Connection>(fd);
}

void ConnectionPool::remove(int fd) {
    connections_.erase(fd);
}

auto ConnectionPool::read_from(int fd) -> std::optional<std::string_view> {
    auto it = connections_.find(fd);
    if (it == connections_.end()) [[unlikely]] {
        return std::nullopt;
    }
    return it->second->handle_read();
}

auto ConnectionPool::send_to(int fd, std::string_view response) -> bool {
    auto it = connections_.find(fd);
    if (it == connections_.end()) [[unlikely]] {
        return false;
    }
    it->second->send_data(response.data(), response.size());
    return true;
}

auto ConnectionPool::contains(int fd) const -> bool {
    return connections_.contains(fd);
}

} // namespace credis::connection
