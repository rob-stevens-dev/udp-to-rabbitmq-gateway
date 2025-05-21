add_executable(simple_gateway simple_gateway.cpp)

target_link_libraries(simple_gateway
    PRIVATE
        udp_gateway
)

# Set compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(simple_gateway PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(simple_gateway PRIVATE /W4)
endif()

# Install examples
install(TARGETS simple_gateway DESTINATION bin/examples)