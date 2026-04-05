#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <sys/select.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using namespace std::string_view_literals;

void send_resp(int fd, const std::vector<std::string>& args) {
    auto msg = RespParser::encode_array(args);
    size_t sent = 0;
    while (sent < msg.size()) {
        auto n = ::send(fd, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            break;
        sent += static_cast<size_t>(n);
    }
}

std::string recv_all(int fd, int timeout_ms = 100) {
    std::string result;
    char buf[4096];
    while (true) {
        struct timeval tv {
            .tv_sec = 0, .tv_usec = timeout_ms * 1000
        };
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        int n = ::select(fd + 1, &set, nullptr, nullptr, &tv);
        if (n <= 0)
            break;
        auto rd = ::read(fd, buf, sizeof(buf));
        if (rd <= 0)
            break;
        result.append(buf, static_cast<size_t>(rd));
    }
    return result;
}

int tcp_connect(int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    return fd;
}

} // namespace

void test_parse_one_single_command() {
    auto input = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"sv;
    auto result = RespParser::parse_one(input);
    assert(result);
    assert(result->args.size() == 3);
    assert(result->args[0] == "SET");
    assert(result->args[1] == "foo");
    assert(result->args[2] == "bar");
    assert(result->consumed == input.size());

    std::cout << "\u2713 Test passed: parse_one parses single RESP command\n";
}

void test_parse_one_incomplete_returns_error() {
    auto input = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n"sv;
    auto result = RespParser::parse_one(input);
    assert(!result);

    std::cout << "\u2713 Test passed: parse_one returns error for incomplete data\n";
}

void test_parse_one_multiple_commands_in_buffer() {
    auto cmd1 = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"sv;
    auto cmd2 = "*2\r\n$4\r\nINCR\r\n$3\r\nfoo\r\n"sv;
    std::string input = std::string(cmd1) + std::string(cmd2);

    auto result1 = RespParser::parse_one(input);
    assert(result1);
    assert(result1->args.size() == 3);
    assert(result1->args[0] == "SET");
    assert(result1->consumed == cmd1.size());

    auto remaining =
        std::string_view(input.data() + result1->consumed, input.size() - result1->consumed);
    auto result2 = RespParser::parse_one(remaining);
    assert(result2);
    assert(result2->args.size() == 2);
    assert(result2->args[0] == "INCR");
    assert(result2->consumed == cmd2.size());

    std::cout << "\u2713 Test passed: parse_one handles multiple commands in buffer\n";
}

void test_replica_applies_single_propagated_command() {
    int sv[2];
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    class FakeReplicaConnector {
        int fd_;
        std::string buffer_;
        CommandHandler& handler_;
      public:
        FakeReplicaConnector(int fd, CommandHandler& h) : fd_(fd), handler_(h) {}

        bool process_propagated_commands() {
            char buf[4096];
            auto n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0)
                return false;
            buffer_.append(buf, static_cast<size_t>(n));

            while (true) {
                auto result = RespParser::parse_one(buffer_);
                if (!result)
                    break;
                auto resp = std::string_view(buffer_.data(), result->consumed);
                handler_.process(resp);
                buffer_.erase(0, result->consumed);
            }
            return true;
        }
    };

    FakeReplicaConnector connector(sv[0], handler);

    send_resp(sv[1], {"SET", "foo", "bar"});
    connector.process_propagated_commands();

    auto val = store.get("foo");
    assert(val.has_value());
    assert(*val == "bar");

    ::close(sv[0]);
    ::close(sv[1]);

    std::cout << "\u2713 Test passed: replica applies single propagated SET command\n";
}

