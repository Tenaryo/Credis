#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/server/server_config.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>
#include <string>

void test_acl_whoami_returns_default() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n";
    auto response = handler.process(input);

    assert(response == "$7\r\ndefault\r\n");

    std::cout << "\u2713 Test passed: ACL WHOAMI returns 'default' as bulk string\n";
}

void test_acl_getuser_default_returns_flags_with_empty_array() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*3\r\n$3\r\nACL\r\n$7\r\nGETUSER\r\n$7\r\ndefault\r\n";
    auto response = handler.process(input);

    assert(response == "*2\r\n$5\r\nflags\r\n*0\r\n");

    std::cout << "\u2713 Test passed: ACL GETUSER default returns [\"flags\", []]\n";
}

int main() {
    std::cout << "Running ACL tests...\n\n";

    test_acl_whoami_returns_default();
    test_acl_getuser_default_returns_flags_with_empty_array();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
