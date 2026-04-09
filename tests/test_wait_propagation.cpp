#include "../src/connection/connection.hpp"
#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include "../src/util/parse.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
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

struct WaitState {
    int client_fd;
    int64_t target_offset;
    int64_t numreplicas;
    std::chrono::steady_clock::time_point deadline;
};

class TestServer {
    int server_fd_;
    int port_;
    Store store_;
    ServerConfig config_;
    CommandHandler handler_;
    std::unordered_map<int, std::unique_ptr<Connection>> conns_;
    std::unordered_set<int> replicas_;
    std::unordered_map<int, int64_t> replica_offsets_;
    std::unordered_map<int, std::string> replica_buffers_;
    int64_t master_offset_{0};
    std::optional<WaitState> wait_state_;
    std::atomic<bool> running_{true};
    std::thread thread_;

    int count_acked() const {
        if (!wait_state_)
            return 0;
        int count = 0;
        for (int rfd : replicas_) {
            auto it = replica_offsets_.find(rfd);
            if (it != replica_offsets_.end() && it->second >= wait_state_->target_offset)
                ++count;
        }
        return count;
    }

    void finish_wait(int count) {
        if (!wait_state_)
            return;
        auto resp = RespParser::encode_integer(count);
        if (auto it = conns_.find(wait_state_->client_fd); it != conns_.end())
            it->second->send_data(resp.c_str(), resp.size());
        wait_state_.reset();
    }

    void handle_replica_data(int fd) {
        char buf[4096];
        auto n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) {
            replicas_.erase(fd);
            replica_offsets_.erase(fd);
            replica_buffers_.erase(fd);
            conns_.erase(fd);
            return;
        }
        replica_buffers_[fd].append(buf, static_cast<size_t>(n));

        auto& buffer = replica_buffers_[fd];
        while (auto result = RespParser::parse_one(buffer)) {
            auto& args = result->args;
            if (args.size() >= 3 && to_upper(args[0]) == "REPLCONF" && to_upper(args[1]) == "ACK")
                replica_offsets_[fd] = std::stoll(args[2]);
            buffer.erase(0, result->consumed);
        }

