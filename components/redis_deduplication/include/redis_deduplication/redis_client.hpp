// components/redis-deduplication/include/redis_deduplication/redis_client.hpp
#pragma once

#include "types.hpp"
#include <hiredis/hiredis.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>

namespace redis_deduplication {

/**
 * @brief RAII wrapper for Redis replies
 */
class RedisReply {
public:
    explicit RedisReply(redisReply* reply) : reply_(reply) {}
    ~RedisReply() { if (reply_) freeReplyObject(reply_); }
    
    // Move semantics
    RedisReply(RedisReply&& other) noexcept : reply_(other.reply_) {
        other.reply_ = nullptr;
    }
    
    RedisReply& operator=(RedisReply&& other) noexcept {
        if (this != &other) {
            if (reply_) freeReplyObject(reply_);
            reply_ = other.reply_;
            other.reply_ = nullptr;
        }
        return *this;
    }
    
    // No copy semantics
    RedisReply(const RedisReply&) = delete;
    RedisReply& operator=(const RedisReply&) = delete;
    
    redisReply* get() const { return reply_; }
    redisReply* operator->() const { return reply_; }
    explicit operator bool() const { return reply_ != nullptr; }

private:
    redisReply* reply_;
};

/**
 * @brief Thread-safe Redis client wrapper
 */
class RedisClient {
public:
    /**
     * @brief Constructor
     * @param host Redis server hostname
     * @param port Redis server port
     * @param password Optional password for authentication
     * @param connectionTimeout Connection timeout in milliseconds
     * @param socketTimeout Socket timeout in milliseconds
     */
    RedisClient(const std::string& host, int port, 
                const std::string& password = "",
                std::chrono::milliseconds connectionTimeout = std::chrono::milliseconds{5000},
                std::chrono::milliseconds socketTimeout = std::chrono::milliseconds{3000});
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~RedisClient();
    
    // Non-copyable, movable
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;
    RedisClient(RedisClient&& other) noexcept;
    RedisClient& operator=(RedisClient&& other) noexcept;
    
    /**
     * @brief Connect to Redis server
     * @return true if connection successful
     */
    bool connect();
    
    /**
     * @brief Disconnect from Redis server
     */
    void disconnect();
    
    /**
     * @brief Check if connected to Redis
     * @return Connection status
     */
    ConnectionStatus getConnectionStatus() const;
    
    /**
     * @brief Check if a key exists with NX flag (atomic check-and-set)
     * @param key Redis key to check
     * @param ttlMs TTL in milliseconds for the key if it doesn't exist
     * @return true if key was newly created (not duplicate), false if key existed (duplicate)
     */
    bool setNX(const std::string& key, std::chrono::milliseconds ttlMs);
    
    /**
     * @brief Batch check for multiple keys using SET NX
     * @param keys Vector of Redis keys to check
     * @param ttlMs TTL in milliseconds for keys that don't exist
     * @return Vector of results (true = newly created, false = existed)
     */
    std::vector<bool> batchSetNX(const std::vector<std::string>& keys, 
                                 std::chrono::milliseconds ttlMs);
    
    /**
     * @brief Delete a key
     * @param key Redis key to delete
     * @return true if key was deleted
     */
    bool deleteKey(const std::string& key);
    
    /**
     * @brief Delete multiple keys
     * @param keys Vector of Redis keys to delete
     * @return Number of keys actually deleted
     */
    int deleteKeys(const std::vector<std::string>& keys);
    
    /**
     * @brief Execute a Redis command
     * @param command Redis command string
     * @return Redis reply wrapped in RAII object
     */
    RedisReply executeCommand(const std::string& command);
    
    /**
     * @brief Execute a formatted Redis command
     * @param format Printf-style format string
     * @param ... Arguments for format string
     * @return Redis reply wrapped in RAII object
     */
    RedisReply executeCommandf(const char* format, ...);
    
    /**
     * @brief Ping the Redis server
     * @return true if ping successful
     */
    bool ping();
    
    /**
     * @brief Get connection information
     */
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    std::chrono::system_clock::time_point getLastConnectTime() const { return lastConnectTime_; }
    std::chrono::system_clock::time_point getLastErrorTime() const { return lastErrorTime_; }
    
    /**
     * @brief Get error information
     */
    std::string getLastError() const;
    int getErrorCount() const { return errorCount_.load(); }
    
    /**
     * @brief Reset error counters
     */
    void resetErrorCounters();

private:
    // Connection parameters
    std::string host_;
    int port_;
    std::string password_;
    std::chrono::milliseconds connectionTimeout_;
    std::chrono::milliseconds socketTimeout_;
    
    // Redis connection
    redisContext* context_;
    mutable std::mutex contextMutex_;
    
    // Connection state
    std::atomic<ConnectionStatus> connectionStatus_;
    std::chrono::system_clock::time_point lastConnectTime_;
    std::chrono::system_clock::time_point lastErrorTime_;
    
    // Error tracking
    std::atomic<int> errorCount_;
    std::string lastError_;
    mutable std::mutex errorMutex_;
    
    /**
     * @brief Authenticate with Redis server
     * @return true if authentication successful or not needed
     */
    bool authenticate();
    
    /**
     * @brief Handle Redis connection error
     * @param operation Description of the operation that failed
     */
    void handleConnectionError(const std::string& operation);
    
    /**
     * @brief Check if Redis reply indicates an error
     * @param reply Redis reply to check
     * @return true if reply indicates error
     */
    bool isErrorReply(redisReply* reply) const;
    
    /**
     * @brief Set last error message
     * @param error Error message
     */
    void setLastError(const std::string& error);
};

} // namespace redis_deduplication