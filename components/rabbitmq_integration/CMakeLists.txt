cmake_minimum_required(VERSION 3.16)
project(rabbitmq-integration VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_DOCUMENTATION "Build documentation" OFF)
option(ENABLE_COMPRESSION "Enable message compression" OFF)
option(ENABLE_SSL "Enable SSL/TLS support" OFF)

# Find required dependencies
find_package(PkgConfig REQUIRED)

# Find librabbitmq-c
pkg_check_modules(RABBITMQ_C REQUIRED librabbitmq>=0.11.0)

# Find other dependencies
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(Threads REQUIRED)

# Find UUID library
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)

# Find spdlog (try CONFIG first, then MODULE)
find_package(spdlog QUIET CONFIG)
if(NOT spdlog_FOUND)
    find_package(spdlog REQUIRED MODULE)
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    
    # Treat warnings as errors in Debug mode only
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()
endif()

# Add library
add_library(rabbitmq-integration
    src/connection.cpp
    src/publisher.cpp
    src/consumer.cpp
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
)

# Link dependencies
target_link_libraries(rabbitmq-integration
    PUBLIC
        nlohmann_json::nlohmann_json
        Boost::system
        Boost::thread
        Boost::filesystem
        Threads::Threads
        fmt::fmt
    PRIVATE
        ${RABBITMQ_C_LIBRARIES}
        ${UUID_LIBRARIES}
)

# Add compile definitions
target_compile_definitions(rabbitmq-integration
    PRIVATE
        ${RABBITMQ_C_CFLAGS_OTHER}
        ${UUID_CFLAGS_OTHER}
)

# Add include directories for dependencies
target_include_directories(rabbitmq-integration
    PRIVATE
        ${RABBITMQ_C_INCLUDE_DIRS}
        ${UUID_INCLUDE_DIRS}
)

# Link directories for dependencies
target_link_directories(rabbitmq-integration
    PRIVATE
        ${RABBITMQ_C_LIBRARY_DIRS}
        ${UUID_LIBRARY_DIRS}
)

# Optional dependencies
if(ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(rabbitmq-integration PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(rabbitmq-integration PRIVATE RABBITMQ_INTEGRATION_SSL_ENABLED)
endif()

if(ENABLE_COMPRESSION)
    find_package(ZLIB REQUIRED)
    target_link_libraries(rabbitmq-integration PRIVATE ZLIB::ZLIB)
    target_compile_definitions(rabbitmq-integration PRIVATE RABBITMQ_INTEGRATION_COMPRESSION_ENABLED)
endif()

# Set library properties
set_target_properties(rabbitmq-integration PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME RabbitMQIntegration
)

# Install rules
include(GNUInstallDirs)

install(TARGETS rabbitmq-integration
    EXPORT rabbitmq-integration-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT rabbitmq-integration-targets
    FILE rabbitmq-integration-targets.cmake
    NAMESPACE RabbitMQIntegration::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rabbitmq-integration
)

# Create config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rabbitmq-integration-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rabbitmq-integration-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rabbitmq-integration
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/rabbitmq-integration-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/rabbitmq-integration-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/rabbitmq-integration-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rabbitmq-integration
)

# Tests
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

# Examples
if(BUILD_EXAMPLES AND FALSE)
    add_subdirectory(example)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

# Documentation
if(BUILD_DOCUMENTATION)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_subdirectory(docs)
    else()
        message(WARNING "Doxygen not found, documentation will not be built")
    endif()
endif()

# Print configuration summary
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
message(STATUS "  SSL/TLS support: ${ENABLE_SSL}")