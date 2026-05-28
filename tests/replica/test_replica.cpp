#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <variant>

#include "handler/command_handler.hpp"
#include "handler/process_result.hpp"
#include "protocol/resp_codec.hpp"
#include "replica/replica_manager.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

namespace {

using namespace std::string_view_literals;

// =============================================================================
// ReplicaManager tests (no network needed)
// =============================================================================

TEST(ReplicaManagerTest, AddReplicaIncreasesCount) {
    credis::replica::ReplicaManager mgr;
    EXPECT_EQ(mgr.count(), 0);

    mgr.add_replica(5);
    EXPECT_EQ(mgr.count(), 1);

    mgr.add_replica(7);
    EXPECT_EQ(mgr.count(), 2);
}

TEST(ReplicaManagerTest, RemoveReplicaDecreasesCount) {
    credis::replica::ReplicaManager mgr;
    mgr.add_replica(5);
    mgr.add_replica(7);
    EXPECT_EQ(mgr.count(), 2);

    mgr.remove_replica(5);
    EXPECT_EQ(mgr.count(), 1);

    mgr.remove_replica(7);
    EXPECT_EQ(mgr.count(), 0);
}

TEST(ReplicaManagerTest, PropagateIncreasesOffset) {
    credis::replica::ReplicaManager mgr;
    EXPECT_EQ(mgr.offset(), 0);

    mgr.propagate("SET foo bar\r\n");
    EXPECT_GT(mgr.offset(), 0);

    auto prev = mgr.offset();
    mgr.propagate("INCR foo\r\n");
    EXPECT_GT(mgr.offset(), prev);
}

TEST(ReplicaManagerTest, StartWaitSetsHasWait) {
    credis::replica::ReplicaManager mgr;
    EXPECT_FALSE(mgr.has_wait());

    mgr.start_wait(1, 2, 5000);
    EXPECT_TRUE(mgr.has_wait());
}

TEST(ReplicaManagerTest, StartWaitZeroReplicasZeroTimeoutResolvesImmediately) {
    credis::replica::ReplicaManager mgr;
    mgr.start_wait(1, 0, 0);

    auto result = mgr.check_wait_timeout();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->client_fd, 1);
    EXPECT_EQ(result->count, 0);
    EXPECT_FALSE(mgr.has_wait());
}

TEST(ReplicaManagerTest, ProcessAckHandlesReplconfGetackReturnsWaitResult) {
    credis::replica::ReplicaManager mgr;
    mgr.add_replica(5);

    mgr.start_wait(1, 1, 5000);

    auto ack_data = credis::protocol::encode_array({"REPLCONF", "ACK", "100"});
    auto result = mgr.process_ack(5, ack_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->client_fd, 1);
    EXPECT_EQ(result->count, 1);
    EXPECT_FALSE(mgr.has_wait());
}

// =============================================================================
// CommandHandler replica commands (no network needed)
// =============================================================================

struct HandlerFixture {
    credis::store::Store store;
    credis::server::ServerConfig config;
    credis::handler::CommandHandler handler{store, config};

    auto process(int fd, std::string_view input) -> credis::handler::ProcessResult {
        std::string client_response;
        auto send_fn = [&](int, const std::string& s) { client_response = s; };
        return handler.process_with_fd(fd, input, send_fn);
    }
};

TEST(CommandHandlerReplicaTest, PsyncReturnsReplicaHandshakeVariant) {
    HandlerFixture f;
    auto input = credis::protocol::encode_array({"PSYNC", "?", "-1"});
    auto result = f.process(1, input);

    EXPECT_TRUE(std::holds_alternative<credis::handler::ProcessResult::ReplicaHandshake>(result.state));
}

TEST(CommandHandlerReplicaTest, ReplconfListeningPortReturnsOk) {
    HandlerFixture f;
    auto input = credis::protocol::encode_array({"REPLCONF", "listening-port", "6380"});
    auto result = f.process(1, input);

    ASSERT_TRUE(std::holds_alternative<credis::handler::ProcessResult::Normal>(result.state));
    const auto& normal = std::get<credis::handler::ProcessResult::Normal>(result.state);
    EXPECT_EQ(normal.response, "+OK\r\n");
}

TEST(CommandHandlerReplicaTest, ReplconfCapaPsync2ReturnsOk) {
    HandlerFixture f;
    auto input = credis::protocol::encode_array({"REPLCONF", "capa", "psync2"});
    auto result = f.process(1, input);

    ASSERT_TRUE(std::holds_alternative<credis::handler::ProcessResult::Normal>(result.state));
    const auto& normal = std::get<credis::handler::ProcessResult::Normal>(result.state);
    EXPECT_EQ(normal.response, "+OK\r\n");
}

TEST(CommandHandlerReplicaTest, PsyncResponseIncludesFullresyncPrefix) {
    HandlerFixture f;
    auto input = credis::protocol::encode_array({"PSYNC", "?", "-1"});
    auto result = f.process(1, input);

    ASSERT_TRUE(std::holds_alternative<credis::handler::ProcessResult::ReplicaHandshake>(result.state));
    const auto& hs = std::get<credis::handler::ProcessResult::ReplicaHandshake>(result.state);

    EXPECT_TRUE(hs.response.starts_with("+FULLRESYNC"));
}

TEST(CommandHandlerReplicaTest, SetCommandGeneratesPropagateArgs) {
    HandlerFixture f;
    auto input = credis::protocol::encode_array({"SET", "foo", "bar"});
    auto result = f.process(1, input);

    ASSERT_TRUE(std::holds_alternative<credis::handler::ProcessResult::Normal>(result.state));

    ASSERT_EQ(result.propagate_args.size(), 3);
    EXPECT_EQ(result.propagate_args[0], "SET");
    EXPECT_EQ(result.propagate_args[1], "foo");
    EXPECT_EQ(result.propagate_args[2], "bar");
}

} // namespace
