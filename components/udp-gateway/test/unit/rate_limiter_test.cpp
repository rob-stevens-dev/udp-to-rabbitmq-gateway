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
        EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    }
    
    // Next message should be rate limited
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    
    // Wait for token refill (at least 1 token)
    std::this_thread::sleep_for(std::chrono::milliseconds(101));  // Just over 0.1s for 1 token at 10 msgs/sec
    
    // Should allow one more message
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    
    // But not two
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
}

// Test priority boost
TEST_F(TokenBucketRateLimiterTest, PriorityBoost) {
    const IMEI deviceId = "123456789012345";
    
    // Use up all normal priority tokens
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    }
    
    // Normal priority should be blocked
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    
    // But CRITICAL priority should work (uses fewer tokens)
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::CRITICAL));
    
    // Even CRITICAL will run out eventually
    for (int i = 0; i < 10; ++i) {
        rateLimiter->checkLimit(deviceId, Priority::CRITICAL);
    }
    
    // Now CRITICAL should be blocked too
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::CRITICAL));
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
    EXPECT_FALSE(rateLimiter->checkLimit(device1, Priority::NORMAL));
    
    // Device2 should still have tokens available
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(device2, Priority::NORMAL));
    }
    
    // Now device2 should be blocked
    EXPECT_FALSE(rateLimiter->checkLimit(device2, Priority::NORMAL));
}

// Test reset functionality
TEST_F(TokenBucketRateLimiterTest, ResetDevice) {
    const IMEI deviceId = "123456789012345";
    
    // Use up all tokens
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    }
    
    // Should be blocked
    EXPECT_FALSE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
    
    // Reset the device
    rateLimiter->resetDevice(deviceId);
    
    // Should have tokens again
    EXPECT_TRUE(rateLimiter->checkLimit(deviceId, Priority::NORMAL));
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
    EXPECT_GT(stats["limited_messages"], 0.0);
    EXPECT_GT(stats["limited_percentage"], 0.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}