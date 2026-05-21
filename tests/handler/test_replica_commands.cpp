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
    EXPECT_TRUE(response.starts_with("+FULLRESYNC 8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb 0\r\n"));
}
