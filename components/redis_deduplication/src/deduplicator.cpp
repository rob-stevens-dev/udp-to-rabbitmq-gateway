// components/redis-deduplication/src/deduplicator.cpp
#include "redis_deduplication/deduplicator.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <regex>

namespace redis_deduplication {

Deduplicator::Deduplicator(const DeduplicationConfig& config)
    : config_(config), 
      localCacheFallbackEnabled_(config.enableLocalCacheFallback) {
    
    spdlog::info("Deduplicator created with window: {}ms, cache size: {}", 
                 config_.deduplicationWindow.count(), config_.localCacheSize);
}

Deduplicator::~Deduplicator() {
    shutdown();
}

bool Deduplicator::initialize() {
    if (initialized_.load()) {
        spdlog::warn("Deduplicator already initialized");
        return true;
    }
    
    spdlog::info("Initializing deduplicator system");
    
    try {
        // Initialize connection pool
        connectionPool_ = std::make_unique<ConnectionPool>(config_);
        if (!connectionPool_->initialize()) {
            spdlog::error("Failed to initialize connection pool");
            return false;
        }
        
        // Initialize local cache
        localCache_ = std::make_unique<LocalCache>(
            config_.localCacheSize, config_.localCacheTtl);
        
        // Start maintenance thread
        maintenanceRunning_ = true;
        maintenanceThread_ = std::thread(&Deduplicator::maintenanceLoop, this);
        
        initialized_ = true;
        spdlog::info("Deduplicator system initialized successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during deduplicator initialization: {}", e.what());
        return false;
    }
}

void Deduplicator::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    spdlog::info("Shutting down deduplicator system");
    
    shuttingDown_ = true;
    
    // Stop maintenance thread
    if (maintenanceRunning_.load()) {
        maintenanceRunning_ = false;
        if (maintenanceThread_.joinable()) {
            maintenanceThread_.join();
        }
    }
    
    // Shutdown connection pool
    if (connectionPool_) {
        connectionPool_->shutdown();
        connectionPool_.reset();
    }
    
    // Clear local cache
    if (localCache_) {
        localCache_->clear();
        localCache_.reset();
    }
    
    initialized_ = false;
    spdlog::info("Deduplicator system shutdown complete");
}

DeduplicationResult Deduplicator::isDuplicate(const std::string& imei, uint32_t sequenceNumber) {
    return isDuplicate(MessageId(imei, sequenceNumber));
}

DeduplicationResult Deduplicator::isDuplicate(const MessageId& messageId) {
    if (!initialized_.load() || shuttingDown_.load()) {
        spdlog::error("Deduplicator not initialized or shutting down");
        return DeduplicationResult::ERROR;
    }
    
    if (!isValidMessageId(messageId)) {
        spdlog::warn("Invalid message ID: IMEI={}, SeqNum={}", 
                     messageId.imei, messageId.sequenceNumber);
        return DeduplicationResult::ERROR;
    }
    
    const auto startTime = std::chrono::high_resolution_clock::now();
    
    // Increment total checks counter
    totalChecks_++;
    
    DeduplicationResult result = DeduplicationResult::ERROR;
    
    try {
        // First check local cache if enabled
        if (localCacheFallbackEnabled_.load()) {
            result = checkWithCache(messageId);
            if (result == DeduplicationResult::DUPLICATE) {
                cacheHits_++;
                const auto endTime = std::chrono::high_resolution_clock::now();
                updateLatencyStats(std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime));
                return result;
            }
        }
        
        // Check with Redis
        result = checkWithRedis(messageId);
        
        // Update local cache based on Redis result
        if (localCacheFallbackEnabled_.load() && result != DeduplicationResult::ERROR) {
            updateLocalCache(messageId, result == DeduplicationResult::UNIQUE);
        }
        
        // Update statistics
        if (result == DeduplicationResult::DUPLICATE) {
            duplicatesFound_++;
        } else if (result == DeduplicationResult::UNIQUE) {
            uniqueMessages_++;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in isDuplicate: {}", e.what());
        result = DeduplicationResult::ERROR;
    }
    
    const auto endTime = std::chrono::high_resolution_clock::now();
    updateLatencyStats(std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime));
    
    return result;
}

