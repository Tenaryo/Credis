#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace credis::util {

enum class ErrorCode : uint8_t {
    kOk,
    kParseError,
    kNetworkError,
    kAuthError,
    kTypeError,
    kNotFound,
    kInternalError,
    kInvalidArgument,
    kUnknownCommand,
    kWrongNumberOfArgs,
    kWrongType,
    kTimeout,
    kProtocolError,
};

struct Error {
    ErrorCode code;
    std::string message;

    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {
    }

    [[nodiscard]] auto to_string() const -> const std::string& {
        return message;
    }
};

} // namespace credis::util
