#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

void test_xadd_auto_seq_empty_stream() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result = store.xadd("mystream", "1-*", fields);
    assert(result == "1-0");
    std::cout << "✓ Test 1 passed: Auto-generate sequence 0 for empty stream\n";
}

void test_xadd_auto_seq_increment() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    store.xadd("mystream", "1-*", fields);
    std::string result = store.xadd("mystream", "1-*", fields);
    assert(result == "1-1");
    std::cout << "✓ Test 2 passed: Auto-generate incremented sequence number\n";
}

void test_xadd_auto_seq_multiple_increments() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result1 = store.xadd("mystream", "1-*", fields);
    assert(result1 == "1-0");

    std::string result2 = store.xadd("mystream", "1-*", fields);
    assert(result2 == "1-1");

    std::string result3 = store.xadd("mystream", "1-*", fields);
    assert(result3 == "1-2");
    std::cout << "✓ Test 3 passed: Multiple auto-generated sequence numbers\n";
}

void test_xadd_auto_seq_different_timestamps() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    store.xadd("mystream", "1-*", fields);
    store.xadd("mystream", "1-*", fields);

    std::string result = store.xadd("mystream", "2-*", fields);
    assert(result == "2-0");
    std::cout << "✓ Test 4 passed: Different timestamps start sequence at 0\n";
}

void test_xadd_auto_seq_zero_timestamp() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result = store.xadd("mystream", "0-*", fields);
    assert(result == "0-1");
    std::cout << "✓ Test 5 passed: Zero timestamp starts sequence at 1\n";
}

void test_xadd_auto_seq_zero_timestamp_increment() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    store.xadd("mystream", "0-*", fields);
    std::string result = store.xadd("mystream", "0-*", fields);
    assert(result == "0-2");
    std::cout << "✓ Test 6 passed: Zero timestamp increments from 1\n";
}

void test_xadd_auto_seq_mixed_explicit_and_auto() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    store.xadd("mystream", "100-0", fields);
    store.xadd("mystream", "100-5", fields);

    std::string result = store.xadd("mystream", "100-*", fields);
    assert(result == "100-6");
    std::cout << "✓ Test 7 passed: Auto-seq after explicit IDs\n";
}

void test_xadd_auto_seq_multiple_streams() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result1 = store.xadd("stream1", "1-*", fields);
    assert(result1 == "1-0");

    std::string result2 = store.xadd("stream2", "1-*", fields);
    assert(result2 == "1-0");

    std::string result3 = store.xadd("stream1", "1-*", fields);
    assert(result3 == "1-1");
    std::cout << "✓ Test 8 passed: Multiple streams are independent\n";
}

int main() {
    std::cout << "Running XADD auto-generate sequence tests...\n\n";

    test_xadd_auto_seq_empty_stream();
    test_xadd_auto_seq_increment();
    test_xadd_auto_seq_multiple_increments();
    test_xadd_auto_seq_different_timestamps();
    test_xadd_auto_seq_zero_timestamp();
    test_xadd_auto_seq_zero_timestamp_increment();
    test_xadd_auto_seq_mixed_explicit_and_auto();
    test_xadd_auto_seq_multiple_streams();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
