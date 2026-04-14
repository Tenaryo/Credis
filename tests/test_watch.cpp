#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_watch_returns_ok() {
    Store store;
    CommandHandler handler(store);

    std::string input = "*2\r\n$5\r\nWATCH\r\n$3\r\nkey\r\n";
    auto result = handler.process_with_fd(-1, input, nullptr);

    assert(!result.should_block);
    assert(result.response == "+OK\r\n");

    std::cout << "\u2713 Test 1 passed: WATCH returns OK\n";
}

void test_watch_without_key_returns_error() {
    Store store;
    CommandHandler handler(store);

    std::string input = "*1\r\n$5\r\nWATCH\r\n";
    auto result = handler.process_with_fd(-1, input, nullptr);

    assert(!result.should_block);
    assert(result.response == "-ERR wrong number of arguments for 'watch' command\r\n");

    std::cout << "\u2713 Test 2 passed: WATCH without key returns error\n";
}

void test_watch_inside_multi_returns_error() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    std::string watch_input = "*2\r\n$5\r\nWATCH\r\n$3\r\nkey\r\n";
    auto result = handler.process_with_fd(1, watch_input, nullptr);

    assert(!result.should_block);
    assert(result.response.find("ERR") != std::string::npos);
    assert(result.response.find("WATCH") != std::string::npos);
    assert(result.response.find("inside MULTI") != std::string::npos);
    assert(result.response.find("not allowed") != std::string::npos);

    std::cout << "\u2713 Test 3 passed: WATCH inside MULTI returns error\n";
}

void test_unwatch_returns_ok() {
    Store store;
    CommandHandler handler(store);

    auto result = handler.process_with_fd(1, "*1\r\n$7\r\nUNWATCH\r\n", nullptr);

    assert(!result.should_block);
    assert(result.response == "+OK\r\n");

    std::cout << "\u2713 Test 4 passed: UNWATCH returns OK\n";
}

void test_unwatch_clears_watched_keys_then_exec_succeeds() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n200\r\n", nullptr);

    handler.process_with_fd(1, "*3\r\n$5\r\nWATCH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n", nullptr);

    handler.process_with_fd(2, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n200\r\n", nullptr);

    handler.process_with_fd(1, "*1\r\n$7\r\nUNWATCH\r\n", nullptr);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n400\r\n", nullptr);

    auto result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);
    assert(result.response == "*1\r\n+OK\r\n");

    auto foo_val = store.get("foo");
    assert(foo_val.has_value());
    assert(foo_val.value() == "400");

    std::cout << "\u2713 Test 5 passed: UNWATCH clears watched keys, EXEC succeeds\n";
}

int main() {
    std::cout << "Running WATCH/UNWATCH command tests...\n\n";

    test_watch_returns_ok();
    test_watch_without_key_returns_error();
    test_watch_inside_multi_returns_error();
    test_unwatch_returns_ok();
    test_unwatch_clears_watched_keys_then_exec_succeeds();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
