file(GLOB TEST_SOURCES *.cpp)

foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name}
        ${test_source}
        ${CMAKE_SOURCE_DIR}/src/block_manager/blocking_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/handler/command_handler.cpp
        ${CMAKE_SOURCE_DIR}/src/protocol/resp_parser.cpp
        ${CMAKE_SOURCE_DIR}/src/store/store.cpp
    )
    target_link_libraries(${test_name} PRIVATE Threads::Threads)
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(${test_name} PRIVATE -UNDEBUG -g -O0)
endforeach()
