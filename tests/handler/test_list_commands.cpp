#include <gtest/gtest.h>

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

TEST_F(HandlerListTest, Llen) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    auto response = handler_.process("*2\r\n$4\r\nLLEN\r\n$4\r\nlist\r\n");
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(HandlerListTest, Lrange) {
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\na\r\n");
    handler_.process("*3\r\n$5\r\nRPUSH\r\n$4\r\nlist\r\n$1\r\nb\r\n");
    auto response = handler_.process("*4\r\n$6\r\nLRANGE\r\n$4\r\nlist\r\n$1\r\n0\r\n$2\r\n-1\r\n");
    EXPECT_EQ(response, "*2\r\n$1\r\na\r\n$1\r\nb\r\n");
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
