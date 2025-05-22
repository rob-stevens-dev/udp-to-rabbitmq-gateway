// components/redis-deduplication/include/redis_deduplication/types.hpp
#pragma once

#include <chrono>
#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace redis_deduplication {

/**
 * @brief Configuration structure for Redis Deduplication component
 */
struct DeduplicationConfig {
    // Redis Master Configuration
    std::string masterHost = "localhost";
    int masterPort = 6379;
    
    // Redis Standby Configuration (optional)
    std::string standbyHost;
    int standbyPort = 6379;
    
    // Authentication
    std::string password;
    
    // Connection Pool Settings
    int connectionPoolSize = 5;
    std::chrono::milliseconds connectionTimeout{5000};
    std::chrono::milliseconds socketTimeout{3000};
    
    // Deduplication Settings
    std::chrono::milliseconds deduplicationWindow{3600000}; // 1 hour
    std::string keyPrefix = "dedup";
    
    // Local Cache Settings
    size_t localCacheSize = 10000;
    bool enableLocalCacheFallback = true;
    std::chrono::milliseconds localCacheTtl{3600000}; // 1 hour
    
    // Reconnection Settings
    std::chrono::milliseconds reconnectInterval{5000};
    int maxReconnectAttempts = 10;
    double backoffMultiplier = 2.0;
    std::chrono::milliseconds maxReconnectInterval{300000}; // 5 minutes
    
    // Performance Settings
    bool enablePipelining = true;
    int maxPipelineSize = 100;
    
    // Logging
    bool enableDebugLogging = false;
};

/**
 * @brief Statistics for the deduplication system
 */
struct DeduplicationStats {
    // Operation Counters
    uint64_t totalChecks = 0;
    uint64_t duplicatesFound = 0;
    uint64_t uniqueMessages = 0;
    
    // Redis Statistics
    uint64_t redisConnections = 0;
    uint64_t redisFailures = 0;
    uint64_t redisReconnects = 0;
    uint64_t redisTimeouts = 0;
    
    // Local Cache Statistics
    uint64_t localCacheHits = 0;
    uint64_t localCacheMisses = 0;
    uint64_t localCacheEvictions = 0;
    
    // Performance Metrics
    std::chrono::microseconds averageCheckLatency{0};
    std::chrono::microseconds maxCheckLatency{0};
    std::chrono::microseconds minCheckLatency{0};
    
    // Connection Pool Statistics
    int activeConnections = 0;
    int totalConnections = 0;
    int failedConnections = 0;
    
    // Timestamp of last update
    std::chrono::system_clock::time_point lastUpdated;
};

/**
 * @brief Connection pool statistics
 */
struct ConnectionPoolStats {
    int totalConnections = 0;
    int activeConnections = 0;
    int idleConnections = 0;
    int failedConnections = 0;
    int reconnectAttempts = 0;
    std::chrono::system_clock::time_point lastConnectionTime;
    std::chrono::system_clock::time_point lastFailureTime;
};

/**
 * @brief Message identifier for deduplication
 */
struct MessageId {
    std::string imei;
    uint32_t sequenceNumber;
    
    MessageId() = default;
    MessageId(const std::string& deviceImei, uint32_t seqNum)
        : imei(deviceImei), sequenceNumber(seqNum) {}
    
    // Generate Redis key for this message
    std::string toRedisKey(const std::string& prefix = "dedup") const {
        return prefix + ":" + imei + ":" + std::to_string(sequenceNumber);
    }
    
    // Comparison operators for container usage
    bool operator==(const MessageId& other) const {
        return imei == other.imei && sequenceNumber == other.sequenceNumber;
    }
    
    bool operator<(const MessageId& other) const {
        if (imei != other.imei) {
            return imei < other.imei;
        }
        return sequenceNumber < other.sequenceNumber;
    }
};

/**
 * @brief Result of a deduplication check
 */
enum class DeduplicationResult {
    UNIQUE,        // Message is unique, should be processed
    DUPLICATE,     // Message is a duplicate, should be discarded
    ERROR,         // Error occurred during check, conservative handling needed
    CACHE_MISS     // Local cache miss, needs Redis check
};

/**
 * @brief Redis connection status
 */
enum class ConnectionStatus {
    CONNECTED,
    DISCONNECTED,
    CONNECTING,
    FAILED,
    STANDBY_ACTIVE
};

/**
 * @brief Batch deduplication result
 */
struct BatchDeduplicationResult {
    std::vector<DeduplicationResult> results;
    std::chrono::microseconds totalLatency{0};
    int redisOperations = 0;
    int cacheHits = 0;
    int errors = 0;
};

// Function declarations for utility functions
std::string toString(const DeduplicationStats& stats);
std::string toString(const ConnectionPoolStats& stats);
std::string toString(DeduplicationResult result);
std::string toString(ConnectionStatus status);

bool isValid(const DeduplicationConfig& config);

DeduplicationConfig createDefaultConfig();
DeduplicationConfig createProductionConfig(const std::string& masterHost, 
                                         int masterPort,
                                         const std::string& password);
DeduplicationConfig createDevelopmentConfig(const std::string& masterHost,
                                          int masterPort);

double calculateSuccessRate(const BatchDeduplicationResult& result);
int countByResult(const BatchDeduplicationResult& result, DeduplicationResult targetResult);

} // namespace redis_deduplication