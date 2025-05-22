# components/redis-deduplication/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(redis-deduplication VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_TESTING "Enable testing" ON)
option(ENABLE_EXAMPLES "Build examples" ON)
option(ENABLE_BENCHMARKS "Build benchmarks" OFF)

# Find required dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(spdlog REQUIRED)

# Optional dependencies
find_package(GTest QUIET)
find_package(benchmark QUIET)

# Create main library
add_library(redis-deduplication
    src/types.cpp
    src/redis_client.cpp
    src/local_cache.cpp
    src/connection_pool.cpp
    src/deduplicator.cpp
)

# Add alias for easier linking
add_library(redis_deduplication::redis_deduplication ALIAS redis-deduplication)

# Set target properties
set_target_properties(redis-deduplication PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

# Include directories
target_include_directories(redis-deduplication
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${HIREDIS_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(redis-deduplication
    PUBLIC
        spdlog::spdlog
    PRIVATE
        ${HIREDIS_LIBRARIES}
        Boost::system
        Boost::thread
)

# Compiler definitions
target_compile_definitions(redis-deduplication
    PRIVATE
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
)

# Compiler options
target_compile_options(redis-deduplication
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Werror>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        ${HIREDIS_CFLAGS_OTHER}
)

# Link directories
target_link_directories(redis-deduplication
    PRIVATE
        ${HIREDIS_LIBRARY_DIRS}
)

# Installation
include(GNUInstallDirs)

install(TARGETS redis-deduplication
    EXPORT redis-deduplication-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Export targets
install(EXPORT redis-deduplication-targets
    FILE redis-deduplication-targets.cmake
    NAMESPACE redis_deduplication::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redis-deduplication
)

# Create config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/redis-deduplication-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/redis-deduplication-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redis-deduplication
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/redis-deduplication-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/redis-deduplication-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/redis-deduplication-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redis-deduplication
)

# Tests
if(ENABLE_TESTING AND GTest_FOUND)
    enable_testing()
    add_subdirectory(test)
endif()

# Examples
if(ENABLE_EXAMPLES)
    add_subdirectory(example)
endif()

# Benchmarks
if(ENABLE_BENCHMARKS AND benchmark_FOUND)
    add_subdirectory(benchmark)
endif()

# Packaging
set(CPACK_PACKAGE_NAME "redis-deduplication")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Redis-based message deduplication library")
set(CPACK_PACKAGE_VENDOR "UDP Gateway Project")
set(CPACK_PACKAGE_CONTACT "dev@example.com")

include(CPack)