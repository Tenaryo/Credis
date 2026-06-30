#include <gtest/gtest.h>

#include <string>
#include <variant>

#include "blocking_manager/blocking_manager.hpp"
#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerListTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerListTest, RpushReturnsCount) {
    auto response = handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    EXPECT_EQ(response, ":1\r\n");

    response = handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(HandlerListTest, LpushReturnsCount) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    auto response = handler_.process("*3\r\n$5\r\nLPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(HandlerListTest, RpushMultiple) {
    auto response = handler_.process("*4\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n$1\r\nb\r\n");
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(HandlerListTest, LpushMultiple) {
    auto response = handler_.process("*4\r\n$5\r\nLPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n$1\r\nb\r\n");
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(HandlerListTest, RpushWrongType) {
    handler_.process("*3\r\n$3\r\nSET\r\n$4\r\nlist\r\n$5\r\nhello\r\n");
    auto response = handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    EXPECT_TRUE(response.starts_with("-WRONGTYPE"));
}

TEST_F(HandlerListTest, Llen) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    auto response = handler_.process("*2\r\n$4\r\nLLEN\r\n$4\r\nlist\r\n");
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(HandlerListTest, LlenEmpty) {
    auto response = handler_.process("*2\r\n$4\r\nLLEN\r\n$5\r\nempty\r\n");
    EXPECT_EQ(response, ":0\r\n");
}

TEST_F(HandlerListTest, Lrange) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    auto response = handler_.process("*4\r\n$6\r\nLRANGE\r\n$4\r\nlist\r\n$1\r\n0\r\n$2\r\n-1\r\n");
    EXPECT_EQ(response, "*2\r\n$1\r\na\r\n$1\r\nb\r\n");
}

TEST_F(HandlerListTest, LrangeEmptyList) {
    auto response = handler_.process("*4\r\n$6\r\nLRANGE\r\n$5\r\nempty\r\n$1\r\n0\r\n$2\r\n-1\r\n");
    EXPECT_EQ(response, "*0\r\n");
}

TEST_F(HandlerListTest, LrangeInvalidArgs) {
    auto response = handler_.process("*4\r\n$6\r\nLRANGE\r\n$4\r\nlist\r\n$3\r\nabc\r\n$1\r\n0\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerListTest, Lpop) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    auto response = handler_.process("*2\r\n$4\r\nLPOP\r\n$4\r\nlist\r\n");
    EXPECT_EQ(response, "$1\r\na\r\n");
}

TEST_F(HandlerListTest, LpopEmptyList) {
    auto response = handler_.process("*2\r\n$4\r\nLPOP\r\n$8\r\nemptylst\r\n");
    EXPECT_EQ(response, "$-1\r\n");
}

TEST_F(HandlerListTest, LpopCounted) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nc\r\n");
    auto response = handler_.process("*3\r\n$4\r\nLPOP\r\n$4\r\nlist\r\n$1\r\n2\r\n");
    EXPECT_EQ(response, "*2\r\n$1\r\na\r\n$1\r\nb\r\n");
}

TEST_F(HandlerListTest, LpopCountedInvalidCount) {
    auto response = handler_.process("*3\r\n$4\r\nLPOP\r\n$4\r\nlist\r\n$3\r\nabc\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerListTest, LpopCountedZero) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    auto response = handler_.process("*3\r\n$4\r\nLPOP\r\n$4\r\nlist\r\n$1\r\n0\r\n");
    EXPECT_EQ(response, "*0\r\n");
}

TEST_F(HandlerListTest, Rpop) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    auto response = handler_.process("*2\r\n$4\r\nRPOP\r\n$4\r\nlist\r\n");
    EXPECT_EQ(response, "$1\r\nb\r\n");
}

TEST_F(HandlerListTest, RpopEmptyList) {
    auto response = handler_.process("*2\r\n$4\r\nRPOP\r\n$8\r\nemptylst\r\n");
    EXPECT_EQ(response, "$-1\r\n");
}

TEST_F(HandlerListTest, RpopCounted) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nc\r\n");
    auto response = handler_.process("*3\r\n$4\r\nRPOP\r\n$4\r\nlist\r\n$1\r\n2\r\n");
    EXPECT_EQ(response, "*2\r\n$1\r\nc\r\n$1\r\nb\r\n");
}

