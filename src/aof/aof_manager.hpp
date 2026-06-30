#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <thread>

#include "store/store.hpp"

namespace credis::aof {

class AofManager {
  public:
    AofManager() = default;

    [[nodiscard]] auto appendonly() const noexcept -> const std::string& {
        return appendonly_;
    }
    [[nodiscard]] auto appenddirname() const noexcept -> const std::string& {
        return appenddirname_;
    }
    [[nodiscard]] auto appendfilename() const noexcept -> const std::string& {
        return appendfilename_;
    }
    [[nodiscard]] auto appendfsync() const noexcept -> const std::string& {
        return appendfsync_;
    }

    void set_appendonly(std::string val) {
        appendonly_ = std::move(val);
    }
    void set_appenddirname(std::string val) {
        appenddirname_ = std::move(val);
    }
    void set_appendfilename(std::string val) {
        appendfilename_ = std::move(val);
    }
    void set_appendfsync(std::string val);

    void ensure_directory(const std::string& base_dir) const;
    void ensure_file(const std::string& base_dir) const;
    void ensure_manifest(const std::string& base_dir) const;

    [[nodiscard]] auto read_aof_content(const std::string& base_dir) const -> std::string;

    void open(const std::string& base_dir);
    void append(std::string_view data);
    void close();

    [[nodiscard]] auto is_fsync_thread_running() const noexcept -> bool {
        return fsync_thread_.joinable();
    }

    auto start_rewrite(const credis::store::Store& store, const std::string& base_dir) -> bool;
    void append_to_rewrite_buffer(std::string_view data);
    auto check_rewrite_complete() -> bool;
    [[nodiscard]] auto is_rewriting() const noexcept -> bool {
        return rewrite_in_progress_;
    }

    ~AofManager();

  private:
    void start_fsync_thread();
    void stop_fsync_thread();

    static void
    rewrite_child_main(const credis::store::Store& store, const std::string& temp_path, const std::string& dir);
    static auto encode_resp_command(const std::vector<std::string>& args) -> std::string;

    std::string appendonly_ = "no";
    std::string appenddirname_ = "appendonlydir";
    std::string appendfilename_ = "appendonly.aof";
    std::string appendfsync_ = "everysec";
    int aof_fd_ = -1;
    int current_seq_ = 0;
    std::string base_dir_;

    std::jthread fsync_thread_;
    std::atomic<bool> fsync_enabled_{false};

    bool rewrite_in_progress_{false};
    pid_t rewrite_child_pid_{-1};
    std::string rewrite_buffer_;
    std::string rewrite_temp_file_;
};

} // namespace credis::aof
