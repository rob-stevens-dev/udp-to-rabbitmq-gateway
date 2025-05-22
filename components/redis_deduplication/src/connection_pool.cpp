// components/redis-deduplication/src/connection_pool.cpp
#include "redis_deduplication/connection_pool.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace redis_deduplication {

// PooledConnection Implementation
PooledConnection::PooledConnection(std::shared_ptr<RedisClient> client, 
                                   std::function<void(std::shared_ptr<RedisClient>)> returnFunc)
    : client_(std::move(client)), returnFunc_(std::move(returnFunc)), released_(false) {
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : client_(std::move(other.client_)), returnFunc_(std::move(other.returnFunc_)),
      released_(other.released_) {
    other.released_ = true;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        release(); // Release current connection
        
        client_ = std::move(other.client_);
        returnFunc_ = std::move(other.returnFunc_);
        released_ = other.released_;
        
        other.released_ = true;
    }
    return *this;
}

PooledConnection::~PooledConnection() {
    release();
}

void PooledConnection::release() {
    if (!released_ && client_ && returnFunc_) {
        returnFunc_(client_);
        released_ = true;
    }
}

// ConnectionPool Implementation
ConnectionPool::ConnectionPool(const DeduplicationConfig& config)
    : config_(config), lastConnectionTime_(std::chrono::system_clock::now()),
      lastFailureTime_(std::chrono::system_clock::time_point::min()) {
    
    spdlog::info("ConnectionPool created with pool size: {}", config_.connectionPoolSize);
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

bool ConnectionPool::initialize() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    if (initialized_.load()) {
        spdlog::warn("ConnectionPool already initialized");
        return true;
    }
    
    spdlog::info("Initializing connection pool with {} connections", config_.connectionPoolSize);
    
    // Create master connections
    for (int i = 0; i < config_.connectionPoolSize; ++i) {
        auto connection = createConnection(false);
        if (connection) {
            availableConnections_.push(connection);
            totalConnections_++;
        } else {
            spdlog::error("Failed to create connection {} during initialization", i + 1);
        }
    }
    
    // Create standby connections if standby is configured
    if (!config_.standbyHost.empty()) {
        for (int i = 0; i < config_.connectionPoolSize; ++i) {
            auto connection = createConnection(true);
            if (connection) {
                standbyConnections_.push(connection);
            } else {
                spdlog::warn("Failed to create standby connection {} during initialization", i + 1);
            }
        }
    }
    
    if (availableConnections_.empty()) {
        spdlog::error("Failed to create any connections during initialization");
        return false;
    }
    
    // Start health monitoring thread
    healthMonitorRunning_ = true;
    healthMonitorThread_ = std::thread(&ConnectionPool::healthMonitorLoop, this);
    
    initialized_ = true;
    spdlog::info("ConnectionPool initialized with {} master connections", 
                 availableConnections_.size());
    
    return true;
}

void ConnectionPool::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    spdlog::info("Shutting down connection pool");
    
    shuttingDown_ = true;
    
    // Stop health monitoring
    if (healthMonitorRunning_.load()) {
        healthMonitorRunning_ = false;
        if (healthMonitorThread_.joinable()) {
            healthMonitorThread_.join();
        }
    }
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Clear all connections
    while (!availableConnections_.empty()) {
        availableConnections_.pop();
    }
    
    while (!standbyConnections_.empty()) {
        standbyConnections_.pop();
    }
    
    totalConnections_ = 0;
    activeConnections_ = 0;
    initialized_ = false;
    
    spdlog::info("Connection pool shutdown complete");
}