TEST_F(HandlerListTest, RpopCountedInvalidCount) {
    auto response = handler_.process("*3\r\n$4\r\nRPOP\r\n$4\r\nlist\r\n$3\r\nabc\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

class HandlerListBlockingTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
    credis::blocking::BlockingManager blocking_manager_;

    void SetUp() override {
        handler_.set_blocking_manager(blocking_manager_);
    }
};

TEST_F(HandlerListBlockingTest, BlpopExistingKey) {
    constexpr int kClientFd = 1;

    handler_.process_with_fd(kClientFd, "*3\r\n$5\r\nRPUSH\r\n$2\r\nbl\r\n$1\r\na\r\n", nullptr);

    std::string response;
    handler_.set_output(response);
    auto result = handler_.process_with_fd(kClientFd, "*3\r\n$5\r\nBLPOP\r\n$2\r\nbl\r\n$1\r\n0\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(response, "*2\r\n$2\r\nbl\r\n$1\r\na\r\n");
}

TEST_F(HandlerListBlockingTest, BlpopEmptyListReturnsBlock) {
    constexpr int kClientFd = 1;

    auto result = handler_.process_with_fd(kClientFd, "*3\r\n$5\r\nBLPOP\r\n$5\r\nnokey\r\n$1\r\n1\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Block>(result.state))
        << "BLPOP on empty list with BlockingManager should return Block state";
    EXPECT_TRUE(blocking_manager_.is_blocked(kClientFd));
}

TEST_F(HandlerListBlockingTest, BlpopNegativeTimeout) {
    constexpr int kFd = 2;
    std::string response;
    handler_.set_output(response);
    auto result = handler_.process_with_fd(kFd, "*3\r\n$5\r\nBLPOP\r\n$3\r\nkey\r\n$2\r\n-1\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerListBlockingTest, BlpopInvalidTimeout) {
    constexpr int kFd = 3;
    std::string response;
    handler_.set_output(response);
    auto result = handler_.process_with_fd(kFd, "*3\r\n$5\r\nBLPOP\r\n$3\r\nkey\r\n$3\r\nabc\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerListBlockingTest, RpushWakesBlpop) {
    constexpr int kBlockedFd = 10;
    constexpr int kPushFd = 20;

    auto result = handler_.process_with_fd(kBlockedFd, "*3\r\n$5\r\nBLPOP\r\n$2\r\nbl\r\n$1\r\n0\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Block>(result.state));

    std::string delivered_to_blocked;
    auto send_cb = [&delivered_to_blocked](int fd, const std::string& data) {
        if (fd == kBlockedFd) {
            delivered_to_blocked = data;
        }
    };

    result = handler_.process_with_fd(kPushFd, "*3\r\n$5\r\nRPUSH\r\n$2\r\nbl\r\n$1\r\na\r\n", send_cb);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(delivered_to_blocked, "*2\r\n$2\r\nbl\r\n$1\r\na\r\n");
}

TEST_F(HandlerListBlockingTest, LpushWakesBlpop) {
    constexpr int kBlockedFd = 11;
    constexpr int kPushFd = 21;

    auto result = handler_.process_with_fd(kBlockedFd, "*3\r\n$5\r\nBLPOP\r\n$2\r\nbl\r\n$1\r\n0\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Block>(result.state));

    std::string delivered_to_blocked;
    auto send_cb = [&delivered_to_blocked](int fd, const std::string& data) {
        if (fd == kBlockedFd) {
            delivered_to_blocked = data;
        }
    };

    result = handler_.process_with_fd(kPushFd, "*3\r\n$5\r\nLPUSH\r\n$2\r\nbl\r\n$1\r\nz\r\n", send_cb);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(delivered_to_blocked, "*2\r\n$2\r\nbl\r\n$1\r\nz\r\n");
}
