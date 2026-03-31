#include "../src/block_manager/blocking_manager.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

void test_block_client_for_stream() {
    BlockingManager manager;

    manager.block_client_for_stream(
        10, "mystream", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));

    assert(manager.is_blocked(10));
    assert(manager.blocked_count() == 1);

    std::cout << "Test 1 passed: block_client_for_stream registers client\n";
}

void test_wake_client_no_match() {
    BlockingManager manager;

    manager.block_client_for_stream(
        10, "mystream", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));

    auto client = manager.wake_client_for_stream("mystream", "50-0");

    assert(!client.has_value());
    assert(manager.is_blocked(10));

    std::cout << "Test 2 passed: wake_client_for_stream returns nullopt when ID not greater\n";
}

void test_wake_client_with_match() {
    BlockingManager manager;

    manager.block_client_for_stream(
        10, "mystream", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));

    auto client = manager.wake_client_for_stream("mystream", "200-0");

    assert(client.has_value());
    assert(client->fd == 10);
    assert(client->key == "mystream");
    assert(!manager.is_blocked(10));

    std::cout << "Test 3 passed: wake_client_for_stream wakes client when ID is greater\n";
}

void test_multiple_clients_fifo() {
    BlockingManager manager;

    manager.block_client_for_stream(
        10, "stream", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));
    manager.block_client_for_stream(
        20, "stream", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));

    auto client1 = manager.wake_client_for_stream("stream", "200-0");
    assert(client1.has_value());
    assert(client1->fd == 10);

    auto client2 = manager.wake_client_for_stream("stream", "300-0");
    assert(client2.has_value());
    assert(client2->fd == 20);

    std::cout << "Test 4 passed: multiple clients are woken in FIFO order\n";
}

void test_different_last_ids() {
    BlockingManager manager;

    manager.block_client_for_stream(
        10, "stream", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));
    manager.block_client_for_stream(
        20, "stream", *StreamId::parse("200-0"), std::chrono::milliseconds(1000));

    auto client1 = manager.wake_client_for_stream("stream", "150-0");
    assert(client1.has_value());
    assert(client1->fd == 10);

    auto client2 = manager.wake_client_for_stream("stream", "250-0");
    assert(client2.has_value());
    assert(client2->fd == 20);

    auto client3 = manager.wake_client_for_stream("stream", "300-0");
    assert(!client3.has_value());

    std::cout << "Test 5 passed: clients with different last_ids are woken correctly\n";
}

void test_different_streams() {
    BlockingManager manager;

    manager.block_client_for_stream(
        10, "stream1", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));
    manager.block_client_for_stream(
        20, "stream2", *StreamId::parse("100-0"), std::chrono::milliseconds(1000));

    auto client1 = manager.wake_client_for_stream("stream1", "200-0");
    assert(client1.has_value());
    assert(client1->fd == 10);

    assert(manager.is_blocked(20));

    auto client2 = manager.wake_client_for_stream("stream2", "200-0");
    assert(client2.has_value());
    assert(client2->fd == 20);

    std::cout << "Test 6 passed: clients blocked on different streams are isolated\n";
}

void test_timeout_indefinite() {
    BlockingManager manager;

    manager.block_client_for_stream(
        10, "stream", *StreamId::parse("100-0"), std::chrono::milliseconds(0));

    auto expired = manager.get_expired_clients();
    assert(expired.empty());
    assert(manager.is_blocked(10));

    std::cout << "Test 7 passed: timeout=0 means indefinite wait\n";
}

int main() {
    std::cout << "Running XREAD BLOCK tests...\n\n";

    test_block_client_for_stream();
    test_wake_client_no_match();
    test_wake_client_with_match();
    test_multiple_clients_fifo();
    test_different_last_ids();
    test_different_streams();
    test_timeout_indefinite();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
