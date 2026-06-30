#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "blocking_manager/blocking_manager.hpp"

using namespace credis::blocking;

class BlockingManagerTest : public ::testing::Test {
  protected:
    BlockingManager mgr_;
};

TEST_F(BlockingManagerTest, BlockClientMarksAsBlocked) {
    mgr_.block_client(1, "key", std::chrono::milliseconds(1000));
    EXPECT_TRUE(mgr_.is_blocked(1));
}

TEST_F(BlockingManagerTest, UnblockClientRemovesBlock) {
    mgr_.block_client(1, "key", std::chrono::milliseconds(1000));
    mgr_.unblock_client(1);
    EXPECT_FALSE(mgr_.is_blocked(1));
}

TEST_F(BlockingManagerTest, WakeClientReturnsBlockedClient) {
    mgr_.block_client(1, "key", std::chrono::milliseconds(0));
    auto result = mgr_.wake_client("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fd, 1);
    EXPECT_EQ(result->key, "key");
}

TEST_F(BlockingManagerTest, WakeClientOnEmptyKeyReturnsNullopt) {
    auto result = mgr_.wake_client("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(BlockingManagerTest, BlockClientIndefiniteTimeout) {
    mgr_.block_client(1, "key", std::chrono::milliseconds(0));
    EXPECT_TRUE(mgr_.is_blocked(1));
    auto expired = mgr_.get_expired_clients();
    EXPECT_TRUE(expired.empty());
}

TEST_F(BlockingManagerTest, GetExpiredClientsReturnsExpiredFd) {
    mgr_.block_client(1, "key", std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto expired = mgr_.get_expired_clients();
    ASSERT_EQ(expired.size(), 1uz);
    EXPECT_EQ(expired[0], 1);
    EXPECT_FALSE(mgr_.is_blocked(1));
}

TEST_F(BlockingManagerTest, GetNextDeadlineReturnsEarliest) {
    mgr_.block_client(1, "k1", std::chrono::milliseconds(1000));
    auto dl = mgr_.get_next_deadline();
    EXPECT_TRUE(dl.has_value());
}

TEST_F(BlockingManagerTest, GetNextDeadlineEmptyReturnsNullopt) {
    auto dl = mgr_.get_next_deadline();
    EXPECT_FALSE(dl.has_value());
}

TEST_F(BlockingManagerTest, BlockClientForStream) {
    mgr_.block_client_for_stream(1, "stream", credis::protocol::StreamId{0, 0}, std::chrono::milliseconds(1000));
    EXPECT_TRUE(mgr_.is_blocked(1));
}

TEST_F(BlockingManagerTest, WakeClientForStreamWithNewerId) {
    mgr_.block_client_for_stream(1, "stream", credis::protocol::StreamId{0, 0}, std::chrono::milliseconds(1000));
    auto result = mgr_.wake_client_for_stream("stream", "1-0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->fd, 1);
    EXPECT_FALSE(mgr_.is_blocked(1));
}

TEST_F(BlockingManagerTest, WakeClientForStreamWithOlderIdReturnsNullopt) {
    mgr_.block_client_for_stream(1, "stream", credis::protocol::StreamId{5, 0}, std::chrono::milliseconds(1000));
    auto result = mgr_.wake_client_for_stream("stream", "3-0");
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(mgr_.is_blocked(1));
}

TEST_F(BlockingManagerTest, WakeClientForStreamEmptyKey) {
    auto result = mgr_.wake_client_for_stream("nonexistent", "1-0");
    EXPECT_FALSE(result.has_value());
}

TEST_F(BlockingManagerTest, IsBlockedReturnsFalseForUnknownFd) {
    EXPECT_FALSE(mgr_.is_blocked(42));
}

TEST_F(BlockingManagerTest, UnblockClientUnknownFdDoesNotCrash) {
    EXPECT_NO_THROW(mgr_.unblock_client(42));
}
