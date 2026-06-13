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

TEST(ConfigGetAofOverrides, ReturnsFlagValues) {
    credis::store::Store store;
    credis::server::ServerConfig config;
    config.appendonly = "yes";
    config.appenddirname = "mydir";
    config.appendfilename = "myfile.aof";
    config.appendfsync = "always";
    CommandHandler handler(store, config);

    auto r1 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$10\r\nappendonly\r\n");
    EXPECT_EQ(r1, "*2\r\n$10\r\nappendonly\r\n$3\r\nyes\r\n");

    auto r2 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$13\r\nappenddirname\r\n");
    EXPECT_EQ(r2, "*2\r\n$13\r\nappenddirname\r\n$5\r\nmydir\r\n");

    auto r3 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$14\r\nappendfilename\r\n");
    EXPECT_EQ(r3, "*2\r\n$14\r\nappendfilename\r\n$10\r\nmyfile.aof\r\n");

    auto r4 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$11\r\nappendfsync\r\n");
    EXPECT_EQ(r4, "*2\r\n$11\r\nappendfsync\r\n$6\r\nalways\r\n");
}