PooledConnection ConnectionPool::getConnection(std::chrono::milliseconds timeout) {
    if (!initialized_.load() || shuttingDown_.load()) {
        spdlog::error("Connection pool not initialized or shutting down");
        return PooledConnection();
    }
    
    std::unique_lock<std::mutex> lock(poolMutex_);
    
    auto deadline = std::chrono::steady_clock::now() + timeout;
    
    // Wait for available connection or timeout
    while (availableConnections_.empty() && !shuttingDown_.load()) {
        if (connectionAvailable_.wait_until(lock, deadline) == std::cv_status::timeout) {
            spdlog::warn("Timeout waiting for available connection");
            return PooledConnection();
        }
    }
    
    if (shuttingDown_.load() || availableConnections_.empty()) {
        return PooledConnection();
    }
    
    // Get connection from pool
    auto connection = availableConnections_.front();
    availableConnections_.pop();
    activeConnections_++;
    
    lock.unlock();
    
    // Test connection health before returning
    if (!isConnectionHealthy(connection)) {
        spdlog::warn("Unhealthy connection detected, attempting to replace");
        
        lock.lock();
        activeConnections_--;
        failedConnections_++;
        
        // Try to create a replacement connection
        auto replacement = createConnection(usingStandby_.load());
        if (replacement) {
            totalConnections_++;
            activeConnections_++;
            lock.unlock();
            
            return PooledConnection(replacement, 
                [this](std::shared_ptr<RedisClient> client) {
                    this->returnConnection(client);
                });
        } else {
            lock.unlock();
            return PooledConnection();
        }
    }
    
    return PooledConnection(connection, 
        [this](std::shared_ptr<RedisClient> client) {
            this->returnConnection(client);
        });
}

bool ConnectionPool::isHealthy() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return !availableConnections_.empty() || activeConnections_.load() > 0;
}

bool ConnectionPool::failoverToStandby() {
    if (config_.standbyHost.empty()) {
        spdlog::warn("No standby host configured for failover");
        return false;
    }
    
    if (usingStandby_.load()) {
        spdlog::info("Already using standby connections");
        return true;
    }
    
    spdlog::info("Failing over to standby Redis instance");
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    switchToStandby();
    
    return !availableConnections_.empty();
}

bool ConnectionPool::failbackToMaster() {
    if (!usingStandby_.load()) {
        spdlog::info("Already using master connections");
        return true;
    }
    
    spdlog::info("Failing back to master Redis instance");
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Test master connectivity first
    auto testConnection = createConnection(false);
    if (!testConnection || !isConnectionHealthy(testConnection)) {
        spdlog::warn("Master Redis still not accessible, staying on standby");
        return false;
    }
    
    switchToMaster();
    return true;
}

ConnectionPoolStats ConnectionPool::getStats() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    ConnectionPoolStats stats;
    stats.totalConnections = totalConnections_.load();
    stats.activeConnections = activeConnections_.load();
    stats.idleConnections = static_cast<int>(availableConnections_.size());
    stats.failedConnections = failedConnections_.load();
    stats.reconnectAttempts = reconnectAttempts_.load();
    stats.lastConnectionTime = lastConnectionTime_;
    stats.lastFailureTime = lastFailureTime_;
    
    return stats;
}

void ConnectionPool::resetStats() {
    totalConnections_ = static_cast<int>(availableConnections_.size()) + activeConnections_.load();
    failedConnections_ = 0;
    reconnectAttempts_ = 0;
    lastConnectionTime_ = std::chrono::system_clock::now();
    lastFailureTime_ = std::chrono::system_clock::time_point::min();
    
    spdlog::debug("Connection pool stats reset");
}

int ConnectionPool::testConnections() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    int healthyCount = 0;
    std::queue<std::shared_ptr<RedisClient>> tempQueue;
    
    // Test all available connections
    while (!availableConnections_.empty()) {
        auto connection = availableConnections_.front();
        availableConnections_.pop();
        
        if (isConnectionHealthy(connection)) {
            tempQueue.push(connection);
            healthyCount++;
        } else {
            failedConnections_++;
            spdlog::debug("Found unhealthy connection during test");
        }
    }
    
    // Restore healthy connections
    availableConnections_ = std::move(tempQueue);
    
    spdlog::info("Connection test completed: {}/{} connections healthy", 
                 healthyCount, healthyCount + failedConnections_.load());
    
    return healthyCount;
}

std::shared_ptr<RedisClient> ConnectionPool::createConnection(bool useStandby) {
    const std::string& host = useStandby ? config_.standbyHost : config_.masterHost;
    const int port = useStandby ? config_.standbyPort : config_.masterPort;
    
    try {
        auto client = std::make_shared<RedisClient>(
            host, port, config_.password, 
            config_.connectionTimeout, config_.socketTimeout);
        
        if (client->connect()) {
            lastConnectionTime_ = std::chrono::system_clock::now();
            spdlog::debug("Created new connection to {}:{}", host, port);
            return client;
        } else {
            lastFailureTime_ = std::chrono::system_clock::now();
            spdlog::error("Failed to connect to {}:{}", host, port);
            return nullptr;
        }
    } catch (const std::exception& e) {
        lastFailureTime_ = std::chrono::system_clock::now();
        spdlog::error("Exception creating connection to {}:{}: {}", host, port, e.what());
        return nullptr;
    }
}

