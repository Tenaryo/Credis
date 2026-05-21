#include <gtest/gtest.h>

#include <string>

#include "protocol/resp_parser.hpp"
#include "replica/replica_connector.hpp"

namespace {

using namespace std::string_view_literals;

TEST(ReplicaConnectorTest, GetackGeneratesAckResponseAndTracksOffset) {
    credis::replica::ReplicaConnector connector("localhost", 6379);

    auto data = credis::protocol::encode_array({"REPLCONF", "GETACK", "*"});
    connector.append_to_pending_buffer(data);

    auto result = connector.process_pending_buffer();

    EXPECT_TRUE(result.commands.empty());

    auto expected_ack = credis::protocol::encode_array({"REPLCONF", "ACK", "0"});
    EXPECT_EQ(result.ack_responses, expected_ack);
}

TEST(ReplicaConnectorTest, GetackOffsetIncrementsAcrossMultipleRequests) {
    credis::replica::ReplicaConnector connector("localhost", 6379);

    auto getack = credis::protocol::encode_array({"REPLCONF", "GETACK", "*"});

    // First GETACK: offset is 0
    connector.append_to_pending_buffer(getack);
    auto result1 = connector.process_pending_buffer();
    auto expected_ack0 = credis::protocol::encode_array({"REPLCONF", "ACK", "0"});
    EXPECT_EQ(result1.ack_responses, expected_ack0);

    // Second GETACK: offset should include the first GETACK's bytes
    connector.append_to_pending_buffer(getack);
    auto result2 = connector.process_pending_buffer();
    auto expected_ack1 = credis::protocol::encode_array({"REPLCONF", "ACK", std::to_string(getack.size())});
    EXPECT_EQ(result2.ack_responses, expected_ack1);
}

TEST(ReplicaConnectorTest, MixedGetackAndCommands) {
    credis::replica::ReplicaConnector connector("localhost", 6379);

    auto getack = credis::protocol::encode_array({"REPLCONF", "GETACK", "*"});
    auto ping = credis::protocol::encode_array({"PING"});

    std::string combined = getack + ping;
    connector.append_to_pending_buffer(combined);

    auto result = connector.process_pending_buffer();

    // GETACK should not appear in commands, only PING
    ASSERT_EQ(result.commands.size(), 1);

    // ACK response should use offset 0 (no commands before first GETACK)
    auto expected_ack = credis::protocol::encode_array({"REPLCONF", "ACK", "0"});
    EXPECT_EQ(result.ack_responses, expected_ack);
}

TEST(ReplicaConnectorTest, MultipleGetacksWithCommands) {
    credis::replica::ReplicaConnector connector("localhost", 6379);

    auto getack = credis::protocol::encode_array({"REPLCONF", "GETACK", "*"});
    auto ping = credis::protocol::encode_array({"PING"});
    auto set_foo = credis::protocol::encode_array({"SET", "foo", "1"});

    // Request 1: GETACK alone
    {
        connector.append_to_pending_buffer(getack);
        auto result = connector.process_pending_buffer();
        auto expected_ack = credis::protocol::encode_array({"REPLCONF", "ACK", "0"});
        EXPECT_EQ(result.ack_responses, expected_ack);
        EXPECT_TRUE(result.commands.empty());
    }

    // Request 2: PING + SET + GETACK
    {
        std::string combined = std::string(ping) + std::string(set_foo) + std::string(getack);
        connector.append_to_pending_buffer(combined);
        auto result = connector.process_pending_buffer();

        // PING and SET should be in commands, GETACK should not
        ASSERT_EQ(result.commands.size(), 2);

        // ACK should reflect offset before current GETACK was processed
        auto expected_ack = credis::protocol::encode_array(
            {"REPLCONF", "ACK", std::to_string(getack.size() + ping.size() + set_foo.size())});
        EXPECT_EQ(result.ack_responses, expected_ack);
    }
}

} // namespace
