#include "../src/connection/connection.hpp"
#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cassert>
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

class TestServer {
    int server_fd_;
    int port_;
    Store store_;
    ServerConfig config_;
    CommandHandler handler_;
    std::unordered_map<int, std::unique_ptr<Connection>> conns_;
    std::unordered_set<int> replicas_;
    std::atomic<bool> running_{true};
    std::thread thread_;
  public:
    TestServer() : config_{}, handler_(store_, config_) {
        server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        int reuse = 1;
        ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(0);
        ::bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        ::listen(server_fd_, 10);

        socklen_t len = sizeof(addr);
        ::getsockname(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len);
        port_ = ntohs(addr.sin_port);

        thread_ = std::thread([this] { run(); });
    }

    ~TestServer() {
        running_ = false;
        if (thread_.joinable())
            thread_.join();
        conns_.clear();
        ::close(server_fd_);
    }

    int port() const { return port_; }
  private:
    void run() {
        while (running_) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(server_fd_, &fds);
            int max_fd = server_fd_;

            for (const auto& [fd, conn] : conns_) {
                FD_SET(fd, &fds);
                if (fd > max_fd)
                    max_fd = fd;
            }

            struct timeval tv {
                .tv_sec = 0, .tv_usec = 10000
            };
            int n = ::select(max_fd + 1, &fds, nullptr, nullptr, &tv);
            if (n <= 0)
                continue;

            if (FD_ISSET(server_fd_, &fds)) {
                struct sockaddr_in addr {};
                socklen_t len = sizeof(addr);
                int fd = ::accept(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len);
                if (fd >= 0) {
                    conns_[fd] = std::make_unique<Connection>(fd);
                }
            }

            std::vector<int> ready;
            for (const auto& [fd, conn] : conns_) {
                if (FD_ISSET(fd, &fds))
                    ready.push_back(fd);
            }

            for (int fd : ready) {
                auto it = conns_.find(fd);
                if (it == conns_.end())
                    continue;

                auto data = it->second->handle_read();
                if (!data) {
                    replicas_.erase(fd);
                    conns_.erase(it);
                    continue;
                }

                auto result = handler_.process_with_fd(fd, *data, nullptr);
                if (!std::holds_alternative<ProcessResult::Block>(result.state)) {
                    std::string resp;
                    if (std::holds_alternative<ProcessResult::Normal>(result.state)) {
                        resp = std::get<ProcessResult::Normal>(result.state).response;
                    } else {
                        resp = std::get<ProcessResult::ReplicaHandshake>(result.state).response;
                    }
                    it->second->send_data(resp.c_str(), resp.size());
                }

                if (std::holds_alternative<ProcessResult::ReplicaHandshake>(result.state)) {
                    replicas_.insert(fd);
                }

                if (!result.propagate_args.empty()) {
                    auto msg = RespParser::encode_array(result.propagate_args);
                    for (int rfd : replicas_) {
                        if (auto rit = conns_.find(rfd); rit != conns_.end()) {
                            rit->second->send_data(msg.c_str(), msg.size());
                        }
                    }
                }
            }
        }
    }
};

int handshake_replica(int port) {
    int fd = tcp_connect(port);

    send_resp(fd, {"PING"});
    assert(recv_all(fd) == "+PONG\r\n");

    send_resp(fd, {"REPLCONF", "listening-port", "6380"});
    assert(recv_all(fd) == "+OK\r\n");

    send_resp(fd, {"REPLCONF", "capa", "psync2"});
    assert(recv_all(fd) == "+OK\r\n");

    send_resp(fd, {"PSYNC", "?", "-1"});
    auto psync_resp = recv_all(fd);
    assert(psync_resp.starts_with("+FULLRESYNC"));

    return fd;
}

} // namespace

void test_psync_marks_replica_connection() {
    Store store;
    ServerConfig config;
    CommandHandler handler(store, config);

    auto input = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
    auto result = handler.process_with_fd(42, input, nullptr);
    assert(std::holds_alternative<ProcessResult::ReplicaHandshake>(result.state));
    assert(result.propagate_args.empty());

    std::cout << "\u2713 Test passed: PSYNC marks connection as replica\n";
}

void test_set_propagated_to_replica() {
    TestServer server;

    int replica_fd = handshake_replica(server.port());
    int client_fd = tcp_connect(server.port());

    send_resp(client_fd, {"SET", "foo", "1"});
    auto resp = recv_all(client_fd);
    assert(resp == "+OK\r\n");

    auto propagated = recv_all(replica_fd);
    assert(propagated == "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$1\r\n1\r\n");

    ::close(replica_fd);
    ::close(client_fd);

    std::cout << "\u2713 Test passed: SET command propagated to replica\n";
}

void test_multiple_writes_propagated_in_order() {
    TestServer server;

    int replica_fd = handshake_replica(server.port());
    int client_fd = tcp_connect(server.port());

    send_resp(client_fd, {"SET", "foo", "1"});
    recv_all(client_fd);

    send_resp(client_fd, {"SET", "bar", "2"});
    recv_all(client_fd);

    send_resp(client_fd, {"SET", "baz", "3"});
    recv_all(client_fd);

    auto propagated = recv_all(replica_fd);
    assert(propagated == "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$1\r\n1\r\n"
                         "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$1\r\n2\r\n"
                         "*3\r\n$3\r\nSET\r\n$3\r\nbaz\r\n$1\r\n3\r\n");

    ::close(replica_fd);
    ::close(client_fd);

    std::cout << "\u2713 Test passed: multiple write commands propagated in order\n";
}

void test_read_commands_not_propagated() {
    TestServer server;

    int replica_fd = handshake_replica(server.port());
    int client_fd = tcp_connect(server.port());

    send_resp(client_fd, {"GET", "foo"});
    recv_all(client_fd);

    send_resp(client_fd, {"PING"});
    recv_all(client_fd);

    send_resp(client_fd, {"ECHO", "hello"});
    recv_all(client_fd);

    auto propagated = recv_all(replica_fd, 50);
    assert(propagated.empty());

    ::close(replica_fd);
    ::close(client_fd);

    std::cout << "\u2713 Test passed: read commands not propagated to replica\n";
}

int main() {
    std::cout << "Running command propagation tests...\n\n";

    test_psync_marks_replica_connection();
    test_set_propagated_to_replica();
    test_multiple_writes_propagated_in_order();
    test_read_commands_not_propagated();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
