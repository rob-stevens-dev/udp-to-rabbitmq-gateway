cmake_minimum_required(VERSION 3.16)
project(rabbitmq-integration VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_DOCUMENTATION "Build documentation" OFF)
option(ENABLE_COMPRESSION "Enable compression support" OFF)
option(ENABLE_SSL_TLS "Enable SSL/TLS support" OFF)

# Find required dependencies
find_package(PkgConfig REQUIRED)

# RabbitMQ C client
pkg_check_modules(RABBITMQ_C REQUIRED librabbitmq>=0.11.0)

# JSON library
find_package(nlohmann_json REQUIRED)

# Boost
find_package(Boost REQUIRED COMPONENTS system thread filesystem)

# Threading
find_package(Threads REQUIRED)

# UUID (for message IDs)
pkg_check_modules(UUID REQUIRED uuid)

# spdlog (but try to avoid fmt dependency issues)
find_package(spdlog REQUIRED)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Display configuration
message(STATUS "RabbitMQ Integration Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  RabbitMQ C client: ${RABBITMQ_C_VERSION}")
message(STATUS "  Boost version: ${Boost_VERSION}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Build testing: ${BUILD_TESTING}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "  Build documentation: ${BUILD_DOCUMENTATION}")
message(STATUS "  Compression support: ${ENABLE_COMPRESSION}")
message(STATUS "  SSL/TLS support: ${ENABLE_SSL_TLS}")

# Add the main library
add_library(rabbitmq-integration
    src/connection.cpp
    src/consumer.cpp
    src/publisher.cpp
    src/queue_manager.cpp
    src/message.cpp
    src/types.cpp
    src/local_queue.cpp
)

# Set include directories
target_include_directories(rabbitmq-integration
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${RABBITMQ_C_INCLUDE_DIRS}
        ${UUID_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(rabbitmq-integration
    PUBLIC
        nlohmann_json::nlohmann_json
        Boost::system
        Boost::thread
        Boost::filesystem
        Threads::Threads
    PRIVATE
        ${RABBITMQ_C_LIBRARIES}
        ${UUID_LIBRARIES}
        spdlog::spdlog
)

# Set compile options
target_compile_options(rabbitmq-integration
    PRIVATE
        ${RABBITMQ_C_CFLAGS_OTHER}
        ${UUID_CFLAGS_OTHER}
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# Set properties
set_target_properties(rabbitmq-integration PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Enable testing
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

# Add examples
if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# Installation
install(TARGETS rabbitmq-integration
    EXPORT rabbitmq-integration-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT rabbitmq-integration-targets
    FILE rabbitmq-integration-targets.cmake
    NAMESPACE rabbitmq-integration::
    DESTINATION lib/cmake/rabbitmq-integration
)

# Create package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    rabbitmq-integration-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/rabbitmq-integration-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rabbitmq-integration-config.cmake
    INSTALL_DESTINATION lib/cmake/rabbitmq-integration
)