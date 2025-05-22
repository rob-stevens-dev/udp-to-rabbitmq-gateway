# Example programs for the protocol parser

# Simple parser example
add_executable(simple_parser
    simple_parser.cpp
)

target_link_libraries(simple_parser
    PRIVATE
        protocol_parser
)

target_include_directories(simple_parser
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Note: integration_example.cpp is currently disabled due to compilation issues
# It can be re-enabled once the issues are resolved

# Uncomment below to add the integration example when fixed:
# add_executable(integration_example
#     integration_example.cpp
# )
# 
# target_link_libraries(integration_example
#     PRIVATE
#         protocol_parser
# )
# 
# target_include_directories(integration_example
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/../include
# )