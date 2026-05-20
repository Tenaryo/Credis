#include "../src/cli/cli_parser.hpp"
#include <cassert>
#include <iostream>

void test_parse_port_custom() {
    char arg0[] = "redis";
    char arg1[] = "--port";
    char arg2[] = "6380";
    char* argv[] = {arg0, arg1, arg2};
    int argc = 3;

    int port = parse_port(argc, argv);

    assert(port == 6380);

    std::cout << "Test 1 passed: parse_port returns 6380 for --port 6380\n";
}

void test_parse_port_default() {
    char arg0[] = "redis";
    char* argv[] = {arg0};
    int argc = 1;

    int port = parse_port(argc, argv);

    assert(port == 6379);

    std::cout << "Test 2 passed: parse_port returns 6379 when no --port provided\n";
}

void test_parse_dir() {
    char arg0[] = "redis";
    char arg1[] = "--dir";
    char arg2[] = "/tmp/redis-files";
    char* argv[] = {arg0, arg1, arg2};
    int argc = 3;

    auto dir = parse_dir(argc, argv);

    assert(dir == "/tmp/redis-files");

    std::cout << "Test 3 passed: parse_dir returns /tmp/redis-files for --dir /tmp/redis-files\n";
}

void test_parse_dir_default() {
    char arg0[] = "redis";
    char* argv[] = {arg0};
    int argc = 1;

    auto dir = parse_dir(argc, argv);

    assert(dir.empty());

    std::cout << "Test 4 passed: parse_dir returns empty string when no --dir provided\n";
}

void test_parse_dbfilename() {
    char arg0[] = "redis";
    char arg1[] = "--dbfilename";
    char arg2[] = "dump.rdb";
    char* argv[] = {arg0, arg1, arg2};
    int argc = 3;

    auto dbfilename = parse_dbfilename(argc, argv);

    assert(dbfilename == "dump.rdb");

    std::cout << "Test 5 passed: parse_dbfilename returns dump.rdb for --dbfilename dump.rdb\n";
}

void test_parse_dbfilename_default() {
    char arg0[] = "redis";
    char* argv[] = {arg0};
    int argc = 1;

    auto dbfilename = parse_dbfilename(argc, argv);

    assert(dbfilename.empty());

    std::cout
        << "Test 6 passed: parse_dbfilename returns empty string when no --dbfilename provided\n";
}

int main() {
    std::cout << "Running CLI parser tests...\n\n";

    test_parse_port_custom();
    test_parse_port_default();
    test_parse_dir();
    test_parse_dir_default();
    test_parse_dbfilename();
    test_parse_dbfilename_default();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
