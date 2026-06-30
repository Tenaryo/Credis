#include "aof/aof_manager.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "util/logger.hpp"

namespace credis::aof {

using namespace std::chrono_literals;

void AofManager::set_appendfsync(std::string val) {
    appendfsync_ = std::move(val);
    if (aof_fd_ >= 0) {
        stop_fsync_thread();
        start_fsync_thread();
    }
}

void AofManager::start_fsync_thread() {
    if (appendfsync_ != "everysec") {
        return;
    }
    fsync_running_ = true;
    fsync_thread_ = std::jthread([this](const std::stop_token& st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(1s);
            if (fsync_running_.load(std::memory_order_acquire) && aof_fd_ >= 0) {
                if (::fsync(aof_fd_) < 0) [[unlikely]] {
                    LOG_ERROR("AOF background fsync failed");
                }
            }
        }
    });
}

void AofManager::stop_fsync_thread() {
    fsync_running_.store(false, std::memory_order_release);
    if (fsync_thread_.joinable()) {
        fsync_thread_.request_stop();
        fsync_thread_.join();
    }
}

void AofManager::ensure_directory(const std::string& base_dir) const {
    if (appendonly_ == "yes") {
        std::filesystem::create_directories(base_dir + "/" + appenddirname_);
    }
}

void AofManager::ensure_file(const std::string& base_dir) const {
    if (appendonly_ == "yes") {
        auto path = base_dir + "/" + appenddirname_ + "/" + appendfilename_ + ".1.incr.aof";
        if (!std::filesystem::exists(path)) {
            std::ofstream file(path);
        }
    }
}

void AofManager::ensure_manifest(const std::string& base_dir) const {
    if (appendonly_ == "yes") {
        auto path = base_dir + "/" + appenddirname_ + "/" + appendfilename_ + ".manifest";
        if (!std::filesystem::exists(path)) {
            std::ofstream file(path);
            file << "file " << appendfilename_ << ".1.incr.aof seq 1 type i\n";
        }
    }
}

auto AofManager::read_aof_content(const std::string& base_dir) const -> std::string {
    if (appendonly_ != "yes") {
        return {};
    }
    auto manifest_path = base_dir + "/" + appenddirname_ + "/" + appendfilename_ + ".manifest";
    std::ifstream mf(manifest_path);
    if (!mf) [[unlikely]] {
        return {};
    }
    std::string file_token;
    std::string aof_name;
    mf >> file_token >> aof_name;
    auto aof_path = base_dir + "/" + appenddirname_ + "/" + aof_name;
    std::ifstream af(aof_path);
    if (!af) [[unlikely]] {
        return {};
    }
    return {std::istreambuf_iterator<char>(af), std::istreambuf_iterator<char>()};
}

void AofManager::open(const std::string& base_dir) {
    if (appendonly_ != "yes") {
        return;
    }
    auto manifest_path = base_dir + "/" + appenddirname_ + "/" + appendfilename_ + ".manifest";
    std::ifstream mf(manifest_path);
    if (!mf) [[unlikely]] {
        return;
    }
    std::string file_token;
    std::string aof_name;
    mf >> file_token >> aof_name;
    auto aof_path = base_dir + "/" + appenddirname_ + "/" + aof_name;
    aof_fd_ = ::open(aof_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (aof_fd_ >= 0) {
        start_fsync_thread();
    }
}

void AofManager::append(std::string_view data) {
    if (aof_fd_ < 0) [[unlikely]] {
        return;
    }
    auto written = ::write(aof_fd_, data.data(), data.size());
    if (written != static_cast<ssize_t>(data.size())) [[unlikely]] {
        LOG_ERROR("AOF write failed: " + std::to_string(written));
    }
    if (appendfsync_ == "always") {
        if (::fsync(aof_fd_) < 0) [[unlikely]] {
            LOG_ERROR("AOF fsync failed");
        }
    }
}

void AofManager::close() {
    stop_fsync_thread();
    if (aof_fd_ >= 0) {
        ::close(aof_fd_);
        aof_fd_ = -1;
    }
}

AofManager::~AofManager() {
    close();
}

} // namespace credis::aof
