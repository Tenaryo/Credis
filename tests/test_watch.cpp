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

int main() {
    std::cout << "Running WATCH command tests...\n\n";

    test_watch_returns_ok();
    test_watch_without_key_returns_error();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
