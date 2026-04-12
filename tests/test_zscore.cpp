#include "../src/handler/command_handler.hpp"
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

void test_zscore_high_precision_via_handler() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process(
        "*4\r\n$4\r\nZADD\r\n$8\r\nzset_key\r\n$16\r\n27.9285743025792\r\n$4\r\nmem1\r\n");

    auto response = handler.process("*3\r\n$6\r\nZSCORE\r\n$8\r\nzset_key\r\n$4\r\nmem1\r\n");

    assert(response.find("27.9285743025792") != std::string::npos);

    std::cout << "✓ Test 3 passed: ZSCORE preserves high precision score via handler\n";
}

int main() {
    std::cout << "Running ZSCORE command tests...\n\n";

    test_zscore_returns_correct_score();
    test_zscore_returns_nullopt_for_missing();
    test_zscore_high_precision_via_handler();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
