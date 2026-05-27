#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>

#include <csignal>

#include "blocking_manager/blocking_manager.hpp"
#include "cli/cli_parser.hpp"
#include "connection/connection_pool.hpp"
#include "event_loop/event_loop.hpp"
#include "handler/command_handler.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "rdb/rdb_parser.hpp"
#include "replica/replica_connector.hpp"
#include "replica/replica_manager.hpp"
#include "server/event_dispatch.hpp"
#include "server/server.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"
#include "util/logger.hpp"

namespace {

credis::event_loop::EventLoop* g_loop = nullptr;

void load_rdb(credis::store::Store& store, const credis::server::ServerConfig& config) {
    if (config.dir.empty() || config.dbfilename.empty()) [[likely]] {
        return;
    }

    auto path = std::filesystem::path(config.dir) / config.dbfilename;
    auto entries = credis::rdb::load_rdb_file(path.string());

    using namespace std::chrono;
    auto now_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    for (auto& [key, entry] : entries) {
        if (!std::holds_alternative<credis::store::String>(entry.value)) [[unlikely]] {
            continue;
        }

        std::optional<uint64_t> ttl;
        if (entry.expire_ms) [[unlikely]] {
            auto remaining = static_cast<int64_t>(*entry.expire_ms) - now_ms;
            if (remaining <= 0) [[unlikely]] {
                continue;
            }
            ttl = static_cast<uint64_t>(remaining);
        }

        store.set(key, std::move(std::get<credis::store::String>(entry.value)), ttl);
    }
}

} // namespace

auto main(int argc, char* argv[]) -> int {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // 1. Parse configuration
    credis::server::ServerConfig server_config;
    server_config.replica = credis::cli::parse_replicaof(argc, argv);
    server_config.generate_replid();

    auto port = credis::cli::parse_port(argc, argv);
    server_config.dir = credis::cli::parse_dir(argc, argv);
    if (server_config.dir.empty()) {
        server_config.dir = std::filesystem::current_path().string();
    }
    server_config.dbfilename = credis::cli::parse_dbfilename(argc, argv);

    // 2. Create TCP listener
    auto listener = credis::server::TcpListener::create(port);
    if (!listener) [[unlikely]] {
        LOG_ERROR(listener.error().to_string());
        return 1;
    }

    // 3. Create core components
    auto store = credis::store::Store{};
    auto handler = credis::handler::CommandHandler(store, server_config);
    auto blocking = credis::blocking::BlockingManager{};
    auto pubsub = credis::pubsub::PubSubManager{};
    auto conn_pool = credis::connection::ConnectionPool{};
    auto replica_mgr = credis::replica::ReplicaManager{};
    auto loop = credis::event_loop::EventLoop{};

    // 4. Load RDB file
    load_rdb(store, server_config);

    // 5. Replica handshake
    std::optional<credis::replica::ReplicaConnector> replica_conn
        = credis::replica::connect_if_replica(server_config, port);
    if (server_config.is_replica() && !replica_conn) [[unlikely]] {
        LOG_ERROR("Failed to complete replica handshake");
        return 1;
    }

    // 6. Wire dependencies
    handler.set_blocking_manager(blocking);
    handler.set_pubsub_manager(pubsub);
    handler.set_replica_count_fn([&replica_mgr] { return replica_mgr.count(); });

    // 7. Add initial fds to event loop
    loop.add_fd(listener->fd());
    if (replica_conn) [[unlikely]] {
        loop.add_fd(replica_conn->master_fd());

        auto buffered = replica_conn->process_pending_buffer();
        if (!buffered.commands.empty()) [[unlikely]] {
            for (auto& cmd : buffered.commands) {
                handler.process(cmd);
            }
        }
        if (!buffered.ack_responses.empty()) [[unlikely]] {
            replica_conn->send_response(buffered.ack_responses);
        }
    }

    // 9. Create event context
    auto ctx = credis::server::EventContext{
        *listener, loop, handler, conn_pool, replica_mgr, replica_conn, blocking, pubsub};

    // 10. Run event loop
    // TODO: graceful shutdown — drain in-flight requests, flush RDB/AOF, notify replicas
    // before returning from run(). Currently SIGINT/SIGTERM just stops the event loop;
    // RAII destructors clean up fds and connections.
    g_loop = &loop;
    std::signal(SIGINT, [](int) { if (g_loop) g_loop->stop(); });
    std::signal(SIGTERM, [](int) { if (g_loop) g_loop->stop(); });

    loop.run(
        listener->fd(),
        [&ctx](int fd) { credis::server::dispatch_event(fd, ctx); },
        [&ctx]() -> std::chrono::milliseconds { return credis::server::compute_timeout(ctx); });
    return 0;
}
