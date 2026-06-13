#include "aof/aof_manager.hpp"

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
        std::ofstream file(path);
    }
}

} // namespace credis::aof
