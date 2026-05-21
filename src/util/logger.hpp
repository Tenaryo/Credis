#pragma once

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace credis::util {

enum class LogLevel : uint8_t { kDebug = 0, kInfo = 1, kWarn = 2, kError = 3 };

inline void log(LogLevel level, std::string_view file, int line, std::string_view msg) {
    static constexpr std::string_view kPrefixes[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    auto idx = static_cast<int>(level);
    std::cerr << "[" << kPrefixes[idx] << "] (" << file << ":" << line << ") " << msg << '\n';
}

} // namespace credis::util

#define LOG_DEBUG(msg) credis::util::log(credis::util::LogLevel::kDebug, __FILE__, __LINE__, msg)
#define LOG_INFO(msg) credis::util::log(credis::util::LogLevel::kInfo, __FILE__, __LINE__, msg)
#define LOG_WARN(msg) credis::util::log(credis::util::LogLevel::kWarn, __FILE__, __LINE__, msg)
#define LOG_ERROR(msg) credis::util::log(credis::util::LogLevel::kError, __FILE__, __LINE__, msg)
