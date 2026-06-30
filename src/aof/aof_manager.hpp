#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <thread>

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

    ~AofManager();

    [[nodiscard]] auto is_fsync_thread_running() const noexcept -> bool {
        return fsync_thread_.joinable();
    }

  private:
    void start_fsync_thread();
    void stop_fsync_thread();

    std::string appendonly_ = "no";
    std::string appenddirname_ = "appendonlydir";
    std::string appendfilename_ = "appendonly.aof";
    std::string appendfsync_ = "everysec";
    int aof_fd_ = -1;

    std::jthread fsync_thread_;
    std::atomic<bool> fsync_enabled_{false};
};

} // namespace credis::aof
