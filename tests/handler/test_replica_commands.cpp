#include <gtest/gtest.h>

#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerReplicaTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerReplicaTest, ReplconfListeningPort) {
    auto response = handler_.process("*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n6380\r\n");
    EXPECT_EQ(response, "+OK\r\n");
}

TEST_F(HandlerReplicaTest, ReplconfCapaPsync2) {
    auto response = handler_.process("*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n");
    EXPECT_EQ(response, "+OK\r\n");
}

TEST_F(HandlerReplicaTest, Psync) {
    auto response = handler_.process("*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n");
    EXPECT_TRUE(response.starts_with("+FULLRESYNC"));
    EXPECT_TRUE(response.find("$88\r\n") != std::string::npos);
}

TEST_F(HandlerReplicaTest, ReplconfGetack) {
    auto response = handler_.process("*2\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n");
    EXPECT_TRUE(response.starts_with("*3\r\n"));
    EXPECT_NE(response.find("REPLCONF"), std::string::npos);
    EXPECT_NE(response.find("ACK"), std::string::npos);
}

TEST_F(HandlerReplicaTest, WaitWrongArgCount) {
    auto response = handler_.process("*2\r\n$4\r\nWAIT\r\n$1\r\n1\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerReplicaTest, WaitInvalidArgs) {
    auto response = handler_.process("*3\r\n$4\r\nWAIT\r\n$3\r\nabc\r\n$1\r\n0\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}
