#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_zadd_creates_new_sorted_set() {
    Store store;

    auto result = store.zadd("myzset", 10.0, "member1");

    assert(result == 1);
    assert(store.exists("myzset"));

    std::cout << "✓ Test 1 passed: ZADD creates new sorted set and returns 1\n";
}

void test_zadd_type_is_zset() {
    Store store;

    store.zadd("myzset", 10.0, "member1");
    auto type = store.get_type("myzset");

    assert(type == "zset");

    std::cout << "✓ Test 2 passed: TYPE returns zset for sorted set key\n";
}

void test_zadd_existing_member_updates_score() {
    Store store;

    auto r1 = store.zadd("myzset", 10.0, "member1");
    assert(r1 == 1);

    auto r2 = store.zadd("myzset", 20.0, "member1");
    assert(r2 == 0);

    std::cout << "✓ Test 3 passed: ZADD returns 0 when updating existing member score\n";
}

int main() {
    std::cout << "Running ZADD command tests...\n\n";

    test_zadd_creates_new_sorted_set();
    test_zadd_type_is_zset();
    test_zadd_existing_member_updates_score();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
