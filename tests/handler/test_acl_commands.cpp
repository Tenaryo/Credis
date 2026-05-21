#include <gtest/gtest.h>

#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerAclTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerAclTest, AclWhoami) {
    auto response = handler_.process("*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n");
    EXPECT_EQ(response, "$7\r\ndefault\r\n");
}

TEST_F(HandlerAclTest, AuthWrongPassword) {
    handler_.process("*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n");
    auto response = handler_.process("*3\r\n$4\r\nAUTH\r\n$7\r\ndefault\r\n$12\r\nwrongpassword\r\n");
    EXPECT_TRUE(response.starts_with("-WRONGPASS"));
}

TEST_F(HandlerAclTest, AclSetuserNewUser) {
    auto response
        = handler_.process("*5\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\nnewuser\r\n$2\r\non\r\n$10\r\n>password\r\n");
    EXPECT_EQ(response, "+OK\r\n");
}
