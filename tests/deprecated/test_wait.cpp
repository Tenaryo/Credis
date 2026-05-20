#include "../src/handler/command_handler.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_wait_sets_is_wait_flag() {
    Store store;
    CommandHandler handler(store);

    auto result =
        handler.process_with_fd(42, "*3\r\n$4\r\nWAIT\r\n$1\r\n0\r\n$5\r\n60000\r\n", nullptr);

    assert(std::holds_alternative<ProcessResult::Wait>(result.state));
    assert(std::get<ProcessResult::Wait>(result.state).numreplicas == 0);
    assert(std::get<ProcessResult::Wait>(result.state).timeout_ms == 60000);

    std::cout << "\u2713 Test 1 passed: WAIT sets is_wait flag with correct parameters\n";
}

void test_wait_missing_arguments() {
    Store store;
    CommandHandler handler(store);

    auto result = handler.process_with_fd(42, "*1\r\n$4\r\nWAIT\r\n", nullptr);
    assert(std::get<ProcessResult::Normal>(result.state)
               .response.find("ERR wrong number of arguments") != std::string::npos);

    result = handler.process_with_fd(42, "*2\r\n$4\r\nWAIT\r\n$1\r\n0\r\n", nullptr);
    assert(std::get<ProcessResult::Normal>(result.state)
               .response.find("ERR wrong number of arguments") != std::string::npos);

    std::cout << "\u2713 Test 2 passed: WAIT with missing arguments returns error\n";
}

int main() {
    std::cout << "Running WAIT command tests...\n\n";

    test_wait_sets_is_wait_flag();
    test_wait_missing_arguments();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
