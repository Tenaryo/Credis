#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "handler/transaction_state.hpp"

namespace credis::connection {

class Connection {
    int fd_{-1};
    std::vector<char> buffer_;
    size_t read_pos_{0};
    size_t data_len_{0};
    std::string pending_write_;
    bool authenticated_{false};
    bool dirty_{false};
    std::unique_ptr<credis::handler::TransactionState> tx_;

  public:
    explicit Connection(int fd);
    ~Connection();

    Connection(const Connection&) = delete;
    auto operator=(const Connection&) -> Connection& = delete;
    Connection(Connection&&) noexcept;
    auto operator=(Connection&&) noexcept -> Connection&;

    [[nodiscard]] auto fd() const noexcept -> int {
        return fd_;
    }
    auto handle_read() -> std::optional<std::string_view>;
    void consume(size_t n);
    [[nodiscard]] auto pending_write() noexcept -> std::string& {
        return pending_write_;
    }
    void send_data(const char* data, size_t len);
    void flush();
    [[nodiscard]] auto pending_data() const -> std::string_view {
        return pending_write_;
    }
    void clear_pending() {
        pending_write_.clear();
    }
    void trim_pending(size_t n) {
        pending_write_.erase(0, n);
    }

    auto authenticated() const noexcept -> bool {
        return authenticated_;
    }
    void set_authenticated(bool val) noexcept {
        authenticated_ = val;
    }

    [[nodiscard]] auto dirty() const noexcept -> bool {
        return dirty_;
    }
    void set_dirty(bool val) noexcept {
        dirty_ = val;
    }

    auto tx() noexcept -> credis::handler::TransactionState* {
        return tx_.get();
    }
    auto tx_or_create() -> credis::handler::TransactionState& {
        if (!tx_) {
            tx_ = std::make_unique<credis::handler::TransactionState>();
        }
        return *tx_;
    }
    void clear_tx() {
        tx_.reset();
    }

  private:
    void close();
    void compact();

    static constexpr size_t kInitialBufferSize = 4096;
    static constexpr size_t kMaxBufferSize = 512ULL * 1024 * 1024;
};

} // namespace credis::connection
