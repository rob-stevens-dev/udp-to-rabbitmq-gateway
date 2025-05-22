// components/redis-deduplication/src/local_cache.cpp
#include "redis_deduplication/local_cache.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace redis_deduplication {

LocalCache::LocalCache(size_t maxSize, std::chrono::milliseconds ttl)
    : maxSize_(maxSize), ttl_(ttl), hits_(0), misses_(0), 
      insertions_(0), evictions_(0), expiredRemovals_(0),
      cleanupInterval_(std::chrono::milliseconds{300000}), // 5 minutes
      lastCleanup_(std::chrono::system_clock::now()),
      nextCleanup_(lastCleanup_ + cleanupInterval_) {
    
    if (maxSize_ == 0) {
        throw std::invalid_argument("Cache max size must be greater than 0");
    }
    
    spdlog::debug("LocalCache created with max size: {}, TTL: {}ms", 
                  maxSize_, ttl_.count());
}

bool LocalCache::contains(const MessageId& messageId) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    checkAndRunCleanup();
    
    const std::string key = generateKey(messageId);
    auto it = cacheMap_.find(key);
    
    if (it == cacheMap_.end()) {
        misses_++;
        return false;
    }
    
    // Check if entry is expired
    if (it->second->isExpired()) {
        // Remove expired entry
        cacheList_.erase(it->second);
        cacheMap_.erase(it);
        expiredRemovals_++;
        misses_++;
        return false;
    }
    
    // Move to front (most recently used)
    moveToFront(it->second);
    hits_++;
    
    return true;
}

bool LocalCache::insert(const MessageId& messageId) {
    auto expiryTime = std::chrono::system_clock::now() + ttl_;
    return insert(messageId, expiryTime);
}

bool LocalCache::insert(const MessageId& messageId, 
                        std::chrono::system_clock::time_point expiryTime) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    checkAndRunCleanup();
    
    const std::string key = generateKey(messageId);
    
    // Check if key already exists
    auto existingIt = cacheMap_.find(key);
    if (existingIt != cacheMap_.end()) {
        // Update existing entry's position and expiry time
        // Use const_cast to modify the expiry time through friend access
        const_cast<CacheEntry&>(*existingIt->second).expiryTime_ = expiryTime;
        moveToFront(existingIt->second);
        return true;
    }
    
    // Make room if cache is full
    if (cacheList_.size() >= maxSize_) {
        if (!evictLRU()) {
            spdlog::warn("Failed to evict LRU entry from cache");
            return false;
        }
    }
    
    // Insert new entry at front
    cacheList_.emplace_front(messageId, expiryTime);
    cacheMap_[key] = cacheList_.begin();
    insertions_++;
    
    return true;
}

bool LocalCache::remove(const MessageId& messageId) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    const std::string key = generateKey(messageId);
    auto it = cacheMap_.find(key);
    
    if (it == cacheMap_.end()) {
        return false;
    }
    
    cacheList_.erase(it->second);
    cacheMap_.erase(it);
    
    return true;
}

void LocalCache::clear() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    cacheList_.clear();
    cacheMap_.clear();
    
    spdlog::debug("LocalCache cleared");
}

size_t LocalCache::cleanupExpired() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return removeExpiredEntries();
}

size_t LocalCache::size() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cacheList_.size();
}

size_t LocalCache::effectiveSize() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    const auto now = std::chrono::system_clock::now();
    size_t count = 0;
    
    for (const auto& entry : cacheList_) {
        if (!entry.isExpired(now)) {
            count++;
        }
    }
    
    return count;
}

bool LocalCache::isFull() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cacheList_.size() >= maxSize_;
}

LocalCache::CacheStats LocalCache::getStats() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    CacheStats stats;
    stats.currentSize = cacheList_.size();
    stats.maxSize = maxSize_;
    stats.hits = hits_.load();
    stats.misses = misses_.load();
    stats.insertions = insertions_.load();
    stats.evictions = evictions_.load();
    stats.expiredRemovals = expiredRemovals_.load();
    stats.hitRate = calculateHitRate();
    stats.lastCleanup = lastCleanup_;
    
    return stats;
}

void LocalCache::resetStats() {
    hits_ = 0;
    misses_ = 0;
    insertions_ = 0;
    evictions_ = 0;
    expiredRemovals_ = 0;
    
    std::lock_guard<std::mutex> lock(cacheMutex_);
    lastCleanup_ = std::chrono::system_clock::now();
    
    spdlog::debug("LocalCache stats reset");
}

void LocalCache::setCleanupInterval(std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    cleanupInterval_ = interval;
    if (interval.count() > 0) {
        nextCleanup_ = std::chrono::system_clock::now() + interval;
    } else {
        // Disable automatic cleanup
        nextCleanup_ = std::chrono::system_clock::time_point::max();
    }
    
    spdlog::debug("LocalCache cleanup interval set to {}ms", interval.count());
}

std::string LocalCache::generateKey(const MessageId& messageId) const {
    return messageId.imei + ":" + std::to_string(messageId.sequenceNumber);
}

void LocalCache::moveToFront(CacheIterator it) {
    // Move iterator to front of list (most recently used)
    cacheList_.splice(cacheList_.begin(), cacheList_, it);
}

bool LocalCache::evictLRU() {
    if (cacheList_.empty()) {
        return false;
    }
    
    // Remove least recently used entry (back of list)
    auto lruIt = std::prev(cacheList_.end());
    const std::string key = generateKey(lruIt->getMessageId());
    
    cacheMap_.erase(key);
    cacheList_.erase(lruIt);
    evictions_++;
    
    return true;
}

size_t LocalCache::removeExpiredEntries() {
    const auto now = std::chrono::system_clock::now();
    size_t removedCount = 0;
    
    // Iterate through list and remove expired entries
    auto it = cacheList_.begin();
    while (it != cacheList_.end()) {
        if (it->isExpired(now)) {
            const std::string key = generateKey(it->getMessageId());
            cacheMap_.erase(key);
            it = cacheList_.erase(it);
            removedCount++;
            expiredRemovals_++;
        } else {
            ++it;
        }
    }
    
    if (removedCount > 0) {
        spdlog::debug("Removed {} expired entries from cache", removedCount);
    }
    
    return removedCount;
}

void LocalCache::checkAndRunCleanup() {
    const auto now = std::chrono::system_clock::now();
    
    if (cleanupInterval_.count() > 0 && now >= nextCleanup_) {
        size_t removed = removeExpiredEntries();
        lastCleanup_ = now;
        nextCleanup_ = now + cleanupInterval_;
        
        if (removed > 0) {
            spdlog::debug("Automatic cleanup removed {} expired entries", removed);
        }
    }
}

double LocalCache::calculateHitRate() const {
    const uint64_t totalRequests = hits_.load() + misses_.load();
    if (totalRequests == 0) {
        return 0.0;
    }
    
    return static_cast<double>(hits_.load()) / totalRequests;
}

} // namespace redis_deduplication