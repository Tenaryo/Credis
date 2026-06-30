#include <gtest/gtest.h>

#include "protocol/resp_codec.hpp"

using namespace credis::protocol;

TEST(ParseRespTest, SimpleStringReturnsError) {
    EXPECT_FALSE(parse_one("+OK\r\n").has_value());
}

TEST(ParseRespTest, BulkStringReturnsError) {
    EXPECT_FALSE(parse_one("$3\r\nfoo\r\n").has_value());
}

TEST(ParseRespTest, IntegerReturnsError) {
    EXPECT_FALSE(parse_one(":42\r\n").has_value());
}

TEST(ParseRespTest, ParsesArray) {
    auto result = parse_one("*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->args.size(), 2);
    EXPECT_EQ(result->args[0], "GET");
    EXPECT_EQ(result->args[1], "key");
}

TEST(ParseRespTest, ParsesEmptyArray) {
    auto result = parse_one("*0\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->args.empty());
}

TEST(ParseRespTest, NullBulkStringReturnsError) {
    auto result = parse_one("$-1\r\n");
    EXPECT_FALSE(result.has_value());
}

TEST(ParseRespTest, ParsesPingCommand) {
    auto result = parse_one("*1\r\n$4\r\nPING\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->args.size(), 1);
    EXPECT_EQ(result->args[0], "PING");
}

TEST(ParseRespTest, MalformedInputReturnsError) {
    auto result = parse_one("garbage");
    EXPECT_FALSE(result.has_value());
}

TEST(EncodeTest, SimpleString) {
    EXPECT_EQ(encode_simple_string("OK"), "+OK\r\n");
}

TEST(EncodeTest, BulkString) {
    EXPECT_EQ(encode_bulk_string("hello"), "$5\r\nhello\r\n");
}

TEST(EncodeTest, NullBulkString) {
    EXPECT_EQ(encode_null_bulk_string(), "$-1\r\n");
}

TEST(EncodeTest, PositiveInteger) {
    EXPECT_EQ(encode_integer(42), ":42\r\n");
}

TEST(EncodeTest, NegativeInteger) {
    EXPECT_EQ(encode_integer(-7), ":-7\r\n");
}

TEST(EncodeTest, Array) {
    EXPECT_EQ(encode_array({"GET", "key"}), "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n");
}

TEST(EncodeTest, Error) {
    EXPECT_EQ(encode_error("ERR message"), "-ERR message\r\n");
}

TEST(ParseOneTest, ParsesSingleCompleteCommand) {
    auto result = parse_one("*1\r\n$4\r\nPING\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->args.size(), 1);
    EXPECT_EQ(result->args[0], "PING");
    EXPECT_EQ(result->consumed, 14);
}

TEST(EncodeTest, NullArray) {
    EXPECT_EQ(encode_null_array(), "*-1\r\n");
}

TEST(EncodeTest, RawArray) {
    auto result = encode_raw_array({"+OK\r\n", ":1\r\n"});
    EXPECT_EQ(result, "*2\r\n+OK\r\n:1\r\n");
}

TEST(EncodeTest, Entries) {
    using namespace credis::store;
    StreamEntry e1;
    e1.id = "1-0";
    e1.fields = {{"f1", "v1"}};
    std::vector<StreamEntry> vec{e1};
    auto result = encode_entries(vec);
    EXPECT_TRUE(result.starts_with("*1\r\n"));
    EXPECT_NE(result.find("1-0"), std::string::npos);
    EXPECT_NE(result.find("f1"), std::string::npos);
}

TEST(EncodeTest, StreamEntries) {
    using namespace credis::store;
    StreamEntry e1;
    e1.id = "1-0";
    e1.fields = {{"f1", "v1"}};
    std::vector<StreamEntry> vec{e1};
    std::vector<std::pair<std::string, std::span<const StreamEntry>>> streams;
    streams.emplace_back("key", vec);
    auto result = encode_stream_entries(streams);
    EXPECT_TRUE(result.starts_with("*1\r\n"));
    EXPECT_NE(result.find("key"), std::string::npos);
    EXPECT_NE(result.find("1-0"), std::string::npos);
}

TEST(ParseRespTest, IncompleteArrayReturnsError) {
    auto result = parse_one("*3\r\n$3\r\nGET\r\n");
    EXPECT_FALSE(result.has_value());
}

TEST(ParseRespTest, NegativeCountArrayReturnsError) {
    auto result = parse_one("*-1\r\n");
    EXPECT_FALSE(result.has_value());
}