BatchDeduplicationResult Deduplicator::areDuplicates(const std::vector<MessageId>& messages) {
    BatchDeduplicationResult batchResult;
    batchResult.results.reserve(messages.size());
    
    if (!initialized_.load() || shuttingDown_.load()) {
        spdlog::error("Deduplicator not initialized or shutting down");
        batchResult.results.assign(messages.size(), DeduplicationResult::ERROR);
        batchResult.errors = static_cast<int>(messages.size());
        return batchResult;
    }
    
    if (messages.empty()) {
        return batchResult;
    }
    
    const auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // Use batch Redis operation for better performance
        batchResult = batchCheckWithRedis(messages);
        
        // Update local cache for all results
        if (localCacheFallbackEnabled_.load()) {
            for (size_t i = 0; i < messages.size(); ++i) {
                if (batchResult.results[i] != DeduplicationResult::ERROR) {
                    updateLocalCache(messages[i], 
                                   batchResult.results[i] == DeduplicationResult::UNIQUE);
                }
            }
        }
        
        // Update statistics
        for (const auto& result : batchResult.results) {
            totalChecks_++;
            if (result == DeduplicationResult::DUPLICATE) {
                duplicatesFound_++;
            } else if (result == DeduplicationResult::UNIQUE) {
                uniqueMessages_++;
            }
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in batch deduplication: {}", e.what());
        batchResult.results.assign(messages.size(), DeduplicationResult::ERROR);
        batchResult.errors = static_cast<int>(messages.size());
    }
    
    const auto endTime = std::chrono::high_resolution_clock::now();
    batchResult.totalLatency = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    updateLatencyStats(batchResult.totalLatency);
    
    return batchResult;
}

BatchDeduplicationResult Deduplicator::areDuplicates(
        const std::vector<std::pair<std::string, uint32_t>>& messages) {
    
    std::vector<MessageId> messageIds;
    messageIds.reserve(messages.size());
    
    for (const auto& pair : messages) {
        messageIds.emplace_back(pair.first, pair.second);
    }
    
    return areDuplicates(messageIds);
}

int Deduplicator::clearHistory(const std::string& imei) {
    // Placeholder implementation
    spdlog::info("Clearing history for IMEI: {}", imei);
    return 0;
}

bool Deduplicator::clearHistory(const MessageId& messageId) {
    // Placeholder implementation
    spdlog::info("Clearing history for message: {}:{}", 
                 messageId.imei, messageId.sequenceNumber);
    return true;
}

int Deduplicator::clearAllHistory() {
    // Placeholder implementation
    spdlog::warn("Clearing all history");
    return 0;
}

bool Deduplicator::isConnected() const {
    return initialized_.load() && connectionPool_ && connectionPool_->isHealthy();
}

bool Deduplicator::isUsingStandby() const {
    return connectionPool_ && connectionPool_->isUsingStandby();
}

DeduplicationStats Deduplicator::getStats() const {
    DeduplicationStats stats;
    
    // Operation counters
    stats.totalChecks = totalChecks_.load();
    stats.duplicatesFound = duplicatesFound_.load();
    stats.uniqueMessages = uniqueMessages_.load();
    
    // Redis statistics
    stats.redisFailures = redisFailures_.load();
    
    // Local cache statistics
    stats.localCacheHits = cacheHits_.load();
    stats.localCacheMisses = cacheMisses_.load();
    
    if (localCache_) {
        auto cacheStats = localCache_->getStats();
        stats.localCacheEvictions = cacheStats.evictions;
    }
    
    // Performance metrics
    stats.averageCheckLatency = calculateAverageLatency();
    stats.maxCheckLatency = maxLatency_;
    stats.minCheckLatency = (minLatency_ == std::chrono::microseconds::max()) ? 
                           std::chrono::microseconds{0} : minLatency_;
    
    // Connection pool statistics
    if (connectionPool_) {
        auto poolStats = connectionPool_->getStats();
        stats.activeConnections = poolStats.activeConnections;
        stats.totalConnections = poolStats.totalConnections;
        stats.failedConnections = poolStats.failedConnections;
        stats.redisReconnects = poolStats.reconnectAttempts;
    }
    
    stats.lastUpdated = std::chrono::system_clock::now();
    
    return stats;
}

