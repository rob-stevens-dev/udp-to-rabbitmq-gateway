find_package(GTest REQUIRED)

# Try to find GMock, fall back to using GTest's version if available
find_package(GMock QUIET)
if(NOT GMock_FOUND)
    # Some GTest installations include GMock
    find_library(GMOCK_LIBRARY gmock)
    find_library(GMOCK_MAIN_LIBRARY gmock_main)
    if(GMOCK_LIBRARY AND GMOCK_MAIN_LIBRARY)
        add_library(GMock::GMock UNKNOWN IMPORTED)
        set_target_properties(GMock::GMock PROPERTIES IMPORTED_LOCATION ${GMOCK_LIBRARY})
        add_library(GMock::Main UNKNOWN IMPORTED)
        set_target_properties(GMock::Main PROPERTIES IMPORTED_LOCATION ${GMOCK_MAIN_LIBRARY})
    endif()
endif()

# Include directories for tests
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Create mock interface headers for testing
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_interfaces.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/mock_interfaces.hpp
    @ONLY
)

# Unit tests
add_executable(rate_limiter_test 
    unit/rate_limiter_test.cpp
    ${CMAKE_SOURCE_DIR}/src/rate_limiter.cpp
)

target_link_libraries(rate_limiter_test
    PRIVATE
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(rate_limiter_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Add gateway types test
add_executable(types_test
    unit/types_test.cpp
)

target_link_libraries(types_test
    PRIVATE
        GTest::GTest
        GTest::Main
)

target_include_directories(types_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Session test with mocks
add_executable(session_test
    unit/session_test.cpp
    ${CMAKE_SOURCE_DIR}/src/session.cpp
    ${CMAKE_SOURCE_DIR}/src/rate_limiter.cpp
)

target_link_libraries(session_test
    PRIVATE
        GTest::GTest
        GTest::Main
        Boost::system
        Threads::Threads
)

if(TARGET GMock::GMock)
    target_link_libraries(session_test PRIVATE GMock::GMock)
endif()

target_include_directories(session_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Gateway test with mocks
add_executable(gateway_test
    unit/gateway_test.cpp
    ${CMAKE_SOURCE_DIR}/src/gateway.cpp
    ${CMAKE_SOURCE_DIR}/src/session.cpp
    ${CMAKE_SOURCE_DIR}/src/rate_limiter.cpp
)

target_link_libraries(gateway_test
    PRIVATE
        GTest::GTest
        GTest::Main
        Boost::system
        Threads::Threads
)

if(TARGET GMock::GMock)
    target_link_libraries(gateway_test PRIVATE GMock::GMock)
endif()

target_include_directories(gateway_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Add the tests to CTest
add_test(NAME RateLimiterTest COMMAND rate_limiter_test)
add_test(NAME TypesTest COMMAND types_test)
add_test(NAME SessionTest COMMAND session_test)
add_test(NAME GatewayTest COMMAND gateway_test)

# Set compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rate_limiter_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(types_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(session_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gateway_test PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(rate_limiter_test PRIVATE /W4)
    target_compile_options(types_test PRIVATE /W4)
    target_compile_options(session_test PRIVATE /W4)
    target_compile_options(gateway_test PRIVATE /W4)
endif()