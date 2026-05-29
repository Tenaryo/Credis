#include "replica_connector.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <charconv>
#include <cstring>
#include <memory>

#include "protocol/resp_codec.hpp"
#include "server/server_config.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::replica {

ReplicaConnector::ReplicaConnector(std::string host, int port) : host_(std::move(host)), port_(port) {
}

ReplicaConnector::~ReplicaConnector() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

ReplicaConnector::ReplicaConnector(ReplicaConnector&& other) noexcept
    : host_(std::move(other.host_)), port_(other.port_), fd_(other.fd_),
      pending_buffer_(std::move(other.pending_buffer_)), offset_(other.offset_) {
    other.fd_ = -1;
}

auto ReplicaConnector::operator=(ReplicaConnector&& other) noexcept -> ReplicaConnector& {
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        host_ = std::move(other.host_);
        port_ = other.port_;
        fd_ = other.fd_;
        pending_buffer_ = std::move(other.pending_buffer_);
        offset_ = other.offset_;
        other.fd_ = -1;
    }
    return *this;
}

auto ReplicaConnector::connect_to_master() -> bool {
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    if (::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result) != 0) [[unlikely]] {
        return false;
    }

    auto guard = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>(result, &freeaddrinfo);

    fd_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd_ < 0) [[unlikely]] {
        return false;
    }

    if (::connect(fd_, result->ai_addr, result->ai_addrlen) < 0) [[unlikely]] {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    return true;
}

auto ReplicaConnector::send_and_expect(const std::vector<std::string>& args,
                                       std::string_view expected_response) -> bool {
    return send_and_check(args, [expected_response](std::string_view resp) { return resp == expected_response; });
}

template <typename Pred>
auto ReplicaConnector::send_and_check(const std::vector<std::string>& args, Pred&& pred) -> bool {
    if (fd_ < 0 && !connect_to_master()) [[unlikely]] {
        return false;
    }

    auto msg = credis::protocol::encode_array(args);
    size_t sent = 0;
    while (sent < msg.size()) {
        auto n = ::send(fd_, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) [[unlikely]] {
            return false;
        }
        sent += static_cast<size_t>(n);
    }

    char buf[256]{};
    auto n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0) [[unlikely]] {
        return false;
    }

    std::string_view resp(buf, static_cast<size_t>(n));
    return static_cast<bool>(pred(resp));
}

auto ReplicaConnector::send_ping() -> bool {
    return send_and_expect({"PING"}, "+PONG\r\n");
}

auto ReplicaConnector::send_replconf(int listening_port) -> bool {
    if (!send_and_expect({"REPLCONF", "listening-port", std::to_string(listening_port)}, "+OK\r\n")) [[unlikely]] {
        return false;
    }
    return send_and_expect({"REPLCONF", "capa", "psync2"}, "+OK\r\n");
}

auto ReplicaConnector::send_psync() -> bool {
    if (fd_ < 0 && !connect_to_master()) [[unlikely]] {
        return false;
    }

    auto msg = credis::protocol::encode_array({"PSYNC", "?", "-1"});
    size_t sent = 0;
    while (sent < msg.size()) {
        auto n = ::send(fd_, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) [[unlikely]] {
            return false;
        }
        sent += static_cast<size_t>(n);
    }

    char buf[512]{};
    auto n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0) [[unlikely]] {
        return false;
    }

    std::string_view all(buf, static_cast<size_t>(n));
    if (!all.starts_with("+FULLRESYNC")) [[unlikely]] {
        return false;
    }

    auto crlf = all.find("\r\n");
    if (crlf == std::string_view::npos) [[unlikely]] {
        return false;
    }

    size_t remaining = all.size() - crlf - 2;
    if (remaining > 0) {
        pending_buffer_.assign(buf + crlf + 2, remaining);
    }

    return true;
}

