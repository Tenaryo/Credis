cmake_minimum_required(VERSION 3.21)

project(redis-starter-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_executable(redis
    src/block_manager/blocking_manager.cpp
    src/connection/connection.cpp
    src/event_loop/event_loop.cpp
    src/handler/command_handler.cpp
    src/main.cpp
    src/protocol/resp_parser.cpp
    src/server/server.cpp
    src/store/store.cpp
)

target_compile_features(redis PUBLIC cxx_std_23)
target_include_directories(redis PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(redis PRIVATE Threads::Threads)

target_compile_options(redis PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