void Deduplicator::resetStats() {
    totalChecks_ = 0;
    duplicatesFound_ = 0;
    uniqueMessages_ = 0;
    redisFailures_ = 0;
    cacheHits_ = 0;
    cacheMisses_ = 0;
    
    // Reset latency tracking
    {
        std::lock_guard<std::mutex> lock(latencyMutex_);
        recentLatencies_.clear();
        totalLatency_ = std::chrono::microseconds{0};
        maxLatency_ = std::chrono::microseconds{0};
        minLatency_ = std::chrono::microseconds::max();
    }
    
    // Reset cache stats
    if (localCache_) {
        localCache_->resetStats();
    }
    
    // Reset connection pool stats
    if (connectionPool_) {
        connectionPool_->resetStats();
    }
    
    spdlog::info("Deduplication statistics reset");
}

bool Deduplicator::healthCheck() {
    if (!initialized_.load() || shuttingDown_.load()) {
        return false;
    }
    
    // Simplified health check
    return connectionPool_ && connectionPool_->isHealthy();
}

void Deduplicator::updateDeduplicationWindow(std::chrono::milliseconds newWindow) {
    if (newWindow.count() <= 0) {
        spdlog::warn("Invalid deduplication window: {}ms", newWindow.count());
        return;
    }
    
    config_.deduplicationWindow = newWindow;
    spdlog::info("Deduplication window updated to {}ms", newWindow.count());
}

void Deduplicator::setLocalCacheFallbackEnabled(bool enabled) {
    localCacheFallbackEnabled_ = enabled;
    spdlog::info("Local cache fallback {}", enabled ? "enabled" : "disabled");
}

// Private methods
DeduplicationResult Deduplicator::checkWithRedis(const MessageId& messageId) {
    auto connection = connectionPool_->getConnection();
    if (!connection) {
        handleRedisFailure("Failed to get connection", &messageId);
        return DeduplicationResult::ERROR;
    }
    
    try {
        std::string key = generateRedisKey(messageId);
        bool wasUnique = connection->setNX(key, config_.deduplicationWindow);
        
        return wasUnique ? DeduplicationResult::UNIQUE : DeduplicationResult::DUPLICATE;
        
    } catch (const std::exception& e) {
        handleRedisFailure("Redis operation failed: " + std::string(e.what()), &messageId);
        return DeduplicationResult::ERROR;
    }
}

BatchDeduplicationResult Deduplicator::batchCheckWithRedis(const std::vector<MessageId>& messages) {
    BatchDeduplicationResult result;
    result.results.reserve(messages.size());
    
    // Simplified implementation - check each message individually
    for (const auto& messageId : messages) {
        result.results.push_back(checkWithRedis(messageId));
    }
    
    return result;
}

DeduplicationResult Deduplicator::checkWithCache(const MessageId& messageId) {
    if (!localCache_) {
        return DeduplicationResult::CACHE_MISS;
    }
    
    try {
        if (localCache_->contains(messageId)) {
            return DeduplicationResult::DUPLICATE;
        } else {
            cacheMisses_++;
            return DeduplicationResult::CACHE_MISS;
        }
    } catch (const std::exception& e) {
        spdlog::warn("Local cache check failed: {}", e.what());
        return DeduplicationResult::CACHE_MISS;
    }
}