void test_replica_applies_multiple_commands_in_one_read() {
    int sv[2];
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    class FakeReplicaConnector {
        int fd_;
        std::string buffer_;
        CommandHandler& handler_;
      public:
        FakeReplicaConnector(int fd, CommandHandler& h) : fd_(fd), handler_(h) {}

        bool process_propagated_commands() {
            char buf[4096];
            auto n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0)
                return false;
            buffer_.append(buf, static_cast<size_t>(n));

            while (true) {
                auto result = RespParser::parse_one(buffer_);
                if (!result)
                    break;
                auto resp = std::string_view(buffer_.data(), result->consumed);
                handler_.process(resp);
                buffer_.erase(0, result->consumed);
            }
            return true;
        }
    };

    FakeReplicaConnector connector(sv[0], handler);

    auto cmd1 = RespParser::encode_array({"SET", "foo", "1"});
    auto cmd2 = RespParser::encode_array({"INCR", "foo"});
    auto combined = cmd1 + cmd2;
    ::send(sv[1], combined.c_str(), combined.size(), MSG_NOSIGNAL);

    connector.process_propagated_commands();

    auto val = store.get("foo");
    assert(val.has_value());
    assert(*val == "2");

    ::close(sv[0]);
    ::close(sv[1]);

    std::cout << "\u2713 Test passed: replica applies multiple commands in one TCP segment\n";
}

void test_replica_does_not_reply_to_master() {
    int sv[2];
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    class FakeReplicaConnector {
        int fd_;
        std::string buffer_;
        CommandHandler& handler_;
      public:
        FakeReplicaConnector(int fd, CommandHandler& h) : fd_(fd), handler_(h) {}

        bool process_propagated_commands() {
            char buf[4096];
            auto n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0)
                return false;
            buffer_.append(buf, static_cast<size_t>(n));

            while (true) {
                auto result = RespParser::parse_one(buffer_);
                if (!result)
                    break;
                auto resp = std::string_view(buffer_.data(), result->consumed);
                handler_.process(resp);
                buffer_.erase(0, result->consumed);
            }
            return true;
        }
    };

    FakeReplicaConnector connector(sv[0], handler);

    send_resp(sv[1], {"SET", "foo", "bar"});
    connector.process_propagated_commands();

    char buf[64]{};
    struct timeval tv {
        .tv_sec = 0, .tv_usec = 100000
    };
    ::setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    auto n = ::read(sv[1], buf, sizeof(buf));
    assert(n <= 0);

    ::close(sv[0]);
    ::close(sv[1]);

    std::cout << "\u2713 Test passed: replica does not reply to master\n";
}

void test_replica_handles_various_command_types() {
    int sv[2];
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    class FakeReplicaConnector {
        int fd_;
        std::string buffer_;
        CommandHandler& handler_;
      public:
        FakeReplicaConnector(int fd, CommandHandler& h) : fd_(fd), handler_(h) {}

        bool process_propagated_commands() {
            char buf[4096];
            auto n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0)
                return false;
            buffer_.append(buf, static_cast<size_t>(n));

            while (true) {
                auto result = RespParser::parse_one(buffer_);
                if (!result)
                    break;
                auto resp = std::string_view(buffer_.data(), result->consumed);
                handler_.process(resp);
                buffer_.erase(0, result->consumed);
            }
            return true;
        }
    };

    FakeReplicaConnector connector(sv[0], handler);

    auto combined = RespParser::encode_array({"SET", "key", "val"}) +
                    RespParser::encode_array({"INCR", "counter"}) +
                    RespParser::encode_array({"RPUSH", "mylist", "a"}) +
                    RespParser::encode_array({"LPUSH", "mylist", "b"});
    ::send(sv[1], combined.c_str(), combined.size(), MSG_NOSIGNAL);

    connector.process_propagated_commands();

    auto val = store.get("key");
    assert(val.has_value() && *val == "val");

    auto counter = store.get("counter");
    assert(counter.has_value() && *counter == "1");

    assert(store.llen("mylist") == 2);

    ::close(sv[0]);
    ::close(sv[1]);

    std::cout << "\u2713 Test passed: replica handles various command types\n";
}

int main() {
    std::cout << "Running replica command processing tests...\n\n";

    test_parse_one_single_command();
    test_parse_one_incomplete_returns_error();
    test_parse_one_multiple_commands_in_buffer();
    test_replica_applies_single_propagated_command();
    test_replica_applies_multiple_commands_in_one_read();
    test_replica_does_not_reply_to_master();
    test_replica_handles_various_command_types();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
