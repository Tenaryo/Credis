#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_xread_basic() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields1 = {{"temperature", "36"},
                                                                {"humidity", "95"}};
    std::vector<std::pair<std::string, std::string>> fields2 = {{"temperature", "37"},
                                                                {"humidity", "94"}};

    store.xadd("some_key", "1526985054069-0", fields1);
    store.xadd("some_key", "1526985054079-0", fields2);

    auto result = store.xread("some_key", "1526985054069-0");

    assert(result.size() == 1);
    assert(result[0].id == "1526985054079-0");
    assert(result[0].fields.size() == 2);
    assert(result[0].fields[0].first == "temperature");
    assert(result[0].fields[0].second == "37");

    std::cout << "Test 1 passed: XREAD returns entries after specified ID (exclusive)\n";
}

void test_xread_exclusive() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "200-0", fields);
    store.xadd("stream", "300-0", fields);

    auto result = store.xread("stream", "200-0");
    assert(result.size() == 1);
    assert(result[0].id == "300-0");

    result = store.xread("stream", "99-0");
    assert(result.size() == 3);

    result = store.xread("stream", "300-0");
    assert(result.empty());

    std::cout << "Test 2 passed: XREAD is exclusive (ID > specified)\n";
}

void test_xread_empty_stream() {
    Store store;

    auto result = store.xread("nonexistent", "0-0");
    assert(result.empty());

    std::cout << "Test 3 passed: XREAD on nonexistent key returns empty\n";
}

void test_xread_all_entries() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "200-0", fields);
    store.xadd("stream", "300-0", fields);

    auto result = store.xread("stream", "0-0");
    assert(result.size() == 3);
    assert(result[0].id == "100-0");
    assert(result[1].id == "200-0");
    assert(result[2].id == "300-0");

    std::cout << "Test 4 passed: XREAD with 0-0 returns all entries\n";
}

void test_xread_single_entry() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"temp", "25"}};

    store.xadd("stream", "100-0", fields);

    auto result = store.xread("stream", "0-0");
    assert(result.size() == 1);
    assert(result[0].id == "100-0");

    result = store.xread("stream", "100-0");
    assert(result.empty());

    std::cout << "Test 5 passed: XREAD works with single entry\n";
}

void test_xread_partial_id() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "100-5", fields);
    store.xadd("stream", "200-0", fields);

    auto result = store.xread("stream", "100");
    assert(result.size() == 2);
    assert(result[0].id == "100-5");
    assert(result[1].id == "200-0");

    std::cout << "Test 6 passed: XREAD with partial ID (no sequence) defaults to seq 0\n";
}

int main() {
    std::cout << "Running XREAD tests...\n\n";

    test_xread_basic();
    test_xread_exclusive();
    test_xread_empty_stream();
    test_xread_all_entries();
    test_xread_single_entry();
    test_xread_partial_id();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
