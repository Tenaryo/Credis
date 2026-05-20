#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_incr_positive_integer() {
    Store store;
    store.set("counter", "5");

    auto result = store.incr("counter");

    assert(result.has_value());
    assert(*result == 6);

    auto value = store.get("counter");
    assert(value.has_value());
    assert(*value == "6");

    std::cout << "✓ Test 1 passed: INCR increments positive integer\n";
}

void test_incr_zero() {
    Store store;
    store.set("counter", "0");

    auto result = store.incr("counter");

    assert(result.has_value());
    assert(*result == 1);

    auto value = store.get("counter");
    assert(value.has_value());
    assert(*value == "1");

    std::cout << "✓ Test 2 passed: INCR increments zero to 1\n";
}

void test_incr_negative_integer() {
    Store store;
    store.set("counter", "-3");

    auto result = store.incr("counter");

    assert(result.has_value());
    assert(*result == -2);

    auto value = store.get("counter");
    assert(value.has_value());
    assert(*value == "-2");

    std::cout << "✓ Test 3 passed: INCR increments negative integer\n";
}

void test_incr_multiple_times() {
    Store store;
    store.set("counter", "100");

    store.incr("counter");
    store.incr("counter");
    auto result = store.incr("counter");

    assert(result.has_value());
    assert(*result == 103);

    std::cout << "✓ Test 4 passed: Multiple INCR operations work correctly\n";
}

void test_incr_large_number() {
    Store store;
    store.set("counter", "9223372036854775806");

    auto result = store.incr("counter");

    assert(result.has_value());
    assert(*result == 9223372036854775807);

    std::cout << "✓ Test 5 passed: INCR works with large numbers near INT64_MAX\n";
}

void test_incr_non_existent_key() {
    Store store;

    auto result = store.incr("nonexistent");

    assert(result.has_value());
    assert(*result == 1);

    auto value = store.get("nonexistent");
    assert(value.has_value());
    assert(*value == "1");

    std::cout << "✓ Test 6 passed: INCR on non-existent key returns 1\n";
}

int main() {
    std::cout << "Running INCR command tests...\n\n";

    test_incr_positive_integer();
    test_incr_zero();
    test_incr_negative_integer();
    test_incr_multiple_times();
    test_incr_large_number();
    test_incr_non_existent_key();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
