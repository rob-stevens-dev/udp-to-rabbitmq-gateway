cmake_minimum_required(VERSION 3.16)
project(udp_rabbitmq_gateway VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_TESTING "Enable testing" ON)
option(ENABLE_EXAMPLES "Build examples" ON)
option(ENABLE_SANITIZERS "Enable sanitizers (Address, Thread, etc.)" OFF)
option(ENABLE_CODE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_STATIC_ANALYSIS "Run static analysis during build" OFF)
option(ENABLE_DOCUMENTATION "Build documentation" OFF)

# Component options
option(ENABLE_COMPONENT_UDP_GATEWAY "Enable UDP Gateway component" ON)
option(ENABLE_COMPONENT_PROTOCOL_PARSER "Enable Protocol Parser component" ON)
option(ENABLE_COMPONENT_REDIS_DEDUPLICATION "Enable Redis Deduplication component" ON)
option(ENABLE_COMPONENT_RABBITMQ_INTEGRATION "Enable RabbitMQ Integration component" ON)
option(ENABLE_COMPONENT_DEVICE_MANAGER "Enable Device Manager component" ON)
option(ENABLE_COMPONENT_REGIONAL_ROUTER "Enable Regional Router component" ON)
option(ENABLE_COMPONENT_MONITORING_SYSTEM "Enable Monitoring System component" ON)
option(ENABLE_COMPONENT_DEBUG_TOOLS "Enable Debug Tools component" ON)

# Add a compilation database for tools like clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find common dependencies
find_package(Boost REQUIRED)
find_package(nlohmann_json CONFIG QUIET)

# If nlohmann_json wasn't found through CONFIG mode, try to find it another way
if(NOT nlohmann_json_FOUND)
    # Check if the single-header version exists
    if(EXISTS "/usr/include/nlohmann/json.hpp")
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE /usr/include)
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        set(nlohmann_json_FOUND TRUE)
    else()
        message(FATAL_ERROR "nlohmann/json library not found")
    endif()
endif()

# Export compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Setup testing if enabled
if(ENABLE_TESTING)
    enable_testing()
    include(CTest)
endif()

# Setup sanitizers if enabled
if(ENABLE_SANITIZERS)
    include(Sanitizers)
    add_sanitizers()
endif()

# Setup code coverage if enabled
if(ENABLE_CODE_COVERAGE)
    include(CodeCoverage)
    setup_code_coverage()
endif()

# Setup static analysis if enabled
if(ENABLE_STATIC_ANALYSIS)
    include(StaticAnalysis)
    setup_static_analysis()
endif()

# Setup documentation if enabled
if(ENABLE_DOCUMENTATION)
    include(Documentation)
    setup_documentation()
endif()

# Add components
if(ENABLE_COMPONENT_PROTOCOL_PARSER)
    add_subdirectory(components/protocol_parser)
endif()

if(ENABLE_COMPONENT_UDP_GATEWAY)
    add_subdirectory(components/udp_gateway)
endif()

if(ENABLE_COMPONENT_REDIS_DEDUPLICATION)
    # add_subdirectory(components/redis_deduplication)
    message(STATUS "Redis Deduplication component not implemented yet")
endif()

if(ENABLE_COMPONENT_RABBITMQ_INTEGRATION)
    # add_subdirectory(components/rabbitmq_integration)
    message(STATUS "RabbitMQ Integration component not implemented yet")
endif()

if(ENABLE_COMPONENT_DEVICE_MANAGER)
    # add_subdirectory(components/device_manager)
    message(STATUS "Device Manager component not implemented yet")
endif()

if(ENABLE_COMPONENT_REGIONAL_ROUTER)
    # add_subdirectory(components/regional_router)
    message(STATUS "Regional Router component not implemented yet")
endif()

if(ENABLE_COMPONENT_MONITORING_SYSTEM)
    # add_subdirectory(components/monitoring_system)
    message(STATUS "Monitoring System component not implemented yet")
endif()

if(ENABLE_COMPONENT_DEBUG_TOOLS)
    # add_subdirectory(components/debug_tools)
    message(STATUS "Debug Tools component not implemented yet")
endif()

# Create a target to build all components
add_custom_target(build_all ALL
    COMMENT "Building all components"
)

# Create a target to build all tests
if(ENABLE_TESTING)
    add_custom_target(build_tests
        COMMENT "Building all tests"
    )
endif()

# Create a target to build all examples
if(ENABLE_EXAMPLES)
    add_custom_target(build_examples
        COMMENT "Building all examples"
    )
endif()

# Create a target to run all tests with verbose output
if(ENABLE_TESTING)
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --verbose
        COMMENT "Running all tests with verbose output"
    )
endif()

# Print build configuration summary
message(STATUS "Build configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build shared libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  Enable testing: ${ENABLE_TESTING}")
message(STATUS "  Enable examples: ${ENABLE_EXAMPLES}")
message(STATUS "  Enable sanitizers: ${ENABLE_SANITIZERS}")
message(STATUS "  Enable code coverage: ${ENABLE_CODE_COVERAGE}")
message(STATUS "  Enable static analysis: ${ENABLE_STATIC_ANALYSIS}")
message(STATUS "  Enable documentation: ${ENABLE_DOCUMENTATION}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  UDP Gateway: ${ENABLE_COMPONENT_UDP_GATEWAY}")
message(STATUS "  Protocol Parser: ${ENABLE_COMPONENT_PROTOCOL_PARSER}")
message(STATUS "  Redis Deduplication: ${ENABLE_COMPONENT_REDIS_DEDUPLICATION}")
message(STATUS "  RabbitMQ Integration: ${ENABLE_COMPONENT_RABBITMQ_INTEGRATION}")
message(STATUS "  Device Manager: ${ENABLE_COMPONENT_DEVICE_MANAGER}")
message(STATUS "  Regional Router: ${ENABLE_COMPONENT_REGIONAL_ROUTER}")
message(STATUS "  Monitoring System: ${ENABLE_COMPONENT_MONITORING_SYSTEM}")
message(STATUS "  Debug Tools: ${ENABLE_COMPONENT_DEBUG_TOOLS}")