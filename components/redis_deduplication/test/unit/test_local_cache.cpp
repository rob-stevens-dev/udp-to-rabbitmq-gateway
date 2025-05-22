// components/redis-deduplication/test/unit/test_local_cache.cpp
#include <gtest/gtest.h>
#include "redis_deduplication/local_cache.hpp"
#include "redis_deduplication/types.hpp"
#include <thread>
#include <chrono>
#include <vector>

using namespace redis_deduplication;

class LocalCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<LocalCache>(maxSize, ttl);
    }
    
    void TearDown() override {
        cache.reset();
    }
    
    static constexpr size_t maxSize = 100;
    static constexpr auto ttl = std::chrono::milliseconds{1000};
    std::unique_ptr<LocalCache> cache;
};

TEST_F(LocalCacheTest, BasicInsertAndContains) {
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 101);
    
    // Initially empty
    EXPECT_FALSE(cache->contains(msg1));
    EXPECT_FALSE(cache->contains(msg2));
    EXPECT_EQ(0, cache->size());
    
    // Insert first message
    EXPECT_TRUE(cache->insert(msg1));
    EXPECT_TRUE(cache->contains(msg1));
    EXPECT_FALSE(cache->contains(msg2));
    EXPECT_EQ(1, cache->size());
    
    // Insert second message
    EXPECT_TRUE(cache->insert(msg2));
    EXPECT_TRUE(cache->contains(msg1));
    EXPECT_TRUE(cache->contains(msg2));
    EXPECT_EQ(2, cache->size());
}

TEST_F(LocalCacheTest, DuplicateInsert) {
    MessageId msg("123456789012345", 100);
    
    // Insert first time
    EXPECT_TRUE(cache->insert(msg));
    EXPECT_EQ(1, cache->size());
    
    // Insert same message again
    EXPECT_TRUE(cache->insert(msg));
    EXPECT_EQ(1, cache->size()); // Size should not increase
    EXPECT_TRUE(cache->contains(msg));
}

TEST_F(LocalCacheTest, LRUEviction) {
    // Fill cache to capacity
    std::vector<MessageId> messages;
    for (size_t i = 0; i < maxSize; ++i) {
        messages.emplace_back("123456789012345", static_cast<uint32_t>(i + 1));
        EXPECT_TRUE(cache->insert(messages.back()));
    }
    
    EXPECT_EQ(maxSize, cache->size());
    
    // All messages should be present
    for (const auto& msg : messages) {
        EXPECT_TRUE(cache->contains(msg));
    }
    
    // Insert one more message, should evict the least recently used (first one)
    MessageId newMsg("123456789012345", static_cast<uint32_t>(maxSize + 1));
    EXPECT_TRUE(cache->insert(newMsg));
    
    EXPECT_EQ(maxSize, cache->size());
    EXPECT_FALSE(cache->contains(messages[0])); // First message should be evicted
    EXPECT_TRUE(cache->contains(newMsg));       // New message should be present
    
    // Other messages should still be present
    for (size_t i = 1; i < messages.size(); ++i) {
        EXPECT_TRUE(cache->contains(messages[i]));
    }
}

TEST_F(LocalCacheTest, TTLExpiration) {
    MessageId msg("123456789012345", 100);
    
    // Insert with very short TTL
    auto shortTtl = std::chrono::milliseconds{50};
    LocalCache shortCache(10, shortTtl);
    
    EXPECT_TRUE(shortCache.insert(msg));
    EXPECT_TRUE(shortCache.contains(msg));
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    
    // Message should be expired
    EXPECT_FALSE(shortCache.contains(msg));
}

TEST_F(LocalCacheTest, Remove) {
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 101);
    
    // Insert messages
    EXPECT_TRUE(cache->insert(msg1));
    EXPECT_TRUE(cache->insert(msg2));
    EXPECT_EQ(2, cache->size());
    
    // Remove first message
    EXPECT_TRUE(cache->remove(msg1));
    EXPECT_FALSE(cache->contains(msg1));
    EXPECT_TRUE(cache->contains(msg2));
    EXPECT_EQ(1, cache->size());
    
    // Try to remove non-existent message
    EXPECT_FALSE(cache->remove(msg1));
    EXPECT_EQ(1, cache->size());
    
    // Remove second message
    EXPECT_TRUE(cache->remove(msg2));
    EXPECT_FALSE(cache->contains(msg2));
    EXPECT_EQ(0, cache->size());
}

