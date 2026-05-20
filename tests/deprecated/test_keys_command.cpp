#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/server/server_config.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>
#include <string>

void test_keys_star_returns_all_keys() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    handler.process("*3\r\n$3\r\nSET\r\n$3\r\nbaz\r\n$3\r\nqux\r\n");

    auto response = handler.process("*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n");

    assert(response.starts_with("*2\r\n"));
    assert(response.find("foo") != std::string::npos);
    assert(response.find("baz") != std::string::npos);

    std::cout << "\u2713 Test 1 passed: KEYS * returns all keys\n";
}

void test_keys_star_empty_store() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    auto response = handler.process("*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n");

    assert(response == "*0\r\n");

    std::cout << "\u2713 Test 2 passed: KEYS * on empty store returns empty array\n";
}

void test_keys_star_single_key() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process("*3\r\n$3\r\nSET\r\n$5\r\nhello\r\n$5\r\nworld\r\n");

    auto response = handler.process("*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n");

    assert(response == "*1\r\n$5\r\nhello\r\n");

    std::cout << "\u2713 Test 3 passed: KEYS * returns single key\n";
}

int main() {
    std::cout << "Running KEYS command tests...\n\n";

    test_keys_star_returns_all_keys();
    test_keys_star_empty_store();
    test_keys_star_single_key();

    std::cout << "\n\u2713 All KEYS command tests passed!\n";
    return 0;
}
