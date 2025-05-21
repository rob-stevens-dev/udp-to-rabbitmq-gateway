cmake_minimum_required(VERSION 3.16)
project(udp-gateway VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(Boost REQUIRED COMPONENTS system program_options)
find_package(Threads REQUIRED)

# Add library
add_library(udp_gateway
    src/gateway.cpp
    src/session.cpp
    src/rate_limiter.cpp
)

# Set include directories
target_include_directories(udp_gateway
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(udp_gateway
    PUBLIC
        Boost::system
        Threads::Threads
    PRIVATE
        # These would be actual component libraries in the real implementation
        # protocol-parser
        # redis-deduplication
        # rabbitmq-integration
        # device-manager
        # monitoring-system
)

# Add executable
add_executable(udp-gateway-server src/main.cpp)
target_link_libraries(udp-gateway-server 
    PRIVATE 
        udp_gateway 
        Boost::program_options
)

# Set compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(udp_gateway PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(udp-gateway-server PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(udp_gateway PRIVATE /W4)
    target_compile_options(udp-gateway-server PRIVATE /W4)
endif()

# Install rules
install(TARGETS udp_gateway udp-gateway-server
    EXPORT udp-gateway-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

# Add examples if enabled
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Add tests if enabled
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

# Export targets
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/udp-gateway-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(cmake/udp-gateway-config.cmake
    "${CMAKE_CURRENT_BINARY_DIR}/udp-gateway-config.cmake"
    COPYONLY
)

install(EXPORT udp-gateway-targets
    FILE udp-gateway-targets.cmake
    NAMESPACE udp-gateway::
    DESTINATION lib/cmake/udp-gateway
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/udp-gateway-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/udp-gateway-config-version.cmake"
    DESTINATION lib/cmake/udp-gateway
)