void Deduplicator::updateLocalCache(const MessageId& messageId, bool /* wasUnique */) {
    if (!localCache_ || !localCacheFallbackEnabled_.load()) {
        return;
    }
    
    try {
        localCache_->insert(messageId);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to update local cache: {}", e.what());
    }
}

void Deduplicator::handleRedisFailure(const std::string& operation, const MessageId* messageId) {
    redisFailures_++;
    
    std::string logMessage = "Redis failure: " + operation;
    if (messageId) {
        logMessage += " (IMEI: " + messageId->imei + 
                     ", SeqNum: " + std::to_string(messageId->sequenceNumber) + ")";
    }
    
    spdlog::error(logMessage);
}

void Deduplicator::updateLatencyStats(std::chrono::microseconds latency) {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    
    // Update min/max
    if (latency > maxLatency_) {
        maxLatency_ = latency;
    }
    
    if (latency < minLatency_) {
        minLatency_ = latency;
    }
    
    // Keep a rolling window of recent latencies for average calculation
    recentLatencies_.push_back(latency);
    totalLatency_ += latency;
    
    // Limit the size of recent latencies to prevent unbounded growth
    const size_t maxRecentSamples = 1000;
    if (recentLatencies_.size() > maxRecentSamples) {
        totalLatency_ -= recentLatencies_.front();
        recentLatencies_.erase(recentLatencies_.begin());
    }
}

void Deduplicator::maintenanceLoop() {
    spdlog::debug("Maintenance thread started");
    
    while (maintenanceRunning_.load()) {
        std::this_thread::sleep_for(maintenanceInterval_);
        
        if (!maintenanceRunning_.load()) {
            break;
        }
        
        try {
            performMaintenance();
        } catch (const std::exception& e) {
            spdlog::error("Exception in maintenance loop: {}", e.what());
        }
    }
    
    spdlog::debug("Maintenance thread stopped");
}

void Deduplicator::performMaintenance() {
    if (shuttingDown_.load()) {
        return;
    }
    
    try {
        // Clean up expired entries from local cache
        if (localCache_) {
            size_t removed = localCache_->cleanupExpired();
            if (removed > 0) {
                spdlog::debug("Maintenance removed {} expired cache entries", removed);
            }
        }
        
        // Test connection pool health
        if (connectionPool_) {
            int healthyConnections = connectionPool_->testConnections();
            spdlog::debug("Maintenance found {} healthy connections", healthyConnections);
        }
        
        // Log periodic statistics
        auto stats = getStats();
        spdlog::info("Maintenance stats - Checks: {}, Duplicates: {}, Cache hits: {}, Redis failures: {}",
                     stats.totalChecks, stats.duplicatesFound, stats.localCacheHits, stats.redisFailures);
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during maintenance: {}", e.what());
    }
}

std::chrono::microseconds Deduplicator::calculateAverageLatency() const {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    
    if (recentLatencies_.empty()) {
        return std::chrono::microseconds{0};
    }
    
    return std::chrono::microseconds{totalLatency_.count() / recentLatencies_.size()};
}

std::string Deduplicator::generateRedisKey(const MessageId& messageId) const {
    return messageId.toRedisKey(config_.keyPrefix);
}

bool Deduplicator::isValidMessageId(const MessageId& messageId) const {
    // IMEI should be 15 digits
    if (messageId.imei.length() != 15) {
        return false;
    }
    
    // IMEI should contain only digits
    if (!std::all_of(messageId.imei.begin(), messageId.imei.end(), ::isdigit)) {
        return false;
    }
    
    // Sequence number should be reasonable (not zero, not too large)
    if (messageId.sequenceNumber == 0 || messageId.sequenceNumber > 0xFFFFFF) {
        return false;
    }
    
    return true;
}

bool Deduplicator::resultToBool(DeduplicationResult result) {
    return result == DeduplicationResult::DUPLICATE;
}

} // namespace redis_deduplication