        if (wait_state_) {
            int acked = count_acked();
            if (acked >= wait_state_->numreplicas)
                finish_wait(acked);
        }
    }

    void process_client(int fd, Connection& conn, std::string_view data) {
        auto result = handler_.process_with_fd(fd, data, nullptr);

        if (result.is_wait) {
            int acked = 0;
            for (int rfd : replicas_) {
                auto it = replica_offsets_.find(rfd);
                if (it != replica_offsets_.end() && it->second >= master_offset_)
                    ++acked;
            }

            if (master_offset_ == 0 || acked >= result.wait_numreplicas) {
                acked = static_cast<int>(replicas_.size());
                auto resp = RespParser::encode_integer(acked);
                conn.send_data(resp.c_str(), resp.size());
            } else {
                auto deadline = result.wait_timeout_ms == 0
                                    ? std::chrono::steady_clock::now()
                                    : std::chrono::steady_clock::now() +
                                          std::chrono::milliseconds(result.wait_timeout_ms);
                wait_state_.emplace(
                    WaitState{fd, master_offset_, result.wait_numreplicas, deadline});

                for (int rfd : replicas_) {
                    if (auto rit = conns_.find(rfd); rit != conns_.end()) {
                        auto getack = RespParser::encode_array({"REPLCONF", "GETACK", "*"});
                        rit->second->send_data(getack.c_str(), getack.size());
                    }
                }
            }
        } else {
            if (!result.should_block)
                conn.send_data(result.response.c_str(), result.response.size());
        }

        if (result.is_replica_handshake)
            replicas_.insert(fd);

        if (!result.propagate_args.empty()) {
            auto msg = RespParser::encode_array(result.propagate_args);
            master_offset_ += static_cast<int64_t>(msg.size());
            for (int rfd : replicas_) {
                if (auto rit = conns_.find(rfd); rit != conns_.end())
                    rit->second->send_data(msg.c_str(), msg.size());
            }
        }
    }
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
            if (n <= 0) {
                if (wait_state_ && std::chrono::steady_clock::now() >= wait_state_->deadline)
                    finish_wait(count_acked());
                continue;
            }

            if (FD_ISSET(server_fd_, &fds)) {
                struct sockaddr_in addr {};
                socklen_t len = sizeof(addr);
                int fd = ::accept(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len);
                if (fd >= 0)
                    conns_[fd] = std::make_unique<Connection>(fd);
            }

            std::vector<int> ready;
            for (const auto& [fd, conn] : conns_) {
                if (FD_ISSET(fd, &fds))
                    ready.push_back(fd);
            }

            for (int fd : ready) {
                if (replicas_.contains(fd)) {
                    handle_replica_data(fd);
                    continue;
                }

                auto it = conns_.find(fd);
                if (it == conns_.end())
                    continue;

                auto data = it->second->handle_read();
                if (!data) {
                    replicas_.erase(fd);
                    replica_offsets_.erase(fd);
                    replica_buffers_.erase(fd);
                    conns_.erase(it);
                    continue;
                }

                process_client(fd, *it->second, *data);
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

struct ReplicaState {
    int fd;
    int64_t offset{0};
    std::string buffer;

    explicit ReplicaState(int fd) : fd(fd) {}

    void drain_and_respond(int timeout_ms = 50) {
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
            buffer.append(buf, static_cast<size_t>(rd));
        }

        while (auto result = RespParser::parse_one(buffer)) {
            offset += static_cast<int64_t>(result->consumed);
            auto& args = result->args;
            if (args.size() >= 2 && to_upper(args[0]) == "REPLCONF" &&
                to_upper(args[1]) == "GETACK") {
                send_resp(fd, {"REPLCONF", "ACK", std::to_string(offset)});
            }
            buffer.erase(0, result->consumed);
        }
    }
};

} // namespace

void test_wait_with_replica_ack() {
    TestServer server;

    ReplicaState replica(handshake_replica(server.port()));
    int client_fd = tcp_connect(server.port());

    send_resp(client_fd, {"SET", "foo", "123"});
    assert(recv_all(client_fd) == "+OK\r\n");

    replica.drain_and_respond();

    send_resp(client_fd, {"WAIT", "1", "5000"});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    replica.drain_and_respond();

    auto resp = recv_all(client_fd, 3000);
    assert(resp == ":1\r\n");

    ::close(replica.fd);
    ::close(client_fd);

    std::cout << "\u2713 Test passed: WAIT with replica ACK returns confirmed count\n";
}

void test_wait_timeout_returns_partial() {
    TestServer server;

    ReplicaState replica(handshake_replica(server.port()));
    int client_fd = tcp_connect(server.port());

    send_resp(client_fd, {"SET", "foo", "bar"});
    recv_all(client_fd);

    replica.drain_and_respond();

    send_resp(client_fd, {"WAIT", "2", "200"});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    replica.drain_and_respond();

    auto resp = recv_all(client_fd, 2000);
    assert(resp == ":1\r\n");

    ::close(replica.fd);
    ::close(client_fd);

    std::cout << "\u2713 Test passed: WAIT timeout returns partial acknowledgment count\n";
}

void test_wait_no_writes_returns_replica_count() {
    TestServer server;

    ReplicaState replica(handshake_replica(server.port()));
    int client_fd = tcp_connect(server.port());

    send_resp(client_fd, {"WAIT", "1", "5000"});

    auto resp = recv_all(client_fd, 1000);
    assert(resp == ":1\r\n");

    ::close(replica.fd);
    ::close(client_fd);

    std::cout << "\u2713 Test passed: WAIT without writes returns replica count immediately\n";
}

int main() {
    std::cout << "Running WAIT propagation tests...\n\n";

    test_wait_with_replica_ack();
    test_wait_timeout_returns_partial();
    test_wait_no_writes_returns_replica_count();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
