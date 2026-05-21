#include <gtest/gtest.h>

#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerZsetTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerZsetTest, ZaddNewMember) {
    auto response = handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n1.0\r\n$1\r\na\r\n");
    EXPECT_EQ(response, ":1\r\n");
}

TEST_F(HandlerZsetTest, ZaddUpdateMember) {
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n1.0\r\n$1\r\na\r\n");
    auto response = handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n2.0\r\n$1\r\na\r\n");
    EXPECT_EQ(response, ":0\r\n");
}

TEST_F(HandlerZsetTest, Zrank) {
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n1.0\r\n$1\r\na\r\n");
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n2.0\r\n$1\r\nb\r\n");
    auto response = handler_.process("*3\r\n$5\r\nZRANK\r\n$4\r\nzset\r\n$1\r\na\r\n");
    EXPECT_EQ(response, ":0\r\n");
}

TEST_F(HandlerZsetTest, Zrange) {
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n1.0\r\n$1\r\na\r\n");
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n2.0\r\n$1\r\nb\r\n");
    auto response = handler_.process("*4\r\n$6\r\nZRANGE\r\n$4\r\nzset\r\n$1\r\n0\r\n$2\r\n-1\r\n");
    EXPECT_EQ(response, "*2\r\n$1\r\na\r\n$1\r\nb\r\n");
}

TEST_F(HandlerZsetTest, Zcard) {
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n1.0\r\n$1\r\na\r\n");
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n2.0\r\n$1\r\nb\r\n");
    auto response = handler_.process("*2\r\n$5\r\nZCARD\r\n$4\r\nzset\r\n");
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(HandlerZsetTest, Zscore) {
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n1.5\r\n$1\r\na\r\n");
    auto response = handler_.process("*3\r\n$6\r\nZSCORE\r\n$4\r\nzset\r\n$1\r\na\r\n");
    EXPECT_EQ(response, "$3\r\n1.5\r\n");
}

TEST_F(HandlerZsetTest, Zrem) {
    handler_.process("*4\r\n$4\r\nZADD\r\n$4\r\nzset\r\n$3\r\n1.0\r\n$1\r\na\r\n");
    auto response = handler_.process("*3\r\n$4\r\nZREM\r\n$4\r\nzset\r\n$1\r\na\r\n");
    EXPECT_EQ(response, ":1\r\n");
}
