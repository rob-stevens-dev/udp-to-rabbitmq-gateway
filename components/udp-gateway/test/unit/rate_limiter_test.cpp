#include "udp_gateway/rate_limiter.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace udp_gateway;

// Test fixture
class TokenBucketRateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        TokenBucketRateLimiter::Config config;
        config.defaultMessagesPerSecond = 10.0;
        config.defaultBurstSize = 5;
        config.enablePriorityBoost = true;
        
        rateLimiter = std::make_unique<TokenBucketRateLimiter>(config);
    }
    
    std::unique_ptr<TokenBucketRateLimiter> rateLimiter;
};

// Test basic rate limiting functionality
TEST_F(TokenBucketRateLimiterTest, BasicLimit) {
    const IMEI deviceId = "123456789012345";
    
    // Should allow burst size messages immediately
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL)) 
            << "Message " << i << " should be allowed";
    }
    
    // Next message should be rate limited
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL))
        << "Message beyond burst size should be rate limited";
    
    // Wait for token refill (at least 1 token)
    std::this_thread::sleep_for(std::chrono::milliseconds(110));  // Just over 0.1s for 1 token at 10 msgs/sec
    
    // Should allow one more message
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL))
        << "Message should be allowed after token refill";
    
    // But not two
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL))
        << "Second message should still be rate limited";
}

// Test priority boost
TEST_F(TokenBucketRateLimiterTest, PriorityBoost) {
    const IMEI deviceId = "123456789012345";
    
    // Use up all normal priority tokens
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    }
    
    // Normal priority should be blocked
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL))
        << "Normal priority should be blocked after using all tokens";
    
    // CRITICAL priority should work (uses fewer tokens due to 4x multiplier)
    // Since CRITICAL has a 4x multiplier, it only uses 0.25 tokens per message
    // So we should be able to send at least a few CRITICAL messages
    bool criticalWorked = false;
    for (int i = 0; i < 4; ++i) {
        if (rateLimiter->checkLimit(deviceId, Priority::CRITICAL)) {
            criticalWorked = true;
            break;
        }
    }
    EXPECT_TRUE(criticalWorked) << "CRITICAL priority should work when normal is blocked";
}

// Test device-specific limits
TEST_F(TokenBucketRateLimiterTest, DeviceSpecificLimits) {
    const IMEI device1 = "123456789012345";
    const IMEI device2 = "543210987654321";
    
    // Set a higher limit for device2
    rateLimiter->setDeviceLimit(device2, 20.0, 10);
    
    // Use up all tokens for device1
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(device1, Priority::NORMAL));
    }
    
    // Device1 should be blocked
    EXPECT_FALSE(rateLimiter->checkLimit(device1, Priority::NORMAL))
        << "Device1 should be blocked after using default limit";
    
    // Device2 should still have tokens available
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(device2, Priority::NORMAL))
            << "Device2 should have higher limit";
    }
    
    // Now device2 should be blocked
    EXPECT_FALSE(rateLimiter->checkLimit(device2, Priority::NORMAL))
        << "Device2 should be blocked after using its higher limit";
}

// Test reset functionality
TEST_F(TokenBucketRateLimiterTest, ResetDevice) {
    const IMEI deviceId = "123456789012345";
    
    // Use up all tokens
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    }
    
    // Should be blocked
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL))
        << "Device should be blocked after using all tokens";
    
    // Reset the device
    rateLimiter->resetDevice(deviceId);
    
    // Should have tokens again
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL))
        << "Device should have tokens after reset";
}

// Test statistics
TEST_F(TokenBucketRateLimiterTest, Statistics) {
    const IMEI deviceId = "123456789012345";
    
    // Send some messages
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    }
    
    // Get statistics
    auto stats = rateLimiter->getDeviceStatistics(deviceId);
    
    // Check basic statistics
    EXPECT_EQ(stats["tokens_per_second"], 10.0);
    EXPECT_EQ(stats["bucket_size"], 5.0);
    EXPECT_EQ(stats["total_messages"], 3.0);
    EXPECT_EQ(stats["limited_messages"], 0.0);
    EXPECT_EQ(stats["limited_percentage"], 0.0);
    
    // Try to exceed limit
    for (int i = 0; i < 5; ++i) {
        rateLimiter->checkLimit(deviceId, Priority::NORMAL);
    }
    
    // Get updated statistics
    stats = rateLimiter->getDeviceStatistics(deviceId);
    
    // Should have some limited messages now
    EXPECT_GT(stats["limited_messages"], 0.0)
        << "Should have some limited messages after exceeding limit";
    EXPECT_GT(stats["limited_percentage"], 0.0)
        << "Limited percentage should be greater than 0";
}

// Test default rate limiter creation
TEST(RateLimiterFactoryTest, CreateDefaultRateLimiter) {
    auto limiter = createDefaultRateLimiter();
    ASSERT_NE(nullptr, limiter);
    
    // Test that it works
    const IMEI deviceId = "123456789012345";
    EXPECT_TRUE(limiter->checkLimit(deviceId, Priority::NORMAL));
}

// Test different priority levels
TEST_F(TokenBucketRateLimiterTest, AllPriorityLevels) {
    const IMEI deviceId = "123456789012345";
    
    // Test all priority levels
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::LOW));
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::HIGH));
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::CRITICAL));
}

// Test priority multiplier behavior
TEST_F(TokenBucketRateLimiterTest, PriorityMultipliers) {
    const IMEI deviceId = "123456789012345";
    
    // Create a fresh rate limiter to test priority multipliers
    TokenBucketRateLimiter::Config testConfig;
    testConfig.defaultMessagesPerSecond = 4.0;  // 4 tokens per second  
    testConfig.defaultBurstSize = 4;             // 4 token bucket
    testConfig.enablePriorityBoost = true;
    
    auto testRateLimiter = std::make_unique<TokenBucketRateLimiter>(testConfig);
    
    // CRITICAL priority should allow 4x more messages than NORMAL
    // because it uses 1/4 the tokens (4.0 multiplier)
    
    // With 4 tokens available, we should be able to send:
    // - 4 NORMAL messages (1 token each)
    // - 16 CRITICAL messages (0.25 tokens each)
    
    int criticalCount = 0;
    for (int i = 0; i < 20; ++i) {
        if (testRateLimiter->checkLimit(deviceId, Priority::CRITICAL)) {
            criticalCount++;
        } else {
            break;
        }
    }
    
    // Should be able to send at least 12 CRITICAL messages
    EXPECT_GE(criticalCount, 12) 
        << "CRITICAL priority should allow many more messages due to 4x multiplier";
}

// Test statistics for non-existent device
TEST_F(TokenBucketRateLimiterTest, StatisticsForNonExistentDevice) {
    const IMEI deviceId = "999999999999999";
    
    auto stats = rateLimiter->getDeviceStatistics(deviceId);
    
    // Should return default values
    EXPECT_EQ(stats["tokens_per_second"], 10.0);
    EXPECT_EQ(stats["bucket_size"], 5.0);
    EXPECT_EQ(stats["total_messages"], 0.0);
    EXPECT_EQ(stats["limited_messages"], 0.0);
    EXPECT_EQ(stats["limited_percentage"], 0.0);
}