void ConnectionPool::returnConnection(std::shared_ptr<RedisClient> client) {
    if (!client || shuttingDown_.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    activeConnections_--;
    
    // Test connection health before returning to pool
    if (isConnectionHealthy(client)) {
        availableConnections_.push(client);
        connectionAvailable_.notify_one();
    } else {
        failedConnections_++;
        spdlog::debug("Discarding unhealthy connection");
    }
}

void ConnectionPool::healthMonitorLoop() {
    spdlog::debug("Health monitor thread started");
    
    while (healthMonitorRunning_.load()) {
        std::this_thread::sleep_for(healthCheckInterval_);
        
        if (!healthMonitorRunning_.load()) {
            break;
        }
        
        try {
            // Replace failed connections
            int replaced = replaceFailedConnections();
            if (replaced > 0) {
                spdlog::info("Replaced {} failed connections", replaced);
            }
            
            // Check for master recovery if using standby
            if (usingStandby_.load() && !config_.masterHost.empty()) {
                auto testConnection = createConnection(false);
                if (testConnection && isConnectionHealthy(testConnection)) {
                    spdlog::info("Master Redis appears to be healthy again");
                    // Don't auto-failback, let application decide
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception in health monitor: {}", e.what());
        }
    }
    
    spdlog::debug("Health monitor thread stopped");
}

bool ConnectionPool::isConnectionHealthy(const std::shared_ptr<RedisClient>& client) {
    if (!client) {
        return false;
    }
    
    return client->getConnectionStatus() == ConnectionStatus::CONNECTED && 
           client->ping();
}

int ConnectionPool::replaceFailedConnections() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    const int targetSize = config_.connectionPoolSize;
    const int currentSize = static_cast<int>(availableConnections_.size());
    const int needed = targetSize - currentSize;
    
    if (needed <= 0) {
        return 0;
    }
    
    int created = 0;
    for (int i = 0; i < needed; ++i) {
        auto connection = createConnection(usingStandby_.load());
        if (connection) {
            availableConnections_.push(connection);
            created++;
            reconnectAttempts_++;
        }
    }
    
    totalConnections_ = static_cast<int>(availableConnections_.size()) + activeConnections_.load();
    
    if (created > 0) {
        connectionAvailable_.notify_all();
    }
    
    return created;
}

int ConnectionPool::reconnectFailedConnections() {
    // This is handled by replaceFailedConnections
    return replaceFailedConnections();
}

void ConnectionPool::switchToStandby() {
    if (config_.standbyHost.empty()) {
        return;
    }
    
    spdlog::info("Switching to standby connections");
    
    // Move current connections to temporary storage
    std::queue<std::shared_ptr<RedisClient>> oldConnections;
    availableConnections_.swap(oldConnections);
    
    // Switch to standby connections
    availableConnections_.swap(standbyConnections_);
    usingStandby_ = true;
    
    // Refill standby queue with old connections (for potential failback)
    standbyConnections_.swap(oldConnections);
    
    spdlog::info("Switched to standby with {} connections", availableConnections_.size());
    connectionAvailable_.notify_all();
}

void ConnectionPool::switchToMaster() {
    spdlog::info("Switching to master connections");
    
    // Create fresh master connections
    std::queue<std::shared_ptr<RedisClient>> newMasterConnections;
    
    for (int i = 0; i < config_.connectionPoolSize; ++i) {
        auto connection = createConnection(false);
        if (connection) {
            newMasterConnections.push(connection);
        }
    }
    
    if (!newMasterConnections.empty()) {
        // Switch to new master connections
        availableConnections_.swap(newMasterConnections);
        usingStandby_ = false;
        
        spdlog::info("Switched to master with {} connections", availableConnections_.size());
        connectionAvailable_.notify_all();
    } else {
        spdlog::error("Failed to create master connections for failback");
    }
}

std::string ConnectionPool::getCurrentHost() const {
    return usingStandby_.load() ? config_.standbyHost : config_.masterHost;
}

int ConnectionPool::getCurrentPort() const {
    return usingStandby_.load() ? config_.standbyPort : config_.masterPort;
}

} // namespace redis_deduplication