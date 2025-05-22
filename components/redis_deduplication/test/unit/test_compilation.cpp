// components/redis-deduplication/test/unit/test_compilation.cpp
#include <gtest/gtest.h>
#include "redis_deduplication/types.hpp"

using namespace redis_deduplication;

// Simple compilation test to ensure headers compile correctly
TEST(CompilationTest, BasicTypes) {
    MessageId msg("123456789012345", 42);
    EXPECT_EQ("123456789012345", msg.imei);
    EXPECT_EQ(42, msg.sequenceNumber);
    
    DeduplicationConfig config;
    EXPECT_EQ("localhost", config.masterHost);
    EXPECT_EQ(6379, config.masterPort);
}

TEST(CompilationTest, MessageIdOperators) {
    MessageId msg1("123456789012345", 42);
    MessageId msg2("123456789012345", 42);
    MessageId msg3("123456789012345", 43);
    
    EXPECT_TRUE(msg1 == msg2);
    EXPECT_FALSE(msg1 == msg3);
    EXPECT_TRUE(msg1 < msg3);
}

TEST(CompilationTest, RedisKeyGeneration) {
    MessageId msg("123456789012345", 42);
    std::string key = msg.toRedisKey();
    EXPECT_EQ("dedup:123456789012345:42", key);
    
    std::string customKey = msg.toRedisKey("custom");
    EXPECT_EQ("custom:123456789012345:42", customKey);
}