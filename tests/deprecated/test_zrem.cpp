#include "../src/handler/command_handler.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_zrem_existing_member() {
    Store store;

    store.zadd("zset_key", 80.5, "foo");
    store.zadd("zset_key", 50.3, "baz");
    store.zadd("zset_key", 80.5, "bar");

    auto removed = store.zrem("zset_key", "baz");
    assert(removed == 1);
    assert(store.zcard("zset_key") == 2);

    auto members = store.zrange("zset_key", 0, -1);
    assert(members.size() == 2);
    assert(members[0] == "bar");
    assert(members[1] == "foo");

    std::cout << "✓ Test 1 passed: ZREM removes existing member and returns 1\n";
}

void test_zrem_nonexistent_key_and_member() {
    Store store;

    auto removed_missing_key = store.zrem("missing_key", "member1");
    assert(removed_missing_key == 0);

    store.zadd("zset_key", 10.0, "member1");
    auto removed_missing_member = store.zrem("zset_key", "nonexistent");
    assert(removed_missing_member == 0);
    assert(store.zcard("zset_key") == 1);

    std::cout << "✓ Test 2 passed: ZREM returns 0 for nonexistent key/member\n";
}

void test_zrem_via_handler() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$4\r\n80.5\r\n$3\r\nfoo\r\n");
    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$4\r\n50.3\r\n$3\r\nbaz\r\n");
    handler.process("*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$4\r\n80.5\r\n$3\r\nbar\r\n");

    auto rem_response = handler.process("*3\r\n$4\r\nZREM\r\n$8\r\nzset_key\r\n$3\r\nbaz\r\n");
    assert(rem_response == ":1\r\n");

    auto range_response =
        handler.process("*4\r\n$6\r\nZRANGE\r\n$8\r\nzset_key\r\n$1\r\n0\r\n$2\r\n-1\r\n");
    assert(range_response == "*2\r\n$3\r\nbar\r\n$3\r\nfoo\r\n");

    auto rem_missing =
        handler.process("*3\r\n$4\r\nZREM\r\n$8\r\nzset_key\r\n$10\r\nno_member\r\n");
    assert(rem_missing == ":0\r\n");

    std::cout << "✓ Test 3 passed: ZREM via handler end-to-end works correctly\n";
}

int main() {
    std::cout << "Running ZREM command tests...\n\n";

    test_zrem_existing_member();
    test_zrem_nonexistent_key_and_member();
    test_zrem_via_handler();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
