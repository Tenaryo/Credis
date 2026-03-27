#include "../src/block_manager/blocking_manager.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void test_get_max_id_empty_stream() {
    Store store;
    auto max_id = store.get_stream_max_id("nonexistent");
    assert(!max_id.has_value());

    std::cout << "Test 1 passed: get_max_id returns nullopt for nonexistent stream\n";
}

void test_get_max_id_single_entry() {
    Store store;

    std::vector<std::pair<std::string, std::string>> fields = {{"field", "value"}};
    store.xadd("mystream", "100-0", fields);

    auto max_id = store.get_stream_max_id("mystream");
    assert(max_id.has_value());
    assert(max_id.value() == "100-0");

    std::cout << "Test 2 passed: get_max_id returns correct ID for single entry\n";
}

void test_get_max_id_multiple_entries() {
    Store store;

    std::vector<std::pair<std::string, std::string>> fields = {{"field", "value"}};
    store.xadd("mystream", "100-0", fields);
    store.xadd("mystream", "200-0", fields);
    store.xadd("mystream", "150-0", fields);

    auto max_id = store.get_stream_max_id("mystream");
    assert(max_id.has_value());
    assert(max_id.value() == "200-0");

    std::cout << "Test 3 passed: get_max_id returns highest ID for multiple entries\n";
}

void test_xread_with_dollar_empty_stream() {
    Store store;

    auto entries = store.xread("mystream", "100-0");
    assert(entries.empty());

    std::cout << "Test 4 passed: xread with ID greater than max returns empty\n";
}

void test_xread_with_dollar_no_new_entries() {
    Store store;

    std::vector<std::pair<std::string, std::string>> fields = {{"field", "value"}};
    store.xadd("mystream", "100-0", fields);

    auto max_id = store.get_stream_max_id("mystream");
    assert(max_id.has_value());

    auto entries = store.xread("mystream", max_id.value());
    assert(entries.empty());

    std::cout << "Test 5 passed: xread with max ID returns no entries\n";
}

void test_xread_with_dollar_new_entries() {
    Store store;

    std::vector<std::pair<std::string, std::string>> fields = {{"field", "value"}};
    store.xadd("mystream", "100-0", fields);

    auto max_id = store.get_stream_max_id("mystream");
    assert(max_id.has_value());

    store.xadd("mystream", "200-0", fields);
    store.xadd("mystream", "300-0", fields);

    auto entries = store.xread("mystream", max_id.value());
    assert(entries.size() == 2);
    assert(entries[0].id == "200-0");
    assert(entries[1].id == "300-0");

    std::cout << "Test 6 passed: xread with previous max ID returns new entries\n";
}

void test_blocking_with_dollar_id() {
    BlockingManager manager;

    manager.block_client_for_stream(10, "stream", "100-0", std::chrono::milliseconds(1000));

    auto client = manager.wake_client_for_stream("stream", "200-0");
    assert(client.has_value());
    assert(client->fd == 10);

    std::cout << "Test 7 passed: blocking with dollar-equivalent ID works\n";
}

int main() {
    std::cout << "Running XREAD $ ID tests...\n\n";

    test_get_max_id_empty_stream();
    test_get_max_id_single_entry();
    test_get_max_id_multiple_entries();
    test_xread_with_dollar_empty_stream();
    test_xread_with_dollar_no_new_entries();
    test_xread_with_dollar_new_entries();
    test_blocking_with_dollar_id();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
