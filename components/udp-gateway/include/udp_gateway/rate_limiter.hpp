// components/udp-gateway/include/udp_gateway/rate_limiter.hpp
#pragma once

#include "udp_gateway/types.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace udp_gateway {

// Forward declarations for test compatibility
enum class MessagePriority;
struct RateLimiterStatistics;
struct TokenBucket;

/**
 * @class IRateLimiter
 * @brief Interface for rate limiting functionality
 * 
 * This interface defines the contract for rate limiters that will
 * control the flow of messages from devices.
 */
class IRateLimiter {
public:
    virtual ~IRateLimiter() = default;

    /**
     * @brief Check if a device has exceeded its rate limit
     * 
     * @param deviceId The device identifier (IMEI)
     * @param priority The priority of the message
     * @return true if the device is allowed to send a message, false if rate limited
     */
    virtual bool checkLimit(const IMEI& deviceId, Priority priority) = 0;

    /**
     * @brief Record a message from a device
     * 
     * @param deviceId The device identifier (IMEI)
     * @param priority The priority of the message
     * @param size The size of the message in bytes
     */
    virtual void recordMessage(const IMEI& deviceId, Priority priority, size_t size) = 0;

    /**
     * @brief Set the rate limit for a specific device
     * 
     * @param deviceId The device identifier (IMEI)
     * @param messagesPerSecond Maximum number of messages per second
     * @param burstSize Maximum burst size
     */
    virtual void setDeviceLimit(const IMEI& deviceId, double messagesPerSecond, size_t burstSize) = 0;

    /**
     * @brief Set the default rate limit for devices
     * 
     * @param messagesPerSecond Maximum number of messages per second
     * @param burstSize Maximum burst size
     */
    virtual void setDefaultLimit(double messagesPerSecond, size_t burstSize) = 0;

    /**
     * @brief Reset the rate limiter state for a device
     * 
     * @param deviceId The device identifier (IMEI)
     */
    virtual void resetDevice(const IMEI& deviceId) = 0;

    /**
     * @brief Get rate limiting statistics for a device
     * 
     * @param deviceId The device identifier (IMEI)
     * @return Statistics about rate limiting for the device
     */
    virtual std::unordered_map<std::string, double> getDeviceStatistics(const IMEI& deviceId) const = 0;
};

/**
 * @class TokenBucketRateLimiter
 * @brief Implementation of rate limiting using the token bucket algorithm
 * 
 * This class implements the IRateLimiter interface using a token bucket
 * algorithm to control message rates from devices.
 */
class TokenBucketRateLimiter : public IRateLimiter {
public:
    /**
     * @brief Configuration for the token bucket rate limiter
     */
    struct Config {
        double defaultMessagesPerSecond = 10.0;  // Default rate limit
        size_t defaultBurstSize = 20;            // Default burst size
        bool enablePriorityBoost = true;         // Whether higher priority gets rate limit boosts
    };

    /**
     * @brief Construct a new Token Bucket Rate Limiter
     * 
     * @param config Configuration for the rate limiter
     */
    explicit TokenBucketRateLimiter(const Config& config);
    
    // Custom destructor to handle PIMPL
    ~TokenBucketRateLimiter();

    // IRateLimiter interface
    bool checkLimit(const IMEI& deviceId, Priority priority) override;
    void recordMessage(const IMEI& deviceId, Priority priority, size_t size) override;
    void setDeviceLimit(const IMEI& deviceId, double messagesPerSecond, size_t burstSize) override;
    void setDefaultLimit(double messagesPerSecond, size_t burstSize) override;
    void resetDevice(const IMEI& deviceId) override;
    std::unordered_map<std::string, double> getDeviceStatistics(const IMEI& deviceId) const override;

    // Test interface methods (used by unit tests)
    bool tryAcquire(const std::string& deviceId, int priority = 0);
    RateLimiterStatistics getStatistics(const std::string& deviceId) const;

protected:
    // Internal structure to track token bucket state for a device
    struct DeviceBucket {
        double tokensPerSecond;
        size_t bucketSize;
        double availableTokens;
        std::chrono::steady_clock::time_point lastUpdated;
        uint64_t totalMessages = 0;
        uint64_t limitedMessages = 0;

        DeviceBucket(double rate, size_t size);
        bool consumeToken(double priorityMultiplier);
        void refill();
    };

    // Convert priority to token consumption multiplier
    double getPriorityMultiplier(Priority priority) const;
    
    // Helper method for test interface
    void refillTestBucket(TokenBucket& bucket, std::chrono::steady_clock::time_point now);

    Config config_;
    std::unordered_map<IMEI, DeviceBucket> deviceBuckets_;
    std::unique_ptr<void, void(*)(void*)> testBuckets_;  // Use custom deleter
    mutable std::mutex mutex_;
};

/**
 * @brief Create a rate limiter with default configuration
 * 
 * @return std::unique_ptr<IRateLimiter> A unique pointer to the rate limiter
 */
std::unique_ptr<IRateLimiter> createDefaultRateLimiter();

} // namespace udp_gateway