// components/redis-deduplication/test/unit/test_types.cpp
#include <gtest/gtest.h>
#include "redis_deduplication/types.hpp"

using namespace redis_deduplication;

class TypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TypesTest, MessageIdCreation) {
    MessageId msg1;
    EXPECT_TRUE(msg1.imei.empty());
    EXPECT_EQ(0, msg1.sequenceNumber);
    
    MessageId msg2("123456789012345", 42);
    EXPECT_EQ("123456789012345", msg2.imei);
    EXPECT_EQ(42, msg2.sequenceNumber);
}

TEST_F(TypesTest, MessageIdComparison) {
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 100);
    MessageId msg3("123456789012345", 101);
    MessageId msg4("987654321098765", 100);
    
    // Test equality
    EXPECT_TRUE(msg1 == msg2);
    EXPECT_FALSE(msg1 == msg3);
    EXPECT_FALSE(msg1 == msg4);
    
    // Test less than (for container usage)
    EXPECT_TRUE(msg1 < msg3);  // Same IMEI, different sequence
    EXPECT_TRUE(msg1 < msg4);  // Different IMEI
}

TEST_F(TypesTest, MessageIdRedisKey) {
    MessageId msg("123456789012345", 42);
    
    std::string key1 = msg.toRedisKey();
    std::string key2 = msg.toRedisKey("custom");
    
    EXPECT_EQ("dedup:123456789012345:42", key1);
    EXPECT_EQ("custom:123456789012345:42", key2);
}

TEST_F(TypesTest, DeduplicationConfigDefaults) {
    DeduplicationConfig config;
    
    EXPECT_EQ("localhost", config.masterHost);
    EXPECT_EQ(6379, config.masterPort);
    EXPECT_TRUE(config.standbyHost.empty());
    EXPECT_EQ(5, config.connectionPoolSize);
    EXPECT_EQ(3600000, config.deduplicationWindow.count()); // 1 hour in ms
    EXPECT_EQ(10000, config.localCacheSize);
    EXPECT_TRUE(config.enableLocalCacheFallback);
}

TEST_F(TypesTest, ConfigValidation) {
    DeduplicationConfig validConfig;
    EXPECT_TRUE(isValid(validConfig));
    
    // Invalid configurations
    DeduplicationConfig invalidConfig1;
    invalidConfig1.masterHost = "";
    EXPECT_FALSE(isValid(invalidConfig1));
    
    DeduplicationConfig invalidConfig2;
    invalidConfig2.masterPort = 0;
    EXPECT_FALSE(isValid(invalidConfig2));
    
    DeduplicationConfig invalidConfig3;
    invalidConfig3.connectionPoolSize = 0;
    EXPECT_FALSE(isValid(invalidConfig3));
}

TEST_F(TypesTest, ConfigFactoryFunctions) {
    auto devConfig = createDevelopmentConfig("localhost", 6379);
    EXPECT_EQ("localhost", devConfig.masterHost);
    EXPECT_EQ(6379, devConfig.masterPort);
    EXPECT_TRUE(devConfig.enableDebugLogging);
    EXPECT_EQ(3, devConfig.connectionPoolSize);
    
    auto prodConfig = createProductionConfig("redis.example.com", 6379, "password");
    EXPECT_EQ("redis.example.com", prodConfig.masterHost);
    EXPECT_EQ("password", prodConfig.password);
    EXPECT_FALSE(prodConfig.enableDebugLogging);
    EXPECT_EQ(10, prodConfig.connectionPoolSize);
}

TEST_F(TypesTest, DeduplicationResultToString) {
    EXPECT_EQ("UNIQUE", toString(DeduplicationResult::UNIQUE));
    EXPECT_EQ("DUPLICATE", toString(DeduplicationResult::DUPLICATE));
    EXPECT_EQ("ERROR", toString(DeduplicationResult::ERROR));
    EXPECT_EQ("CACHE_MISS", toString(DeduplicationResult::CACHE_MISS));
}

TEST_F(TypesTest, ConnectionStatusToString) {
    EXPECT_EQ("CONNECTED", toString(ConnectionStatus::CONNECTED));
    EXPECT_EQ("DISCONNECTED", toString(ConnectionStatus::DISCONNECTED));
    EXPECT_EQ("CONNECTING", toString(ConnectionStatus::CONNECTING));
    EXPECT_EQ("FAILED", toString(ConnectionStatus::FAILED));
    EXPECT_EQ("STANDBY_ACTIVE", toString(ConnectionStatus::STANDBY_ACTIVE));
}

TEST_F(TypesTest, BatchResultUtilities) {
    BatchDeduplicationResult result;
    result.results = {
        DeduplicationResult::UNIQUE,
        DeduplicationResult::DUPLICATE,
        DeduplicationResult::UNIQUE,
        DeduplicationResult::ERROR
    };
    
    // Test success rate calculation (3 out of 4 non-error results)
    double successRate = calculateSuccessRate(result);
    EXPECT_DOUBLE_EQ(0.75, successRate);
    
    // Test count by result
    EXPECT_EQ(2, countByResult(result, DeduplicationResult::UNIQUE));
    EXPECT_EQ(1, countByResult(result, DeduplicationResult::DUPLICATE));
    EXPECT_EQ(1, countByResult(result, DeduplicationResult::ERROR));
    EXPECT_EQ(0, countByResult(result, DeduplicationResult::CACHE_MISS));
}