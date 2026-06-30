#include "aof/aof_manager.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "util/logger.hpp"

namespace credis::aof {

using namespace std::chrono_literals;

void AofManager::set_appendfsync(std::string val) {
    if (val == appendfsync_) {
        return;
    }
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
    fsync_enabled_ = true;
    fsync_thread_ = std::jthread([this](const std::stop_token& st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(1s);
            if (fsync_enabled_.load(std::memory_order_acquire) && aof_fd_ >= 0) {
                if (::fsync(aof_fd_) < 0) [[unlikely]] {
                    LOG_ERROR("AOF background fsync failed");
                }
            }
        }
    });
}

void AofManager::stop_fsync_thread() {
    fsync_enabled_.store(false, std::memory_order_release);
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
    base_dir_ = base_dir;
    auto manifest_path = base_dir + "/" + appenddirname_ + "/" + appendfilename_ + ".manifest";
    std::ifstream mf(manifest_path);
    if (!mf) [[unlikely]] {
        return;
    }
    std::string file_token;
    std::string aof_name;
    std::string seq_token;
    int seq_num = 1;
    mf >> file_token >> aof_name >> seq_token >> seq_num;
    current_seq_ = seq_num;
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

void AofManager::append_to_rewrite_buffer(std::string_view data) {
    if (rewrite_in_progress_) {
        rewrite_buffer_ += data;
    }
}

auto AofManager::encode_resp_command(const std::vector<std::string>& args) -> std::string {
    std::string result;
    result += "*" + std::to_string(args.size()) + "\r\n";
    for (const auto& arg : args) {
        result += "$" + std::to_string(arg.size()) + "\r\n";
        result += arg;
        result += "\r\n";
    }
    return result;
}

void AofManager::rewrite_child_main(const credis::store::Store& store,
                                    const std::string& temp_path,
                                    const std::string& dir) {
    std::filesystem::create_directories(dir);

    int fd = ::open(temp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        _exit(1);
    }

    store.for_each_valid_entry([fd](std::string_view key, const credis::store::Store::Entry& entry) {
        std::vector<std::string> cmd;
        if (std::holds_alternative<credis::store::String>(entry.value)) {
            const auto& val = std::get<credis::store::String>(entry.value);
            cmd = {"SET", std::string(key), val};
            if (entry.expiry) {
                auto now = std::chrono::steady_clock::now();
                if (*entry.expiry > now) {
                    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(*entry.expiry - now).count();
                    cmd.emplace_back("PX");
                    cmd.push_back(std::to_string(remaining));
                }
            }
        } else if (std::holds_alternative<credis::store::List>(entry.value)) {
            const auto& list = std::get<credis::store::List>(entry.value);
            if (list.empty()) {
                return;
            }
            cmd = {"RPUSH", std::string(key)};
            for (const auto& elem : list) {
                cmd.push_back(elem);
            }
        } else if (std::holds_alternative<credis::store::Stream>(entry.value)) {
            const auto& stream = std::get<credis::store::Stream>(entry.value);
            for (const auto& s_entry : stream) {
                std::vector<std::string> xadd_cmd = {"XADD", std::string(key), s_entry.id};
                for (const auto& [f, v] : s_entry.fields) {
                    xadd_cmd.push_back(f);
                    xadd_cmd.push_back(v);
                }
                auto data = encode_resp_command(xadd_cmd);
                if (::write(fd, data.data(), data.size()) < 0) {
                    ::close(fd);
                    _exit(1);
                }
            }
            return;
        } else if (std::holds_alternative<credis::store::SortedSet>(entry.value)) {
            const auto& zset = std::get<credis::store::SortedSet>(entry.value);
            if (zset.entries.empty()) {
                return;
            }
            cmd = {"ZADD", std::string(key)};
            for (const auto& [score, member] : zset.entries) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.17g", score);
                cmd.emplace_back(buf);
                cmd.emplace_back(member);
            }
        }

        if (!cmd.empty()) {
            auto data = encode_resp_command(cmd);
            if (::write(fd, data.data(), data.size()) < 0) {
                ::close(fd);
                _exit(1);
            }
        }
    });

    ::close(fd);
    _exit(0);
}

auto AofManager::start_rewrite(const credis::store::Store& store, const std::string& base_dir) -> bool {
    if (rewrite_in_progress_ || appendonly_ != "yes") {
        return false;
    }

    base_dir_ = base_dir;
    auto new_seq = current_seq_ + 1;
    rewrite_temp_file_ = appenddirname_ + "/" + appendfilename_ + "." + std::to_string(new_seq) + ".incr.aof";
    auto temp_path = base_dir + "/" + rewrite_temp_file_;

    stop_fsync_thread();

    pid_t pid = ::fork();
    if (pid < 0) [[unlikely]] {
        start_fsync_thread();
        return false;
    }

    if (pid == 0) {
        rewrite_child_main(store, temp_path, base_dir + "/" + appenddirname_);
    }

    rewrite_child_pid_ = pid;
    rewrite_in_progress_ = true;
    rewrite_buffer_.clear();

    start_fsync_thread();
    return true;
}

auto AofManager::check_rewrite_complete() -> bool {
    if (!rewrite_in_progress_) {
        return false;
    }

    int status = 0;
    pid_t result = ::waitpid(rewrite_child_pid_, &status, WNOHANG);
    if (result == 0) {
        return false;
    }

    rewrite_child_pid_ = -1;

    if (result < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) [[unlikely]] {
        LOG_ERROR("AOF rewrite child failed");
        rewrite_in_progress_ = false;
        rewrite_buffer_.clear();
        return true;
    }

    if (!rewrite_buffer_.empty()) {
        auto buf_path = base_dir_ + "/" + rewrite_temp_file_;
        int fd = ::open(buf_path.c_str(), O_WRONLY | O_APPEND);
        if (fd >= 0) {
            auto written = ::write(fd, rewrite_buffer_.data(), rewrite_buffer_.size());
            if (written != static_cast<ssize_t>(rewrite_buffer_.size())) [[unlikely]] {
                LOG_ERROR("AOF rewrite buffer append failed");
            }
            ::close(fd);
        }
    }

    auto new_seq = current_seq_ + 1;
    auto manifest_path = base_dir_ + "/" + rewrite_temp_file_.substr(0, rewrite_temp_file_.rfind('/') + 1)
                         + appendfilename_ + ".manifest";
    std::ofstream mf(manifest_path);
    if (mf) {
        mf << "file " << appendfilename_ << "." << new_seq << ".incr.aof seq " << new_seq << " type i\n";
    }

    stop_fsync_thread();

    if (aof_fd_ >= 0) {
        ::close(aof_fd_);
        aof_fd_ = -1;
    }

    auto new_aof_path = base_dir_ + "/" + rewrite_temp_file_;
    aof_fd_ = ::open(new_aof_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    current_seq_ = new_seq;

    start_fsync_thread();

    rewrite_in_progress_ = false;
    rewrite_buffer_.clear();
    rewrite_temp_file_.clear();
    return true;
}

AofManager::~AofManager() {
    close();
}

} // namespace credis::aof
