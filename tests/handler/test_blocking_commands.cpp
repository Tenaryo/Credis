#include <gtest/gtest.h>

#include "blocking_manager/blocking_manager.hpp"
#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerBlockingTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
    credis::blocking::BlockingManager blocking_manager_;

    void SetUp() override {
        handler_.set_blocking_manager(blocking_manager_);
    }
};

TEST_F(HandlerBlockingTest, BlpopExistingKey) {
    constexpr int kClientFd = 1;

    handler_.process_with_fd(kClientFd, "*3\r\n$5\r\nRPUSH\r\n$2\r\nbl\r\n$1\r\na\r\n", nullptr);

    auto result = handler_.process_with_fd(kClientFd, "*3\r\n$5\r\nBLPOP\r\n$2\r\nbl\r\n$1\r\n0\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, "*2\r\n$2\r\nbl\r\n$1\r\na\r\n");
}

TEST_F(HandlerBlockingTest, BlpopEmptyListReturnsBlock) {
    constexpr int kClientFd = 1;

    auto result = handler_.process_with_fd(kClientFd, "*3\r\n$5\r\nBLPOP\r\n$5\r\nnokey\r\n$1\r\n1\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Block>(result.state))
        << "BLPOP on empty list with BlockingManager should return Block state";
    EXPECT_TRUE(blocking_manager_.is_blocked(kClientFd));
}
