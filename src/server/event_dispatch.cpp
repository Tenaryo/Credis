#include "event_dispatch.hpp"

#include <variant>

#include "blocking_manager/blocking_manager.hpp"
#include "connection/connection_pool.hpp"
#include "event_loop/event_loop.hpp"
#include "handler/command_handler.hpp"
#include "protocol/resp_parser.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "replica/replica_connector.hpp"
#include "replica/replica_manager.hpp"
#include "server/server.hpp"

namespace credis::server {

void dispatch_event(int fd, EventContext& ctx) {
    if (ctx.replica_conn && fd == ctx.replica_conn->master_fd()) [[unlikely]] {
        auto result = ctx.replica_conn->process_propagated_commands();
        if (!result.has_value()) [[unlikely]] {
            ctx.replica_conn.reset();
        } else if (!result->commands.empty()) [[likely]] {
            for (auto& cmd : result->commands) {
                ctx.handler.process(cmd);
            }
            if (!result->ack_responses.empty()) [[unlikely]] {
                ctx.replica_conn->send_response(result->ack_responses);
            }
        }
        return;
    }

    if (ctx.replica_mgr.replica_fds().contains(fd)) [[unlikely]] {
        auto data = ctx.conn_pool.read_from(fd);
        if (!data) [[unlikely]] {
            ctx.replica_mgr.remove_replica(fd);
            ctx.conn_pool.remove(fd);
            ctx.loop.remove_fd(fd);
            return;
        }
        auto wait_result = ctx.replica_mgr.process_ack(fd, *data);
        if (wait_result) [[unlikely]] {
            ctx.conn_pool.send_to(wait_result->client_fd, credis::protocol::encode_integer(wait_result->count));
        }
        return;
    }

    if (fd == ctx.listener.fd()) [[unlikely]] {
        if (auto client = ctx.listener.accept_connection()) [[likely]] {
            ctx.conn_pool.add(*client);
            ctx.loop.add_fd(*client);
        }
        return;
    }

    auto data = ctx.conn_pool.read_from(fd);
    if (!data) [[unlikely]] {
        ctx.replica_mgr.remove_replica(fd);
        ctx.blocking.unblock_client(fd);
        ctx.pubsub.unsubscribe(fd);
        ctx.handler.remove_connection(fd);
        ctx.conn_pool.remove(fd);
        ctx.loop.remove_fd(fd);
        return;
    }

    auto result = ctx.handler.process_with_fd(
        fd, *data, [&ctx](int client_fd, const std::string& resp) { ctx.conn_pool.send_to(client_fd, resp); });

    using credis::handler::ProcessResult;

    if (std::holds_alternative<ProcessResult::Wait>(result.state)) [[unlikely]] {
        auto& w = std::get<ProcessResult::Wait>(result.state);
        int64_t acked = 0;
        for (int rfd : ctx.replica_mgr.replica_fds()) {
            // count acked replicas at current offset
            (void)rfd;
        }
        if (ctx.replica_mgr.offset() == 0 || acked >= w.numreplicas) [[unlikely]] {
            ctx.conn_pool.send_to(
                fd, credis::protocol::encode_integer(static_cast<int64_t>(ctx.replica_mgr.replica_fds().size())));
        } else {
            ctx.replica_mgr.start_wait(fd, w.numreplicas, w.timeout_ms);
            auto getack = credis::protocol::encode_array({"REPLCONF", "GETACK", "*"});
            for (int rfd : ctx.replica_mgr.replica_fds()) {
                ctx.conn_pool.send_to(rfd, getack);
            }
        }
    } else if (!std::holds_alternative<ProcessResult::Block>(result.state)) [[likely]] {
        std::string resp;
        if (std::holds_alternative<ProcessResult::Normal>(result.state)) [[likely]] {
            resp = std::get<ProcessResult::Normal>(result.state).response;
        } else [[unlikely]] {
            resp = std::get<ProcessResult::ReplicaHandshake>(result.state).response;
        }
        ctx.conn_pool.send_to(fd, resp);
    }

    if (std::holds_alternative<ProcessResult::ReplicaHandshake>(result.state)) [[unlikely]] {
        ctx.replica_mgr.add_replica(fd);
    }

    if (!result.propagate_args.empty()) [[likely]] {
        auto msg = credis::protocol::encode_array(result.propagate_args);
        ctx.replica_mgr.propagate(msg);
        for (int rfd : ctx.replica_mgr.replica_fds()) {
            ctx.conn_pool.send_to(rfd, msg);
        }
    }
}

auto compute_timeout(EventContext& ctx) -> std::chrono::milliseconds {
    for (int fd : ctx.blocking.get_expired_clients()) {
        ctx.conn_pool.send_to(fd, credis::protocol::encode_null_array());
    }

    if (auto wait_result = ctx.replica_mgr.check_wait_timeout()) [[unlikely]] {
        ctx.conn_pool.send_to(wait_result->client_fd, credis::protocol::encode_integer(wait_result->count));
    }

    auto now = std::chrono::steady_clock::now();
    std::optional<std::chrono::steady_clock::time_point> earliest;

    if (auto blocking_deadline = ctx.blocking.get_next_deadline()) [[likely]] {
        earliest = *blocking_deadline;
    }

    if (auto wait_deadline = ctx.replica_mgr.next_deadline()) [[unlikely]] {
        if (!earliest || *wait_deadline < *earliest) [[unlikely]] {
            earliest = *wait_deadline;
        }
    }

    if (!earliest) [[unlikely]] {
        return std::chrono::milliseconds(-1);
    }
    return *earliest <= now ? std::chrono::milliseconds(0)
                            : std::chrono::duration_cast<std::chrono::milliseconds>(*earliest - now);
}

} // namespace credis::server
