cmake_minimum_required(VERSION 3.16)
project(udp-rabbitmq-gateway VERSION 0.1.0 LANGUAGES CXX)

# Options
option(BUILD_ALL_COMPONENTS "Build all components" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Add components
if(BUILD_ALL_COMPONENTS)
  add_subdirectory(components/udp-gateway)
  add_subdirectory(components/protocol-parser)
  add_subdirectory(components/redis-deduplication)
  add_subdirectory(components/rabbitmq-integration)
  add_subdirectory(components/device-manager)
  add_subdirectory(components/regional-router)
  add_subdirectory(components/monitoring-system)
  add_subdirectory(components/debug-tools)
endif()

# Add examples if requested
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()