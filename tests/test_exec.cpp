#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_exec_without_multi() {
    Store store;
    CommandHandler handler(store);

    std::string input = "*1\r\n$4\r\nEXEC\r\n";
    auto result = handler.process_with_fd(1, input, nullptr);

    assert(!result.should_block);
    assert(result.response == "-ERR EXEC without MULTI\r\n");

    std::cout << "\u2713 Test 1 passed: EXEC without MULTI returns error\n";
}

void test_queued_after_multi() {
    Store store;
    CommandHandler handler(store);

    std::string multi_input = "*1\r\n$5\r\nMULTI\r\n";
    auto r1 = handler.process_with_fd(1, multi_input, nullptr);
    assert(r1.response == "+OK\r\n");

    std::string set_input = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$2\r\n41\r\n";
    auto r2 = handler.process_with_fd(1, set_input, nullptr);
    assert(r2.response == "+QUEUED\r\n");

    auto val = store.get("foo");
    assert(!val.has_value());

    std::cout << "\u2713 Test 2 passed: commands after MULTI return QUEUED\n";
}

void test_exec_executes_and_returns_results() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$2\r\n41\r\n", nullptr);
    handler.process_with_fd(1, "*2\r\n$4\r\nINCR\r\n$3\r\nfoo\r\n", nullptr);

    std::string exec_input = "*1\r\n$4\r\nEXEC\r\n";
    auto result = handler.process_with_fd(1, exec_input, nullptr);

    assert(!result.should_block);

    std::string expected = "*2\r\n+OK\r\n:42\r\n";
    assert(result.response == expected);

    std::cout << "\u2713 Test 3 passed: EXEC executes queued commands and returns results\n";
}

void test_exec_clears_transaction_state() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n", nullptr);
    handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);

    std::string get_input = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    auto result = handler.process_with_fd(1, get_input, nullptr);

    assert(result.response == "$3\r\nbar\r\n");

    std::cout << "\u2713 Test 4 passed: EXEC clears transaction state\n";
}

void test_exec_empty_transaction() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);

    std::string exec_input = "*1\r\n$4\r\nEXEC\r\n";
    auto result = handler.process_with_fd(1, exec_input, nullptr);

    assert(!result.should_block);
    assert(result.response == "*0\r\n");

    std::cout << "\u2713 Test 5 passed: EXEC empty transaction returns empty array\n";
}

void test_exec_aborts_when_watched_key_modified() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n200\r\n", nullptr);

    handler.process_with_fd(1, "*2\r\n$5\r\nWATCH\r\n$3\r\nfoo\r\n", nullptr);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n300\r\n", nullptr);

    handler.process_with_fd(2, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n200\r\n", nullptr);

    auto result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);
    assert(result.response == "*-1\r\n");

    auto bar_val = store.get("bar");
    assert(bar_val.has_value());
    assert(bar_val.value() == "200");

    std::cout << "\u2713 Test 6 passed: EXEC aborts when watched key modified by another client\n";
}

void test_exec_succeeds_when_watched_key_not_modified() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbaz\r\n$3\r\n100\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\ncaz\r\n$3\r\n200\r\n", nullptr);

    handler.process_with_fd(1, "*2\r\n$5\r\nWATCH\r\n$3\r\nbaz\r\n", nullptr);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\ncaz\r\n$3\r\n400\r\n", nullptr);

    handler.process_with_fd(2, "*3\r\n$3\r\nSET\r\n$3\r\ncaz\r\n$3\r\n300\r\n", nullptr);

    auto result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);
    assert(result.response == "*1\r\n+OK\r\n");

    auto caz_val = store.get("caz");
    assert(caz_val.has_value());
    assert(caz_val.value() == "400");

    std::cout << "\u2713 Test 7 passed: EXEC succeeds when watched key not modified\n";
}

void test_exec_aborts_when_watched_key_modified_and_restored() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n", nullptr);

    handler.process_with_fd(1, "*2\r\n$5\r\nWATCH\r\n$3\r\nfoo\r\n", nullptr);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n999\r\n", nullptr);

    handler.process_with_fd(2, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n200\r\n", nullptr);
    handler.process_with_fd(2, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n", nullptr);

    auto result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);
    assert(result.response == "*-1\r\n");

    std::cout << "\u2713 Test 8 passed: EXEC aborts when watched key modified and restored\n";
}

void test_exec_clears_watch_state_after_abort() {
    Store store;
    CommandHandler handler(store);

    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n200\r\n", nullptr);

    handler.process_with_fd(1, "*2\r\n$5\r\nWATCH\r\n$3\r\nfoo\r\n", nullptr);

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n300\r\n", nullptr);

    handler.process_with_fd(2, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n999\r\n", nullptr);

    auto abort_result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);
    assert(abort_result.response == "*-1\r\n");

    handler.process_with_fd(1, "*1\r\n$5\r\nMULTI\r\n", nullptr);
    handler.process_with_fd(1, "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n400\r\n", nullptr);

    auto result = handler.process_with_fd(1, "*1\r\n$4\r\nEXEC\r\n", nullptr);
    assert(result.response == "*1\r\n+OK\r\n");

    auto bar_val = store.get("bar");
    assert(bar_val.has_value());
    assert(bar_val.value() == "400");

    std::cout << "\u2713 Test 9 passed: EXEC clears watch state after abort\n";
}

int main() {
    std::cout << "Running EXEC command tests...\n\n";

    test_exec_without_multi();
    test_queued_after_multi();
    test_exec_executes_and_returns_results();
    test_exec_clears_transaction_state();
    test_exec_empty_transaction();
    test_exec_aborts_when_watched_key_modified();
    test_exec_succeeds_when_watched_key_not_modified();
    test_exec_aborts_when_watched_key_modified_and_restored();
    test_exec_clears_watch_state_after_abort();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
