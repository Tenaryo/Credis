#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>
#include <string>

void test_info_replication_returns_role_master() {
    Store store;
    CommandHandler handler(store);

    std::string input = "*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n";
    auto response = handler.process(input);

    assert(response.starts_with("$"));
    auto crlf = response.find("\r\n");
    assert(crlf != std::string::npos);
    int len = std::stoi(response.substr(1, crlf - 1));
    std::string content = response.substr(crlf + 2, len);

    assert(content.find("# Replication") != std::string::npos);
    assert(content.find("role:master") != std::string::npos);

    std::cout << "\u2713 Test 1 passed: INFO replication returns # Replication and role:master\n";
}

void test_info_without_args_returns_replication_section() {
    Store store;
    CommandHandler handler(store);

    std::string input = "*1\r\n$4\r\nINFO\r\n";
    auto response = handler.process(input);

    assert(response.starts_with("$"));
    auto crlf = response.find("\r\n");
    assert(crlf != std::string::npos);
    int len = std::stoi(response.substr(1, crlf - 1));
    std::string content = response.substr(crlf + 2, len);

    assert(content.find("# Replication") != std::string::npos);
    assert(content.find("role:master") != std::string::npos);

    std::cout
        << "\u2713 Test 2 passed: INFO without args returns replication section with role:master\n";
}

int main() {
    std::cout << "Running INFO command tests...\n\n";

    test_info_replication_returns_role_master();
    test_info_without_args_returns_replication_section();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
