// components/redis-deduplication/include/redis_deduplication/deduplicator.hpp
#pragma once

#include "types.hpp"
#include "connection_pool.hpp"
#include "local_cache.hpp"
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>

namespace redis_deduplication {

/**
 * @brief Main deduplication service class
 * 
 * This class provides the primary interface for message deduplication using
 * Redis as the backend store with local cache fallback. It handles both
 * single message and batch deduplication operations with automatic failover
 * and performance monitoring.
 */
class Deduplicator {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the deduplication system
     */
    explicit Deduplicator(const DeduplicationConfig& config);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~Deduplicator();
    
    // Non-copyable, non-movable (contains mutex and threads)
    Deduplicator(const Deduplicator&) = delete;
    Deduplicator& operator=(const Deduplicator&) = delete;
    Deduplicator(Deduplicator&&) = delete;
    Deduplicator& operator=(Deduplicator&&) = delete;
    
    /**
     * @brief Initialize the deduplication system
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the deduplication system gracefully
     */
    void shutdown();
    
    /**
     * @brief Check if a message is a duplicate
     * @param imei Device IMEI identifier
     * @param sequenceNumber Message sequence number
     * @return DeduplicationResult indicating if message is unique, duplicate, or error
     */
    DeduplicationResult isDuplicate(const std::string& imei, uint32_t sequenceNumber);
    
    /**
     * @brief Check if a message is a duplicate (using MessageId)
     * @param messageId Message identifier
     * @return DeduplicationResult indicating if message is unique, duplicate, or error
     */
    DeduplicationResult isDuplicate(const MessageId& messageId);
    
    /**
     * @brief Batch check for duplicates (more efficient for multiple messages)
     * @param messages Vector of message identifiers to check
     * @return BatchDeduplicationResult with results for each message
     */
    BatchDeduplicationResult areDuplicates(const std::vector<MessageId>& messages);
    
    /**
     * @brief Batch check for duplicates using IMEI and sequence pairs
     * @param messages Vector of IMEI and sequence number pairs
     * @return BatchDeduplicationResult with results for each message
     */
    BatchDeduplicationResult areDuplicates(
        const std::vector<std::pair<std::string, uint32_t>>& messages);
    
    /**
     * @brief Clear deduplication history for a specific device
     * @param imei Device IMEI identifier
     * @return Number of entries cleared
     */
    int clearHistory(const std::string& imei);
    
    /**
     * @brief Clear deduplication history for a specific message
     * @param messageId Message identifier to clear
     * @return true if entry was found and cleared
     */
    bool clearHistory(const MessageId& messageId);
    
    /**
     * @brief Clear all deduplication history (use with caution)
     * @return Number of entries cleared
     */
    int clearAllHistory();
    
    /**
     * @brief Check if the deduplication system is connected and operational
     * @return true if system is operational
     */
    bool isConnected() const;
    
    /**
     * @brief Check if currently using standby Redis instance
     * @return true if using standby instance
     */
    bool isUsingStandby() const;
    
    /**
     * @brief Get comprehensive system statistics
     * @return Current deduplication statistics
     */
    DeduplicationStats getStats() const;
    
    /**
     * @brief Reset all statistics counters
     */
    void resetStats();
    
    /**
     * @brief Force a health check of the system
     * @return true if system is healthy
     */
    bool healthCheck();
    
    /**
     * @brief Get configuration being used
     * @return Current configuration
     */
    const DeduplicationConfig& getConfig() const { return config_; }
    
    /**
     * @brief Update deduplication window (affects future operations)
     * @param newWindow New deduplication window duration
     */
    void updateDeduplicationWindow(std::chrono::milliseconds newWindow);
    
    /**
     * @brief Enable or disable local cache fallback
     * @param enabled Whether to enable local cache fallback
     */
    void setLocalCacheFallbackEnabled(bool enabled);

private:
    // Configuration
    DeduplicationConfig config_;
    
    // Core components
    std::unique_ptr<ConnectionPool> connectionPool_;
    std::unique_ptr<LocalCache> localCache_;
    
    // System state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shuttingDown_{false};
    std::atomic<bool> localCacheFallbackEnabled_{true};
    
    // Statistics
    mutable std::atomic<uint64_t> totalChecks_{0};
    mutable std::atomic<uint64_t> duplicatesFound_{0};
    mutable std::atomic<uint64_t> uniqueMessages_{0};
    mutable std::atomic<uint64_t> redisFailures_{0};
    mutable std::atomic<uint64_t> cacheHits_{0};
    mutable std::atomic<uint64_t> cacheMisses_{0};
    
    // Performance tracking
    mutable std::mutex latencyMutex_;
    std::vector<std::chrono::microseconds> recentLatencies_;
    std::chrono::microseconds totalLatency_{0};
    std::chrono::microseconds maxLatency_{0};
    std::chrono::microseconds minLatency_{std::chrono::microseconds::max()};
    
    // Background maintenance
    std::thread maintenanceThread_;
    std::atomic<bool> maintenanceRunning_{false};
    std::chrono::milliseconds maintenanceInterval_{60000}; // 1 minute
    
    /**
     * @brief Perform deduplication check using Redis
     * @param messageId Message to check
     * @return DeduplicationResult from Redis operation
     */
    DeduplicationResult checkWithRedis(const MessageId& messageId);
    
    /**
     * @brief Perform batch deduplication check using Redis
     * @param messages Vector of messages to check
     * @return BatchDeduplicationResult from Redis operations
     */
    BatchDeduplicationResult batchCheckWithRedis(const std::vector<MessageId>& messages);
    
    /**
     * @brief Perform deduplication check using local cache
     * @param messageId Message to check
     * @return DeduplicationResult from cache operation
     */
    DeduplicationResult checkWithCache(const MessageId& messageId);
    
    /**
     * @brief Update local cache with Redis result
     * @param messageId Message that was checked
     * @param wasUnique Whether the message was unique in Redis
     */
    void updateLocalCache(const MessageId& messageId, bool wasUnique);
    
    /**
     * @brief Handle Redis operation failure
     * @param operation Description of failed operation
     * @param messageId Message being processed (optional)
     */
    void handleRedisFailure(const std::string& operation, 
                           const MessageId* messageId = nullptr);
    
    /**
     * @brief Update performance statistics
     * @param latency Latency of the operation
     */
    void updateLatencyStats(std::chrono::microseconds latency);
    
    /**
     * @brief Background maintenance thread function
     */
    void maintenanceLoop();
    
    /**
     * @brief Perform periodic maintenance tasks
     */
    void performMaintenance();
    
    /**
     * @brief Calculate average latency from recent samples
     * @return Average latency in microseconds
     */
    std::chrono::microseconds calculateAverageLatency() const;
    
    /**
     * @brief Generate Redis key for a message
     * @param messageId Message identifier
     * @return Redis key string
     */
    std::string generateRedisKey(const MessageId& messageId) const;
    
    /**
     * @brief Validate message identifier
     * @param messageId Message identifier to validate
     * @return true if valid
     */
    bool isValidMessageId(const MessageId& messageId) const;
    
    /**
     * @brief Convert DeduplicationResult to boolean for backwards compatibility
     * @param result Deduplication result
     * @return true if message is duplicate
     */
    static bool resultToBool(DeduplicationResult result);
};

} // namespace redis_deduplication