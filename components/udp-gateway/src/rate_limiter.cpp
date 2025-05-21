#include "udp_gateway/rate_limiter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace udp_gateway {

// DeviceBucket implementation
TokenBucketRateLimiter::DeviceBucket::DeviceBucket(double rate, size_t size)
    : tokensPerSecond(rate)
    , bucketSize(size)
    , availableTokens(static_cast<double>(size))  // Start with a full bucket
    , lastUpdated(std::chrono::steady_clock::now())
{
}

bool TokenBucketRateLimiter::DeviceBucket::consumeToken(double priorityMultiplier) {
    refill();

    // Reduce token consumption based on priority
    double tokensNeeded = 1.0 / priorityMultiplier;

    if (availableTokens >= tokensNeeded) {
        availableTokens -= tokensNeeded;
        totalMessages++;
        return true;
    } else {
        limitedMessages++;
        return false;
    }
}

void TokenBucketRateLimiter::DeviceBucket::refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastUpdated).count();
    
    // Add tokens based on elapsed time and rate
    double newTokens = elapsed * tokensPerSecond;
    if (newTokens > 0) {
        availableTokens = std::min(static_cast<double>(bucketSize), availableTokens + newTokens);
        lastUpdated = now;
    }
}

// TokenBucketRateLimiter implementation
TokenBucketRateLimiter::TokenBucketRateLimiter(const Config& config)
    : config_(config) 
{
}

bool TokenBucketRateLimiter::checkLimit(const IMEI& deviceId, Priority priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get or create bucket for device
    auto it = deviceBuckets_.find(deviceId);
    if (it == deviceBuckets_.end()) {
        it = deviceBuckets_.emplace(
            deviceId, 
            DeviceBucket(config_.defaultMessagesPerSecond, config_.defaultBurstSize)
        ).first;
    }
    
    // Check rate limit with priority multiplier
    double priorityMultiplier = getPriorityMultiplier(priority);
    return it->second.consumeToken(priorityMultiplier);
}

void TokenBucketRateLimiter::recordMessage(const IMEI& deviceId, Priority priority, size_t size) {
    // Mark unused parameters to avoid compiler warnings
    (void)deviceId;  // Unused parameter
    (void)priority;  // Unused parameter
    // In a more complex implementation, we might track message sizes
    // or other metrics, but for now this is handled by consumeToken
    (void)size;  // Unused parameter
}

void TokenBucketRateLimiter::setDeviceLimit(const IMEI& deviceId, double messagesPerSecond, size_t burstSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get or create bucket for device
    auto it = deviceBuckets_.find(deviceId);
    if (it == deviceBuckets_.end()) {
        it = deviceBuckets_.emplace(
            deviceId, 
            DeviceBucket(messagesPerSecond, burstSize)
        ).first;
    } else {
        // Update existing bucket
        it->second.tokensPerSecond = messagesPerSecond;
        it->second.bucketSize = burstSize;
        
        // Cap available tokens to new bucket size
        it->second.availableTokens = std::min(
            it->second.availableTokens, 
            static_cast<double>(burstSize)
        );
    }
}

void TokenBucketRateLimiter::setDefaultLimit(double messagesPerSecond, size_t burstSize) {
    config_.defaultMessagesPerSecond = messagesPerSecond;
    config_.defaultBurstSize = burstSize;
}

void TokenBucketRateLimiter::resetDevice(const IMEI& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get bucket for device
    auto it = deviceBuckets_.find(deviceId);
    if (it != deviceBuckets_.end()) {
        // Reset to full bucket
        it->second.availableTokens = static_cast<double>(it->second.bucketSize);
        it->second.lastUpdated = std::chrono::steady_clock::now();
    }
}

std::unordered_map<std::string, double> TokenBucketRateLimiter::getDeviceStatistics(const IMEI& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::unordered_map<std::string, double> stats;
    
    auto it = deviceBuckets_.find(deviceId);
    if (it != deviceBuckets_.end()) {
        const auto& bucket = it->second;
        
        stats["tokens_per_second"] = bucket.tokensPerSecond;
        stats["bucket_size"] = static_cast<double>(bucket.bucketSize);
        
        // Ensure we have the latest token count
        const_cast<DeviceBucket&>(bucket).refill();
        stats["available_tokens"] = bucket.availableTokens;
        
        stats["total_messages"] = static_cast<double>(bucket.totalMessages);
        stats["limited_messages"] = static_cast<double>(bucket.limitedMessages);
        
        if (bucket.totalMessages > 0) {
            stats["limited_percentage"] = 100.0 * bucket.limitedMessages / bucket.totalMessages;
        } else {
            stats["limited_percentage"] = 0.0;
        }
    } else {
        // Device not found, return default values
        stats["tokens_per_second"] = config_.defaultMessagesPerSecond;
        stats["bucket_size"] = static_cast<double>(config_.defaultBurstSize);
        stats["available_tokens"] = static_cast<double>(config_.defaultBurstSize);
        stats["total_messages"] = 0.0;
        stats["limited_messages"] = 0.0;
        stats["limited_percentage"] = 0.0;
    }
    
    return stats;
}

double TokenBucketRateLimiter::getPriorityMultiplier(Priority priority) const {
    if (!config_.enablePriorityBoost) {
        return 1.0;  // No priority boost
    }
    
    // Higher priority messages consume fewer tokens
    switch (priority) {
        case Priority::CRITICAL:
            return 4.0;  // 75% reduction in token consumption
        case Priority::HIGH:
            return 2.0;  // 50% reduction
        case Priority::NORMAL:
            return 1.0;  // Standard consumption
        case Priority::LOW:
            return 0.5;  // 2x token consumption (more limited)
        default:
            return 1.0;
    }
}

std::unique_ptr<IRateLimiter> createDefaultRateLimiter() {
    TokenBucketRateLimiter::Config config;
    return std::make_unique<TokenBucketRateLimiter>(config);
}

} // namespace udp_gateway