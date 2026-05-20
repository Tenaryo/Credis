#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/server/server_config.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>
#include <string>

void test_config_get_dir() {
    Store store;
    ServerConfig config;
    config.dir = "/tmp/redis-files";
    CommandHandler handler(store, config);

    std::string input = "*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$3\r\ndir\r\n";
    auto response = handler.process(input);

    assert(response == "*2\r\n$3\r\ndir\r\n$16\r\n/tmp/redis-files\r\n");

    std::cout << "\u2713 Test 1 passed: CONFIG GET dir returns correct value\n";
}

void test_config_get_dbfilename() {
    Store store;
    ServerConfig config;
    config.dbfilename = "dump.rdb";
    CommandHandler handler(store, config);

    std::string input = "*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$10\r\ndbfilename\r\n";
    auto response = handler.process(input);

    assert(response == "*2\r\n$10\r\ndbfilename\r\n$8\r\ndump.rdb\r\n");

    std::cout << "\u2713 Test 2 passed: CONFIG GET dbfilename returns correct value\n";
}

void test_config_get_empty_values() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$3\r\ndir\r\n";
    auto response = handler.process(input);

    assert(response == "*2\r\n$3\r\ndir\r\n$-1\r\n");

    std::cout << "\u2713 Test 3 passed: CONFIG GET dir returns null bulk string when not set\n";
}

int main() {
    std::cout << "Running CONFIG GET tests...\n\n";

    test_config_get_dir();
    test_config_get_dbfilename();
    test_config_get_empty_values();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
