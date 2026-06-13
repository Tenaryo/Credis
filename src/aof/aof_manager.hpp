#pragma once

#include <string>

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
    void set_appendfsync(std::string val) {
        appendfsync_ = std::move(val);
    }

    void ensure_directory(const std::string& base_dir) const;
    void ensure_file(const std::string& base_dir) const;

  private:
    std::string appendonly_ = "no";
    std::string appenddirname_ = "appendonlydir";
    std::string appendfilename_ = "appendonly.aof";
    std::string appendfsync_ = "everysec";
};

} // namespace credis::aof
