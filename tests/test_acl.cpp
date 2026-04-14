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

void test_acl_getuser_default_returns_flags_with_nopass() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*3\r\n$3\r\nACL\r\n$7\r\nGETUSER\r\n$7\r\ndefault\r\n";
    auto response = handler.process(input);

    assert(response == "*4\r\n$5\r\nflags\r\n*1\r\n$6\r\nnopass\r\n$9\r\npasswords\r\n*0\r\n");

    std::cout << "\u2713 Test passed: ACL GETUSER default returns [\"flags\", [\"nopass\"], "
                 "\"passwords\", []]\n";
}

void test_acl_setuser_with_password_returns_ok() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input =
        "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$12\r\n>mypassword\r\n";
    auto response = handler.process(input);

    assert(response == "+OK\r\n");

    std::cout << "\u2713 Test passed: ACL SETUSER default >mypassword returns OK\n";
}

void test_acl_setuser_password_updates_getuser_response() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string setuser_input =
        "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$12\r\n>mypassword\r\n";
    handler.process(setuser_input);

    std::string getuser_input = "*3\r\n$3\r\nACL\r\n$7\r\nGETUSER\r\n$7\r\ndefault\r\n";
    auto response = handler.process(getuser_input);

    assert(response == "*4\r\n$5\r\nflags\r\n*0\r\n$9\r\npasswords\r\n*1\r\n$64\r\n"
                       "89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8\r\n");

    std::cout << "\u2713 Test passed: ACL GETUSER after SETUSER returns no nopass flag and "
                 "SHA-256 password hash\n";
}

int main() {
    std::cout << "Running ACL tests...\n\n";

    test_acl_whoami_returns_default();
    test_acl_getuser_default_returns_flags_with_nopass();
    test_acl_setuser_with_password_returns_ok();
    test_acl_setuser_password_updates_getuser_response();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
