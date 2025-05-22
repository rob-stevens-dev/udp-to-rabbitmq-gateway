// components/redis-deduplication/include/redis_deduplication/connection_pool.hpp
#pragma once

#include "redis_client.hpp"
#include "types.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

namespace redis_deduplication {

/**
 * @brief RAII connection holder that automatically returns connection to pool
 */
class PooledConnection {
public:
    PooledConnection() = default;
    PooledConnection(std::shared_ptr<RedisClient> client, 
                     std::function<void(std::shared_ptr<RedisClient>)> returnFunc);
    
    // Move semantics only
    PooledConnection(PooledConnection&& other) noexcept;
    PooledConnection& operator=(PooledConnection&& other) noexcept;
    
    // No copy semantics
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    
    ~PooledConnection();
    
    // Access to the underlying client
    RedisClient* operator->() { return client_.get(); }
    const RedisClient* operator->() const { return client_.get(); }
    RedisClient& operator*() { return *client_; }
    const RedisClient& operator*() const { return *client_; }
    
    // Check if connection is valid
    explicit operator bool() const { return client_ != nullptr; }
    bool isValid() const { return client_ != nullptr; }
    
    // Release the connection back to pool manually
    void release();

private:
    std::shared_ptr<RedisClient> client_;
    std::function<void(std::shared_ptr<RedisClient>)> returnFunc_;
    bool released_ = false;
};

/**
 * @brief Thread-safe Redis connection pool with health monitoring
 */
class ConnectionPool {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the pool
     */
    explicit ConnectionPool(const DeduplicationConfig& config);
    
    /**
     * @brief Destructor - ensures all connections are properly closed
     */
    ~ConnectionPool();
    
    // Non-copyable, non-movable
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;
    
    /**
     * @brief Initialize the connection pool
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the connection pool
     */
    void shutdown();
    
    /**
     * @brief Get a connection from the pool
     * @param timeout Maximum time to wait for a connection
     * @return RAII connection holder, or invalid holder if timeout/error
     */
    PooledConnection getConnection(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
    
    /**
     * @brief Check if pool is healthy (has active connections)
     * @return true if pool has healthy connections
     */
    bool isHealthy() const;
    
    /**
     * @brief Check if using standby Redis instance
     * @return true if currently connected to standby
     */
    bool isUsingStandby() const { return usingStandby_.load(); }
    
    /**
     * @brief Force failover to standby instance
     * @return true if failover successful
     */
    bool failoverToStandby();
    
    /**
     * @brief Attempt to failback to master instance
     * @return true if failback successful
     */
    bool failbackToMaster();
    
    /**
     * @brief Get connection pool statistics
     * @return Current pool statistics
     */
    ConnectionPoolStats getStats() const;
    
    /**
     * @brief Reset pool statistics
     */
    void resetStats();
    
    /**
     * @brief Test all connections in the pool
     * @return Number of healthy connections
     */
    int testConnections();

private:
    // Configuration
    DeduplicationConfig config_;
    
    // Connection queues
    std::queue<std::shared_ptr<RedisClient>> availableConnections_;
    std::queue<std::shared_ptr<RedisClient>> standbyConnections_;
    
    // Thread safety
    mutable std::mutex poolMutex_;
    std::condition_variable connectionAvailable_;
    
    // Pool state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shuttingDown_{false};
    std::atomic<bool> usingStandby_{false};
    
    // Statistics
    mutable std::atomic<int> totalConnections_{0};
    mutable std::atomic<int> activeConnections_{0};
    mutable std::atomic<int> failedConnections_{0};
    mutable std::atomic<int> reconnectAttempts_{0};
    std::chrono::system_clock::time_point lastConnectionTime_;
    std::chrono::system_clock::time_point lastFailureTime_;
    
    // Health monitoring
    std::thread healthMonitorThread_;
    std::atomic<bool> healthMonitorRunning_{false};
    std::chrono::milliseconds healthCheckInterval_{30000}; // 30 seconds
    
    /**
     * @brief Create a new Redis connection
     * @param useStandby Whether to connect to standby instance
     * @return New Redis client, or nullptr if failed
     */
    std::shared_ptr<RedisClient> createConnection(bool useStandby = false);
    
    /**
     * @brief Return a connection to the pool
     * @param client Connection to return
     */
    void returnConnection(std::shared_ptr<RedisClient> client);
    
    /**
     * @brief Health monitoring thread function
     */
    void healthMonitorLoop();
    
    /**
     * @brief Check health of a single connection
     * @param client Connection to check
     * @return true if connection is healthy
     */
    bool isConnectionHealthy(const std::shared_ptr<RedisClient>& client);
    
    /**
     * @brief Replace failed connections with new ones
     * @return Number of connections replaced
     */
    int replaceFailedConnections();
    
    /**
     * @brief Attempt to reconnect all failed connections
     * @return Number of successful reconnections
     */
    int reconnectFailedConnections();
    
    /**
     * @brief Switch to standby connections
     */
    void switchToStandby();
    
    /**
     * @brief Switch back to master connections
     */
    void switchToMaster();
    
    /**
     * @brief Get current Redis host based on failover state
     */
    std::string getCurrentHost() const;
    
    /**
     * @brief Get current Redis port based on failover state
     */
    int getCurrentPort() const;
    
    friend class PooledConnection;
};

} // namespace redis_deduplication