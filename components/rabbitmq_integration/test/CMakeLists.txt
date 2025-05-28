# test/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find test dependencies
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # Try to find Google Test manually
    find_path(GTEST_INCLUDE_DIR gtest/gtest.h)
    find_library(GTEST_LIBRARY gtest)
    find_library(GTEST_MAIN_LIBRARY gtest_main)
    
    if(GTEST_INCLUDE_DIR AND GTEST_LIBRARY AND GTEST_MAIN_LIBRARY)
        add_library(gtest UNKNOWN IMPORTED)
        set_target_properties(gtest PROPERTIES
            IMPORTED_LOCATION ${GTEST_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${GTEST_INCLUDE_DIR}
        )
        
        add_library(gtest_main UNKNOWN IMPORTED)
        set_target_properties(gtest_main PROPERTIES
            IMPORTED_LOCATION ${GTEST_MAIN_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${GTEST_INCLUDE_DIR}
        )
        
        add_library(GTest::gtest ALIAS gtest)
        add_library(GTest::gtest_main ALIAS gtest_main)
        set(GTest_FOUND TRUE)
    else()
        message(WARNING "Google Test not found, skipping tests")
        return()
    endif()
endif()

# Common test configuration
function(add_rabbitmq_test test_name)
    add_executable(${test_name} ${ARGN})
    
    target_link_libraries(${test_name}
        PRIVATE
            rabbitmq-integration
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    
    target_include_directories(${test_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Add test to CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Set test timeout
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 60)
endfunction()

# Basic unit tests that exist
add_rabbitmq_test(test_message
    unit/test_message.cpp
)

add_rabbitmq_test(test_types
    unit/test_types.cpp
)

add_rabbitmq_test(test_local_queue
    unit/test_local_queue.cpp
)

# Test utilities
add_library(test_utils STATIC
    utils/test_utils.cpp
)

target_link_libraries(test_utils
    PUBLIC
        rabbitmq-integration
        GTest::gtest
)

target_include_directories(test_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Integration tests (require running RabbitMQ)
option(ENABLE_INTEGRATION_TESTS "Enable integration tests (requires RabbitMQ)" OFF)

if(ENABLE_INTEGRATION_TESTS)
    message(STATUS "Integration tests enabled - requires RabbitMQ server")
    
    # Integration tests would go here when implemented
    # add_rabbitmq_test(test_integration_basic
    #     integration/test_basic_integration.cpp
    # )
endif()

# Performance tests
option(ENABLE_PERFORMANCE_TESTS "Enable performance tests" OFF)

if(ENABLE_PERFORMANCE_TESTS)
    message(STATUS "Performance tests enabled")
    
    # Performance tests would go here when implemented
    # add_rabbitmq_test(test_performance_publisher
    #     performance/test_publisher_performance.cpp
    # )
endif()