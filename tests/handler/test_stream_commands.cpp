#include <gtest/gtest.h>

#include <string>

#include "handler/command_handler.hpp"
#include "protocol/resp_codec.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerStreamTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerStreamTest, XaddAutoId) {
    auto response = handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$1\r\n*\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    EXPECT_TRUE(response.starts_with("$"));
    EXPECT_NE(response.find('-'), std::string::npos);
}

TEST_F(HandlerStreamTest, XrangeAllEntries) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n2-0\r\n$2\r\nf1\r\n$2\r\nv2\r\n");

    auto response = handler_.process("*4\r\n$6\r\nXRANGE\r\n$6\r\nstream\r\n$1\r\n-\r\n$1\r\n+\r\n");
    EXPECT_TRUE(response.starts_with("*2\r\n"));
    EXPECT_NE(response.find("1-0"), std::string::npos);
    EXPECT_NE(response.find("2-0"), std::string::npos);
}

TEST_F(HandlerStreamTest, XreadFromZero) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");

    auto response = handler_.process("*4\r\n$5\r\nXREAD\r\n$7\r\nstreams\r\n$6\r\nstream\r\n$3\r\n0-0\r\n");
    EXPECT_TRUE(response.starts_with("*1\r\n"));
    EXPECT_NE(response.find("1-0"), std::string::npos);
}
