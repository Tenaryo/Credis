cmake_minimum_required(VERSION 3.13)

project(redis-starter-cpp)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

set(CMAKE_CXX_STANDARD 23)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_path(ASIO_INCLUDE_DIR NAMES asio.hpp PATHS /usr/include)

add_executable(redis ${SOURCE_FILES})

target_include_directories(redis PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_include_directories(redis PRIVATE ${ASIO_INCLUDE_DIR})
target_link_libraries(redis PRIVATE Threads::Threads)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
