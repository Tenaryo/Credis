#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/server/server_config.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>
#include <string>

void test_info_replication_returns_role_master() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

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
    ServerConfig config;
    CommandHandler handler(store, config);

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

auto extract_info_content = [](const std::string& response) -> std::string {
    assert(response.starts_with("$"));
    auto crlf = response.find("\r\n");
    assert(crlf != std::string::npos);
    int len = std::stoi(response.substr(1, crlf - 1));
    return response.substr(crlf + 2, len);
};

void test_info_master_replid() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n";
    auto response = handler.process(input);
    auto content = extract_info_content(response);

    assert(content.find("master_replid:8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb") !=
           std::string::npos);

    std::cout << "\u2713 Test 4 passed: INFO returns correct master_replid\n";
}

void test_info_master_repl_offset() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n";
    auto response = handler.process(input);
    auto content = extract_info_content(response);

    assert(content.find("master_repl_offset:0") != std::string::npos);

    std::cout << "\u2713 Test 5 passed: INFO returns correct master_repl_offset\n";
}

void test_info_replication_returns_role_slave_when_configured() {
    Store store;
    ServerConfig config;
    config.replicaof = ReplicaOfConfig{"localhost", 6379};
    CommandHandler handler(store, config);

    std::string input = "*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n";
    auto response = handler.process(input);

    assert(response.starts_with("$"));
    auto crlf = response.find("\r\n");
    assert(crlf != std::string::npos);
    int len = std::stoi(response.substr(1, crlf - 1));
    std::string content = response.substr(crlf + 2, len);

    assert(content.find("# Replication") != std::string::npos);
    assert(content.find("role:slave") != std::string::npos);

    std::cout
        << "\u2713 Test 3 passed: INFO replication returns role:slave when configured as slave\n";
}

int main() {
    std::cout << "Running INFO command tests...\n\n";

    test_info_replication_returns_role_master();
    test_info_without_args_returns_replication_section();
    test_info_master_replid();
    test_info_master_repl_offset();
    test_info_replication_returns_role_slave_when_configured();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
