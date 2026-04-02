#include "replica_connector.hpp"
#include "protocol/resp_parser.hpp"
#include <charconv>
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
    : host_(std::move(other.host_)), port_(other.port_), fd_(other.fd_),
      pending_buffer_(std::move(other.pending_buffer_)) {
    other.fd_ = -1;
}

ReplicaConnector& ReplicaConnector::operator=(ReplicaConnector&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0)
            ::close(fd_);
        host_ = std::move(other.host_);
        port_ = other.port_;
        fd_ = other.fd_;
        pending_buffer_ = std::move(other.pending_buffer_);
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

    std::string_view resp(buf, static_cast<size_t>(n));
    if (!pred(resp))
        return false;

    return true;
}

bool ReplicaConnector::send_ping() { return send_and_expect({"PING"}, "+PONG\r\n"); }

bool ReplicaConnector::send_replconf(int listening_port) {
    if (!send_and_expect({"REPLCONF", "listening-port", std::to_string(listening_port)}, "+OK\r\n"))
        return false;
    return send_and_expect({"REPLCONF", "capa", "psync2"}, "+OK\r\n");
}

bool ReplicaConnector::send_psync() {
    if (fd_ < 0 && !connect_to_master())
        return false;

    auto msg = RespParser::encode_array({"PSYNC", "?", "-1"});
    size_t sent = 0;
    while (sent < msg.size()) {
        auto n = ::send(fd_, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        sent += static_cast<size_t>(n);
    }

    char buf[512]{};
    auto n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0)
        return false;

    std::string_view all(buf, static_cast<size_t>(n));
    if (!all.starts_with("+FULLRESYNC"))
        return false;

    auto crlf = all.find("\r\n");
    if (crlf == std::string_view::npos)
        return false;

    size_t remaining = all.size() - crlf - 2;
    if (remaining > 0)
        pending_buffer_.assign(buf + crlf + 2, remaining);

    return true;
}

std::optional<std::string> ReplicaConnector::receive_rdb() {
    if (fd_ < 0)
        return std::nullopt;

    std::string header_buf;

    auto find_crlf = [&]() -> size_t {
        for (size_t i = 0; i + 1 < header_buf.size(); ++i) {
            if (header_buf[i] == '\r' && header_buf[i + 1] == '\n')
                return i;
        }
        return std::string::npos;
    };

    if (!pending_buffer_.empty()) {
        header_buf = std::move(pending_buffer_);
        pending_buffer_.clear();
    }

    while (true) {
        auto crlf_pos = find_crlf();
        if (crlf_pos != std::string::npos) {
            if (header_buf.empty() || header_buf[0] != '$')
                return std::nullopt;

            int len = 0;
            auto [ptr, ec] =
                std::from_chars(header_buf.data() + 1, header_buf.data() + crlf_pos, len);
            if (ec != std::errc{} || len <= 0)
                return std::nullopt;

            size_t header_size = crlf_pos + 2;
            size_t available = header_buf.size() - header_size;

            std::string rdb_data(len, '\0');
            size_t copied = std::min(available, static_cast<size_t>(len));
            std::memcpy(rdb_data.data(), header_buf.data() + header_size, copied);

            while (copied < static_cast<size_t>(len)) {
                auto rd = ::read(fd_, rdb_data.data() + copied, len - copied);
                if (rd <= 0)
                    return std::nullopt;
                copied += static_cast<size_t>(rd);
            }

            return rdb_data;
        }

        char buf[256]{};
        auto n = ::read(fd_, buf, sizeof(buf));
        if (n <= 0)
            return std::nullopt;
        header_buf.append(buf, static_cast<size_t>(n));
    }
}
