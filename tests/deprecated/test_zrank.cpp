#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_zrank_returns_correct_rank() {
    Store store;

    store.zadd("zset_key", 100.0, "foo");
    store.zadd("zset_key", 100.0, "bar");
    store.zadd("zset_key", 20.0, "baz");
    store.zadd("zset_key", 30.1, "caz");
    store.zadd("zset_key", 40.2, "paz");

    auto rank_caz = store.zrank("zset_key", "caz");
    assert(rank_caz.has_value() && *rank_caz == 1);

    auto rank_baz = store.zrank("zset_key", "baz");
    assert(rank_baz.has_value() && *rank_baz == 0);

    auto rank_foo = store.zrank("zset_key", "foo");
    assert(rank_foo.has_value() && *rank_foo == 4);

    auto rank_bar = store.zrank("zset_key", "bar");
    assert(rank_bar.has_value() && *rank_bar == 3);

    std::cout << "✓ Test 1 passed: ZRANK returns correct rank for members\n";
}

void test_zrank_returns_nullopt_for_missing() {
    Store store;

    store.zadd("zset_key", 1.0, "member1");

    auto rank_missing_member = store.zrank("zset_key", "nonexistent");
    assert(!rank_missing_member.has_value());

    auto rank_missing_key = store.zrank("missing_key", "member1");
    assert(!rank_missing_key.has_value());

    std::cout << "✓ Test 2 passed: ZRANK returns nullopt for missing key/member\n";
}

void test_zrank_same_score_lexicographic_order() {
    Store store;

    store.zadd("zset_key", 1.0, "member_with_score_1");
    store.zadd("zset_key", 2.0, "member_with_score_2");
    store.zadd("zset_key", 2.0, "another_member_with_score_2");

    auto rank1 = store.zrank("zset_key", "member_with_score_1");
    assert(rank1.has_value() && *rank1 == 0);

    auto rank_another = store.zrank("zset_key", "another_member_with_score_2");
    assert(rank_another.has_value() && *rank_another == 1);

    auto rank_member = store.zrank("zset_key", "member_with_score_2");
    assert(rank_member.has_value() && *rank_member == 2);

    std::cout << "✓ Test 3 passed: ZRANK orders same-score members lexicographically\n";
}

int main() {
    std::cout << "Running ZRANK command tests...\n\n";

    test_zrank_returns_correct_rank();
    test_zrank_returns_nullopt_for_missing();
    test_zrank_same_score_lexicographic_order();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
