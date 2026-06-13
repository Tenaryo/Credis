include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE
)
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

enable_testing()

function (add_module_test MODULE_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES LIBRARIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_SOURCES)
        file(GLOB ARG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/test_*.cpp")
    endif ()

    add_executable(test_${MODULE_NAME} ${ARG_SOURCES})
    target_compile_features(test_${MODULE_NAME} PRIVATE cxx_std_23)
    target_link_libraries(test_${MODULE_NAME} PRIVATE ${ARG_LIBRARIES} GTest::gtest_main)
    target_compile_options(test_${MODULE_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion)
    gtest_discover_tests(test_${MODULE_NAME})
endfunction ()

add_module_test(store LIBRARIES credis_store credis_util)

add_module_test(protocol LIBRARIES credis_protocol credis_store credis_util)

file(GLOB HANDLER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/handler/test_*.cpp")
add_module_test(
    handler
    SOURCES
    ${HANDLER_SOURCES}
    LIBRARIES
    credis_handler
    credis_store
    credis_server
    credis_blocking_manager
    credis_pubsub
    credis_protocol
    credis_util
    credis_geo
    credis_rdb
)

add_module_test(rdb LIBRARIES credis_rdb credis_store credis_util)

add_module_test(
    replica
    LIBRARIES
    credis_replica
    credis_handler
    credis_store
    credis_server
    credis_protocol
    credis_util
)

add_module_test(client LIBRARIES credis_server credis_util credis_aof)

add_module_test(aof LIBRARIES credis_aof)

add_module_test(e2e LIBRARIES credis_core)
