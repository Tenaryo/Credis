cmake_minimum_required(VERSION 3.21)

project(
    Credis
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_library(
    redis_core STATIC
    src/blocking_manager/blocking_manager.cpp
    src/connection/connection.cpp
    src/connection/connection_pool.cpp
    src/event_loop/event_loop.cpp
    src/handler/command_handler.cpp
    src/handler/geo_commands.cpp
    src/handler/list_commands.cpp
    src/handler/pubsub_commands.cpp
    src/handler/server_commands.cpp
    src/handler/stream_commands.cpp
    src/handler/string_commands.cpp
    src/handler/zset_commands.cpp
    src/protocol/resp_parser.cpp
    src/rdb/rdb_parser.cpp
    src/replica/replica_connector.cpp
    src/replica/replica_manager.cpp
    src/server/event_dispatch.cpp
    src/server/server.cpp
    src/store/store.cpp
    src/util/sha256.cpp
)

target_compile_features(redis_core PUBLIC cxx_std_23)
target_include_directories(redis_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(redis_core PUBLIC Threads::Threads)

target_compile_options(
    redis_core
    PRIVATE -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wconversion
            $<$<CONFIG:Debug>:-g
            -O0>
            $<$<CONFIG:Release>:-O3>
)

target_precompile_headers(
    redis_core
    PUBLIC
    <charconv>
    <chrono>
    <deque>
    <expected>
    <functional>
    <optional>
    <set>
    <string>
    <unordered_map>
    <unordered_set>
    <variant>
    <vector>
)

add_executable(redis src/main.cpp)
target_link_libraries(redis PRIVATE redis_core)

option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)

if (ENABLE_SANITIZERS)
    target_compile_options(redis_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(redis_core PUBLIC -fsanitize=address,undefined)
endif ()

option(BUILD_TESTS "Build tests" OFF)
