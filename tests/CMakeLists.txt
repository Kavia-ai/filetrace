cmake_minimum_required(VERSION 3.15)

# Add test executable
add_executable(filetrace_tests
    test_process_hierarchy.cpp
    test_thread_termination.cpp
    test_file_monitoring.cpp
)

# Link against Google Test libraries
target_link_libraries(filetrace_tests
    PRIVATE
    gtest
    gtest_main
)

# Include directories for test files
target_include_directories(filetrace_tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Add test to CTest
add_test(NAME filetrace_tests COMMAND filetrace_tests)

# Set test properties
set_tests_properties(filetrace_tests
    PROPERTIES
    ENVIRONMENT "GTEST_COLOR=1"
    FAIL_REGULAR_EXPRESSION ".*[Ff]ailed.*"
)
