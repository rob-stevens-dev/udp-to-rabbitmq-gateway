# Example CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Simple publisher example
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simple_publisher.cpp)
    add_executable(simple_publisher simple_publisher.cpp)
    target_link_libraries(simple_publisher rabbitmq-integration)
endif()

# Simple consumer example  
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simple_consumer.cpp)
    add_executable(simple_consumer simple_consumer.cpp)
    target_link_libraries(simple_consumer rabbitmq-integration)
endif()
