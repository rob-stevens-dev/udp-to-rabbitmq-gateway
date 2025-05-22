// components/redis-deduplication/src/types.cpp
#include "redis_deduplication/types.hpp"
#include <sstream>
#include <iomanip>

namespace redis_deduplication {

// Helper functions for DeduplicationStats
std::string toString(const DeduplicationStats& stats) {
    std::ostringstream oss;
    oss << "DeduplicationStats {\n";
    oss << "  Total Checks: " << stats.totalChecks << "\n";
    oss << "  Duplicates Found: " << stats.duplicatesFound << "\n";
    oss << "  Unique Messages: " << stats.uniqueMessages << "\n";
    oss << "  Redis Failures: " << stats.redisFailures << "\n";
    oss << "  Cache Hits: " << stats.localCacheHits << "\n";
    oss << "  Cache Misses: " << stats.localCacheMisses << "\n";
    oss << "  Average Latency: " << stats.averageCheckLatency.count() << "μs\n";
    oss << "  Active Connections: " << stats.activeConnections << "\n";
    oss << "  Total Connections: " << stats.totalConnections << "\n";
    oss << "}";
    return oss.str();
}

// Helper functions for ConnectionPoolStats
std::string toString(const ConnectionPoolStats& stats) {
    std::ostringstream oss;
    oss << "ConnectionPoolStats {\n";
    oss << "  Total Connections: " << stats.totalConnections << "\n";
    oss << "  Active Connections: " << stats.activeConnections << "\n";
    oss << "  Idle Connections: " << stats.idleConnections << "\n";
    oss << "  Failed Connections: " << stats.failedConnections << "\n";
    oss << "  Reconnect Attempts: " << stats.reconnectAttempts << "\n";
    oss << "}";
    return oss.str();
}

// Helper functions for DeduplicationResult
std::string toString(DeduplicationResult result) {
    switch (result) {
        case DeduplicationResult::UNIQUE:
            return "UNIQUE";
        case DeduplicationResult::DUPLICATE:
            return "DUPLICATE";
        case DeduplicationResult::ERROR:
            return "ERROR";
        case DeduplicationResult::CACHE_MISS:
            return "CACHE_MISS";
        default:
            return "UNKNOWN";
    }
}

// Helper functions for ConnectionStatus
std::string toString(ConnectionStatus status) {
    switch (status) {
        case ConnectionStatus::CONNECTED:
            return "CONNECTED";
        case ConnectionStatus::DISCONNECTED:
            return "DISCONNECTED";
        case ConnectionStatus::CONNECTING:
            return "CONNECTING";
        case ConnectionStatus::FAILED:
            return "FAILED";
        case ConnectionStatus::STANDBY_ACTIVE:
            return "STANDBY_ACTIVE";
        default:
            return "UNKNOWN";
    }
}

// Validation functions for configuration
bool isValid(const DeduplicationConfig& config) {
    // Check required fields
    if (config.masterHost.empty()) {
        return false;
    }
    
    if (config.masterPort <= 0 || config.masterPort > 65535) {
        return false;
    }
    
    if (config.connectionPoolSize <= 0 || config.connectionPoolSize > 100) {
        return false;
    }
    
    if (config.deduplicationWindow.count() <= 0) {
        return false;
    }
    
    if (config.localCacheSize == 0) {
        return false;
    }
    
    // Check standby configuration if provided
    if (!config.standbyHost.empty()) {
        if (config.standbyPort <= 0 || config.standbyPort > 65535) {
            return false;
        }
    }
    
    // Check timeout values
    if (config.connectionTimeout.count() <= 0 || 
        config.socketTimeout.count() <= 0) {
        return false;
    }
    
    return true;
}

// Configuration factory functions
DeduplicationConfig createDefaultConfig() {
    DeduplicationConfig config;
    // All defaults are already set in the struct definition
    return config;
}

DeduplicationConfig createProductionConfig(const std::string& masterHost, 
                                         int masterPort,
                                         const std::string& password) {
    DeduplicationConfig config;
    
    config.masterHost = masterHost;
    config.masterPort = masterPort;
    config.password = password;
    
    // Production optimized settings
    config.connectionPoolSize = 10;
    config.connectionTimeout = std::chrono::milliseconds{3000};
    config.socketTimeout = std::chrono::milliseconds{2000};
    config.deduplicationWindow = std::chrono::milliseconds{1800000}; // 30 minutes
    config.localCacheSize = 50000;
    config.enableLocalCacheFallback = true;
    config.localCacheTtl = std::chrono::milliseconds{1800000}; // 30 minutes
    config.reconnectInterval = std::chrono::milliseconds{1000};
    config.maxReconnectAttempts = 5;
    config.backoffMultiplier = 2.0;
    config.maxReconnectInterval = std::chrono::milliseconds{60000}; // 1 minute
    config.enablePipelining = true;
    config.maxPipelineSize = 200;
    config.enableDebugLogging = false;
    
    return config;
}

DeduplicationConfig createDevelopmentConfig(const std::string& masterHost,
                                          int masterPort) {
    DeduplicationConfig config;
    
    config.masterHost = masterHost;
    config.masterPort = masterPort;
    
    // Development friendly settings
    config.connectionPoolSize = 3;
    config.connectionTimeout = std::chrono::milliseconds{5000};
    config.socketTimeout = std::chrono::milliseconds{3000};
    config.deduplicationWindow = std::chrono::milliseconds{300000}; // 5 minutes
    config.localCacheSize = 1000;
    config.enableLocalCacheFallback = true;
    config.localCacheTtl = std::chrono::milliseconds{300000}; // 5 minutes
    config.reconnectInterval = std::chrono::milliseconds{2000};
    config.maxReconnectAttempts = 3;
    config.backoffMultiplier = 1.5;
    config.maxReconnectInterval = std::chrono::milliseconds{30000}; // 30 seconds
    config.enablePipelining = false; // Easier debugging
    config.maxPipelineSize = 10;
    config.enableDebugLogging = true;
    
    return config;
}

// Utility functions for batch results
double calculateSuccessRate(const BatchDeduplicationResult& result) {
    if (result.results.empty()) {
        return 0.0;
    }
    
    int successCount = 0;
    for (const auto& res : result.results) {
        if (res != DeduplicationResult::ERROR) {
            successCount++;
        }
    }
    
    return static_cast<double>(successCount) / result.results.size();
}

int countByResult(const BatchDeduplicationResult& result, DeduplicationResult targetResult) {
    return static_cast<int>(std::count(result.results.begin(), result.results.end(), targetResult));
}

} // namespace redis_deduplication