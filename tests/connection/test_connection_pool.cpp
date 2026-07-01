#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <string>

#include "connection/connection_pool.hpp"

using credis::connection::ConnectionPool;

namespace {

struct SocketPair {
    int server_fd{-1};
    int client_fd{-1};

    SocketPair() {
        std::array<int, 2> sv{};
        EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv.data()), 0);
        server_fd = sv[0];
        client_fd = sv[1];
    }

    void close_client() {
        if (client_fd >= 0) {
            ::close(client_fd);
            client_fd = -1;
        }
    }

    ~SocketPair() {
        close_client();
    }
};

auto recv_available(int fd) -> std::string {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    std::string out;
    std::array<char, 512> buf{};
    while (true) {
        ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
        if (n <= 0) {
            break;
        }
        out.append(buf.data(), static_cast<size_t>(n));
    }
    return out;
}

} // namespace

TEST(ConnectionPoolDirtyTest, SendToMarksConnectionDirty) {
    ConnectionPool pool;
    SocketPair a;
    SocketPair b;
    pool.add(a.server_fd);
    pool.add(b.server_fd);

    EXPECT_EQ(pool.dirty_count(), 0U);

    pool.send_to(a.server_fd, "x");
    EXPECT_EQ(pool.dirty_count(), 1U);

    pool.send_to(a.server_fd, "y");
    EXPECT_EQ(pool.dirty_count(), 1U); // dedup: same fd stays a single entry

    pool.send_to(b.server_fd, "z");
    EXPECT_EQ(pool.dirty_count(), 2U);
}

TEST(ConnectionPoolDirtyTest, FlushClearsDirtySet) {
    ConnectionPool pool;
    SocketPair a;
    pool.add(a.server_fd);

    pool.send_to(a.server_fd, "hello");
    EXPECT_EQ(pool.dirty_count(), 1U);

    pool.flush_all();
    EXPECT_EQ(pool.dirty_count(), 0U);
}

TEST(ConnectionPoolDirtyTest, GetPendingWriteMarksDirty) {
    ConnectionPool pool;
    SocketPair a;
    pool.add(a.server_fd);

    pool.get_pending_write(a.server_fd) += "+OK\r\n";
    EXPECT_EQ(pool.dirty_count(), 1U);

    pool.flush_all();
    EXPECT_EQ(pool.dirty_count(), 0U);
}

TEST(ConnectionPoolDirtyTest, FlushSendsPendingData) {
    ConnectionPool pool;
    SocketPair a;
    pool.add(a.server_fd);

    pool.send_to(a.server_fd, "+OK\r\n");
    pool.flush_all();

    EXPECT_EQ(recv_available(a.client_fd), "+OK\r\n");
}

TEST(ConnectionPoolDirtyTest, UnwrittenConnectionSendsNothing) {
    ConnectionPool pool;
    SocketPair a;
    SocketPair b;
    pool.add(a.server_fd);
    pool.add(b.server_fd);

    pool.send_to(a.server_fd, "data-a");
    pool.flush_all();

    EXPECT_EQ(recv_available(a.client_fd), "data-a");
    EXPECT_EQ(recv_available(b.client_fd), ""); // b was never written to
}

TEST(ConnectionPoolDirtyTest, RemoveAfterDirtyIsSafe) {
    ConnectionPool pool;
    SocketPair a;
    pool.add(a.server_fd);

    pool.send_to(a.server_fd, "x");
    EXPECT_EQ(pool.dirty_count(), 1U);

    pool.remove(a.server_fd); // removed while still in dirty set
    EXPECT_NO_THROW(pool.flush_all());
    EXPECT_EQ(pool.dirty_count(), 0U);
}
