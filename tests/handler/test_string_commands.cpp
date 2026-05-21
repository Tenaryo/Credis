#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerStringTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerStringTest, SetOk) {
    auto response = handler_.process("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    EXPECT_EQ(response, "+OK\r\n");
}

TEST_F(HandlerStringTest, GetAfterSet) {
    handler_.process("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    auto response = handler_.process("*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
    EXPECT_EQ(response, "$3\r\nbar\r\n");
}

TEST_F(HandlerStringTest, GetNonexistent) {
    auto response = handler_.process("*2\r\n$3\r\nGET\r\n$5\r\nnokey\r\n");
    EXPECT_EQ(response, "$-1\r\n");
}

TEST_F(HandlerStringTest, SetWithPxExpiry) {
    auto response = handler_.process("*5\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$2\r\nPX\r\n$3\r\n100\r\n");
    EXPECT_EQ(response, "+OK\r\n");

    auto get_resp = handler_.process("*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
    EXPECT_EQ(get_resp, "$3\r\nbar\r\n");
}

TEST_F(HandlerStringTest, SetWithPxExpires) {
    handler_.process("*5\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$2\r\nPX\r\n$2\r\n50\r\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto get_resp = handler_.process("*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
    EXPECT_EQ(get_resp, "$-1\r\n");
}

TEST_F(HandlerStringTest, IncrNumericString) {
    handler_.process("*3\r\n$3\r\nSET\r\n$3\r\ncnt\r\n$1\r\n5\r\n");
    auto response = handler_.process("*2\r\n$4\r\nINCR\r\n$3\r\ncnt\r\n");
    EXPECT_EQ(response, ":6\r\n");
}

TEST_F(HandlerStringTest, IncrNonexistent) {
    auto response = handler_.process("*2\r\n$4\r\nINCR\r\n$3\r\ncnt\r\n");
    EXPECT_EQ(response, ":1\r\n");
}

TEST_F(HandlerStringTest, Ping) {
    auto response = handler_.process("*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(response, "+PONG\r\n");
}

TEST_F(HandlerStringTest, Echo) {
    auto response = handler_.process("*2\r\n$4\r\nECHO\r\n$5\r\nhello\r\n");
    EXPECT_EQ(response, "$5\r\nhello\r\n");
}
