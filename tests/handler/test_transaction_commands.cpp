#include <gtest/gtest.h>

#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerTransactionTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerTransactionTest, MultiOk) {
    constexpr int kClientFd = 1;
    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, ExecEmptyMulti) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$4\r\nEXEC\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "*0\r\n");
}

TEST_F(HandlerTransactionTest, DiscardInMulti) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$7\r\nDISCARD\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, ExecWithoutMulti) {
    constexpr int kClientFd = 1;
    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$4\r\nEXEC\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "-ERR EXEC without MULTI\r\n");
}

TEST_F(HandlerTransactionTest, WatchOk) {
    constexpr int kClientFd = 1;
    auto result = handler_.process_with_fd(kClientFd, "*2\r\n$5\r\nWATCH\r\n$3\r\nkey\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, UnwatchOk) {
    constexpr int kClientFd = 1;
    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$7\r\nUNWATCH\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, NestedMultiReturnsError) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(std::get<ProcessResult::Normal>(result.state).response.starts_with("-ERR"));
}

TEST_F(HandlerTransactionTest, MultiExecSetGet) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler_.process_with_fd(kClientFd, "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$3\r\nval\r\n", nullptr);
    handler_.process_with_fd(kClientFd, "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n", nullptr);
    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$4\r\nEXEC\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_NE(std::get<ProcessResult::Normal>(result.state).response.find("+OK"), std::string::npos);
}

TEST_F(HandlerTransactionTest, DiscardWithoutMulti) {
    constexpr int kClientFd = 1;
    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$7\r\nDISCARD\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(std::get<ProcessResult::Normal>(result.state).response.starts_with("-ERR"));
}

TEST_F(HandlerTransactionTest, WatchWithVersionConflict) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*2\r\n$5\r\nWATCH\r\n$3\r\nkey\r\n", nullptr);
    store_.set("key", "modified");

    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler_.process_with_fd(kClientFd, "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$3\r\nval\r\n", nullptr);
    auto result = handler_.process_with_fd(kClientFd, "*1\r\n$4\r\nEXEC\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "*-1\r\n");
}

TEST_F(HandlerTransactionTest, WatchMultipleKeys) {
    constexpr int kClientFd = 1;
    auto result = handler_.process_with_fd(kClientFd, "*3\r\n$5\r\nWATCH\r\n$2\r\nk1\r\n$2\r\nk2\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "+OK\r\n");
}