TEST_F(LocalCacheTest, Clear) {
    // Insert multiple messages
    for (uint32_t i = 1; i <= 10; ++i) {
        MessageId msg("123456789012345", i);
        EXPECT_TRUE(cache->insert(msg));
    }
    
    EXPECT_EQ(10, cache->size());
    
    // Clear cache
    cache->clear();
    EXPECT_EQ(0, cache->size());
    
    // Verify all messages are gone
    for (uint32_t i = 1; i <= 10; ++i) {
        MessageId msg("123456789012345", i);
        EXPECT_FALSE(cache->contains(msg));
    }
}

TEST_F(LocalCacheTest, Statistics) {
    auto stats = cache->getStats();
    EXPECT_EQ(0, stats.hits);
    EXPECT_EQ(0, stats.misses);
    EXPECT_EQ(0, stats.insertions);
    EXPECT_EQ(0, stats.evictions);
    
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 101);
    
    // Miss
    EXPECT_FALSE(cache->contains(msg1));
    stats = cache->getStats();
    EXPECT_EQ(0, stats.hits);
    EXPECT_EQ(1, stats.misses);
    
    // Insert
    EXPECT_TRUE(cache->insert(msg1));
    stats = cache->getStats();
    EXPECT_EQ(1, stats.insertions);
    
    // Hit
    EXPECT_TRUE(cache->contains(msg1));
    stats = cache->getStats();
    EXPECT_EQ(1, stats.hits);
    EXPECT_EQ(1, stats.misses);
    
    // Calculate hit rate
    double expectedHitRate = 1.0 / 2.0; // 1 hit out of 2 total accesses
    EXPECT_DOUBLE_EQ(expectedHitRate, stats.hitRate);
}

TEST_F(LocalCacheTest, ThreadSafety) {
    const int numThreads = 10;
    const int messagesPerThread = 100;
    std::vector<std::thread> threads;
    
    // Launch multiple threads that insert and check messages
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                MessageId msg("123456789012345", 
                             static_cast<uint32_t>(t * messagesPerThread + i + 1));
                
                // Insert and immediately check
                cache->insert(msg);
                EXPECT_TRUE(cache->contains(msg));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Cache should not exceed maximum size due to LRU eviction
    EXPECT_LE(cache->size(), maxSize);
    
    // Statistics should be consistent
    auto stats = cache->getStats();
    EXPECT_EQ(numThreads * messagesPerThread, stats.insertions);
    EXPECT_GT(stats.hits, 0); // Should have some hits from the contains() calls
}

TEST_F(LocalCacheTest, ExpiredCleanup) {
    // Create cache with very short TTL
    auto shortTtl = std::chrono::milliseconds{50};
    LocalCache shortCache(10, shortTtl);
    
    // Insert messages
    for (uint32_t i = 1; i <= 5; ++i) {
        MessageId msg("123456789012345", i);
        EXPECT_TRUE(shortCache.insert(msg));
    }
    
    EXPECT_EQ(5, shortCache.size());
    
    // Wait for messages to expire
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    
    // Manually trigger cleanup
    size_t removed = shortCache.cleanupExpired();
    EXPECT_EQ(5, removed);
    EXPECT_EQ(0, shortCache.size());
}

TEST_F(LocalCacheTest, CustomExpiryTime) {
    MessageId msg("123456789012345", 100);
    
    // Insert with custom expiry time (in the past)
    auto pastTime = std::chrono::system_clock::now() - std::chrono::seconds{1};
    EXPECT_TRUE(cache->insert(msg, pastTime));
    
    // Message should be immediately expired
    EXPECT_FALSE(cache->contains(msg));
}

TEST_F(LocalCacheTest, EffectiveSize) {
    // Create cache with short TTL
    auto shortTtl = std::chrono::milliseconds{100};
    LocalCache shortCache(10, shortTtl);
    
    // Insert messages
    for (uint32_t i = 1; i <= 5; ++i) {
        MessageId msg("123456789012345", i);
        EXPECT_TRUE(shortCache.insert(msg));
    }
    
    EXPECT_EQ(5, shortCache.size());
    EXPECT_EQ(5, shortCache.effectiveSize());
    
    // Wait for some messages to expire
    std::this_thread::sleep_for(std::chrono::milliseconds{150});
    
    // Size should still be 5 (lazy cleanup), but effective size should be 0
    EXPECT_EQ(5, shortCache.size());
    EXPECT_EQ(0, shortCache.effectiveSize());
}