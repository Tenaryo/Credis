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

void test_ping_in_subscribed_mode() {
    Store store;
    CommandHandler handler(store);
    PubSubManager pubsub;
    handler.set_pubsub_manager(&pubsub);

    constexpr int kClientFd = 1;
    handler.process_with_fd(kClientFd, "*2\r\n$9\r\nsubscribe\r\n$3\r\nfoo\r\n", nullptr);

    auto result = handler.process_with_fd(kClientFd, "*1\r\n$4\r\nPING\r\n", nullptr);

    auto expected = "*2\r\n$4\r\npong\r\n$0\r\n\r\n";
    assert(result.response == expected);
    std::cout << "Test 3 passed: PING in subscribed mode returns [\"pong\", \"\"]\n";
}

void test_ping_in_normal_mode() {
    Store store;
    CommandHandler handler(store);

    auto response = handler.process("*1\r\n$4\r\nPING\r\n");
    assert(response == "+PONG\r\n");
    std::cout << "Test 4 passed: PING in normal mode returns +PONG\r\n";
}

void test_publish_returns_subscriber_count() {
    Store store;
    CommandHandler handler(store);
    PubSubManager pubsub;
    handler.set_pubsub_manager(&pubsub);

    handler.process_with_fd(1, "*2\r\n$9\r\nsubscribe\r\n$3\r\nbar\r\n", nullptr);
    handler.process_with_fd(2, "*2\r\n$9\r\nsubscribe\r\n$3\r\nbar\r\n", nullptr);
    handler.process_with_fd(3, "*2\r\n$9\r\nsubscribe\r\n$3\r\nfoo\r\n", nullptr);

    auto bar_result =
        handler.process_with_fd(10, "*3\r\n$7\r\nPUBLISH\r\n$3\r\nbar\r\n$3\r\nmsg\r\n", nullptr);
    assert(bar_result.response == RespParser::encode_integer(2));
    std::cout << "Test 5 passed: PUBLISH bar returns 2 (two subscribers)\n";

    auto foo_result =
        handler.process_with_fd(10, "*3\r\n$7\r\nPUBLISH\r\n$3\r\nfoo\r\n$3\r\nmsg\r\n", nullptr);
    assert(foo_result.response == RespParser::encode_integer(1));
    std::cout << "Test 6 passed: PUBLISH foo returns 1 (one subscriber)\n";
}

void test_publish_no_subscribers() {
    Store store;
    CommandHandler handler(store);
    PubSubManager pubsub;
    handler.set_pubsub_manager(&pubsub);

    auto result = handler.process_with_fd(
        10, "*3\r\n$7\r\nPUBLISH\r\n$10\r\nnonexistent\r\n$3\r\nmsg\r\n", nullptr);
    assert(result.response == RespParser::encode_integer(0));
    std::cout << "Test 7 passed: PUBLISH nonexistent returns 0\n";
}

void test_publish_wrong_number_of_args() {
    Store store;
    CommandHandler handler(store);

    auto no_args = handler.process("*1\r\n$7\r\nPUBLISH\r\n");
    assert(no_args.starts_with("-ERR wrong number of arguments for 'publish' command"));
    std::cout << "Test 8a passed: PUBLISH with no args returns error\n";

    auto one_arg = handler.process("*2\r\n$7\r\nPUBLISH\r\n$3\r\nfoo\r\n");
    assert(one_arg.starts_with("-ERR wrong number of arguments for 'publish' command"));
    std::cout << "Test 8b passed: PUBLISH with only channel returns error\n";
}

int main() {
    std::cout << "Running SUBSCRIBE/PUBLISH tests...\n\n";

    test_subscribe_single_channel();
    test_subscribed_mode_rejects_disallowed_commands();
    test_ping_in_subscribed_mode();
    test_ping_in_normal_mode();
    test_publish_returns_subscriber_count();
    test_publish_no_subscribers();
    test_publish_wrong_number_of_args();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
