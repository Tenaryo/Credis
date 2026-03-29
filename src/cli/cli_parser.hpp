#pragma once

#include <cstring>
#include <stdexcept>

inline int parse_port(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            return std::stoi(argv[i + 1]);
        }
    }
    return 6379;
}
