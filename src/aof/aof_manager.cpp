#include "aof/aof_manager.hpp"

#include <filesystem>

namespace credis::aof {

void AofManager::ensure_directory(const std::string& base_dir) const {
    if (appendonly_ == "yes") {
        std::filesystem::create_directories(base_dir + "/" + appenddirname_);
    }
}

} // namespace credis::aof
