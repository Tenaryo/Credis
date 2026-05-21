#include <gtest/gtest.h>

#include "handler/command_handler.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerPubsubTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
    credis::pubsub::PubSubManager pubsub_;

    void SetUp() override {
        handler_.set_pubsub_manager(&pubsub_);
    }
};

TEST_F(HandlerPubsubTest, SubscribeChannel) {
    constexpr int kClientFd = 1;
    auto result = handler_.process_with_fd(kClientFd, "*2\r\n$9\r\nsubscribe\r\n$7\r\nchannel\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    auto& resp = std::get<ProcessResult::Normal>(result.state).response;
    EXPECT_NE(resp.find("subscribe"), std::string::npos);
    EXPECT_NE(resp.find("channel"), std::string::npos);
    EXPECT_NE(resp.find(":1"), std::string::npos);
}

TEST_F(HandlerPubsubTest, PublishNoSubscribers) {
    constexpr int kClientFd = 10;
    auto result = handler_.process_with_fd(kClientFd, "*3\r\n$7\r\nPUBLISH\r\n$7\r\nchannel\r\n$3\r\nmsg\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<ProcessResult::Normal>(result.state).response, ":0\r\n");
}

TEST_F(HandlerPubsubTest, UnsubscribeChannel) {
    constexpr int kClientFd = 1;
    handler_.process_with_fd(kClientFd, "*2\r\n$9\r\nsubscribe\r\n$7\r\nchannel\r\n", nullptr);

    auto result = handler_.process_with_fd(kClientFd, "*2\r\n$11\r\nunsubscribe\r\n$7\r\nchannel\r\n", nullptr);

    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    auto& resp = std::get<ProcessResult::Normal>(result.state).response;
    EXPECT_NE(resp.find("unsubscribe"), std::string::npos);
    EXPECT_NE(resp.find("channel"), std::string::npos);
}
