#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_multi_returns_ok() {
    Store store;
    CommandHandler handler(store);

    std::string input = "*1\r\n$5\r\nMULTI\r\n";
    auto result = handler.process_with_fd(-1, input, nullptr);

    assert(!std::holds_alternative<ProcessResult::Block>(result.state));
    assert(std::get<ProcessResult::Normal>(result.state).response == "+OK\r\n");

    std::cout << "✓ Test 1 passed: MULTI returns OK\n";
}

int main() {
    std::cout << "Running MULTI command tests...\n\n";

    test_multi_returns_ok();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
