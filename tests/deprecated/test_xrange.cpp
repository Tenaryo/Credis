#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_xrange_basic() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields1 = {{"temperature", "36"},
                                                                {"humidity", "95"}};
    std::vector<std::pair<std::string, std::string>> fields2 = {{"temperature", "37"},
                                                                {"humidity", "94"}};

    store.xadd("some_key", "1526985054069-0", fields1);
    store.xadd("some_key", "1526985054079-0", fields2);

    auto result = store.xrange("some_key", "1526985054069-0", "1526985054079-0");

    assert(result.size() == 2);
    assert(result[0].id == "1526985054069-0");
    assert(result[0].fields.size() == 2);
    assert(result[0].fields[0].first == "temperature");
    assert(result[0].fields[0].second == "36");
    assert(result[0].fields[1].first == "humidity");
    assert(result[0].fields[1].second == "95");

    assert(result[1].id == "1526985054079-0");
    assert(result[1].fields.size() == 2);
    assert(result[1].fields[0].first == "temperature");
    assert(result[1].fields[0].second == "37");

    std::cout << "Test 1 passed: XRANGE returns entries within range\n";
}

void test_xrange_inclusive() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "200-0", fields);
    store.xadd("stream", "300-0", fields);

    auto result = store.xrange("stream", "100-0", "300-0");
    assert(result.size() == 3);

    result = store.xrange("stream", "100-0", "200-0");
    assert(result.size() == 2);

    result = store.xrange("stream", "200-0", "200-0");
    assert(result.size() == 1);
    assert(result[0].id == "200-0");

    std::cout << "Test 2 passed: XRANGE range is inclusive\n";
}

void test_xrange_partial_id_start() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "100-5", fields);
    store.xadd("stream", "200-0", fields);

    auto result = store.xrange("stream", "100", "200-0");

    assert(result.size() == 3);
    assert(result[0].id == "100-0");
    assert(result[1].id == "100-5");
    assert(result[2].id == "200-0");

    std::cout << "Test 3 passed: XRANGE start ID without sequence defaults to seq 0\n";
}

void test_xrange_partial_id_end() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "100-5", fields);
    store.xadd("stream", "200-0", fields);

    auto result = store.xrange("stream", "100-0", "100");

    assert(result.size() == 2);
    assert(result[0].id == "100-0");
    assert(result[1].id == "100-5");

    std::cout << "Test 4 passed: XRANGE end ID without sequence defaults to max seq\n";
}

void test_xrange_empty_stream() {
    Store store;

    auto result = store.xrange("nonexistent", "0-0", "9999999-9999999");
    assert(result.empty());

    std::cout << "Test 5 passed: XRANGE on nonexistent key returns empty\n";
}

void test_xrange_no_match() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "200-0", fields);

    auto result = store.xrange("stream", "150-0", "180-0");
    assert(result.empty());

    std::cout << "Test 6 passed: XRANGE with no matching entries returns empty\n";
}

void test_xrange_single_entry() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"temp", "25"}};

    store.xadd("stream", "100-0", fields);

    auto result = store.xrange("stream", "0-0", "9999999999999-9999999");
    assert(result.size() == 1);
    assert(result[0].id == "100-0");
    assert(result[0].fields.size() == 1);
    assert(result[0].fields[0].first == "temp");
    assert(result[0].fields[0].second == "25");

    std::cout << "Test 7 passed: XRANGE works with single entry\n";
}

void test_xrange_dash_start() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields1 = {{"raspberry", "strawberry"}};
    std::vector<std::pair<std::string, std::string>> fields2 = {{"banana", "apple"}};
    std::vector<std::pair<std::string, std::string>> fields3 = {{"banana", "orange"}};
    std::vector<std::pair<std::string, std::string>> fields4 = {{"apple", "mango"}};

    store.xadd("strawberry", "0-1", fields1);
    store.xadd("strawberry", "0-2", fields2);
    store.xadd("strawberry", "0-3", fields3);
    store.xadd("strawberry", "0-4", fields4);

    auto result = store.xrange("strawberry", "-", "0-2");

    assert(result.size() == 2);
    assert(result[0].id == "0-1");
    assert(result[0].fields[0].first == "raspberry");
    assert(result[0].fields[0].second == "strawberry");
    assert(result[1].id == "0-2");
    assert(result[1].fields[0].first == "banana");
    assert(result[1].fields[0].second == "apple");

    std::cout << "Test 8 passed: XRANGE with '-' start returns entries from beginning\n";
}

void test_xrange_plus_end() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "200-0", fields);
    store.xadd("stream", "300-0", fields);

    auto result = store.xrange("stream", "100-0", "+");

    assert(result.size() == 3);
    assert(result[0].id == "100-0");
    assert(result[1].id == "200-0");
    assert(result[2].id == "300-0");

    std::cout << "Test 9 passed: XRANGE with '+' end returns all entries to end\n";
}

void test_xrange_dash_to_plus() {
    Store store;
    std::vector<std::pair<std::string, std::string>> fields = {{"key", "value"}};

    store.xadd("stream", "100-0", fields);
    store.xadd("stream", "200-0", fields);

    auto result = store.xrange("stream", "-", "+");

    assert(result.size() == 2);
    assert(result[0].id == "100-0");
    assert(result[1].id == "200-0");

    std::cout << "Test 10 passed: XRANGE with '-' to '+' returns all entries\n";
}

int main() {
    std::cout << "Running XRANGE tests...\n\n";

    test_xrange_basic();
    test_xrange_inclusive();
    test_xrange_partial_id_start();
    test_xrange_partial_id_end();
    test_xrange_empty_stream();
    test_xrange_no_match();
    test_xrange_single_entry();
    test_xrange_dash_start();
    test_xrange_plus_end();
    test_xrange_dash_to_plus();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
