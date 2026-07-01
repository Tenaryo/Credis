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

# ============================================================
# Module library targets (bottom-up, from leaf to root)
# ============================================================

# --- L0: zero-dependency STATIC libraries ---

add_library(credis_util STATIC src/util/sha256.cpp)
target_include_directories(credis_util PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(credis_aof STATIC src/aof/aof_manager.cpp)
target_include_directories(credis_aof PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(credis_aof PUBLIC credis_store)

add_library(credis_event_loop STATIC src/event_loop/event_loop.cpp)
target_include_directories(credis_event_loop PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(credis_connection STATIC src/connection/connection.cpp src/connection/connection_pool.cpp)
target_include_directories(credis_connection PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(credis_server STATIC src/server/server.cpp)
target_include_directories(credis_server PUBLIC ${CMAKE_SOURCE_DIR}/src)

# --- L0: INTERFACE (header-only) libraries ---

add_library(credis_geo INTERFACE)
target_include_directories(credis_geo INTERFACE ${CMAKE_SOURCE_DIR}/src)

# --- L1: depends on L0 ---

add_library(credis_store STATIC src/store/store.cpp)
target_include_directories(credis_store PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(credis_store PUBLIC credis_util)

add_library(credis_pubsub INTERFACE)
target_include_directories(credis_pubsub INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(credis_pubsub INTERFACE credis_util)

# --- L2: depends on store ---

add_library(credis_protocol STATIC src/protocol/resp_codec.cpp)
target_include_directories(credis_protocol PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(credis_protocol PUBLIC credis_store)

add_library(credis_rdb STATIC src/rdb/rdb_parser.cpp)
target_include_directories(credis_rdb PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(credis_rdb PUBLIC credis_store)

add_library(credis_blocking_manager STATIC src/blocking_manager/blocking_manager.cpp)
target_include_directories(credis_blocking_manager PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(credis_blocking_manager PUBLIC credis_store)

# --- L3: replica (depends on protocol + server + util) ---

add_library(credis_replica STATIC src/replica/replica_connector.cpp src/replica/replica_manager.cpp)
target_include_directories(credis_replica PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
    credis_replica
    PUBLIC credis_server
    PRIVATE credis_protocol credis_util
)

# --- L4: handler (depends on store + protocol + server + blocking + pubsub + util + geo + rdb) ---

add_library(
    credis_handler STATIC
    src/handler/command_handler.cpp
    src/handler/geo_commands.cpp
    src/handler/list_commands.cpp
    src/handler/pubsub_commands.cpp
    src/handler/server_commands.cpp
    src/handler/stream_commands.cpp
    src/handler/string_commands.cpp
    src/handler/zset_commands.cpp
)
target_include_directories(credis_handler PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
    credis_handler
    PUBLIC credis_store credis_server credis_blocking_manager credis_pubsub credis_aof
    PRIVATE credis_protocol credis_util credis_geo credis_rdb credis_connection
)

# --- L5: event dispatch (integration glue) ---

add_library(credis_event_dispatch STATIC src/server/event_dispatch.cpp)
target_include_directories(credis_event_dispatch PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
    credis_event_dispatch
    PRIVATE credis_handler
            credis_connection
            credis_event_loop
            credis_replica
            credis_blocking_manager
            credis_pubsub
            credis_protocol
            credis_server
            credis_store
)

# ============================================================
# Top-level aggregate (INTERFACE, links everything together)
# ============================================================

add_library(credis_core INTERFACE)
target_link_libraries(
    credis_core
    INTERFACE credis_util
              credis_aof
              credis_geo
              credis_event_loop
              credis_connection
              credis_store
              credis_pubsub
              credis_protocol
              credis_rdb
              credis_blocking_manager
              credis_replica
              credis_server
              credis_handler
              credis_event_dispatch
              Threads::Threads
)

target_compile_features(credis_core INTERFACE cxx_std_23)
target_include_directories(credis_core INTERFACE ${CMAKE_SOURCE_DIR}/src)

target_compile_options(
    credis_core
    INTERFACE -Wall
              -Wextra
              -Wpedantic
              -Werror
              -Wshadow
              -Wconversion
              $<$<CONFIG:Debug>:-g
              -O0>
              $<$<CONFIG:Release>:-O3
              -march=native>
)

# ============================================================
# Precompiled headers (base on credis_util, reused by others)
# ============================================================

target_precompile_headers(
    credis_util
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

target_precompile_headers(credis_event_loop REUSE_FROM credis_util)
target_precompile_headers(credis_aof REUSE_FROM credis_util)
target_precompile_headers(credis_connection REUSE_FROM credis_util)
target_precompile_headers(credis_server REUSE_FROM credis_util)
target_precompile_headers(credis_store REUSE_FROM credis_util)
target_precompile_headers(credis_protocol REUSE_FROM credis_util)
target_precompile_headers(credis_rdb REUSE_FROM credis_util)
target_precompile_headers(credis_blocking_manager REUSE_FROM credis_util)
target_precompile_headers(credis_replica REUSE_FROM credis_util)
target_precompile_headers(credis_handler REUSE_FROM credis_util)
target_precompile_headers(credis_event_dispatch REUSE_FROM credis_util)

# ============================================================
# Executable
# ============================================================

add_executable(redis src/main.cpp)
target_link_libraries(redis PRIVATE credis_core)

# ============================================================
# Options
# ============================================================

option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

option(ENABLE_PGO "Enable Profile-Guided Optimization (use existing .gcda)" OFF)
if (ENABLE_PGO)
    target_compile_options(credis_core INTERFACE -fprofile-use -fprofile-correction)
    target_link_options(credis_core INTERFACE -fprofile-use -fprofile-correction)
endif ()

if (ENABLE_SANITIZERS)
    target_compile_options(credis_core INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(credis_core INTERFACE -fsanitize=address,undefined)
endif ()

if (ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif ()

option(BUILD_TESTS "Build tests" OFF)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
