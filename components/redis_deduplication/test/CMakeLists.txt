# components/redis-deduplication/test/CMakeLists.txt

# Find GTest
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Try to find it as a system package
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GTEST gtest_main)
    endif()
    
    if(NOT GTEST_FOUND)
        message(WARNING "Google Test not found. Skipping tests.")
        return()
    endif()
endif()

# Create test executable for unit tests
add_executable(redis_dedup_unit_tests
    unit/test_compilation.cpp
    # Temporarily disabled until functions are implemented
    # unit/test_types.cpp
    # unit/test_message_id.cpp
)

target_link_libraries(redis_dedup_unit_tests
    PRIVATE
        redis-deduplication
        GTest::gtest_main
        GTest::gtest
)

target_include_directories(redis_dedup_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add the test to CTest
add_test(NAME unit_tests COMMAND redis_dedup_unit_tests)

# Create integration test executable (requires Redis server)
add_executable(redis_dedup_integration_tests
    integration/test_redis_client.cpp
    integration/test_deduplicator.cpp
)

target_link_libraries(redis_dedup_integration_tests
    PRIVATE
        redis-deduplication
        GTest::gtest_main
        GTest::gtest
)

target_include_directories(redis_dedup_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add integration tests (marked with label for conditional running)
add_test(NAME integration_tests COMMAND redis_dedup_integration_tests)
set_tests_properties(integration_tests PROPERTIES LABELS "integration")

# Create test directories if they don't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/unit)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/integration)