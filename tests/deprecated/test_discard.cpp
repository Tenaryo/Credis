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

    assert(!std::holds_alternative<ProcessResult::Block>(result.state));
    assert(std::get<ProcessResult::Normal>(result.state).response == "+OK\r\n");

    std::cout << "\u2713 Test 1 passed: DISCARD in MULTI returns OK\n";
}

void test_discard_without_multi_returns_error() {
    Store store;
    CommandHandler handler(store);

    std::string discard_input = "*1\r\n$7\r\nDISCARD\r\n";
    auto result = handler.process_with_fd(1, discard_input, nullptr);

    assert(!std::holds_alternative<ProcessResult::Block>(result.state));
    assert(std::get<ProcessResult::Normal>(result.state).response ==
           "-ERR DISCARD without MULTI\r\n");

    std::cout << "\u2713 Test 2 passed: DISCARD without MULTI returns error\n";
}

void test_discard_then_exec_returns_error() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$2\r\n41\r\n", nullptr);
    handler.process_with_fd(1, "*1\r\n$7\r\nDISCARD\r\n", nullptr);

    auto result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);

    assert(!std::holds_alternative<ProcessResult::Block>(result.state));
    assert(std::get<ProcessResult::Normal>(result.state).response == "-ERR EXEC without MULTI\r\n");

    std::cout << "\u2713 Test 3 passed: EXEC after DISCARD returns error\n";
}

void test_discard_clears_watch_state_allows_subsequent_transaction() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n200\r\n", nullptr);

    handler.process_with_fd(1, "*3\r\n$5\r\nWATCH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n", nullptr);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n300\r\n", nullptr);

    handler.process_with_fd(2, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n400\r\n", nullptr);

    auto discard_result = handler.process_with_fd(1, "*1\r\n$7\r\nDISCARD\r\n", nullptr);
    assert(std::get<ProcessResult::Normal>(discard_result.state).response == "+OK\r\n");

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n300\r\n", nullptr);

    auto exec_result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);
    assert(std::get<ProcessResult::Normal>(exec_result.state).response == "*1\r\n+OK\r\n");

    auto bar_val = store.get("bar");
    assert(bar_val.has_value());
    assert(bar_val.value() == "300");

    std::cout
        << "\u2713 Test 4 passed: DISCARD clears watch state, subsequent transaction succeeds\n";
}

int main() {
    std::cout << "Running DISCARD command tests...\n\n";

    test_discard_in_multi_returns_ok();
    test_discard_without_multi_returns_error();
    test_discard_then_exec_returns_error();
    test_discard_clears_watch_state_allows_subsequent_transaction();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
