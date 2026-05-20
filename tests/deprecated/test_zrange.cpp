#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/server/server_config.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_zrange_basic_range() {
    Store store;
    store.zadd("zset_key", 100.0, "foo");
    store.zadd("zset_key", 100.0, "bar");
    store.zadd("zset_key", 20.0, "baz");
    store.zadd("zset_key", 30.1, "caz");
    store.zadd("zset_key", 40.2, "paz");

    auto result = store.zrange("zset_key", 2, 4);
    assert(result.size() == 3);
    assert(result[0] == "paz");
    assert(result[1] == "bar");
    assert(result[2] == "foo");

    auto all = store.zrange("zset_key", 0, 4);
    assert(all.size() == 5);
    assert(all[0] == "baz");
    assert(all[1] == "caz");
    assert(all[2] == "paz");
    assert(all[3] == "bar");
    assert(all[4] == "foo");

    std::cout << "\u2713 Test 1 passed: ZRANGE basic range query\n";
}

void test_zrange_nonexistent_key() {
    Store store;
    auto result = store.zrange("nonexistent", 0, 1);
    assert(result.empty());

    std::cout << "\u2713 Test 2 passed: ZRANGE returns empty for nonexistent key\n";
}

void test_zrange_start_exceeds_cardinality() {
    Store store;
    store.zadd("myset", 1.0, "a");
    store.zadd("myset", 2.0, "b");
    store.zadd("myset", 3.0, "c");

    auto result = store.zrange("myset", 5, 10);
    assert(result.empty());

    std::cout << "\u2713 Test 3 passed: ZRANGE returns empty when start >= cardinality\n";
}

void test_zrange_stop_exceeds_cardinality() {
    Store store;
    store.zadd("myset", 1.0, "a");
    store.zadd("myset", 2.0, "b");
    store.zadd("myset", 3.0, "c");

    auto result = store.zrange("myset", 0, 10);
    assert(result.size() == 3);
    assert(result[0] == "a");
    assert(result[1] == "b");
    assert(result[2] == "c");

    std::cout << "\u2713 Test 4 passed: ZRANGE clamps stop to last element\n";
}

void test_zrange_start_greater_than_stop() {
    Store store;
    store.zadd("myset", 1.0, "a");
    store.zadd("myset", 2.0, "b");

    auto result = store.zrange("myset", 2, 1);
    assert(result.empty());

    std::cout << "\u2713 Test 5 passed: ZRANGE returns empty when start > stop\n";
}

void test_zrange_start_equals_stop() {
    Store store;
    store.zadd("myset", 1.0, "a");
    store.zadd("myset", 2.0, "b");
    store.zadd("myset", 3.0, "c");

    auto result = store.zrange("myset", 1, 1);
    assert(result.size() == 1);
    assert(result[0] == "b");

    std::cout << "\u2713 Test 6 passed: ZRANGE returns single element when start == stop\n";
}

void test_zrange_single_element() {
    Store store;
    store.zadd("myset", 1.0, "only");

    auto result = store.zrange("myset", 0, 0);
    assert(result.size() == 1);
    assert(result[0] == "only");

    std::cout << "\u2713 Test 7 passed: ZRANGE single element set returns that element\n";
}

void test_zrange_negative_indexes() {
    Store store;
    store.zadd("zset_key", 100.0, "foo");
    store.zadd("zset_key", 100.0, "bar");
    store.zadd("zset_key", 20.0, "baz");
    store.zadd("zset_key", 30.1, "caz");
    store.zadd("zset_key", 40.2, "paz");

    auto neg_last2 = store.zrange("zset_key", -2, -1);
    assert(neg_last2.size() == 2);
    assert(neg_last2[0] == "bar");
    assert(neg_last2[1] == "foo");

    auto mixed = store.zrange("zset_key", 2, -1);
    assert(mixed.size() == 3);
    assert(mixed[0] == "paz");
    assert(mixed[1] == "bar");
    assert(mixed[2] == "foo");

    auto all_except_last2 = store.zrange("zset_key", 0, -3);
    assert(all_except_last2.size() == 3);
    assert(all_except_last2[0] == "baz");
    assert(all_except_last2[1] == "caz");
    assert(all_except_last2[2] == "paz");

    auto neg_oor = store.zrange("zset_key", -100, 0);
    assert(neg_oor.size() == 1);
    assert(neg_oor[0] == "baz");

    std::cout << "\u2713 Test 8 passed: ZRANGE negative index range query\n";
}

void test_zrange_handler_resp_protocol() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$5\r\n100.0\r\n$3\r\nfoo\r\n");
    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$5\r\n100.0\r\n$3\r\nbar\r\n");
    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$4\r\n20.0\r\n$3\r\nbaz\r\n");
    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$4\r\n30.1\r\n$3\r\ncaz\r\n");
    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$4\r\n40.2\r\n$3\r\npaz\r\n");

    auto response =
        handler.process("*4\r\n$6\r\nZRANGE\r\n$8\r\nzset_key\r\n$1\r\n2\r\n$1\r\n4\r\n");
    assert(response == "*3\r\n$3\r\npaz\r\n$3\r\nbar\r\n$3\r\nfoo\r\n");

    std::cout << "\u2713 Test 8 passed: ZRANGE handler returns correct RESP encoding\n";
}

void test_zrange_handler_nonexistent_key() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    auto response = handler.process("*4\r\n$6\r\nZRANGE\r\n$4\r\nnone\r\n$1\r\n0\r\n$1\r\n1\r\n");
    assert(response == "*0\r\n");

    std::cout << "\u2713 Test 9 passed: ZRANGE handler returns empty array for nonexistent key\n";
}

void test_zrange_handler_wrong_number_of_args() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    auto response = handler.process("*2\r\n$6\r\nZRANGE\r\n$3\r\nkey\r\n");
    assert(response == "-ERR wrong number of arguments for 'zrange' command\r\n");

    std::cout << "\u2713 Test 10 passed: ZRANGE handler rejects wrong number of arguments\n";
}

int main() {
    std::cout << "Running ZRANGE command tests...\n\n";

    test_zrange_basic_range();
    test_zrange_nonexistent_key();
    test_zrange_start_exceeds_cardinality();
    test_zrange_stop_exceeds_cardinality();
    test_zrange_start_greater_than_stop();
    test_zrange_start_equals_stop();
    test_zrange_single_element();
    test_zrange_negative_indexes();
    test_zrange_handler_resp_protocol();
    test_zrange_handler_nonexistent_key();
    test_zrange_handler_wrong_number_of_args();

    std::cout << "\n\u2713 All ZRANGE tests passed!\n";
    return 0;
}
