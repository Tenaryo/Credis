#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_discard_in_multi_returns_ok() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    std::string discard_input = "*1\r\n$7\r\nDISCARD\r\n";
    auto result = handler.process_with_fd(1, discard_input, nullptr);

    assert(!result.should_block);
    assert(result.response == "+OK\r\n");

    std::cout << "\u2713 Test 1 passed: DISCARD in MULTI returns OK\n";
}

void test_discard_without_multi_returns_error() {
    Store store;
    CommandHandler handler(store);

    std::string discard_input = "*1\r\n$7\r\nDISCARD\r\n";
    auto result = handler.process_with_fd(1, discard_input, nullptr);

    assert(!result.should_block);
    assert(result.response == "-ERR DISCARD without MULTI\r\n");

    std::cout << "\u2713 Test 2 passed: DISCARD without MULTI returns error\n";
}

void test_discard_then_exec_returns_error() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$2\r\n41\r\n", nullptr);
    handler.process_with_fd(1, "*1\r\n$7\r\nDISCARD\r\n", nullptr);

    auto result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);

    assert(!result.should_block);
    assert(result.response == "-ERR EXEC without MULTI\r\n");

    std::cout << "\u2713 Test 3 passed: EXEC after DISCARD returns error\n";
}

int main() {
    std::cout << "Running DISCARD command tests...\n\n";

    test_discard_in_multi_returns_ok();
    test_discard_without_multi_returns_error();
    test_discard_then_exec_returns_error();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
