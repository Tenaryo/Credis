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

TEST_F(HandlerStringTest, PipelineMultipleCommands) {
    auto input = "*3\r\n$3\r\nSET\r\n$1\r\na\r\n$1\r\n1\r\n"
                 "*3\r\n$3\r\nSET\r\n$1\r\nb\r\n$1\r\n2\r\n"
                 "*3\r\n$3\r\nSET\r\n$1\r\nc\r\n$1\r\n3\r\n";

    std::vector<std::string> sent;
    auto send_fn = [&](int, const std::string& msg) { sent.push_back(msg); };

    auto result = handler_.process_with_fd(1, input, send_fn);

    ASSERT_EQ(sent.size(), 3);
    EXPECT_EQ(sent[0], "+OK\r\n");
    EXPECT_EQ(sent[1], "+OK\r\n");
    EXPECT_EQ(sent[2], "+OK\r\n");
    EXPECT_GT(result.consumed, 0u);

    EXPECT_EQ(handler_.process("*2\r\n$3\r\nGET\r\n$1\r\na\r\n"), "$1\r\n1\r\n");
    EXPECT_EQ(handler_.process("*2\r\n$3\r\nGET\r\n$1\r\nb\r\n"), "$1\r\n2\r\n");
    EXPECT_EQ(handler_.process("*2\r\n$3\r\nGET\r\n$1\r\nc\r\n"), "$1\r\n3\r\n");
}

TEST_F(HandlerStringTest, PipelineConsumePartial) {
    std::string_view input = "*3\r\n$3\r\nSET\r\n$1\r\na\r\n$1\r\n1\r\n"
                             "*3\r\n$3\r\nSET\r\n$1\r\nb\r\n$1\r\n2\r\n";

    std::vector<std::string> sent;
    auto send_fn = [&](int, const std::string& msg) { sent.push_back(msg); };

    auto result = handler_.process_with_fd(1, input, send_fn);

    ASSERT_EQ(sent.size(), 2);
    EXPECT_EQ(result.consumed, input.size());
    EXPECT_EQ(handler_.process("*2\r\n$3\r\nGET\r\n$1\r\na\r\n"), "$1\r\n1\r\n");
    EXPECT_EQ(handler_.process("*2\r\n$3\r\nGET\r\n$1\r\nb\r\n"), "$1\r\n2\r\n");
}
