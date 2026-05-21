#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "rdb/rdb_parser.hpp"

using namespace credis::rdb;

static std::vector<std::uint8_t> make_rdb(const std::vector<std::uint8_t>& db_section) {
    std::vector<std::uint8_t> data;
    for (auto c : "REDIS0011")
        data.push_back(static_cast<std::uint8_t>(c));
    data.push_back(0xFA);
    data.push_back(0x09);
    for (auto c : "redis-ver")
        data.push_back(static_cast<std::uint8_t>(c));
    data.push_back(0x05);
    for (auto c : "7.2.0")
        data.push_back(static_cast<std::uint8_t>(c));
    data.insert(data.end(), db_section.begin(), db_section.end());
    data.push_back(0xFF);
    for (int i = 0; i < 8; ++i)
        data.push_back(0x00);
    return data;
}

TEST(RdbTest, ParseSingleStringKeyValue) {
    // db_section: FE 00 FB 01 01 | type 00 | key "foo" | value "bar"
    auto db_section = std::vector<std::uint8_t>{
        0xFE,
        0x00,
        0xFB,
        0x01,
        0x01,
        0x00,
        0x03,
        'f',
        'o',
        'o',
        0x03,
        'b',
        'a',
        'r',
    };

    auto data = make_rdb(db_section);
    auto result = parse_rdb(data);

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result.contains("foo"));

    auto& entry = result.at("foo");
    auto* str_val = std::get_if<credis::store::String>(&entry.value);
    ASSERT_NE(str_val, nullptr);
    EXPECT_EQ(*str_val, "bar");
    EXPECT_FALSE(entry.expire_ms.has_value());
}

TEST(RdbTest, ParseMultipleStringKeys) {
    // foo="bar", baz="qux"
    auto db_section = std::vector<std::uint8_t>{
        0xFE, 0x00, 0xFB, 0x02, 0x02, 0x00, 0x03, 'f',  'o', 'o', 0x03, 'b',
        'a',  'r',  0x00, 0x03, 'b',  'a',  'z',  0x03, 'q', 'u', 'x',
    };

    auto data = make_rdb(db_section);
    auto result = parse_rdb(data);

    ASSERT_EQ(result.size(), 2);

    ASSERT_TRUE(result.contains("foo"));
    auto* foo_val = std::get_if<credis::store::String>(&result.at("foo").value);
    ASSERT_NE(foo_val, nullptr);
    EXPECT_EQ(*foo_val, "bar");

    ASSERT_TRUE(result.contains("baz"));
    auto* baz_val = std::get_if<credis::store::String>(&result.at("baz").value);
    ASSERT_NE(baz_val, nullptr);
    EXPECT_EQ(*baz_val, "qux");
}

TEST(RdbTest, ParseKeyWithExpireSeconds) {
    // FD + 4-byte LE seconds (1000) = 1000000 ms
    auto db_section = std::vector<std::uint8_t>{
        0xFE, 0x00, 0xFB, 0x01, 0x01, 0xFD, 0xE8, 0x03, 0x00, 0x00, 0x00, 0x03, 'f', 'o', 'o', 0x03, 'b', 'a', 'r',
    };

    auto data = make_rdb(db_section);
    auto result = parse_rdb(data);

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result.contains("foo"));

    auto& entry = result.at("foo");
    auto* str_val = std::get_if<credis::store::String>(&entry.value);
    ASSERT_NE(str_val, nullptr);
    EXPECT_EQ(*str_val, "bar");
    ASSERT_TRUE(entry.expire_ms.has_value());
    EXPECT_EQ(entry.expire_ms.value(), 1000000);
}

TEST(RdbTest, ParseKeyWithExpireMilliseconds) {
    // FC + 8-byte LE milliseconds (5000)
    auto db_section = std::vector<std::uint8_t>{
        0xFE, 0x00, 0xFB, 0x01, 0x01, 0xFC, 0x88, 0x13, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 'f',  'o',  'o',  0x03, 'b',  'a',  'r',
    };

    auto data = make_rdb(db_section);
    auto result = parse_rdb(data);

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result.contains("foo"));

    auto& entry = result.at("foo");
    auto* str_val = std::get_if<credis::store::String>(&entry.value);
    ASSERT_NE(str_val, nullptr);
    EXPECT_EQ(*str_val, "bar");
    ASSERT_TRUE(entry.expire_ms.has_value());
    EXPECT_EQ(entry.expire_ms.value(), 5000);
}

TEST(RdbTest, Parse8BitIntEncodedString) {
    // value: C0 2A -> "42"
    auto db_section = std::vector<std::uint8_t>{
        0xFE,
        0x00,
        0xFB,
        0x01,
        0x01,
        0x00,
        0x03,
        'f',
        'o',
        'o',
        0xC0,
        0x2A,
    };

    auto data = make_rdb(db_section);
    auto result = parse_rdb(data);

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result.contains("foo"));

    auto& entry = result.at("foo");
    auto* str_val = std::get_if<credis::store::String>(&entry.value);
    ASSERT_NE(str_val, nullptr);
    EXPECT_EQ(*str_val, "42");
}

TEST(RdbTest, Parse16BitIntEncodedString) {
    // value: C1 E8 03 -> "1000" (0x3E8 LE)
    auto db_section = std::vector<std::uint8_t>{
        0xFE,
        0x00,
        0xFB,
        0x01,
        0x01,
        0x00,
        0x03,
        'f',
        'o',
        'o',
        0xC1,
        0xE8,
        0x03,
    };

    auto data = make_rdb(db_section);
    auto result = parse_rdb(data);

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result.contains("foo"));

    auto& entry = result.at("foo");
    auto* str_val = std::get_if<credis::store::String>(&entry.value);
    ASSERT_NE(str_val, nullptr);
    EXPECT_EQ(*str_val, "1000");
}

TEST(RdbTest, Parse32BitIntEncodedString) {
    // value: C2 40 E2 01 00 -> "123456" (0x1E240 LE)
    auto db_section = std::vector<std::uint8_t>{
        0xFE,
        0x00,
        0xFB,
        0x01,
        0x01,
        0x00,
        0x03,
        'f',
        'o',
        'o',
        0xC2,
        0x40,
        0xE2,
        0x01,
        0x00,
    };

    auto data = make_rdb(db_section);
    auto result = parse_rdb(data);

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result.contains("foo"));

    auto& entry = result.at("foo");
    auto* str_val = std::get_if<credis::store::String>(&entry.value);
    ASSERT_NE(str_val, nullptr);
    EXPECT_EQ(*str_val, "123456");
}

TEST(RdbTest, LoadRdbFileNonexistentPath) {
    auto result = load_rdb_file("/nonexistent/rdb/path.dump");
    EXPECT_TRUE(result.empty());
}
