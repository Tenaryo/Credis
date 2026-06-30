#include <gtest/gtest.h>

#include <string>
#include <variant>

#include "blocking_manager/blocking_manager.hpp"
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

TEST_F(HandlerStreamTest, XaddExplicitId) {
    auto response = handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n5-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    EXPECT_EQ(response, "$3\r\n5-0\r\n");
}

TEST_F(HandlerStreamTest, XaddAutoSeq) {
    auto response = handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n5-*\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    EXPECT_TRUE(response.starts_with("$"));
    EXPECT_NE(response.find("5-"), std::string::npos);
}

TEST_F(HandlerStreamTest, XaddWrongType) {
    handler_.process("*3\r\n$3\r\nSET\r\n$6\r\nstream\r\n$5\r\nhello\r\n");
    auto response = handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$1\r\n*\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    EXPECT_TRUE(response.starts_with("-WRONGTYPE"));
}

TEST_F(HandlerStreamTest, XaddWrongArgCount) {
    auto response = handler_.process("*4\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$1\r\n*\r\n$2\r\nf1\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerStreamTest, XaddZeroZeroId) {
    auto response = handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n0-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerStreamTest, XaddIdNotGreater) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n5-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    auto response = handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n3-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerStreamTest, XrangeAllEntries) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n2-0\r\n$2\r\nf1\r\n$2\r\nv2\r\n");

    auto response = handler_.process("*4\r\n$6\r\nXRANGE\r\n$6\r\nstream\r\n$1\r\n-\r\n$1\r\n+\r\n");
    EXPECT_TRUE(response.starts_with("*2\r\n"));
    EXPECT_NE(response.find("1-0"), std::string::npos);
    EXPECT_NE(response.find("2-0"), std::string::npos);
}

TEST_F(HandlerStreamTest, XrangeEmptyStream) {
    auto response = handler_.process("*4\r\n$6\r\nXRANGE\r\n$5\r\nnokey\r\n$1\r\n-\r\n$1\r\n+\r\n");
    EXPECT_EQ(response, "*0\r\n");
}

TEST_F(HandlerStreamTest, XreadFromZero) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");

    auto response = handler_.process("*4\r\n$5\r\nXREAD\r\n$7\r\nstreams\r\n$6\r\nstream\r\n$3\r\n0-0\r\n");
    EXPECT_TRUE(response.starts_with("*1\r\n"));
    EXPECT_NE(response.find("1-0"), std::string::npos);
}

TEST_F(HandlerStreamTest, XreadNoData) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$6\r\nstream\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");

    auto response = handler_.process("*4\r\n$5\r\nXREAD\r\n$7\r\nstreams\r\n$6\r\nstream\r\n$3\r\n2-0\r\n");
    EXPECT_TRUE(response.starts_with("*1\r\n"));
    EXPECT_EQ(response.find("1-0"), std::string::npos);
}

