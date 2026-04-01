#include "replica_connector.hpp"
#include "protocol/resp_parser.hpp"
#include <cstring>
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

bool ReplicaConnector::send_ping() {
    if (fd_ < 0 && !connect_to_master())
        return false;

    auto ping = RespParser::encode_array({"PING"});
    size_t sent = 0;
    while (sent < ping.size()) {
        auto n = ::send(fd_, ping.data() + sent, ping.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        sent += static_cast<size_t>(n);
    }

    char buf[256]{};
    auto n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0)
        return false;

    return std::string_view(buf, static_cast<size_t>(n)) == "+PONG\r\n";
}

bool ReplicaConnector::send_replconf([[maybe_unused]] int listening_port) { return false; }
