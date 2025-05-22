// components/redis-deduplication/include/redis_deduplication/local_cache.hpp
#pragma once

#include "types.hpp"
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>
#include <atomic>

namespace redis_deduplication {

/**
 * @brief Thread-safe LRU cache entry with TTL support
 */
class CacheEntry {
public:
    CacheEntry(const MessageId& messageId, 
               std::chrono::system_clock::time_point expiryTime)
        : messageId_(messageId), expiryTime_(expiryTime), 
          createdTime_(std::chrono::system_clock::now()) {}
    
    const MessageId& getMessageId() const { return messageId_; }
    std::chrono::system_clock::time_point getExpiryTime() const { return expiryTime_; }
    std::chrono::system_clock::time_point getCreatedTime() const { return createdTime_; }
    
    bool isExpired() const {
        return std::chrono::system_clock::now() > expiryTime_;
    }
    
    bool isExpired(std::chrono::system_clock::time_point currentTime) const {
        return currentTime > expiryTime_;
    }
    
    // Allow LocalCache to modify expiry time
    friend class LocalCache;

private:
    MessageId messageId_;
    std::chrono::system_clock::time_point expiryTime_;
    std::chrono::system_clock::time_point createdTime_;
};

/**
 * @brief Thread-safe LRU cache with TTL support for message deduplication
 * 
 * This cache uses a combination of hash map for O(1) lookups and 
 * doubly-linked list for O(1) LRU operations. Expired entries are 
 * lazily removed during access operations.
 */
class LocalCache {
public:
    /**
     * @brief Constructor
     * @param maxSize Maximum number of entries in cache
     * @param ttl Time-to-live for cache entries
     */
    LocalCache(size_t maxSize, std::chrono::milliseconds ttl);
    
    /**
     * @brief Destructor
     */
    ~LocalCache() = default;
    
    // Non-copyable, non-movable (contains mutex)
    LocalCache(const LocalCache&) = delete;
    LocalCache& operator=(const LocalCache&) = delete;
    LocalCache(LocalCache&&) = delete;
    LocalCache& operator=(LocalCache&&) = delete;
    
    /**
     * @brief Check if a message exists in cache (and is not expired)
     * @param messageId Message identifier to check
     * @return true if message exists and is not expired
     */
    bool contains(const MessageId& messageId);
    
    /**
     * @brief Insert a message into cache
     * @param messageId Message identifier to insert
     * @return true if insertion successful
     */
    bool insert(const MessageId& messageId);
    
    /**
     * @brief Insert a message with custom expiry time
     * @param messageId Message identifier to insert
     * @param expiryTime Custom expiry time
     * @return true if insertion successful
     */
    bool insert(const MessageId& messageId, 
                std::chrono::system_clock::time_point expiryTime);
    
    /**
     * @brief Remove a specific message from cache
     * @param messageId Message identifier to remove
     * @return true if message was found and removed
     */
    bool remove(const MessageId& messageId);
    
    /**
     * @brief Clear all entries from cache
     */
    void clear();
    
    /**
     * @brief Remove all expired entries
     * @return Number of entries removed
     */
    size_t cleanupExpired();
    
    /**
     * @brief Get current cache size (including expired entries)
     */
    size_t size() const;
    
    /**
     * @brief Get effective cache size (excluding expired entries)
     * This is an expensive operation as it checks all entries
     */
    size_t effectiveSize() const;
    
    /**
     * @brief Get maximum cache size
     */
    size_t maxSize() const { return maxSize_; }
    
    /**
     * @brief Check if cache is full
     */
    bool isFull() const;
    
    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t currentSize;
        size_t maxSize;
        uint64_t hits;
        uint64_t misses;
        uint64_t insertions;
        uint64_t evictions;
        uint64_t expiredRemovals;
        double hitRate;
        std::chrono::system_clock::time_point lastCleanup;
    };
    
    CacheStats getStats() const;
    
    /**
     * @brief Reset statistics counters
     */
    void resetStats();
    
    /**
     * @brief Set automatic cleanup interval
     * @param interval Interval between automatic cleanups (0 to disable)
     */
    void setCleanupInterval(std::chrono::milliseconds interval);

private:
    // Configuration
    const size_t maxSize_;
    const std::chrono::milliseconds ttl_;
    
    // Cache storage using hash map + doubly linked list for LRU
    using CacheList = std::list<CacheEntry>;
    using CacheIterator = CacheList::iterator;
    
    CacheList cacheList_;
    std::unordered_map<std::string, CacheIterator> cacheMap_;
    
    // Thread safety
    mutable std::mutex cacheMutex_;
    
    // Statistics
    mutable std::atomic<uint64_t> hits_;
    mutable std::atomic<uint64_t> misses_;
    mutable std::atomic<uint64_t> insertions_;
    mutable std::atomic<uint64_t> evictions_;
    mutable std::atomic<uint64_t> expiredRemovals_;
    
    // Automatic cleanup
    std::chrono::milliseconds cleanupInterval_;
    std::chrono::system_clock::time_point lastCleanup_;
    std::chrono::system_clock::time_point nextCleanup_;
    
    /**
     * @brief Generate cache key from MessageId
     * @param messageId Message identifier
     * @return String key for internal hash map
     */
    std::string generateKey(const MessageId& messageId) const;
    
    /**
     * @brief Move an entry to the front of the LRU list (most recently used)
     * @param it Iterator to the entry in the list
     * Must be called with cacheMutex_ held
     */
    void moveToFront(CacheIterator it);
    
    /**
     * @brief Evict the least recently used entry
     * Must be called with cacheMutex_ held
     * @return true if an entry was evicted
     */
    bool evictLRU();
    
    /**
     * @brief Remove expired entries from cache
     * Must be called with cacheMutex_ held
     * @return Number of entries removed
     */
    size_t removeExpiredEntries();
    
    /**
     * @brief Check if automatic cleanup should run
     * Must be called with cacheMutex_ held
     */
    void checkAndRunCleanup();
    
    /**
     * @brief Calculate hit rate
     * @return Hit rate as percentage (0.0 to 1.0)
     */
    double calculateHitRate() const;
};

} // namespace redis_deduplication