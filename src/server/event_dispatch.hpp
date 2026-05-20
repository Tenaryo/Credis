#pragma once

#include <chrono>
#include <optional>

namespace credis::handler {
class CommandHandler;
} // namespace credis::handler

namespace credis::connection {
class ConnectionPool;
} // namespace credis::connection

namespace credis::replica {
class ReplicaManager;
class ReplicaConnector;
} // namespace credis::replica

namespace credis::blocking {
class BlockingManager;
} // namespace credis::blocking

namespace credis::pubsub {
class PubSubManager;
} // namespace credis::pubsub

namespace credis::event_loop {
class EventLoop;
} // namespace credis::event_loop

namespace credis::server {

class TcpListener;

struct EventContext {
    TcpListener& listener;
    credis::event_loop::EventLoop& loop;
    credis::handler::CommandHandler& handler;
    credis::connection::ConnectionPool& conn_pool;
    credis::replica::ReplicaManager& replica_mgr;
    std::optional<credis::replica::ReplicaConnector>& replica_conn;
    credis::blocking::BlockingManager& blocking;
    credis::pubsub::PubSubManager& pubsub;
};

void dispatch_event(int fd, EventContext& ctx);
auto compute_timeout(EventContext& ctx) -> std::chrono::milliseconds;

} // namespace credis::server
