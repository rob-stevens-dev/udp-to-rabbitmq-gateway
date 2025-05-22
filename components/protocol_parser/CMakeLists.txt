cmake_minimum_required(VERSION 3.16)
project(protocol_parser VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile options for better warnings and optimizations
add_compile_options(-Wall -Wextra -Wpedantic)

# Set optimization and debug flags based on build type
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")

# Find dependencies
find_package(Boost REQUIRED)

# Handle nlohmann_json dependency
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, downloading it...")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Create the library
add_library(protocol_parser
    src/parser.cpp
    src/message.cpp
    src/validator.cpp
    src/builder.cpp
    src/error.cpp
)

# Add alias for consistent naming
add_library(protocol_parser::protocol_parser ALIAS protocol_parser)

# Set target properties
set_target_properties(protocol_parser PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Set include directories
target_include_directories(protocol_parser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(protocol_parser
    PUBLIC
        Boost::boost
    PRIVATE
        nlohmann_json::nlohmann_json
)

# Compiler-specific options
target_compile_features(protocol_parser PUBLIC cxx_std_17)

# Enable testing if this is the main project or if explicitly requested
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR BUILD_TESTING)
    option(BUILD_TESTING "Build tests" ON)
endif()

if(BUILD_TESTING AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(test)
endif()

# Examples
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# Simple header installation (no exports)
include(GNUInstallDirs)

install(TARGETS protocol_parser
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)