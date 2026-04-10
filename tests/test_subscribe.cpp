#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/pubsub/pubsub_manager.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <iostream>

void test_subscribe_single_channel() {
    Store store;
    CommandHandler handler(store);

    auto response = handler.process("*2\r\n$9\r\nsubscribe\r\n$3\r\nfoo\r\n");

    auto expected = "*3\r\n" + RespParser::encode_bulk_string("subscribe") +
                    RespParser::encode_bulk_string("foo") + RespParser::encode_integer(1);
    assert(response == expected);

    std::cout << "Test 1 passed: SUBSCRIBE foo returns [\"subscribe\", \"foo\", 1]\n";
}

void test_subscribed_mode_rejects_disallowed_commands() {
    Store store;
    CommandHandler handler(store);
    PubSubManager pubsub;
    handler.set_pubsub_manager(&pubsub);

    constexpr int kClientFd = 1;
    handler.process_with_fd(kClientFd, "*2\r\n$9\r\nsubscribe\r\n$3\r\nfoo\r\n", nullptr);

    auto set_result = handler.process_with_fd(
        kClientFd, "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n", nullptr);
    assert(set_result.response.starts_with("-ERR Can't execute 'SET'"));
    std::cout << "Test 2a passed: SET rejected in subscribed mode\n";

    auto get_result =
        handler.process_with_fd(kClientFd, "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n", nullptr);
    assert(get_result.response.starts_with("-ERR Can't execute 'GET'"));
    std::cout << "Test 2b passed: GET rejected in subscribed mode\n";

    auto echo_result =
        handler.process_with_fd(kClientFd, "*2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n", nullptr);
    assert(echo_result.response.starts_with("-ERR Can't execute 'ECHO'"));
    std::cout << "Test 2c passed: ECHO rejected in subscribed mode\n";
}

int main() {
    std::cout << "Running SUBSCRIBE tests...\n\n";

    test_subscribe_single_channel();
    test_subscribed_mode_rejects_disallowed_commands();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
