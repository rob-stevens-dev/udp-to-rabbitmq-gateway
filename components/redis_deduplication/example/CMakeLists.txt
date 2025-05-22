# components/redis-deduplication/example/CMakeLists.txt

# Create the simple deduplication example
add_executable(simple_deduplication
    simple_deduplication.cpp
)

target_link_libraries(simple_deduplication
    PRIVATE
        redis-deduplication
)

target_include_directories(simple_deduplication
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set output directory
set_target_properties(simple_deduplication PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/example
)

# Create a basic test configuration example
add_executable(config_example
    config_example.cpp
)

target_link_libraries(config_example
    PRIVATE
        redis-deduplication
)

target_include_directories(config_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(config_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/example
)