#include "aof/aof_manager.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

namespace credis::aof {

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
}

void AofManager::append(std::string_view data) {
    if (aof_fd_ < 0) [[unlikely]] {
        return;
    }
    ::write(aof_fd_, data.data(), data.size());
    if (appendfsync_ == "always") {
        ::fsync(aof_fd_);
    }
}

void AofManager::close() {
    if (aof_fd_ >= 0) {
        ::close(aof_fd_);
        aof_fd_ = -1;
    }
}

AofManager::~AofManager() {
    close();
}

} // namespace credis::aof