TEST_F(HandlerStreamTest, XreadSyntaxErrorNoStreams) {
    auto response = handler_.process("*2\r\n$5\r\nXREAD\r\n$6\r\nstream\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerStreamTest, XreadWrongArgCount) {
    auto response = handler_.process("*5\r\n$5\r\nXREAD\r\n$7\r\nSTREAMS\r\n$2\r\nk1\r\n$2\r\nk2\r\n$3\r\n0-0\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerStreamTest, XreadMultiStream) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$2\r\ns1\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    handler_.process("*5\r\n$4\r\nXADD\r\n$2\r\ns2\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");

    auto response
        = handler_.process("*6\r\n$5\r\nXREAD\r\n$7\r\nSTREAMS\r\n$2\r\ns1\r\n$2\r\ns2\r\n$3\r\n0-0\r\n$3\r\n0-0\r\n");
    EXPECT_TRUE(response.starts_with("*2\r\n"));
}

class HandlerStreamBlockingTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
    credis::blocking::BlockingManager blocking_manager_;

    void SetUp() override {
        handler_.set_blocking_manager(blocking_manager_);
    }
};

TEST_F(HandlerStreamBlockingTest, XreadBlockDataAvailable) {
    constexpr int kFd = 10;
    auto result = handler_.process_with_fd(
        kFd, "*5\r\n$4\r\nXADD\r\n$2\r\ns1\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));

    result = handler_.process_with_fd(kFd,
                                      "*6\r\n$5\r\nXREAD\r\n$5\r\nBLOCK\r\n$4\r\n1000\r\n$7\r\nSTREAMS\r\n$2\r\ns1\r\n"
                                      "$3\r\n0-0\r\n",
                                      nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_NE(std::get<ProcessResult::Normal>(result.state).response.find("1-0"), std::string::npos);
}

TEST_F(HandlerStreamBlockingTest, XreadBlockNoDataBlocks) {
    constexpr int kFd = 11;
    auto result = handler_.process_with_fd(kFd,
                                           "*6\r\n$5\r\nXREAD\r\n$5\r\nBLOCK\r\n$5\r\n10000\r\n$7\r\nSTREAMS\r\n$2\r\n"
                                           "s1\r\n$3\r\n0-0\r\n",
                                           nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Block>(result.state));
    EXPECT_TRUE(blocking_manager_.is_blocked(kFd));
}

TEST_F(HandlerStreamBlockingTest, XreadBlockDollarId) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$2\r\ns1\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");

    constexpr int kFd = 12;
    auto result = handler_.process_with_fd(kFd,
                                           "*6\r\n$5\r\nXREAD\r\n$5\r\nBLOCK\r\n$5\r\n10000\r\n$7\r\nSTREAMS\r\n$2\r\n"
                                           "s1\r\n$1\r\n$\r\n",
                                           nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Block>(result.state));
}

TEST_F(HandlerStreamBlockingTest, XreadBlockSyntaxErrorNoStreams) {
    constexpr int kFd = 13;
    auto result
        = handler_.process_with_fd(kFd, "*4\r\n$5\r\nXREAD\r\n$5\r\nBLOCK\r\n$4\r\n1000\r\n$6\r\nstream\r\n", nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(std::get<ProcessResult::Normal>(result.state).response.starts_with("-ERR"));
}

TEST_F(HandlerStreamBlockingTest, XreadBlockInvalidTimeout) {
    constexpr int kFd = 14;
    auto result = handler_.process_with_fd(kFd,
                                           "*6\r\n$5\r\nXREAD\r\n$5\r\nBLOCK\r\n$3\r\nabc\r\n$7\r\nSTREAMS\r\n$2\r\n"
                                           "s1\r\n$3\r\n0-0\r\n",
                                           nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(std::get<ProcessResult::Normal>(result.state).response.starts_with("-ERR"));
}

TEST_F(HandlerStreamBlockingTest, XreadBlockMultiStreamError) {
    handler_.process("*5\r\n$4\r\nXADD\r\n$2\r\ns1\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");
    handler_.process("*5\r\n$4\r\nXADD\r\n$2\r\ns2\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n");

    constexpr int kFd = 15;
    auto result = handler_.process_with_fd(
        kFd,
        "*8\r\n$5\r\nXREAD\r\n$5\r\nBLOCK\r\n$4\r\n1000\r\n$7\r\nSTREAMS\r\n$2\r\ns1\r\n$2\r\ns2\r\n$3\r\n0-0\r\n"
        "$3\r\n0-0\r\n",
        nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_TRUE(std::get<ProcessResult::Normal>(result.state).response.starts_with("-ERR"));
}

TEST_F(HandlerStreamBlockingTest, XaddWakesBlockedReader) {
    constexpr int kBlockedFd = 16;
    constexpr int kXaddFd = 99;

    auto result = handler_.process_with_fd(kBlockedFd,
                                           "*6\r\n$5\r\nXREAD\r\n$5\r\nBLOCK\r\n$5\r\n10000\r\n$7\r\nSTREAMS\r\n$2\r\n"
                                           "s2\r\n$3\r\n0-0\r\n",
                                           nullptr);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Block>(result.state));

    std::string delivered_to_blocked;
    auto send_cb = [&delivered_to_blocked](int fd, const std::string& data) {
        if (fd == kBlockedFd) {
            delivered_to_blocked = data;
        }
    };

    result = handler_.process_with_fd(
        kXaddFd, "*5\r\n$4\r\nXADD\r\n$2\r\ns2\r\n$3\r\n1-0\r\n$2\r\nf1\r\n$2\r\nv1\r\n", send_cb);
    ASSERT_TRUE(std::holds_alternative<ProcessResult::Normal>(result.state));
    EXPECT_NE(delivered_to_blocked.find("1-0"), std::string::npos);
    EXPECT_NE(delivered_to_blocked.find("f1"), std::string::npos);
}
