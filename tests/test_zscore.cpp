#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_zscore_returns_correct_score() {
    Store store;

    store.zadd("zset_key", 20.0, "zset_member1");
    store.zadd("zset_key", 30.1, "zset_member2");
    store.zadd("zset_key", 40.2, "zset_member3");
    store.zadd("zset_key", 50.3, "zset_member4");

    auto score2 = store.zscore("zset_key", "zset_member2");
    assert(score2.has_value());
    double diff = *score2 - 30.1;
    assert(diff >= -0.001 && diff <= 0.001);

    auto score1 = store.zscore("zset_key", "zset_member1");
    assert(score1.has_value());
    assert(*score1 == 20.0);

    std::cout << "✓ Test 1 passed: ZSCORE returns correct score for existing members\n";
}

void test_zscore_returns_nullopt_for_missing() {
    Store store;

    store.zadd("zset_key", 1.0, "member1");

    auto missing_member = store.zscore("zset_key", "nonexistent");
    assert(!missing_member.has_value());

    auto missing_key = store.zscore("missing_key", "member1");
    assert(!missing_key.has_value());

    std::cout << "✓ Test 2 passed: ZSCORE returns nullopt for missing key/member\n";
}

int main() {
    std::cout << "Running ZSCORE command tests...\n\n";

    test_zscore_returns_correct_score();
    test_zscore_returns_nullopt_for_missing();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
