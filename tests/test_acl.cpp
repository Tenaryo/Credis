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
        "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n";
    auto response = handler.process(input);

    assert(response == "+OK\r\n");

    std::cout << "\u2713 Test passed: ACL SETUSER default >mypassword returns OK\n";
}

void test_acl_setuser_password_updates_getuser_response() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string setuser_input =
        "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n";
    handler.process(setuser_input);

    std::string getuser_input = "*3\r\n$3\r\nACL\r\n$7\r\nGETUSER\r\n$7\r\ndefault\r\n";
    auto response = handler.process(getuser_input);

    assert(response == "*4\r\n$5\r\nflags\r\n*0\r\n$9\r\npasswords\r\n*1\r\n$64\r\n"
                       "89e01536ac207279409d4de1e5253e01f4a1769e696db0d6062ca9b8f56767c8\r\n");

    std::cout << "\u2713 Test passed: ACL GETUSER after SETUSER returns no nopass flag and "
                 "SHA-256 password hash\n";
}

void test_auth_wrong_password_returns_wrongpass() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string setuser_input =
        "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n";
    handler.process(setuser_input);

    std::string auth_input = "*3\r\n$4\r\nAUTH\r\n$7\r\ndefault\r\n$12\r\nwrongpassword\r\n";
    auto response = handler.process(auth_input);

    assert(response.starts_with("-WRONGPASS"));

    std::cout << "\u2713 Test passed: AUTH with wrong password returns WRONGPASS error\n";
}

void test_auth_correct_password_returns_ok() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string setuser_input =
        "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n";
    handler.process(setuser_input);

    std::string auth_input = "*3\r\n$4\r\nAUTH\r\n$7\r\ndefault\r\n$10\r\nmypassword\r\n";
    auto response = handler.process(auth_input);

    assert(response == "+OK\r\n");

    std::cout << "\u2713 Test passed: AUTH with correct password returns OK\n";
}

void test_auth_nonexistent_user_returns_wrongpass() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);
    std::string auth_input = "*3\r\n$4\r\nAUTH\r\n$11\r\nnonexistent\r\n$10\r\nmypassword\r\n";
    auto response = handler.process(auth_input);

    assert(response.starts_with("-WRONGPASS"));

    std::cout << "\u2713 Test passed: AUTH with nonexistent user returns WRONGPASS error\n";
}

void test_nopass_connection_auto_authenticated() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    auto result = handler.process_with_fd(10, "*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n", nullptr);
    assert(result.response == "$7\r\ndefault\r\n");

    std::cout << "\u2713 Test passed: nopass default user auto-authenticates new connection\n";
}

void test_password_set_new_connection_gets_noauth() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process_with_fd(
        10, "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n", nullptr);

    auto result = handler.process_with_fd(20, "*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n", nullptr);
    assert(result.response.starts_with("-NOAUTH"));

    std::cout << "\u2713 Test passed: new connection gets NOAUTH after password is set\n";
}

void test_existing_connection_remains_authenticated() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process_with_fd(10, "*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n", nullptr);

    handler.process_with_fd(
        10, "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n", nullptr);

    auto result = handler.process_with_fd(10, "*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n", nullptr);
    assert(result.response == "$7\r\ndefault\r\n");

    std::cout << "\u2713 Test passed: existing authenticated connection remains authenticated\n";
}

void test_auth_success_allows_commands() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process_with_fd(
        10, "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n", nullptr);

    auto auth_result =
        handler.process_with_fd(20, "*3\r\n$4\r\nAUTH\r\n$7\r\ndefault\r\n$10\r\nmypassword\r\n", nullptr);
    assert(auth_result.response == "+OK\r\n");

    auto whoami_result =
        handler.process_with_fd(20, "*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n", nullptr);
    assert(whoami_result.response == "$7\r\ndefault\r\n");

    std::cout << "\u2713 Test passed: AUTH success allows subsequent commands\n";
}

void test_auth_failure_keeps_noauth() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    handler.process_with_fd(
        10, "*4\r\n$3\r\nACL\r\n$7\r\nSETUSER\r\n$7\r\ndefault\r\n$11\r\n>mypassword\r\n", nullptr);

    auto auth_result = handler.process_with_fd(
        20, "*3\r\n$4\r\nAUTH\r\n$7\r\ndefault\r\n$12\r\nwrongpassword\r\n", nullptr);
    assert(auth_result.response.starts_with("-WRONGPASS"));

    auto whoami_result =
        handler.process_with_fd(20, "*2\r\n$3\r\nACL\r\n$6\r\nWHOAMI\r\n", nullptr);
    assert(whoami_result.response.starts_with("-NOAUTH"));

    std::cout << "\u2713 Test passed: AUTH failure keeps connection unauthenticated\n";
}

int main() {
    std::cout << "Running ACL tests...\n\n";

    test_acl_whoami_returns_default();
    test_acl_getuser_default_returns_flags_with_nopass();
    test_acl_setuser_with_password_returns_ok();
    test_acl_setuser_password_updates_getuser_response();
    test_auth_wrong_password_returns_wrongpass();
    test_auth_correct_password_returns_ok();
    test_auth_nonexistent_user_returns_wrongpass();
    test_nopass_connection_auto_authenticated();
    test_password_set_new_connection_gets_noauth();
    test_existing_connection_remains_authenticated();
    test_auth_success_allows_commands();
    test_auth_failure_keeps_noauth();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
