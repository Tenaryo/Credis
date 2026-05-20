#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_zcard_returns_member_count() {
    Store store;

    store.zadd("zset_key", 20.0, "member1");
    store.zadd("zset_key", 30.1, "member2");
    store.zadd("zset_key", 40.2, "member3");
    store.zadd("zset_key", 50.3, "member4");

    assert(store.zcard("zset_key") == 4);

    store.zadd("zset_key", 100.0, "member1");

    assert(store.zcard("zset_key") == 4);

    std::cout << "✓ Test 1 passed: ZCARD returns correct member count\n";
}

void test_zcard_returns_zero_for_missing_key() {
    Store store;

    assert(store.zcard("missing_key") == 0);

    std::cout << "✓ Test 2 passed: ZCARD returns 0 for non-existent key\n";
}

int main() {
    std::cout << "Running ZCARD command tests...\n\n";

    test_zcard_returns_member_count();
    test_zcard_returns_zero_for_missing_key();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
