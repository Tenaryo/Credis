#include "replica_connector.hpp"
#include "protocol/resp_parser.hpp"
#include <memory>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

ReplicaConnector::ReplicaConnector(std::string host, int port)
    : host_(std::move(host)), port_(port) {}

ReplicaConnector::~ReplicaConnector() {
    if (fd_ >= 0)
        ::close(fd_);
}

ReplicaConnector::ReplicaConnector(ReplicaConnector&& other) noexcept
    : host_(std::move(other.host_)), port_(other.port_), fd_(other.fd_) {
    other.fd_ = -1;
}

ReplicaConnector& ReplicaConnector::operator=(ReplicaConnector&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0)
            ::close(fd_);
        host_ = std::move(other.host_);
        port_ = other.port_;
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool ReplicaConnector::connect_to_master() {
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    if (::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result) != 0)
        return false;

    auto guard = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>(result, &freeaddrinfo);

    fd_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd_ < 0)
        return false;

    if (::connect(fd_, result->ai_addr, result->ai_addrlen) < 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    return true;
}

bool ReplicaConnector::send_and_expect(const std::vector<std::string>& args,
                                       std::string_view expected_response) {
    return send_and_check(
        args, [expected_response](std::string_view resp) { return resp == expected_response; });
}

template <typename Pred>
bool ReplicaConnector::send_and_check(const std::vector<std::string>& args, Pred&& pred) {
    if (fd_ < 0 && !connect_to_master())
        return false;

    auto msg = RespParser::encode_array(args);
    size_t sent = 0;
    while (sent < msg.size()) {
        auto n = ::send(fd_, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        sent += static_cast<size_t>(n);
    }

    char buf[256]{};
    auto n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0)
        return false;

    return pred(std::string_view(buf, static_cast<size_t>(n)));
}

bool ReplicaConnector::send_ping() { return send_and_expect({"PING"}, "+PONG\r\n"); }

bool ReplicaConnector::send_replconf(int listening_port) {
    if (!send_and_expect({"REPLCONF", "listening-port", std::to_string(listening_port)}, "+OK\r\n"))
        return false;
    return send_and_expect({"REPLCONF", "capa", "psync2"}, "+OK\r\n");
}

bool ReplicaConnector::send_psync() {
    return send_and_check({"PSYNC", "?", "-1"},
                          [](std::string_view resp) { return resp.starts_with("+FULLRESYNC"); });
}
