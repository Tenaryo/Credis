#include "../src/store/store.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>

void test_xadd_auto_id_format() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result = store.xadd("mystream", "*", fields);

    assert(!result.empty());

    size_t dash_pos = result.find('-');
    assert(dash_pos != std::string::npos);

    std::string timestamp_str = result.substr(0, dash_pos);
    std::string seq_str = result.substr(dash_pos + 1);

    assert(!timestamp_str.empty());
    assert(!seq_str.empty());

    for (char c : timestamp_str) {
        assert(std::isdigit(c));
    }
    for (char c : seq_str) {
        assert(std::isdigit(c));
    }

    std::cout << "✓ Test 1 passed: Auto-generated ID has correct format (timestamp-sequence)\n";
}

void test_xadd_auto_id_current_time() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    auto before = std::chrono::system_clock::now();
    std::string result = store.xadd("mystream", "*", fields);
    auto after = std::chrono::system_clock::now();

    size_t dash_pos = result.find('-');
    std::string timestamp_str = result.substr(0, dash_pos);
    int64_t timestamp_ms = std::stoll(timestamp_str);

    auto before_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(before.time_since_epoch()).count();
    auto after_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(after.time_since_epoch()).count();

    assert(timestamp_ms >= before_ms - 1);
    assert(timestamp_ms <= after_ms + 1);

    std::cout << "✓ Test 2 passed: Auto-generated timestamp is current Unix time in milliseconds\n";
}

void test_xadd_auto_id_sequence_zero() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result = store.xadd("mystream", "*", fields);

    size_t dash_pos = result.find('-');
    std::string seq_str = result.substr(dash_pos + 1);
    int64_t sequence = std::stoll(seq_str);

    assert(sequence == 0);

    std::cout << "✓ Test 3 passed: First entry with auto-generated ID has sequence 0\n";
}

void test_xadd_auto_id_multiple_entries() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result1 = store.xadd("mystream", "*", fields);
    std::string result2 = store.xadd("mystream", "*", fields);

    size_t dash1 = result1.find('-');
    size_t dash2 = result2.find('-');

    int64_t ts1 = std::stoll(result1.substr(0, dash1));
    int64_t seq1 = std::stoll(result1.substr(dash1 + 1));
    int64_t ts2 = std::stoll(result2.substr(0, dash2));
    int64_t seq2 = std::stoll(result2.substr(dash2 + 1));

    if (ts1 == ts2) {
        assert(seq2 == seq1 + 1);
    } else {
        assert(ts2 >= ts1);
        assert(seq2 == 0);
    }

    std::cout << "✓ Test 4 passed: Multiple auto-generated IDs follow increment rules\n";
}

void test_xadd_auto_id_empty_stream() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result = store.xadd("mystream", "*", fields);

    size_t dash_pos = result.find('-');
    int64_t timestamp = std::stoll(result.substr(0, dash_pos));
    int64_t sequence = std::stoll(result.substr(dash_pos + 1));

    assert(timestamp > 0 || sequence > 0);

    std::cout << "✓ Test 5 passed: Auto-generated ID for empty stream is valid (not 0-0)\n";
}

void test_xadd_auto_id_multiple_streams() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result1 = store.xadd("stream1", "*", fields);
    std::string result2 = store.xadd("stream2", "*", fields);

    assert(!result1.empty());
    assert(!result2.empty());

    std::cout << "✓ Test 6 passed: Auto-generated IDs work across multiple streams\n";
}

void test_xadd_auto_id_mixed_with_explicit() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"foo", "bar"}};

    std::string result1 = store.xadd("mystream", "100-0", fields);
    assert(result1 == "100-0");

    std::string result2 = store.xadd("mystream", "*", fields);

    size_t dash_pos = result2.find('-');
    int64_t timestamp = std::stoll(result2.substr(0, dash_pos));
    int64_t sequence = std::stoll(result2.substr(dash_pos + 1));

    assert(timestamp > 100 || (timestamp == 100 && sequence > 0));

    std::cout << "✓ Test 7 passed: Auto-generated ID works after explicit ID\n";
}

int main() {
    std::cout << "Running XADD auto-generate full ID tests...\n\n";

    test_xadd_auto_id_format();
    test_xadd_auto_id_current_time();
    test_xadd_auto_id_sequence_zero();
    test_xadd_auto_id_multiple_entries();
    test_xadd_auto_id_empty_stream();
    test_xadd_auto_id_multiple_streams();
    test_xadd_auto_id_mixed_with_explicit();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
