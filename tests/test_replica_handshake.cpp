#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/replica/replica_connector.hpp"
#include "../src/server/server_config.hpp"
#include "../src/store/store.hpp"
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

class MockMaster {
    int server_fd_{-1};
    int port_{0};
  public:
    MockMaster() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        assert(server_fd_ >= 0);

        int reuse = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(0);

        assert(bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);
        assert(listen(server_fd_, 1) == 0);

        socklen_t len = sizeof(addr);
        getsockname(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len);
        port_ = ntohs(addr.sin_port);
    }

    ~MockMaster() {
        if (server_fd_ >= 0)
            ::close(server_fd_);
    }

    [[nodiscard]] int port() const noexcept { return port_; }

    struct HandshakeResult {
        std::string received;
        bool accepted{false};
    };

    struct MultiStepResult {
        std::vector<std::string> messages;
        bool accepted{false};
    };

    HandshakeResult run_ping_handshake() {
        HandshakeResult result;
        struct sockaddr_in client_addr {};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
        if (client_fd < 0)
            return result;

        result.accepted = true;

        char buf[256]{};
        auto n = ::read(client_fd, buf, sizeof(buf));
        if (n > 0)
            result.received = std::string(buf, static_cast<size_t>(n));

        const char* pong = "+PONG\r\n";
        ::send(client_fd, pong, std::strlen(pong), 0);
        ::close(client_fd);
        return result;
    }

    MultiStepResult run_multi_step_handshake(const std::vector<std::string>& responses) {
        MultiStepResult result;
        struct sockaddr_in client_addr {};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
        if (client_fd < 0)
            return result;

        result.accepted = true;

        for (const auto& resp : responses) {
            char buf[512]{};
            auto n = ::read(client_fd, buf, sizeof(buf));
            if (n > 0)
                result.messages.emplace_back(buf, static_cast<size_t>(n));
            ::send(client_fd, resp.c_str(), resp.size(), 0);
        }

        ::close(client_fd);
        return result;
    }
};

void test_replica_ping_handshake_with_localhost() {
    MockMaster master;
    int port = master.port();

    MockMaster::HandshakeResult server_result;

    std::thread server_thread([&]() { server_result = master.run_ping_handshake(); });

    ReplicaConnector connector("localhost", port);
    bool success = connector.send_ping();

    server_thread.join();

    assert(success);
    assert(server_result.accepted);
    assert(server_result.received == "*1\r\n$4\r\nPING\r\n");

    std::cout << "\u2713 Test passed: replica connects via hostname localhost and completes PING "
                 "handshake\n";
}

void test_replica_ping_handshake() {
    MockMaster master;
    int port = master.port();

    MockMaster::HandshakeResult server_result;

    std::thread server_thread([&]() { server_result = master.run_ping_handshake(); });

    ReplicaConnector connector("127.0.0.1", port);
    bool success = connector.send_ping();

    server_thread.join();

    assert(success);
    assert(server_result.accepted);
    assert(server_result.received == "*1\r\n$4\r\nPING\r\n");

    std::cout << "\u2713 Test passed: replica sends RESP-encoded PING and receives PONG\n";
}

void test_replica_sends_replconf_listening_port() {
    MockMaster master;
    int port = master.port();
    int replica_port = 6380;

    MockMaster::MultiStepResult server_result;

    std::thread server_thread([&]() {
        server_result = master.run_multi_step_handshake({"+PONG\r\n", "+OK\r\n"});
    });

    ReplicaConnector connector("127.0.0.1", port);
    assert(connector.send_ping());
    assert(connector.send_replconf(replica_port));

    server_thread.join();

    assert(server_result.accepted);
    assert(server_result.messages.size() >= 2);
    assert(server_result.messages[1] ==
           "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n6380\r\n");

    std::cout << "\u2713 Test passed: replica sends REPLCONF listening-port correctly\n";
}

void test_replica_sends_replconf_capa() {
    MockMaster master;
    int port = master.port();
    int replica_port = 6380;

    MockMaster::MultiStepResult server_result;

    std::thread server_thread([&]() {
        server_result = master.run_multi_step_handshake({"+PONG\r\n", "+OK\r\n", "+OK\r\n"});
    });

    ReplicaConnector connector("127.0.0.1", port);
    assert(connector.send_ping());
    assert(connector.send_replconf(replica_port));

    server_thread.join();

    assert(server_result.accepted);
    assert(server_result.messages.size() == 3);
    assert(server_result.messages[2] == "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n");

    std::cout << "\u2713 Test passed: replica sends REPLCONF capa psync2 correctly\n";
}

void test_master_handles_replconf_listening_port() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n6380\r\n";
    auto response = handler.process(input);

    assert(response == "+OK\r\n");

    std::cout << "\u2713 Test passed: master responds +OK to REPLCONF listening-port\n";
}

void test_master_handles_replconf_capa() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    std::string input = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
    auto response = handler.process(input);

    assert(response == "+OK\r\n");

    std::cout << "\u2713 Test passed: master responds +OK to REPLCONF capa psync2\n";
}

int main() {
    std::cout << "Running replica handshake tests...\n\n";

    test_replica_ping_handshake();
    test_replica_ping_handshake_with_localhost();
    test_replica_sends_replconf_listening_port();
    test_replica_sends_replconf_capa();
    test_master_handles_replconf_listening_port();
    test_master_handles_replconf_capa();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
