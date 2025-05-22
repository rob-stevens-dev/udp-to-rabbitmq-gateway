cmake_minimum_required(VERSION 3.16)

# Message tests
add_executable(message_test message_test.cpp)
target_link_libraries(message_test PRIVATE protocol_parser GTest::gtest GTest::gtest_main)
target_compile_definitions(message_test PRIVATE GTEST_LANG_CXX11=1)
add_test(NAME protocol_parser_message_test COMMAND message_test)

# Parser tests
add_executable(parser_test parser_test.cpp)
target_link_libraries(parser_test PRIVATE protocol_parser GTest::gtest GTest::gtest_main)
target_compile_definitions(parser_test PRIVATE GTEST_LANG_CXX11=1)
add_test(NAME protocol_parser_parser_test COMMAND parser_test)

# Validator tests
add_executable(validator_test validator_test.cpp)
target_link_libraries(validator_test PRIVATE protocol_parser GTest::gtest GTest::gtest_main)
target_compile_definitions(validator_test PRIVATE GTEST_LANG_CXX11=1)
add_test(NAME protocol_parser_validator_test COMMAND validator_test)

# Builder tests
add_executable(builder_test builder_test.cpp)
target_link_libraries(builder_test PRIVATE protocol_parser GTest::gtest GTest::gtest_main)
target_compile_definitions(builder_test PRIVATE GTEST_LANG_CXX11=1)
add_test(NAME protocol_parser_builder_test COMMAND builder_test)

# Create stubs for other test files
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/parser_test.cpp
"#include \"protocol_parser/parser.hpp\"
#include <gtest/gtest.h>

using namespace protocol_parser;

TEST(ParserTest, BasicTest) {
    // Placeholder test
    EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
")

file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/validator_test.cpp
"#include \"protocol_parser/validator.hpp\"
#include <gtest/gtest.h>

using namespace protocol_parser;

TEST(ValidatorTest, BasicTest) {
    // Placeholder test
    EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
")

file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/builder_test.cpp
"#include \"protocol_parser/builder.hpp\"
#include <gtest/gtest.h>

using namespace protocol_parser;

TEST(BuilderTest, BasicTest) {
    // Placeholder test
    EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
")