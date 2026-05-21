#include <gtest/gtest.h>

#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerServerTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerServerTest, InfoWithoutArgs) {
    auto response = handler_.process("*1\r\n$4\r\nINFO\r\n");
    EXPECT_TRUE(response.starts_with("$"));
    EXPECT_NE(response.find("role:master"), std::string::npos);
}

TEST_F(HandlerServerTest, InfoReplication) {
    auto response = handler_.process("*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n");
    EXPECT_TRUE(response.starts_with("$"));
    EXPECT_NE(response.find("# Replication"), std::string::npos);
    EXPECT_NE(response.find("role:master"), std::string::npos);
}