auto ReplicaConnector::receive_rdb() -> std::optional<std::string> {
    if (fd_ < 0) [[unlikely]] {
        return std::nullopt;
    }

    std::string header_buf;

    auto find_crlf = [&]() -> size_t {
        for (size_t i = 0; i + 1 < header_buf.size(); ++i) {
            if (header_buf[i] == '\r' && header_buf[i + 1] == '\n') {
                return i;
            }
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
            if (header_buf.empty() || header_buf[0] != '$') [[unlikely]] {
                return std::nullopt;
            }

            int len = 0;
            auto [ptr, ec] = std::from_chars(header_buf.data() + 1, header_buf.data() + crlf_pos, len);
            if (ec != std::errc{} || len <= 0) [[unlikely]] {
                return std::nullopt;
            }

            size_t header_size = crlf_pos + 2;
            size_t available = header_buf.size() - header_size;

            std::string rdb_data(static_cast<size_t>(len), '\0');
            size_t copied = std::min(available, static_cast<size_t>(len));
            std::memcpy(rdb_data.data(), header_buf.data() + header_size, copied);

            while (copied < static_cast<size_t>(len)) {
                auto rd = ::read(fd_, rdb_data.data() + copied, static_cast<size_t>(len) - copied);
                if (rd <= 0) [[unlikely]] {
                    return std::nullopt;
                }
                copied += static_cast<size_t>(rd);
            }

            if (available > static_cast<size_t>(len)) {
                size_t extra_offset = header_size + static_cast<size_t>(len);
                pending_buffer_.assign(header_buf.data() + extra_offset, header_buf.size() - extra_offset);
            }

            return rdb_data;
        }

        char buf[256]{};
        auto n = ::read(fd_, buf, sizeof(buf));
        if (n <= 0) [[unlikely]] {
            return std::nullopt;
        }
        header_buf.append(buf, static_cast<size_t>(n));
    }
}

auto ReplicaConnector::process_buffer_impl() -> ProcessedBuffer {
    ProcessedBuffer result;
    while (true) {
        auto parsed = credis::protocol::parse_one(pending_buffer_);
        if (!parsed) {
            break;
        }

        bool is_getack = parsed->args.size() >= 2 && credis::util::to_upper(parsed->args[0]) == "REPLCONF"
                         && credis::util::to_upper(parsed->args[1]) == "GETACK";

        if (is_getack) {
            result.ack_responses += credis::protocol::encode_array({"REPLCONF", "ACK", std::to_string(offset_)});
        } else {
            result.commands.emplace_back(pending_buffer_.data(), parsed->consumed);
        }
        offset_ += static_cast<int64_t>(parsed->consumed);
        pending_buffer_.erase(0, parsed->consumed);
    }
    return result;
}

auto ReplicaConnector::process_propagated_commands() -> std::optional<ProcessedBuffer> {
    if (fd_ < 0) [[unlikely]] {
        return std::nullopt;
    }

    char buf[4096];
    auto n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0) [[unlikely]] {
        return std::nullopt;
    }

    pending_buffer_.append(buf, static_cast<size_t>(n));
    return process_buffer_impl();
}

auto ReplicaConnector::process_pending_buffer() -> ProcessedBuffer {
    return process_buffer_impl();
}

void ReplicaConnector::send_response(std::string_view data) const {
    size_t sent = 0;
    while (sent < data.size()) {
        auto n = ::send(fd_, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) [[unlikely]] {
            break;
        }
        sent += static_cast<size_t>(n);
    }
}

auto connect_if_replica(const credis::server::ServerConfig& config,
                        int listening_port) -> std::optional<ReplicaConnector> {
    const auto& replica = config.replica;
    if (!replica) {
        return std::nullopt;
    }
    auto connector = ReplicaConnector(replica->host, replica->port);
    if (!connector.send_ping() || !connector.send_replconf(listening_port) || !connector.send_psync()
        || !connector.receive_rdb().has_value()) [[unlikely]] {
        return std::nullopt;
    }
    return connector;
}

} // namespace credis::replica
