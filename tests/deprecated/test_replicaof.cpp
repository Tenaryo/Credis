#include "../src/cli/cli_parser.hpp"
#include <cassert>
#include <iostream>

void test_parse_replicaof_with_host_port() {
    char arg0[] = "redis";
    char arg1[] = "--replicaof";
    char arg2[] = "localhost 6379";
    char* argv[] = {arg0, arg1, arg2};
    int argc = 3;

    auto result = parse_replicaof(argc, argv);

    assert(result.has_value());
    assert(result->host == "localhost");
    assert(result->port == 6379);

    std::cout << "\u2713 Test 1 passed: parse_replicaof returns {localhost, 6379}\n";
}

void test_parse_replicaof_not_provided() {
    char arg0[] = "redis";
    char* argv[] = {arg0};
    int argc = 1;

    auto result = parse_replicaof(argc, argv);

    assert(!result.has_value());

    std::cout << "\u2713 Test 2 passed: parse_replicaof returns nullopt when not provided\n";
}

int main() {
    std::cout << "Running --replicaof CLI parser tests...\n\n";

    test_parse_replicaof_with_host_port();
    test_parse_replicaof_not_provided();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
