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

    std::string response_;
    auto process_result(int fd, std::string_view input) -> ProcessResult {
        response_.clear();
        handler_.set_output(response_);
        return handler_.process_with_fd(fd, input, nullptr);
    }
};

TEST_F(HandlerTransactionTest, MultiOk) {
    constexpr int kClientFd = 1;
    auto result = process_result(kClientFd, "*1\r\n$5\r\nMULTI\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, ExecEmptyMulti) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    auto result = process_result(kClientFd, "*1\r\n$4\r\nEXEC\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "*0\r\n");
}

TEST_F(HandlerTransactionTest, DiscardInMulti) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    auto result = process_result(kClientFd, "*1\r\n$7\r\nDISCARD\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, ExecWithoutMulti) {
    constexpr int kClientFd = 1;
    auto result = process_result(kClientFd, "*1\r\n$4\r\nEXEC\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "-ERR EXEC without MULTI\r\n");
}

TEST_F(HandlerTransactionTest, WatchOk) {
    constexpr int kClientFd = 1;
    auto result = process_result(kClientFd, "*2\r\n$5\r\nWATCH\r\n$3\r\nkey\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, UnwatchOk) {
    constexpr int kClientFd = 1;
    auto result = process_result(kClientFd, "*1\r\n$7\r\nUNWATCH\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "+OK\r\n");
}

TEST_F(HandlerTransactionTest, NestedMultiReturnsError) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    auto result = process_result(kClientFd, "*1\r\n$5\r\nMULTI\r\n");
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(response_.starts_with("-ERR"));
}

TEST_F(HandlerTransactionTest, MultiExecSetGet) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler_.process_with_fd(kClientFd, "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$3\r\nval\r\n", nullptr);
    handler_.process_with_fd(kClientFd, "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n", nullptr);
    auto result = process_result(kClientFd, "*1\r\n$4\r\nEXEC\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_NE(response_.find("+OK"), std::string::npos);
}

TEST_F(HandlerTransactionTest, DiscardWithoutMulti) {
    constexpr int kClientFd = 1;
    auto result = process_result(kClientFd, "*1\r\n$7\r\nDISCARD\r\n");
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(response_.starts_with("-ERR"));
}

TEST_F(HandlerTransactionTest, WatchWithVersionConflict) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*2\r\n$5\r\nWATCH\r\n$3\r\nkey\r\n", nullptr);
    store_.set("key", "modified");

    handler_.process_with_fd(kClientFd, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler_.process_with_fd(kClientFd, "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$3\r\nval\r\n", nullptr);
    auto result = process_result(kClientFd, "*1\r\n$4\r\nEXEC\r\n");

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "*-1\r\n");
}

TEST_F(HandlerTransactionTest, WatchMultipleKeys) {
    constexpr int kClientFd = 1;
    auto result = process_result(kClientFd, "*3\r\n$5\r\nWATCH\r\n$2\r\nk1\r\n$2\r\nk2\r\n");
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response_, "+OK\r\n");
}
