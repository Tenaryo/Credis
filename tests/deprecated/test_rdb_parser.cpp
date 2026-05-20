#include "../src/rdb/rdb_parser.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

static std::vector<uint8_t> make_rdb(const std::vector<uint8_t>& db_section) {
    std::vector<uint8_t> data;

    // Header: REDIS0011
    for (auto c : "REDIS0011")
        data.push_back(static_cast<uint8_t>(c));

    // Metadata: redis-ver 7.2.0
    data.push_back(0xFA);
    data.push_back(0x09);
    for (auto c : "redis-ver")
        data.push_back(static_cast<uint8_t>(c));
    data.push_back(0x05);
    for (auto c : "7.2.0")
        data.push_back(static_cast<uint8_t>(c));

    // Database section
    data.insert(data.end(), db_section.begin(), db_section.end());

    // EOF marker + dummy checksum (8 bytes)
    data.push_back(0xFF);
    for (int i = 0; i < 8; ++i)
        data.push_back(0x00);

    return data;
}

void test_parse_single_string_key() {
    std::vector<uint8_t> db = {
        0xFE,
        0x00, // SELECTDB 0
        0xFB,
        0x01,
        0x00, // hash table sizes: 1 key, 0 expires
        0x00, // value type = string
        0x03,
        'f',
        'o',
        'o', // key "foo"
        0x03,
        'b',
        'a',
        'r', // value "bar"
    };

    auto rdb = make_rdb(db);
    auto result = RdbParser::parse(rdb);

    assert(result.size() == 1);
    assert(result.contains("foo"));
    assert(std::holds_alternative<Redis::String>(result["foo"].value));
    assert(std::get<Redis::String>(result["foo"].value) == "bar");
    assert(!result["foo"].expire_ms.has_value());

    std::cout << "\u2713 Test 1 passed: parse single string key-value\n";
}

void test_parse_multiple_keys() {
    std::vector<uint8_t> db = {
        0xFE, 0x00,                                      // SELECTDB 0
        0xFB, 0x02, 0x00,                                // 2 keys, 0 expires
        0x00, 0x03, 'f',  'o', 'o', 0x03, 'b', 'a', 'r', // foo=bar
        0x00, 0x03, 'b',  'a', 'z', 0x03, 'q', 'u', 'x', // baz=qux
    };

    auto rdb = make_rdb(db);
    auto result = RdbParser::parse(rdb);

    assert(result.size() == 2);
    assert(result.contains("foo"));
    assert(result.contains("baz"));
    assert(std::get<Redis::String>(result["foo"].value) == "bar");
    assert(std::get<Redis::String>(result["baz"].value) == "qux");

    std::cout << "\u2713 Test 2 passed: parse multiple string keys\n";
}

void test_parse_expire_seconds() {
    std::vector<uint8_t> db = {
        0xFE, 0x00,             // SELECTDB 0
        0xFB, 0x01, 0x01,       // 1 key, 1 expire
        0xFD,                   // expire in seconds
        0x52, 0xED, 0x2A, 0x66, // timestamp 1714089298 (little-endian)
        0x00,                   // value type = string
        0x03, 'b',  'a',  'z',  // key "baz"
        0x03, 'q',  'u',  'x',  // value "qux"
    };

    auto rdb = make_rdb(db);
    auto result = RdbParser::parse(rdb);

    assert(result.size() == 1);
    assert(result.contains("baz"));
    assert(std::get<Redis::String>(result["baz"].value) == "qux");
    assert(result["baz"].expire_ms.has_value());

    std::cout << "\u2713 Test 3 passed: parse key with second expiry\n";
}

void test_parse_expire_milliseconds() {
    std::vector<uint8_t> db = {
        0xFE, 0x00,                                     // SELECTDB 0
        0xFB, 0x01, 0x01,                               // 1 key, 1 expire
        0xFC,                                           // expire in milliseconds
        0x15, 0x72, 0xE7, 0x07, 0x8F, 0x01, 0x00, 0x00, // timestamp 1713824559637
        0x00,                                           // value type = string
        0x03, 'f',  'o',  'o',                          // key "foo"
        0x03, 'b',  'a',  'r',                          // value "bar"
    };

    auto rdb = make_rdb(db);
    auto result = RdbParser::parse(rdb);

    assert(result.size() == 1);
    assert(result.contains("foo"));
    assert(std::get<Redis::String>(result["foo"].value) == "bar");
    assert(result["foo"].expire_ms.has_value());

    std::cout << "\u2713 Test 4 passed: parse key with millisecond expiry\n";
}

void test_parse_integer_encoded_string_8bit() {
    // C0 7B → 8-bit integer 123 → string "123"
    std::vector<uint8_t> db = {
        0xFE,
        0x00,
        0xFB,
        0x01,
        0x00,
        0x00, // value type = string
        0x03,
        'k',
        'e',
        'y', // key "key"
        0xC0,
        0x7B, // value: 8-bit integer 123
    };

    auto rdb = make_rdb(db);
    auto result = RdbParser::parse(rdb);

    assert(result.size() == 1);
    assert(std::get<Redis::String>(result["key"].value) == "123");

    std::cout << "\u2713 Test 5 passed: parse 8-bit integer encoded string\n";
}

void test_parse_integer_encoded_string_16bit() {
    // C1 39 30 → 16-bit little-endian → 0x3039 = 12345 → "12345"
    std::vector<uint8_t> db = {
        0xFE,
        0x00,
        0xFB,
        0x01,
        0x00,
        0x00,
        0x03,
        'k',
        'e',
        'y',
        0xC1,
        0x39,
        0x30,
    };

    auto rdb = make_rdb(db);
    auto result = RdbParser::parse(rdb);

    assert(result.size() == 1);
    assert(std::get<Redis::String>(result["key"].value) == "12345");

    std::cout << "\u2713 Test 6 passed: parse 16-bit integer encoded string\n";
}

void test_parse_integer_encoded_string_32bit() {
    // C2 87 D6 12 00 → 32-bit little-endian → 0x0012D687 = 1234567 → "1234567"
    std::vector<uint8_t> db = {
        0xFE,
        0x00,
        0xFB,
        0x01,
        0x00,
        0x00,
        0x03,
        'k',
        'e',
        'y',
        0xC2,
        0x87,
        0xD6,
        0x12,
        0x00,
    };

    auto rdb = make_rdb(db);
    auto result = RdbParser::parse(rdb);

    assert(result.size() == 1);
    assert(std::get<Redis::String>(result["key"].value) == "1234567");

    std::cout << "\u2713 Test 7 passed: parse 32-bit integer encoded string\n";
}

void test_parse_nonexistent_file() {
    auto result = RdbParser::load_file("/nonexistent/path/dump.rdb");
    assert(result.empty());

    std::cout << "\u2713 Test 8 passed: nonexistent file returns empty\n";
}

int main() {
    std::cout << "Running RDB parser tests...\n\n";

    test_parse_single_string_key();
    test_parse_multiple_keys();
    test_parse_expire_seconds();
    test_parse_expire_milliseconds();
    test_parse_integer_encoded_string_8bit();
    test_parse_integer_encoded_string_16bit();
    test_parse_integer_encoded_string_32bit();
    test_parse_nonexistent_file();

    std::cout << "\n\u2713 All RDB parser tests passed!\n";
    return 0;
}
